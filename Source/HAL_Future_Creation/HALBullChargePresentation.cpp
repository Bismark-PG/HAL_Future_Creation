#include "HALBullChargeComponent.h"
#include "HALBullChargeAbility.h"
#include "HALVehiclePresentationComponent.h"
#include "HALVehicleStatusFXSettings.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Misc/App.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarChargeFXLog(TEXT("hal.BullChargeFXLog"), 0,
		TEXT("Car04 cosmetic lifecycle, mount diagnostics and accepted impact events. 0=off, 1=on."));
	double ServerTime(const UWorld& World)
	{
		const auto* State = World.GetGameState();
		return State ? State->GetServerWorldTimeSeconds() : World.GetTimeSeconds();
	}
	bool CanDraw(const UActorComponent& Component)
	{
		return FApp::CanEverRender() && Component.GetNetMode() != NM_DedicatedServer
			&& Component.GetWorld() && Component.GetWorld()->FXSystem.IsValid()
			&& GetDefault<UHALVehicleStatusFXSettings>()->MaximumParticleLayersPerVehicle > 0;
	}
	void SetColors(UNiagaraComponent& C, const FHALBullChargeFXConfig& FX)
	{
		C.SetVariableLinearColor(TEXT("User.SparkColor"), FX.SparkColor);
		C.SetVariableLinearColor(TEXT("User.HighlightColor"), FX.HighlightColor);
		C.SetVariableVec4(TEXT("User.StyleParameters"), FVector4(FX.GlowStrength, 0, 0, 0));
	}
	void Destroy(UNiagaraComponent* Component)
	{
		if (IsValid(Component)) { Component->DestroyComponent(); }
	}
}

bool FHALBullChargeFXConfig::IsValid() const
{
	const auto R = [](double V, double Min, double Max) { return FMath::IsFinite(V) && V >= Min && V <= Max; };
	const auto Size = [&](FVector2D V, float Max) { return R(V.X, 1, Max) && R(V.Y, 1, Max); };
	const auto Color = [&](FLinearColor C) { return R(C.R, 0, 10) && R(C.G, 0, 10) && R(C.B, 0, 10) && R(C.A, 0, 1); };
	if (!FrontMount.IsValid() || RearMounts.Num() > 4) { return false; }
	for (const auto& Mount : RearMounts) { if (!Mount.IsValid()) { return false; } }
	return R(WindSpawnRate, 0, 100) && R(WindLifetime, .08f, 1) && R(WindSizeScale, .25f, 4) && R(WindLengthScale, .1f, 4)
		&& R(WindFlowSpeed, 0, 8) && R(WindResponseSeconds, 0, .5f)
		&& R(WindTurbulence, 0, 30) && R(PressureTurbulence, 0, 10)
		&& Color(WindContourColor) && R(WindContourWidth, 0, .3f)
		&& R(PressureSize.X, 10, 500) && R(PressureSize.Y, 10, 500) && R(PressureSize.Z, 10, 500)
		&& R(PressureOpacity, 0, 1) && R(PressureFeather, .05f, .6f)
		&& Color(PressureShadowColor) && Color(PressureMidColor) && Color(PressureLightColor)
		&& R(PressureShadowThreshold, 0, 1) && R(PressureLightThreshold, 0, 1)
		&& PressureLightThreshold >= PressureShadowThreshold + .02f && R(PressureToneSoftness, .02f, .4f)
		&& R(WindOpacity, 0, 1) && R(WindFullSpeed, 1, 12000) && R(RearSpawnRate, 0, 300)
		&& R(RearLifetime, .08f, 1.2f) && R(RearSpeed, 100, 4000) && R(RearSpreadDegrees, 1, 40)
		&& Size(RearSize, 100) && ContactCount >= 1 && ContactCount <= 96 && Size(ContactSize, 200)
		&& R(ContactLifetime, .1f, 1.2f) && R(ContactSpeed, 100, 4000) && R(ContactSpreadDegrees, 5, 70)
		&& R(FlashLifetime, .04f, .3f) && R(FlashSize, 10, 400) && R(ShockLifetime, .1f, .8f)
		&& R(ShockSize, 20, 600) && Color(SparkColor) && Color(HighlightColor) && R(GlowStrength, 0, 3);
}

float HALBullChargeFX::Ramp(double Elapsed, float RampSeconds)
{
	if (!FMath::IsFinite(Elapsed) || Elapsed < 0 || !FMath::IsFinite(RampSeconds)) { return 0; }
	return RampSeconds > 0 ? float(FMath::Clamp(Elapsed / RampSeconds, 0., 1.)) : 1.f;
}

float HALBullChargeFX::WindStrength(float Speed, float FullSpeed, float Progress, bool bDriving)
{
	if (!bDriving || !FMath::IsFinite(Speed) || !FMath::IsFinite(FullSpeed) || FullSpeed <= 0
		|| !FMath::IsFinite(Progress)) { return 0; }
	return FMath::Sqrt(FMath::Clamp(Speed / FullSpeed, 0.f, 1.f)) * FMath::Clamp(Progress, 0.f, 1.f);
}

float HALBullChargeFX::SmoothWind(float Current, float Target, float Delta, float Response, bool bDriving)
{
	if (!bDriving || !FMath::IsFinite(Target) || !FMath::IsFinite(Current)) { return 0; }
	const float Alpha = Response > 0 ? 1.f - FMath::Exp(-FMath::Max(0.f, Delta) / Response) : 1.f;
	return FMath::Clamp(FMath::Lerp(Current, Target, Alpha), 0.f, 1.f);
}

void HALBullChargeFX::VelocityBox(const FVector& Direction, float Speed, float Spread, FVector& Min, FVector& Max)
{
	const FVector D = Direction.GetSafeNormal();
	const double Width = FMath::Min(.5, FMath::Tan(FMath::DegreesToRadians(double(Spread))) / FMath::Sqrt(3.));
	const FVector Center = D * Speed;
	Min = Center - FVector(Speed * Width);
	Max = Center + FVector(Speed * Width);
}

void UHALBullChargeComponent::PublishDrivingVisual()
{
	if (!GetOwner()->HasAuthority() || !bActive || !VisualState.FX.bEnabled) { return; }
	const bool bDriving = CanResolveContacts();
	if (VisualState.bDriving != bDriving)
	{
		VisualState.bDriving = bDriving;
		GetOwner()->ForceNetUpdate();
		UE_CLOG(CVarChargeFXLog.GetValueOnGameThread(), LogTemp, Log, TEXT("BullChargeFX driving Pawn=%s Driving=%d Activation=%s"),
			*GetNameSafe(GetOwner()), bDriving, *VisualState.Activation.ToString());
	}
}

void UHALBullChargeComponent::OnRep_VisualState()
{
	if (DrawActivation != VisualState.Activation)
	{
		ClearParticles();
		// Retain the retired identity while inactive so a delayed RPC cannot repopulate the queue.
		if (VisualState.Activation.IsValid())
		{
			DrawActivation = VisualState.Activation;
			LastImpactFXSequence = 0;
		}
		WarnedMounts.Reset();
	}
	const bool bValid = VisualState.Activation.IsValid() && VisualState.FX.bEnabled
		&& VisualState.FX.IsValid() && GetNetMode() != NM_DedicatedServer;
	SetComponentTickEnabled(bValid);
	if (!bValid) { ClearParticles(); PendingImpactFX.Reset(); }
	else
	{
		auto Events = MoveTemp(PendingImpactFX);
		PendingImpactFX.Reset();
		for (const auto& Event : Events) { ConsumeImpactFX(Event); }
	}
	UE_CLOG(CVarChargeFXLog.GetValueOnGameThread(), LogTemp, Log, TEXT("BullChargeFX state Pawn=%s Enabled=%d Driving=%d Activation=%s"),
		*GetNameSafe(GetOwner()), bValid, VisualState.bDriving, *VisualState.Activation.ToString());
}

void UHALBullChargeComponent::ClearParticles()
{
	Destroy(FrontWindComponent); FrontWindComponent = nullptr; FrontMaterial = nullptr; PressureMaterial = nullptr;
	SmoothedWind = 0;
	for (const auto& C : RearSparkComponents) { Destroy(C.Get()); }
	RearSparkComponents.Reset();
	for (const auto& P : ImpactParticles) { Destroy(P.Component.Get()); }
	ImpactParticles.Reset();
}

void UHALBullChargeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
	Super::TickComponent(DeltaTime, TickType, Function);
	if (!VisualState.Activation.IsValid() || !VisualState.FX.bEnabled || !VisualState.FX.IsValid()
		|| ServerTime(*GetWorld()) >= VisualState.StartServerTime + VisualState.DurationSeconds)
	{
		ClearParticles(); PendingImpactFX.Reset(); SetComponentTickEnabled(false); return;
	}
	auto Events = MoveTemp(PendingImpactFX);
	PendingImpactFX.Reset();
	for (const auto& Event : Events) { ConsumeImpactFX(Event); }
	const double Now = ServerTime(*GetWorld());
	for (int32 I = ImpactParticles.Num() - 1; I >= 0; --I)
	{
		if (!ImpactParticles[I].Component.IsValid() || Now >= ImpactParticles[I].Expires)
		{
			Destroy(ImpactParticles[I].Component.Get()); ImpactParticles.RemoveAtSwap(I);
		}
	}
	if (CanDraw(*this)) { UpdateParticles(DeltaTime); }
}

void UHALBullChargeComponent::UpdateParticles(float DeltaTime)
{
	const auto& FX = VisualState.FX;
	auto* Body = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	const auto* Presentation = GetOwner()->FindComponentByClass<UHALVehiclePresentationComponent>();
	if (!Body || !Presentation) { ClearParticles(); return; }
	FHALVehicleFXTargetSnapshot Targets;
	Presentation->CaptureStatusFXTargets(Targets);
	const int32 Budget = FMath::Clamp(GetDefault<UHALVehicleStatusFXSettings>()->MaximumParticleLayersPerVehicle, 0, 16);
	const float Progress = HALBullChargeFX::Ramp(ServerTime(*GetWorld()) - VisualState.StartServerTime, VisualState.RampSeconds);
	const float Speed = FVector::DotProduct(Body->GetPhysicsLinearVelocity(), Body->GetForwardVector().GetSafeNormal2D());
	// Smooth speed response independently, then apply the shared acceleration clock. Smoothing the
	// ramp itself would delay reaching full strength beyond Motion.RampSeconds, especially at low fps.
	const float TargetWind = HALBullChargeFX::WindStrength(Speed, FX.WindFullSpeed, 1, VisualState.bDriving);
	SmoothedWind = HALBullChargeFX::SmoothWind(SmoothedWind, TargetWind, DeltaTime, FX.WindResponseSeconds, VisualState.bDriving);
	const float Wind = SmoothedWind * Progress;
	const float Rear = VisualState.bDriving ? Progress : 0.f;
	bool bCreated = false;
	const auto Ensure = [&](TObjectPtr<UNiagaraComponent>& C, UNiagaraSystem* System, const FHALVehicleFXMount& Mount,
		FName Layer, FHALResolvedFXMount& Resolved) -> bool
	{
		bCreated = false;
		// Activate/Tick on UNiagaraComponent polls pending compilation, DDC and PSO readiness.
		// Gating creation on System->IsReadyToRun would prevent that progress in editor -game.
		if (!System) { Destroy(C); C = nullptr; return false; }
		if (!HALVehicleFX::ResolveMount(Targets, Mount, Resolved))
		{
			if (!WarnedMounts.Contains(Layer))
			{
				WarnedMounts.Add(Layer);
				UE_CLOG(CVarChargeFXLog.GetValueOnGameThread(), LogTemp, Warning,
					TEXT("BullChargeFX mount unavailable Pawn=%s Layer=%s Part=%s Socket=%s"),
					*GetNameSafe(GetOwner()), *Layer.ToString(), *Mount.PartId.ToString(), *Mount.SocketOrBone.ToString());
			}
			Destroy(C); C = nullptr; return false;
		}
		if (C && (C->GetAsset() != System || C->GetAttachParent() != Resolved.Mesh.Get()
			|| C->GetAttachSocketName() != Resolved.Socket)) { Destroy(C); C = nullptr; }
		if (!C)
		{
			bCreated = true;
			if (Layer == TEXT("Front")) { FrontMaterial = nullptr; PressureMaterial = nullptr; }
			C = NewObject<UNiagaraComponent>(GetOwner());
			C->SetAutoActivate(false); C->SetAsset(System);
			C->SetupAttachment(Resolved.Mesh.Get(), Resolved.Socket);
			C->SetRelativeTransform(Resolved.Relative);
			C->RegisterComponent();
			C->AddTickPrerequisiteComponent(this);
			UE_CLOG(CVarChargeFXLog.GetValueOnGameThread(), LogTemp, Log,
				TEXT("BullChargeFX attached Pawn=%s Layer=%s Parent=%s Socket=%s System=%s"),
				*GetNameSafe(GetOwner()), *Layer.ToString(), *GetNameSafe(Resolved.Mesh.Get()),
				*Resolved.Socket.ToString(), *GetNameSafe(System));
			if (Resolved.bUsedFallback && !WarnedMounts.Contains(Layer))
			{
				WarnedMounts.Add(Layer);
				UE_CLOG(CVarChargeFXLog.GetValueOnGameThread(), LogTemp, Warning,
					TEXT("BullChargeFX explicit mesh-origin fallback Pawn=%s Layer=%s Socket=%s"),
					*GetNameSafe(GetOwner()), *Layer.ToString(), *Mount.SocketOrBone.ToString());
			}
		}
		C->SetRelativeTransform(Resolved.Relative);
		return true;
	};
	FHALResolvedFXMount Mount;
	if (Budget > 0 && Ensure(FrontWindComponent, FX.FrontWind, FX.FrontMount, TEXT("Front"), Mount))
	{
		FrontWindComponent->SetVariableFloat(TEXT("User.SpawnRate"), FX.WindSpawnRate * Wind);
		FrontWindComponent->SetVariableFloat(TEXT("User.Lifetime"), FX.WindLifetime);
		const FVector WindScale(FX.WindSizeScale * FX.WindLengthScale, FX.WindSizeScale, FX.WindSizeScale);
		FrontWindComponent->SetVariableVec3(TEXT("User.ScaleMin"), WindScale * .85f);
		FrontWindComponent->SetVariableVec3(TEXT("User.ScaleMax"), WindScale * 1.15f);
		FrontWindComponent->SetVariableVec3(TEXT("User.WindScale"), WindScale);
		FrontWindComponent->SetVariableVec3(TEXT("User.PressureScale"), FX.PressureSize / 100.f);
		// Sim-space offset is independent of the flow mesh scale: both layers share the shell apex.
		FrontWindComponent->SetVariableVec3(TEXT("User.FlowOrigin"), FVector(FX.PressureSize.X, 0, 0));
		if (!FrontMaterial)
		{
			const FNiagaraVariable Variable(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()), TEXT("User.WindMaterial"));
			if (auto* Material = Cast<UMaterialInterface>(FX.FrontWind->GetExposedParameters().GetUObject(Variable)))
			{
				FrontMaterial = UMaterialInstanceDynamic::Create(Material, this);
				FrontWindComponent->SetVariableMaterial(TEXT("User.WindMaterial"), FrontMaterial);
			}
		}
		if (FrontMaterial)
		{
			FrontMaterial->SetScalarParameterValue(TEXT("Envelope"), Wind);
			FrontMaterial->SetScalarParameterValue(TEXT("Opacity"), FX.WindOpacity);
			FrontMaterial->SetScalarParameterValue(TEXT("FlowSpeed"), FX.WindFlowSpeed);
			FrontMaterial->SetScalarParameterValue(TEXT("UseRuntimeTime"), 1);
			FrontMaterial->SetScalarParameterValue(TEXT("MotionTime"), float(ServerTime(*GetWorld()) - VisualState.StartServerTime));
			FrontMaterial->SetScalarParameterValue(TEXT("Turbulence"), FX.WindTurbulence / FX.WindSizeScale);
			FrontMaterial->SetScalarParameterValue(TEXT("ContourWidth"), FX.WindContourWidth);
			FrontMaterial->SetVectorParameterValue(TEXT("ContourColor"), FX.WindContourColor);
		}
		if (!PressureMaterial)
		{
			const FNiagaraVariable Variable(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()), TEXT("User.PressureMaterial"));
			if (auto* Material = Cast<UMaterialInterface>(FX.FrontWind->GetExposedParameters().GetUObject(Variable)))
			{
				PressureMaterial = UMaterialInstanceDynamic::Create(Material, this);
				FrontWindComponent->SetVariableMaterial(TEXT("User.PressureMaterial"), PressureMaterial);
			}
		}
		if (PressureMaterial)
		{
			PressureMaterial->SetScalarParameterValue(TEXT("Envelope"), Wind);
			PressureMaterial->SetScalarParameterValue(TEXT("PressureOpacity"), FX.PressureOpacity);
			PressureMaterial->SetScalarParameterValue(TEXT("Feather"), FX.PressureFeather);
			PressureMaterial->SetScalarParameterValue(TEXT("FlowSpeed"), FX.WindFlowSpeed);
			PressureMaterial->SetScalarParameterValue(TEXT("UseRuntimeTime"), 1);
			PressureMaterial->SetScalarParameterValue(TEXT("MotionTime"), float(ServerTime(*GetWorld()) - VisualState.StartServerTime));
			PressureMaterial->SetScalarParameterValue(TEXT("Turbulence"), FX.PressureTurbulence);
			PressureMaterial->SetVectorParameterValue(TEXT("ShadowColor"), FX.PressureShadowColor);
			PressureMaterial->SetVectorParameterValue(TEXT("MidColor"), FX.PressureMidColor);
			PressureMaterial->SetVectorParameterValue(TEXT("LightColor"), FX.PressureLightColor);
			PressureMaterial->SetScalarParameterValue(TEXT("ShadowThreshold"), FX.PressureShadowThreshold);
			PressureMaterial->SetScalarParameterValue(TEXT("LightThreshold"), FX.PressureLightThreshold);
			PressureMaterial->SetScalarParameterValue(TEXT("ToneSoftness"), FX.PressureToneSoftness);
		}
		if (bCreated) { FrontWindComponent->Activate(true); }
	}
	else { Destroy(FrontWindComponent); FrontWindComponent = nullptr; FrontMaterial = nullptr; PressureMaterial = nullptr; }
	while (RearSparkComponents.Num() > FX.RearMounts.Num()) { Destroy(RearSparkComponents.Pop()); }
	RearSparkComponents.SetNum(FX.RearMounts.Num());
	for (int32 I = 0; I < RearSparkComponents.Num(); ++I)
	{
		auto& C = RearSparkComponents[I];
		if (I + (FrontWindComponent ? 1 : 0) >= Budget
			|| !Ensure(C, FX.RearSparks, FX.RearMounts[I], FName(*FString::Printf(TEXT("Rear%d"), I)), Mount))
		{
			Destroy(C); C = nullptr; continue;
		}
		FVector Min, Max;
		HALBullChargeFX::VelocityBox(-Mount.World.GetUnitAxis(EAxis::X), FX.RearSpeed, FX.RearSpreadDegrees, Min, Max);
		C->SetVariableFloat(TEXT("User.SpawnRate"), FX.RearSpawnRate * Rear);
		C->SetVariableFloat(TEXT("User.LifetimeMin"), FX.RearLifetime * .65f);
		C->SetVariableFloat(TEXT("User.LifetimeMax"), FX.RearLifetime);
		C->SetVariableVec3(TEXT("User.VelocityMin"), Min); C->SetVariableVec3(TEXT("User.VelocityMax"), Max);
		C->SetVariableVec2(TEXT("User.SizeMin"), FX.RearSize * .65); C->SetVariableVec2(TEXT("User.SizeMax"), FX.RearSize);
		SetColors(*C, FX);
		if (bCreated) { C->Activate(true); }
	}
}

FHALBullChargeImpactFXEvent UHALBullChargeComponent::MakeImpactFX(const FHALPreparedCombatHit& Hit)
{
	FHALBullChargeImpactFXEvent Event;
	if (!Hit.Action || !Config || !Config->FX.bEnabled || !Config->FX.IsValid() || !bActive) { return Event; }
	Event.Activation = VisualState.Activation;
	Event.Sequence = ++NextImpactFXSequence;
	Event.ServerTime = ServerTime(*GetWorld());
	Event.Point = Hit.Request.Hit.ImpactPoint;
	Event.Normal = Hit.Request.Hit.ImpactNormal.GetSafeNormal();
	Event.Strength = FMath::Clamp(float(Hit.Request.Hit.SourceVelocity.Size2D() / Config->FullDamageSpeed), 0.f, 1.f);
	return Event;
}

void UHALBullChargeComponent::MulticastImpactFX_Implementation(FHALBullChargeImpactFXEvent Event)
{
	ConsumeImpactFX(Event);
}

void UHALBullChargeComponent::ConsumeImpactFX(const FHALBullChargeImpactFXEvent& Event)
{
	if (!GetWorld() || Event.Sequence == 0 || !Event.Activation.IsValid() || Event.Point.ContainsNaN()
		|| Event.Normal.ContainsNaN() || !FMath::IsFinite(Event.Strength)
		|| !FMath::IsFinite(Event.ServerTime) || ServerTime(*GetWorld()) - Event.ServerTime > .5
		|| Event.ServerTime > ServerTime(*GetWorld()) + .25) { return; }
	if (!VisualState.Activation.IsValid())
	{
		if (Event.Activation == DrawActivation) { return; }
		if (PendingImpactFX.Num() < 8) { PendingImpactFX.Add(Event); }
		return;
	}
	if (Event.Activation != VisualState.Activation || Event.Sequence <= LastImpactFXSequence
		|| !VisualState.FX.bEnabled || !VisualState.FX.IsValid()
		|| ServerTime(*GetWorld()) >= VisualState.StartServerTime + VisualState.DurationSeconds) { return; }
	LastImpactFXSequence = Event.Sequence;
	++ReceivedImpactFXCount;
	UE_CLOG(CVarChargeFXLog.GetValueOnGameThread(), LogTemp, Log,
		TEXT("BullChargeFX impact Pawn=%s Sequence=%u Strength=%.3f Activation=%s Point=%s"),
		*GetNameSafe(GetOwner()), Event.Sequence, Event.Strength, *Event.Activation.ToString(), *FVector(Event.Point).ToCompactString());
	SpawnImpactFX(Event);
}

void UHALBullChargeComponent::SpawnImpactFX(const FHALBullChargeImpactFXEvent& Event)
{
	if (!CanDraw(*this)) { return; }
	const auto& FX = VisualState.FX;
	const int32 Budget = FMath::Clamp(GetDefault<UHALVehicleStatusFXSettings>()->MaximumParticleLayersPerVehicle, 0, 16);
	if (!FX.ContactImpact
		|| ImpactParticles.Num() + RearSparkComponents.Num() + (FrontWindComponent ? 1 : 0) >= Budget) { return; }
	auto* C = NewObject<UNiagaraComponent>(GetOwner());
	C->SetAutoActivate(false); C->SetAsset(FX.ContactImpact);
	C->SetWorldLocation(Event.Point + Event.Normal * 8);
	C->SetWorldRotation(FRotationMatrix::MakeFromX(Event.Normal).ToQuat());
	C->SetRandomSeedOffset(int32(Event.Sequence));
	C->SetVariableInt(TEXT("User.BurstCount"), FX.ContactCount);
	C->SetVariableVec2(TEXT("User.SizeMin"), FX.ContactSize * .65); C->SetVariableVec2(TEXT("User.SizeMax"), FX.ContactSize);
	C->SetVariableFloat(TEXT("User.LifetimeMin"), FX.ContactLifetime * .65f);
	C->SetVariableFloat(TEXT("User.LifetimeMax"), FX.ContactLifetime);
	FVector Min, Max;
	HALBullChargeFX::VelocityBox(FVector::ForwardVector, FX.ContactSpeed * FMath::Lerp(.65f, 1.f, Event.Strength), FX.ContactSpreadDegrees, Min, Max);
	C->SetVariableVec3(TEXT("User.VelocityMin"), Min); C->SetVariableVec3(TEXT("User.VelocityMax"), Max);
	C->SetVariableFloat(TEXT("User.FlashLifetime"), FX.FlashLifetime);
	C->SetVariableVec2(TEXT("User.FlashSize"), FVector2D(FX.FlashSize));
	C->SetVariableFloat(TEXT("User.ShockLifetime"), FX.ShockLifetime);
	C->SetVariableVec3(TEXT("User.ShockScale"), FVector(FX.ShockSize / 200.f));
	SetColors(*C, FX);
	C->RegisterComponent(); C->Activate(true);
	ImpactParticles.Add({C, ServerTime(*GetWorld()) + FMath::Max3(FX.ContactLifetime, FX.ShockLifetime, FX.FlashLifetime) + .1});
}
