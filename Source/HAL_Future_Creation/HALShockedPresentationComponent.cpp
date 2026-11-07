#include "HALShockedPresentationComponent.h"
#include "HALAbilitySystemComponent.h"
#include "ArcadeVehicleMovementComponent.h"
#include "HALGameplayTags.h"
#include "Components/MeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TestVehiclePawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Misc/App.h"
#include "NiagaraComponent.h"
#include "HALVehicleStatusFXComponent.h"
#include "HALVehicleStatusFXRenderer.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

#include "HAL/IConsoleManager.h"

// Off by default. Every layer of this effect fails silently when a name or a
// component does not line up, so the only cheap way to tell "never ran" from
// "ran and drew nothing" is to say so out loud on request: hal.ShockedPresentationLog 1
static TAutoConsoleVariable<int32> CVarHALShockedPresentationLog(TEXT("hal.ShockedPresentationLog"), 0,
	TEXT("Log shocked presentation mounting and phase changes (0 off, 1 on)."), ECVF_Default);

namespace HALShockedOverlay
{
	static bool WantsLog() { return CVarHALShockedPresentationLog.GetValueOnGameThread() != 0; }

	static const FName BrightnessParam(TEXT("Brightness"));
	static const FName FilamentPowerParam(TEXT("FilamentPower"));
	static const FName FlashParam(TEXT("Flash"));
	static const FName WipeRadiusParam(TEXT("WipeRadius"));
	static const FName FlickerStepsParam(TEXT("FlickerSteps"));
	static const FName ScorchAmountParam(TEXT("ScorchAmount"));
	static const FName HitLocationParam(TEXT("HitLocationLocal"));

	// User parameters the ribbon asset reads. Names are the contract with that asset.
	static const FName ArcPositionsParam(TEXT("ArcPositions"));
	static const FName ArcWidthsParam(TEXT("ArcWidths"));
	static const FName ArcPointsPerRibbonParam(TEXT("ArcPointsPerRibbon"));
	static const FName ArcRibbonCountParam(TEXT("ArcRibbonCount"));
	static const FName ArcAlphaParam(TEXT("ArcAlpha"));
	static const FName ArcColorParam(TEXT("ArcColor"));

	/** Mesh is a candidate when it is a drawn body part, not the hidden physics root. */
	static bool IsOverlayTarget(const UMeshComponent& Mesh, const USceneComponent* Root)
	{
		if (&Mesh == Root || Mesh.bHiddenInGame || !Mesh.GetVisibleFlag()) { return false; }
		if (const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(&Mesh)) { return Static->GetStaticMesh() != nullptr; }
		if (const USkeletalMeshComponent* Skeletal = Cast<USkeletalMeshComponent>(&Mesh)) { return Skeletal->GetSkeletalMeshAsset() != nullptr; }
		return false;
	}
}

UHALShockedPresentationComponent::UHALShockedPresentationComponent()
{
	// The autonomous owner samples completed physics before the frame is drawn.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	if (FRichCurve* Curve = BrightnessCurve.GetRichCurve())
	{
		Curve->AddKey(0.f, 1.6f); Curve->AddKey(0.4f, 1.f); Curve->AddKey(1.f, 0.55f); Curve->AddKey(1.6f, 0.4f);
	}
	if (FRichCurve* Curve = FilamentPowerCurve.GetRichCurve())
	{
		Curve->AddKey(0.f, 34.f); Curve->AddKey(0.4f, 48.f); Curve->AddKey(1.6f, 70.f);
	}
}
void UHALShockedPresentationComponent::Bind(UHALAbilitySystemComponent* InASC)
{
	if (!InASC || InASC->GetAvatarActor() != GetOwner()) { Unbind(); return; }
	// PlayerState may bind the ASC before ClientRestart establishes local ownership.
	// Refresh the local source even when the ASC and its subscription are unchanged.
	if (ASC != InASC || !TagHandle.IsValid())
	{
		Unbind();
		ASC = InASC;
		TagHandle = ASC->RegisterGameplayTagEvent(HALGameplayTags::State_Visual_Shocked,EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this,&ThisClass::TagChanged);
	}

	const auto* Pawn = Cast<ATestVehiclePawn>(GetOwner());
	LocalBounceMovement = Pawn && Pawn->GetNetMode() == NM_Client && Pawn->IsLocallyControlled()
		? Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>() : nullptr;
	if (LocalBounceMovement.IsValid())
	{
		SetComponentTickEnabled(true);
	}
	RefreshShocked();
}
void UHALShockedPresentationComponent::Unbind()
{
	if (ASC) { ASC->RegisterGameplayTagEvent(HALGameplayTags::State_Visual_Shocked,EGameplayTagEventType::NewOrRemoved).Remove(TagHandle); }
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(ShockHandoffTimer); }
	bAwaitingAuthorityShock = false;
	TagHandle.Reset(); ASC = nullptr; LocalBounceMovement.Reset(); SetShocked(false);
	if (Phase == EHALShockPhase::Idle) { SetComponentTickEnabled(false); }
}
void UHALShockedPresentationComponent::NotifyShockOrigin(const FVector& WorldLocation)
{
	ShockOriginWS = WorldLocation; bHasShockOrigin = true;
	// Already running: re-seat the wipe origin so a second hit reads from its own direction.
	if (Phase == EHALShockPhase::Active) { PhaseSeconds = 0.f; }
}
void UHALShockedPresentationComponent::SetShocked(bool bActive)
{
	if (bShocked == bActive) { return; }
	bShocked = bActive;
	if (bShocked) { StartArc(); BeginTimeline(); } else { StopArc(); ReleaseTimeline(); }
	OnShockedChanged.Broadcast(bShocked);
}
void UHALShockedPresentationComponent::StartArc()
{
	AActor* Owner = GetOwner();
	USceneComponent* Attach = Owner ? Owner->GetRootComponent() : nullptr;
	// Headless servers, NullRHI and automation have no FX system; the shocked state itself stays inspectable.
	if (!ArcSystem || !Attach || !GetWorld() || !GetWorld()->FXSystem.IsValid() || !FApp::CanEverRender()) { return; }
	if (!IsValid(ArcComponent))
	{
		ArcComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(ArcSystem, Attach, NAME_None, ArcOffsetCm,
			FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, false, false, ENCPoolMethod::None, false);
		if (!ArcComponent) { return; }
		ArcComponent->SetRelativeScale3D(FVector(ArcScale));
	}
	ArcComponent->Activate(true);
}
void UHALShockedPresentationComponent::StopArc()
{
	// Deactivate instead of destroying so already spawned particles finish their own lifetime.
	if (IsValid(ArcComponent)) { ArcComponent->Deactivate(); }
}
UHALVehicleStatusFXRenderer* UHALShockedPresentationComponent::OverlayOwner()
{
	if (auto* Status = GetOwner() ? GetOwner()->FindComponentByClass<UHALVehicleStatusFXComponent>() : nullptr)
	{ return Status->GetSurfaceRenderer(); }
	if (!FallbackOverlayOwner) { FallbackOverlayOwner = NewObject<UHALVehicleStatusFXRenderer>(this); }
	return FallbackOverlayOwner;
}
void UHALShockedPresentationComponent::EnsureOverlay()
{
	AActor* Owner = GetOwner();
	if (!OverlayMaterial || !Owner) { return; }
	if (!OverlayInstances.IsEmpty())
	{
		// Re-shocked during the fade: the shell is still mounted, just remount what is still valid.
		for (int32 Index = 0; Index < OverlayInstances.Num(); ++Index)
		{
			if (OverlayMeshes[Index] && OverlayInstances[Index]) { OverlayOwner()->SetLegacy(OverlayMeshes[Index], OverlayInstances[Index]); }
		}
		return;
	}
	const USceneComponent* Root = Owner->GetRootComponent();
	TArray<UMeshComponent*> Candidates;
	Owner->GetComponents<UMeshComponent>(Candidates);
	for (UMeshComponent* Mesh : Candidates)
	{
		if (!Mesh) { continue; }
		if (!HALShockedOverlay::IsOverlayTarget(*Mesh, Root))
		{
			if (HALShockedOverlay::WantsLog())
			{
				UE_LOG(LogTemp, Log, TEXT("ShockedPresentation: %s skipped (root=%d hiddenInGame=%d visible=%d)"),
					*Mesh->GetName(), Mesh == Root ? 1 : 0, Mesh->bHiddenInGame ? 1 : 0, Mesh->GetVisibleFlag() ? 1 : 0);
			}
			continue;
		}
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(OverlayMaterial, this);
		if (!Instance) { continue; }
		OverlayOwner()->SetLegacy(Mesh, Instance);
		OverlayMeshes.Add(Mesh); OverlayInstances.Add(Instance);
		if (HALShockedOverlay::WantsLog())
		{
			UE_LOG(LogTemp, Log, TEXT("ShockedPresentation: overlay mounted on %s"), *Mesh->GetName());
		}
	}
}
void UHALShockedPresentationComponent::EnsureLight()
{
	AActor* Owner = GetOwner();
	USceneComponent* Attach = Owner ? Owner->GetRootComponent() : nullptr;
	if (!bEnableLight || !Attach || IsValid(ShockLight)) { return; }
	ShockLight = NewObject<UPointLightComponent>(Owner, NAME_None, RF_Transient);
	if (!ShockLight) { return; }
	ShockLight->SetupAttachment(Attach);
	ShockLight->SetRelativeLocation(LightOffsetCm);
	ShockLight->SetMobility(EComponentMobility::Movable);
	// Four vehicles can be shocked at once; flickering shadows are not worth the cost and hurt readability.
	ShockLight->SetCastShadows(false);
	ShockLight->SetIntensityUnits(ELightUnits::Candelas);
	ShockLight->SetAttenuationRadius(LightRadiusCm);
	ShockLight->SetLightColor(LightColor);
	ShockLight->SetIntensity(0.f);
	ShockLight->SetVisibility(false);
	ShockLight->RegisterComponent();
}
void UHALShockedPresentationComponent::EnsureRibbon()
{
	AActor* Owner = GetOwner();
	USceneComponent* Attach = Owner ? Owner->GetRootComponent() : nullptr;
	if (!RibbonSystem || !Attach || !GetWorld() || !GetWorld()->FXSystem.IsValid()) { return; }
	// Body extent drives arc scale, so a bigger vehicle gets proportionally bigger arcs.
	for (const TObjectPtr<UMeshComponent>& Mesh : OverlayMeshes)
	{
		if (Mesh) { BodyExtentCm = Mesh->GetLocalBounds().BoxExtent; break; }
	}
	if (!IsValid(RibbonComponent))
	{
		RibbonComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(RibbonSystem, Attach, NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
			false, false, ENCPoolMethod::None, false);
		if (!RibbonComponent) { return; }
	}
	// Layout is fixed for the session, so the asset can size its spawn burst from these.
	RibbonComponent->SetVariableInt(HALShockedOverlay::ArcPointsPerRibbonParam, HALShockArcGeometry::PointsPerRibbon(ArcSettings));
	RibbonComponent->SetVariableInt(HALShockedOverlay::ArcRibbonCountParam, HALShockArcGeometry::RibbonCount(ArcSettings));
	RibbonComponent->SetVariableLinearColor(HALShockedOverlay::ArcColorParam, ArcColor);
}
void UHALShockedPresentationComponent::PushArcs(float FilamentAlpha)
{
	if (!IsValid(RibbonComponent)) { return; }
	// Rebuild only when the jump clock steps: the arcs snap to new shapes instead of sliding.
	const int32 Step = FMath::FloorToInt(HeldSeconds * FMath::Max(1.f, FlickerSteps));
	bool bRebuilt = false;
	if (Step != LastArcStep)
	{
		LastArcStep = Step;
		HALShockArcGeometry::Build(BodyExtentCm, ArcSettings,
			uint32(Step) * 2654435761u ^ FlickerSeed, ArcPositions, ArcBaseWidths);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(RibbonComponent,
			HALShockedOverlay::ArcPositionsParam, ArcPositions);
		bRebuilt = true;
	}
	// Withdraw the arc from its tail rather than dimming the whole ribbon. Widths are already
	// per point, so the retracting front is just a mask over them and Niagara needs no change.
	const float Retract = bRetractArcsOnRelease ? 1.f - FMath::Clamp(FilamentAlpha, 0.f, 1.f) : 0.f;
	if (bRebuilt || !FMath::IsNearlyEqual(Retract, LastArcRetract))
	{
		LastArcRetract = Retract;
		const int32 PerRibbon = FMath::Max(2, HALShockArcGeometry::PointsPerRibbon(ArcSettings));
		const float Feather = FMath::Max(0.01f, ArcRetractFeather);
		// Front starts behind the tail and ends past the head, so Retract 0 and 1 are
		// exactly "whole arc" and "nothing left" instead of clipping either end early.
		const float Front = Retract * (1.f + Feather) - Feather;
		const float Step01 = 1.f / float(PerRibbon - 1);
		ArcWidths.SetNumUninitialized(ArcBaseWidths.Num());
		for (int32 Index = 0; Index < ArcBaseWidths.Num(); ++Index)
		{
			const float Along = float(Index % PerRibbon) * Step01;
			ArcWidths[Index] = ArcBaseWidths[Index] * FMath::SmoothStep(Front, Front + Feather, Along);
		}
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(RibbonComponent,
			HALShockedOverlay::ArcWidthsParam, ArcWidths);
	}
	// Retraction already removes the arc, so holding alpha up keeps the surviving part
	// at full heat instead of fading twice over.
	RibbonComponent->SetVariableFloat(HALShockedOverlay::ArcAlphaParam,
		bRetractArcsOnRelease ? 1.f : FilamentAlpha);
}
void UHALShockedPresentationComponent::BeginTimeline()
{
	if (!FApp::CanEverRender()) { return; }
	// Per-vehicle seed, so four shocked cars never flicker or arc in sync.
	if (FlickerSeed == 0 && GetOwner()) { FlickerSeed = GetTypeHash(GetOwner()->GetFName()); }
	EnsureOverlay();
	EnsureLight();
	EnsureRibbon();
	if (HALShockedOverlay::WantsLog())
	{
		UE_LOG(LogTemp, Log, TEXT("ShockedPresentation: begin on %s overlays=%d light=%d ribbon=%d material=%s"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("?"), OverlayInstances.Num(),
			IsValid(ShockLight) ? 1 : 0, IsValid(RibbonComponent) ? 1 : 0,
			OverlayMaterial ? *OverlayMaterial->GetName() : TEXT("NONE"));
	}
	if (OverlayInstances.IsEmpty() && !IsValid(ShockLight) && !IsValid(RibbonComponent)) { return; }
	LastArcStep = -1; LastArcRetract = -1.f;
	// A second shock during the burn-off keeps the soot already on the body.
	const float Carried = (Phase == EHALShockPhase::Cooling || Phase == EHALShockPhase::Release) ? ScorchPeak : 0.f;
	Phase = EHALShockPhase::Active; PhaseSeconds = 0.f;
	HeldSeconds = (ScorchRiseSeconds > 0.f) ? Carried * ScorchRiseSeconds : 0.f;
	if (IsValid(ShockLight)) { ShockLight->SetVisibility(true); }
	// Populate the initial points, widths and alpha before Niagara's first spawn.
	ApplyTimeline(1.f, ScorchRamp());
	if (IsValid(RibbonComponent))
	{
		RibbonComponent->Activate(true);
	}
	SetComponentTickEnabled(true);
}
void UHALShockedPresentationComponent::ReleaseTimeline()
{
	if (Phase != EHALShockPhase::Active) { return; }
	ScorchPeak = ScorchRamp();
	Phase = EHALShockPhase::Release; PhaseSeconds = 0.f;
	if (FadeOutSeconds <= 0.f) { BeginCooling(); }
}
void UHALShockedPresentationComponent::BeginCooling()
{
	Phase = EHALShockPhase::Cooling; PhaseSeconds = 0.f;
	if (ScorchFadeSeconds <= 0.f || ScorchPeak <= 0.f) { ClearTimeline(); return; }
	// Current is gone from here on; only the soot is still on the body.
	if (IsValid(ShockLight)) { ShockLight->SetIntensity(0.f); ShockLight->SetVisibility(false); }
	ApplyTimeline(0.f, ScorchPeak);
}
void UHALShockedPresentationComponent::ClearTimeline()
{
	for (int32 Index = 0; Index < OverlayMeshes.Num(); ++Index)
	{
		if (OverlayMeshes[Index]) { OverlayOwner()->SetLegacy(OverlayMeshes[Index], nullptr); }
	}
	if (IsValid(ShockLight)) { ShockLight->SetIntensity(0.f); ShockLight->SetVisibility(false); }
	// Deactivate rather than destroy, so ribbons already on screen finish their own lifetime.
	if (IsValid(RibbonComponent)) { RibbonComponent->Deactivate(); }
	Phase = EHALShockPhase::Idle; PhaseSeconds = 0.f; HeldSeconds = 0.f; ScorchPeak = 0.f; LastArcStep = -1; LastArcRetract = -1.f;
	SetComponentTickEnabled(LocalBounceMovement.IsValid());
}
float UHALShockedPresentationComponent::ScorchRamp() const
{
	return ScorchRiseSeconds > 0.f ? FMath::Min(1.f, HeldSeconds / ScorchRiseSeconds) : 1.f;
}
float UHALShockedPresentationComponent::ComputeFlicker() const
{
	// Same jump clock as the material, so the light steps in lockstep with the filaments.
	const int32 Step = FMath::FloorToInt(HeldSeconds * FMath::Max(1.f, FlickerSteps));
	FRandomStream Stream(int32(uint32(Step) * 73856093u ^ FlickerSeed));
	return FMath::Lerp(FMath::Clamp(LightFlickerFloor, 0.f, 1.f), 1.f, Stream.GetFraction());
}
void UHALShockedPresentationComponent::ApplyTimeline(float FilamentAlpha, float ScorchAlpha)
{
	const float Flash = (Phase == EHALShockPhase::Active && FlashSeconds > 0.f)
		? FMath::Max(0.f, 1.f - PhaseSeconds / FlashSeconds) : 0.f;
	const float Envelope = BrightnessCurve.GetRichCurveConst()->Eval(HeldSeconds);
	const float Filament = FilamentPowerCurve.GetRichCurveConst()->Eval(HeldSeconds)
		+ (1.f - FilamentAlpha) * FadeFilamentGain;
	const float Wipe = (Phase == EHALShockPhase::Active && WipeSeconds > 0.f)
		? FMath::Min(1.f, PhaseSeconds / WipeSeconds) * WipeRadiusCm : WipeRadiusCm;

	for (int32 Index = 0; Index < OverlayInstances.Num(); ++Index)
	{
		UMaterialInstanceDynamic* Instance = OverlayInstances[Index];
		const UMeshComponent* Mesh = OverlayMeshes[Index];
		if (!Instance || !Mesh) { continue; }
		Instance->SetScalarParameterValue(HALShockedOverlay::BrightnessParam, BaseBrightness * Envelope * FilamentAlpha);
		Instance->SetScalarParameterValue(HALShockedOverlay::FilamentPowerParam, Filament);
		Instance->SetScalarParameterValue(HALShockedOverlay::FlashParam, Flash);
		Instance->SetScalarParameterValue(HALShockedOverlay::WipeRadiusParam, Wipe);
		Instance->SetScalarParameterValue(HALShockedOverlay::FlickerStepsParam, FlickerSteps);
		Instance->SetScalarParameterValue(HALShockedOverlay::ScorchAmountParam, FMath::Clamp(ScorchAlpha, 0.f, 1.f));
		// The material measures the wipe in the mesh's local space, so the origin follows the body as it moves.
		const FVector Local = bHasShockOrigin ? Mesh->GetComponentTransform().InverseTransformPosition(ShockOriginWS) : FVector::ZeroVector;
		Instance->SetVectorParameterValue(HALShockedOverlay::HitLocationParam, FLinearColor(Local));
	}
	if (IsValid(ShockLight))
	{
		ShockLight->SetIntensity(LightPeakIntensity * Envelope * ComputeFlicker() * (1.f + Flash * LightFlashBoost) * FilamentAlpha);
	}
	PushArcs(FilamentAlpha);
}
void UHALShockedPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (LocalBounceMovement.IsValid()) { RefreshShocked(); }
	if (Phase == EHALShockPhase::Idle)
	{
		SetComponentTickEnabled(LocalBounceMovement.IsValid());
		return;
	}
	PhaseSeconds += DeltaTime;
	if (Phase == EHALShockPhase::Active)
	{
		HeldSeconds += DeltaTime;
		ApplyTimeline(1.f, ScorchRamp());
		return;
	}
	if (Phase == EHALShockPhase::Release)
	{
		const float Remaining = FadeOutSeconds > 0.f ? FMath::Max(0.f, 1.f - PhaseSeconds / FadeOutSeconds) : 0.f;
		if (Remaining <= 0.f) { BeginCooling(); return; }
		ApplyTimeline(Remaining, ScorchPeak);
		return;
	}
	const float Cooling = ScorchFadeSeconds > 0.f ? FMath::Max(0.f, 1.f - PhaseSeconds / ScorchFadeSeconds) : 0.f;
	if (Cooling <= 0.f) { ClearTimeline(); return; }
	ApplyTimeline(0.f, ScorchPeak * Cooling);
}
void UHALShockedPresentationComponent::RefreshShocked()
{
	const auto* Movement = LocalBounceMovement.Get();
	const bool bLocalBounce = Movement && Movement->IsBounceBackActive();
	const bool bAuthorityShocked = ASC && ASC->HasMatchingGameplayTag(HALGameplayTags::State_Visual_Shocked);
	const auto* Pawn = Cast<ATestVehiclePawn>(GetOwner());
	const bool bObservedBounce = ASC && Pawn && Pawn->GetLocalRole() == ROLE_SimulatedProxy
		&& Pawn->IsReplicatedElectricBounceActive();
	const bool bBounce = bLocalBounce || bObservedBounce;
	UWorld* World = GetWorld();
	if (bAuthorityShocked || bBounce)
	{
		if (World) { World->GetTimerManager().ClearTimer(ShockHandoffTimer); }
		if (bAuthorityShocked) { bAwaitingAuthorityShock = false; }
		else if (!bShocked) { bAwaitingAuthorityShock = true; }
		SetShocked(true);
		return;
	}

	// Keep the same visual episode through a rewind or the gap before the GAS tag arrives.
	if (World && bAwaitingAuthorityShock)
	{
		// A long packet gap can outlast the bounce itself. Cap the visual hold so
		// an unconfirmed contact still clears without an authority tag.
		constexpr float ShockHandoffSeconds = 1.5f;
		auto& Timers = World->GetTimerManager();
		if (!Timers.TimerExists(ShockHandoffTimer))
		{
			Timers.SetTimer(ShockHandoffTimer, this, &ThisClass::ExpireShockHandoff, ShockHandoffSeconds, false);
		}
		return;
	}
	SetShocked(false);
}

void UHALShockedPresentationComponent::ExpireShockHandoff()
{
	bAwaitingAuthorityShock = false;
	RefreshShocked();
}

void UHALShockedPresentationComponent::TagChanged(FGameplayTag Tag,int32 Count) { RefreshShocked(); }
void UHALShockedPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Unbind();
	ClearTimeline();
	OverlayMeshes.Reset(); OverlayInstances.Reset();
	if (IsValid(ShockLight)) { ShockLight->DestroyComponent(); }
	ShockLight = nullptr;
	if (IsValid(RibbonComponent)) { RibbonComponent->DestroyComponent(); }
	RibbonComponent = nullptr;
	if (IsValid(ArcComponent)) { ArcComponent->DestroyComponent(); }
	ArcComponent = nullptr;
	Super::EndPlay(Reason);
}
