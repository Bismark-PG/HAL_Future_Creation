#include "HALBattleTopComponent.h"
#include "HALBattleTopAbility.h"
#include "HALVehiclePresentationComponent.h"
#include "HALVehicleFXTargets.h"
#include "HALImpactReceiverComponent.h"
#include "HALVehicleStatusFXSettings.h"
#include "ArcadeVehicleMovementComponent.h"
#include "VehicleRecoveryComponent.h"
#include "VehicleKnockbackSettings.h"
#include "VehicleStabilityRules.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Misc/App.h"

namespace
{
	double Smooth(double T)
	{
		T = FMath::Clamp(T, 0., 1.);
		return T * T * (3 - 2 * T);
	}
	double IntegralSmooth(double T)
	{
		T = FMath::Clamp(T, 0., 1.);
		return T * T * T - .5 * T * T * T * T;
	}
	double Time(const UWorld& World)
	{
		const auto* GS = World.GetGameState();
		return GS ? GS->GetServerWorldTimeSeconds() : World.GetTimeSeconds();
	}
	FVehicleContactObservation BeforeMotion(UPrimitiveComponent& Body)
	{
		if (const auto* M = Body.GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>())
		{
			const auto S = M->GetContactObservation();
			if (S.bValid)
			{
				return S;
			}
		}
		FVehicleContactObservation S;
		S.bValid = true;
		S.Position = Body.GetComponentLocation();
		S.Rotation = Body.GetComponentQuat();
		S.Velocity = Body.GetPhysicsLinearVelocity();
		S.AngularVelocity = Body.GetPhysicsAngularVelocityInRadians();
		return S;
	}
} // namespace

bool FHALBattleTopFXConfig::IsValid() const
{
	const auto Range = [](double V, double L, double H) { return FMath::IsFinite(V) && V >= L && V <= H; };
	const auto Color = [](FLinearColor C) {
		return FMath::IsFinite(C.R) && FMath::IsFinite(C.G) && FMath::IsFinite(C.B) && FMath::IsFinite(C.A);
	};
	return Color(MainColor) && Color(AccentColor) && Color(HighlightColor) && Range(Opacity, 0, 1) &&
		   Range(RadiusScale, 1, 2) && Range(ContactInterval, .03f, 1) && Range(MinimumSlipSpeed, 0, 2000) &&
		   ContactCount >= 1 && ContactCount <= 64 && Range(GroundInterval, .05f, 1) &&
		   Range(ContactSizeMin.X, 1, 200) && Range(ContactSizeMin.Y, 1, 200) &&
		   Range(ContactSizeMax.X, ContactSizeMin.X, 200) && Range(ContactSizeMax.Y, ContactSizeMin.Y, 200) &&
		   Range(ContactLifetimeMin, .08f, 1.2f) && Range(ContactLifetimeMax, ContactLifetimeMin, 1.2f) &&
		   Range(ContactSpeedMin, 100, 4000) && Range(ContactSpeedMax, ContactSpeedMin, 4000) &&
		   Range(ContactSpreadDegrees, 5, 80) && Range(ContactGravityScale, 0, 3) && Range(ContactDrag, 0, 15) &&
		   Range(GroundMinimumSpeed, 1, 2000) && Range(GroundDistance, 1, 100);
}

float FHALBattleTopVisualState::AngleAt(double Elapsed) const
{
	const double T = FMath::Max(0., Elapsed), W = SpinRate;
	if (DurationSeconds <= 0)
	{
		return float(W * T);
	} // Legacy/native fixtures with no timeline.
	if (T >= DurationSeconds)
	{
		return 0;
	}
	if (EnterSeconds > 0 && T < EnterSeconds)
	{
		return float(W * EnterSeconds * IntegralSmooth(T / EnterSeconds));
	}
	const double ExitStart = DurationSeconds - ExitSeconds;
	if (ExitSeconds <= 0 || T <= ExitStart)
	{
		return float(W * (T - .5 * EnterSeconds));
	}
	const double U = (T - ExitStart) / ExitSeconds;
	const double A = W * (ExitStart - .5 * EnterSeconds);
	const double RawEnd = A + .5 * W * ExitSeconds;
	// Integrate the speed ramp, then smoothly remove the shortest residual yaw.
	return float(A + W * ExitSeconds * (U - IntegralSmooth(U)) - FMath::UnwindDegrees(RawEnd) * Smooth(U));
}
float FHALBattleTopVisualState::AngularSpeedAt(double Elapsed) const
{
	const double T = FMath::Max(0., Elapsed), W = SpinRate;
	if (DurationSeconds <= 0)
	{
		return float(W);
	}
	if (T >= DurationSeconds)
	{
		return 0;
	}
	if (EnterSeconds > 0 && T < EnterSeconds)
	{
		return float(W * Smooth(T / EnterSeconds));
	}
	const double ExitStart = DurationSeconds - ExitSeconds;
	if (ExitSeconds <= 0 || T <= ExitStart)
	{
		return float(W);
	}
	const double U = (T - ExitStart) / ExitSeconds;
	const double RawEnd = W * (ExitStart - .5 * EnterSeconds + .5 * ExitSeconds);
	return float(W * (1 - Smooth(U)) - FMath::UnwindDegrees(RawEnd) * 6 * U * (1 - U) / ExitSeconds);
}
float FHALBattleTopVisualState::EnvelopeAt(double Elapsed) const
{
	if (DurationSeconds <= 0)
	{
		return 1;
	}
	if (Elapsed < 0 || Elapsed >= DurationSeconds)
	{
		return 0;
	}
	return float((EnterSeconds > 0 ? Smooth(Elapsed / EnterSeconds) : 1.) *
				 (ExitSeconds > 0 ? 1 - Smooth((Elapsed - (DurationSeconds - ExitSeconds)) / ExitSeconds) : 1.));
}
FVector HALBattleTopFX::SparkDirection(const FVector& RelativePointVelocity, const FVector& Normal)
{
	if (RelativePointVelocity.ContainsNaN() || Normal.ContainsNaN())
	{
		return FVector::ZeroVector;
	}
	const FVector N = Normal.GetSafeNormal();
	return -FVector::VectorPlaneProject(RelativePointVelocity, N).GetSafeNormal();
}

HALBattleTopFX::FContactSprayMotion HALBattleTopFX::ContactSprayMotion(const FHALBattleTopFXConfig& Config,
																	   const FVector& TangentDirection,
																	   const FVector& SurfaceNormal)
{
	FContactSprayMotion Out;
	const FVector N = SurfaceNormal.GetSafeNormal();
	const FVector T = FVector::VectorPlaneProject(TangentDirection, N).GetSafeNormal();
	if (N.IsNearlyZero() || T.IsNearlyZero() || !Config.IsValid())
	{
		return Out;
	}
	// Y points away from the contacted surface, Z preserves the opposite sliding direction.
	Out.Rotation = FRotationMatrix::MakeFromZY(T, N).ToQuat();
	const double Spread =
		Config.ContactSpeedMin * FMath::Tan(FMath::DegreesToRadians(Config.ContactSpreadDegrees)) / FMath::Sqrt(2.);
	const double Up = FVector::DotProduct(Out.Rotation.GetAxisX(), FVector::UpVector);
	Out.VelocityMin = FVector(Up > .25 ? 0 : -Spread, 0, Config.ContactSpeedMin);
	Out.VelocityMax = FVector(Up < -.25 ? 0 : Spread, Spread, Config.ContactSpeedMax);
	Out.Gravity = Out.Rotation.UnrotateVector(FVector(0, 0, -980 * Config.ContactGravityScale));
	return Out;
}

void UHALBattleTopComponent::ObserveContact(UPrimitiveComponent& Body, UPrimitiveComponent& Other, const FVector&,
											const FHitResult& Hit)
{
	if (!CanResolveContacts() || !VisualFX.IsValid())
	{
		return;
	}
	const auto* Rules = GetWorld()->GetSubsystem<UVehicleStabilitySubsystem>();
	const bool bVehicle = Other.GetOwner()->FindComponentByClass<UHALImpactReceiverComponent>() != nullptr;
	const bool bWall =
		Rules && Rules->GetContactRules().IsOrdinaryWall(
					 HALVehicleStability::ClassifySupportSource(&Other, Hit.ImpactPoint) == TEXT("StationaryWorld"),
					 Hit.ImpactNormal);
	if (!bVehicle && !bWall)
	{
		return;
	}
	TrackContact(Other, Hit);
	auto* Contact = TrackedContacts.Find(&Other);
	if (!Contact)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < Contact->NextSpark)
	{
		return;
	}
	const auto PointVelocity = [&](UPrimitiveComponent& B) {
		const auto Motion = BeforeMotion(B);
		FVector Omega = Motion.AngularVelocity;
		if (const auto* Top = B.GetOwner()->FindComponentByClass<UHALBattleTopComponent>(); Top && Top->IsActive())
		{
			Omega += Motion.Rotation.GetUpVector() * FMath::DegreesToRadians(Top->VisualState.AngularSpeedAt(
														 Time(*GetWorld()) - Top->VisualState.StartServerTime));
		}
		return Motion.Velocity + FVector::CrossProduct(Omega, Hit.ImpactPoint - Motion.Position);
	};
	const FVector Relative = PointVelocity(Body) - PointVelocity(Other);
	const FVector Tangent = FVector::VectorPlaneProject(Relative, Hit.ImpactNormal.GetSafeNormal());
	if (Tangent.SizeSquared() < FMath::Square(VisualFX.MinimumSlipSpeed))
	{
		return;
	}
	const FVector Direction = HALBattleTopFX::SparkDirection(Relative, Hit.ImpactNormal);
	if (Direction.IsNearlyZero())
	{
		return;
	}
	Contact->NextSpark = Now + VisualFX.ContactInterval;
	MulticastContactSpark(VisualState.Activation, Hit.ImpactPoint + Hit.ImpactNormal.GetSafeNormal() * 8, Direction,
						  Hit.ImpactNormal.GetSafeNormal());
}
void UHALBattleTopComponent::MulticastContactSpark_Implementation(FGuid Activation, FVector_NetQuantize Point,
																  FVector_NetQuantizeNormal Direction,
																  FVector_NetQuantizeNormal Normal)
{
	if (!IsActive() || Activation != VisualState.Activation || !GetWorld() ||
		Time(*GetWorld()) >= VisualState.StartServerTime + VisualState.DurationSeconds)
	{
		return;
	}
	SpawnSparks(false, Point, Direction, Normal);
}
void UHALBattleTopComponent::SpawnSparks(bool bGround, const FVector& Point, const FVector& Direction,
										 const FVector& Normal)
{
	if (!FApp::CanEverRender() || GetNetMode() == NM_DedicatedServer || !GetWorld() || !GetWorld()->FXSystem.IsValid())
	{
		return;
	}
	auto* System = bGround ? VisualFX.GroundSparks.Get() : VisualFX.ContactSparks.Get();
	if (!System || !System->IsReadyToRun())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 I = SparkComponents.Num() - 1; I >= 0; --I)
	{
		if (!SparkComponents[I].Component.IsValid() || Now >= SparkComponents[I].Expires)
		{
			if (auto* C = SparkComponents[I].Component.Get())
			{
				C->DestroyComponent();
			}
			SparkComponents.RemoveAtSwap(I);
		}
	}
	if (SparkComponents.Num() >= 24 || GetDefault<UHALVehicleStatusFXSettings>()->MaximumParticleLayersPerVehicle <= 0)
	{
		return;
	}
	auto* C = NewObject<UNiagaraComponent>(GetOwner());
	C->SetAsset(System);
	C->SetAutoActivate(false);
	C->SetWorldLocation(Point);
	const auto Motion = HALBattleTopFX::ContactSprayMotion(VisualFX, Direction, Normal);
	C->SetWorldRotation(bGround ? FQuat::FindBetweenNormals(FVector::UpVector, Direction.GetSafeNormal())
								: Motion.Rotation);
	C->SetVariableInt(TEXT("User.BurstCount"), bGround ? 2 : VisualFX.ContactCount);
	C->SetVariableVec3(TEXT("User.VelocityMin"), bGround ? FVector(-80, -80, 160) : Motion.VelocityMin);
	C->SetVariableVec3(TEXT("User.VelocityMax"), bGround ? FVector(80, 80, 280) : Motion.VelocityMax);
	if (!bGround)
	{
		C->SetRandomSeedOffset(int32(GetTypeHash(Point) ^ GetTypeHash(Now)));
		C->SetVariableVec2(TEXT("User.SizeMin"), VisualFX.ContactSizeMin);
		C->SetVariableVec2(TEXT("User.SizeMax"), VisualFX.ContactSizeMax);
		C->SetVariableFloat(TEXT("User.LifetimeMin"), VisualFX.ContactLifetimeMin);
		C->SetVariableFloat(TEXT("User.LifetimeMax"), VisualFX.ContactLifetimeMax);
		C->SetVariableVec3(TEXT("User.Gravity"), Motion.Gravity);
		C->SetVariableFloat(TEXT("User.Drag"), VisualFX.ContactDrag);
	}
	C->SetVariableLinearColor(TEXT("User.SparkColor"), VisualFX.HighlightColor);
	C->RegisterComponent();
	C->Activate(true);
	SparkComponents.Add({C, Now + (bGround ? .6 : FMath::Max(.6f, VisualFX.ContactLifetimeMax + .1f))});
}
void UHALBattleTopComponent::ClearParticles()
{
	if (WindComponent)
	{
		WindComponent->DestroyComponent();
		WindComponent = nullptr;
	}
	WindMaterial = nullptr;
	for (const auto& P : SparkComponents)
	{
		if (auto* C = P.Component.Get())
		{
			C->DestroyComponent();
		}
	}
	SparkComponents.Reset();
	LastGroundSpark = -1;
}
void UHALBattleTopComponent::UpdateParticles(float, float Angle, float Envelope)
{
	if (!GetWorld() || !VisualFX.IsValid())
	{
		return;
	}
	auto* Body = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	if (!Body)
	{
		return;
	}
	FHALVehicleFXTargetSnapshot Targets;
	if (const auto* P = GetOwner()->FindComponentByClass<UHALVehiclePresentationComponent>())
	{
		P->CaptureStatusFXTargets(Targets);
	}
	float Radius = FMath::Max(Body->Bounds.BoxExtent.X, Body->Bounds.BoxExtent.Y);
	FVector Center = Body->Bounds.Origin;
	if (!Targets.Targets.IsEmpty())
	{
		const auto& T = Targets.Targets[0];
		const FVector E = T.LocalBounds.GetExtent() * T.MeshToWorld.GetScale3D().GetAbs();
		Radius = FMath::Max(E.X, E.Y);
		Center = T.MeshToWorld.TransformPosition(T.LocalBounds.GetCenter());
	}
	if (WindComponent && WindComponent->GetAsset() != VisualFX.Wind)
	{
		WindComponent->DestroyComponent();
		WindComponent = nullptr;
		WindMaterial = nullptr;
	}
	if (!WindComponent && VisualFX.Wind && VisualFX.Wind->IsReadyToRun() &&
		GetDefault<UHALVehicleStatusFXSettings>()->MaximumParticleLayersPerVehicle > 0)
	{
		WindComponent = NewObject<UNiagaraComponent>(GetOwner());
		WindComponent->SetAsset(VisualFX.Wind);
		WindComponent->SetAutoActivate(false);
		WindComponent->SetupAttachment(Body);
		WindComponent->RegisterComponent();
		auto* M = VisualFX.WindMaterial.Get();
		if (!M)
		{
			const FNiagaraVariable Binding(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()),
										   TEXT("User.WindMaterial"));
			M = Cast<UMaterialInterface>(VisualFX.Wind->GetExposedParameters().GetUObject(Binding));
		}
		if (M)
		{
			WindMaterial = UMaterialInstanceDynamic::Create(M, this);
			WindComponent->SetVariableMaterial(TEXT("User.WindMaterial"), WindMaterial);
		}
		WindComponent->Activate(true);
	}
	if (WindComponent)
	{
		const float Diameter = FMath::Clamp(Radius * VisualFX.RadiusScale * 2.f, 100.f, 4000.f);
		WindComponent->SetRelativeLocation(Body->GetComponentTransform().InverseTransformPosition(Center));
		WindComponent->SetVariableVec2(TEXT("User.WindSize"), FVector2D(Diameter));
		if (WindMaterial)
		{
			WindMaterial->SetScalarParameterValue(TEXT("SpinAngle"), Angle / 360);
			WindMaterial->SetScalarParameterValue(TEXT("Envelope"), Envelope);
			WindMaterial->SetScalarParameterValue(TEXT("Opacity"), VisualFX.Opacity);
			WindMaterial->SetVectorParameterValue(TEXT("MainColor"), VisualFX.MainColor);
			WindMaterial->SetVectorParameterValue(TEXT("AccentColor"), VisualFX.AccentColor);
			WindMaterial->SetVectorParameterValue(TEXT("HighlightColor"), VisualFX.HighlightColor);
		}
	}
	const auto* Recovery = GetOwner()->FindComponentByClass<UVehicleRecoveryComponent>();
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 I = SparkComponents.Num() - 1; I >= 0; --I)
	{
		if (!SparkComponents[I].Component.IsValid() || Now >= SparkComponents[I].Expires)
		{
			if (auto* C = SparkComponents[I].Component.Get())
			{
				C->DestroyComponent();
			}
			SparkComponents.RemoveAtSwap(I);
		}
	}
	if ((Recovery && Recovery->IsExecuting()) || Envelope <= 0 || Now < LastGroundSpark + VisualFX.GroundInterval)
	{
		return;
	}
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	if (Velocity.SizeSquared2D() < FMath::Square(VisualFX.GroundMinimumSpeed))
	{
		return;
	}
	if (const auto* M = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>())
	{
		const auto Pose = M->GetStabilityObservation();
		if (Pose.bValid && !Pose.bGrounded)
		{
			return;
		}
	}
	FVector Start = Body->Bounds.Origin;
	Start.Z -= Body->Bounds.BoxExtent.Z - 5;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleTopGroundFX), false, GetOwner());
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, Start - FVector::UpVector * (VisualFX.GroundDistance + 5),
												FCollisionObjectQueryParams(ECC_WorldStatic), Query) &&
		Hit.ImpactNormal.Z > .5)
	{
		LastGroundSpark = Now;
		SpawnSparks(true, Hit.ImpactPoint + Hit.ImpactNormal * 2,
					(-Velocity.GetSafeNormal2D() + Hit.ImpactNormal * .15).GetSafeNormal());
	}
}
