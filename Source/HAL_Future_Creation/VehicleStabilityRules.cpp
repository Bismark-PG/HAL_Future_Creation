#include "VehicleStabilityRules.h"

#include "Misc/Crc.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "BasicBallActor.h"
#include "HALImpactReceiverComponent.h"
#include "HALImpactSourceComponent.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"

namespace { constexpr int32 VehicleGravityGroup = 1; } // Group 0 remains world/ball gravity.

float HALVehicleStability::GetVehicleGravityZ(const UPrimitiveComponent* Body)
{
	check(IsInGameThread());
	const auto* World=Body?Body->GetWorld():nullptr;
	if (!World) { return 0; }
	const auto* Shared=World->GetSubsystem<UVehicleStabilitySubsystem>();
	return Shared && Body->BodyInstance.GravityGroupIndex==VehicleGravityGroup ? Shared->GetVehicleGravityZ() : World->GetGravityZ();
}
float HALVehicleStability::FlipAngularSpeed(float VerticalSpeed,float Turns,float GravityZ,float MaximumSpeed)
{
	if (!FMath::IsFinite(VerticalSpeed) || !FMath::IsFinite(Turns) || !FMath::IsFinite(GravityZ) || !FMath::IsFinite(MaximumSpeed)
		|| VerticalSpeed<=0 || Turns<=0 || GravityZ>=-KINDA_SMALL_NUMBER || MaximumSpeed<=0) { return 0; }
	return FMath::Min(MaximumSpeed,2*PI*Turns/FMath::Max(0.1f,2*VerticalSpeed/-GravityZ));
}

FName HALVehicleStability::ClassifySupportSource(const UPrimitiveComponent* SourceBody, const FVector& ContactPoint)
{
	check(IsInGameThread());
	const AActor* Source = IsValid(SourceBody) ? SourceBody->GetOwner() : nullptr;
	if (!IsValid(Source) || ContactPoint.ContainsNaN()) { return TEXT("InvalidSource"); }
	if (Source->IsA<ABasicBallActor>() || Source->FindComponentByClass<UHALImpactReceiverComponent>()
		|| Source->FindComponentByClass<UHALImpactSourceComponent>()) { return TEXT("SpecializedSource"); }
	if (SourceBody->IsSimulatingPhysics()) { return TEXT("SimulatedSource"); }
	if (SourceBody->GetCollisionObjectType() != ECC_WorldStatic) { return TEXT("NonWorldStatic"); }
	const FBodyInstance* Body = SourceBody->GetBodyInstance();
	if (!Body || !Body->IsValidBodyInstance()) { return TEXT("NoPhysicsBody"); }
	// ComponentVelocity alone may omit kinematic rotation. Check both physics motion channels too.
	const FVector ComponentV = SourceBody->GetComponentVelocity();
	const FVector PointV = SourceBody->GetPhysicsLinearVelocityAtPoint(ContactPoint);
	const FVector AngularV = SourceBody->GetPhysicsAngularVelocityInRadians();
	if (ComponentV.ContainsNaN() || PointV.ContainsNaN() || AngularV.ContainsNaN()) { return TEXT("InvalidMotion"); }
	if (!ComponentV.IsNearlyZero(KINDA_SMALL_NUMBER) || !PointV.IsNearlyZero(KINDA_SMALL_NUMBER)
		|| !AngularV.IsNearlyZero(KINDA_SMALL_NUMBER)) { return TEXT("MovingSource"); }
	// Catch a new kinematic target before the solver has produced its velocity.
	const FTransform PhysicsPose = Body->GetUnrealWorldTransform(false, true);
	const FTransform ComponentPose = SourceBody->GetComponentTransform();
	if (PhysicsPose.ContainsNaN() || !PhysicsPose.GetLocation().Equals(ComponentPose.GetLocation(), KINDA_SMALL_NUMBER)
		|| !PhysicsPose.GetRotation().Equals(ComponentPose.GetRotation(), KINDA_SMALL_NUMBER)) { return TEXT("PendingMotion"); }
	return TEXT("StationaryWorld");
}

bool FVehicleContactStabilityRules::SuppressesSupplement(bool bOrdinaryStationaryEnvironment,
	const FVector& Normal, const FVector& ImpulseDirection) const
{
	return bSuppressSupportSupplement && bOrdinaryStationaryEnvironment && !Normal.ContainsNaN()
		&& !ImpulseDirection.ContainsNaN() && Normal.GetSafeNormal().Z >= MinimumSupportUpDot
		&& ImpulseDirection.GetSafeNormal().Z >= MinimumSupportUpDot;
}

bool UVehicleStabilityRules::Validate(FString& Error) const
{
	if (!FMath::IsFinite(GravityScale) || GravityScale<0.1f || GravityScale>5
		|| !FMath::IsFinite(WallDropNormalDeltaSpeed) || WallDropNormalDeltaSpeed<0 || WallDropNormalDeltaSpeed>5000
		|| !FMath::IsFinite(MinimumWallSlopeDegrees) || MinimumWallSlopeDegrees<45 || MinimumWallSlopeDegrees>90)
	{ Error=TEXT("Invalid gravity scale or wall ball-drop threshold/slope"); return false; }
	if (!Recovery.Validate(Error)) { return false; }
	if (!bReadyForUse || !IsValid(VehiclePhysicalMaterial))
	{
		Error = TEXT("ReadyForUse and a vehicle physical material are required"); return false;
	}
	if (!FMath::IsFinite(MaximumSupportSlopeDegrees) || MaximumSupportSlopeDegrees < 0 || MaximumSupportSlopeDegrees > 80)
	{
		Error = TEXT("MaximumSupportSlopeDegrees must be finite and within [0,80] degrees"); return false;
	}
	const auto* PM = VehiclePhysicalMaterial.Get();
	for (const float Value : {PM->Density, PM->SleepLinearVelocityThreshold, PM->SleepAngularVelocityThreshold,
		PM->SoftCollisionThickness, PM->BaseFrictionImpulse})
	{
		if (!FMath::IsFinite(Value) || Value < 0) { Error = TEXT("Invalid material density/sleep/soft-contact parameter"); return false; }
	}
	if (PM->SleepCounterThreshold < 0 || !FMath::IsFinite(PM->RaiseMassToPower) || PM->RaiseMassToPower <= 0 || PM->RaiseMassToPower > 1
		|| uint8(PM->SoftCollisionMode) > uint8(EPhysicalMaterialSoftCollisionMode::AbsoluteThickess))
	{
		Error = TEXT("Invalid material sleep counter/mass power/soft-contact mode"); return false;
	}
	if (!FMath::IsFinite(PM->Restitution) || PM->Restitution < 0 || PM->Restitution > 1
		|| !FMath::IsFinite(PM->Friction) || PM->Friction < 0 || !FMath::IsFinite(PM->StaticFriction) || PM->StaticFriction < 0
		|| uint8(PM->FrictionCombineMode) > uint8(EFrictionCombineMode::Max)
		|| uint8(PM->RestitutionCombineMode) > uint8(EFrictionCombineMode::Max))
	{
		Error = TEXT("Invalid contact material: finite nonnegative friction, restitution [0,1], valid combine modes required"); return false;
	}
	return true;
}

bool UVehicleStabilitySubsystem::InitializeRules()
{
	check(IsInGameThread());
	if (bInitialized) { return bValid; }
	bInitialized = true;
	VehicleGravityZ=GetWorld()->GetGravityZ();
	const auto* Settings = GetDefault<UVehicleStabilitySettings>();
	if (!Settings->bUseSharedRules)
	{
		RecoveryRules.bEnabled = false;
		Signature = FCrc::StrCrc32(TEXT("VehicleContactStabilityV1:Disabled"));
		bValid = true; return true;
	}
	const auto* Definition = Settings->SharedRules.LoadSynchronous();
	FString Error;
	if (!Definition || !Definition->Validate(Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid VehicleStabilityRules %s: %s. Vehicle blocked; no fallback."), *Settings->SharedRules.ToString(), *Error);
		return false;
	}
	ContactRules.bSuppressSupportSupplement = Definition->bSuppressSupportSupplement;
	ContactRules.WallDropNormalDeltaSpeed=Definition->WallDropNormalDeltaSpeed;
	ContactRules.MaximumWallUpDot=FMath::Cos(FMath::DegreesToRadians(Definition->MinimumWallSlopeDegrees));
	VehicleGravityZ*=Definition->GravityScale;
	if (!FMath::IsFinite(VehicleGravityZ)) { return false; }
	auto* Scene=GetWorld()->GetPhysicsScene(); auto* Solver=Scene?Scene->GetSolver():nullptr;
	if (!Solver) { UE_LOG(LogTemp,Error,TEXT("Vehicle gravity requires a physics solver")); return false; }
	// Native Chaos group gravity is integrated in normal and rewind steps. No extra Tick/forces.
	Solver->EnqueueCommandImmediate([Solver,G=VehicleGravityZ]()
	{ Solver->GetEvolution()->GetGravityForces().SetAcceleration(Chaos::FVec3(0,0,G),VehicleGravityGroup); });
	bUseVehicleGravity=true;
	RecoveryRules = Definition->Recovery;
	ContactRules.MinimumSupportUpDot = FMath::Cos(FMath::DegreesToRadians(Definition->MaximumSupportSlopeDegrees));
	// Freeze the material too: editing the source DA/PM during PIE cannot change this world's history.
	Material = DuplicateObject<UPhysicalMaterial>(Definition->VehiclePhysicalMaterial, this);
	FString Key = FString::Printf(TEXT("VehicleContactStabilityV1:%s:%d:%.9g:%s:%.9g:%.9g:%.9g:%d:%d:%d:%d"),
		*Definition->GetPathName(), ContactRules.bSuppressSupportSupplement, Definition->MaximumSupportSlopeDegrees,
		*Definition->VehiclePhysicalMaterial->GetPathName(), Material->Friction, Material->StaticFriction, Material->Restitution,
		Material->bOverrideFrictionCombineMode, int32(Material->FrictionCombineMode),
		Material->bOverrideRestitutionCombineMode, int32(Material->RestitutionCombineMode));
	Key += FString::Printf(TEXT(":%.9g:%.9g:%.9g:%d:%.9g:%d:%.9g:%.9g"), Material->Density,
		Material->SleepLinearVelocityThreshold, Material->SleepAngularVelocityThreshold, Material->SleepCounterThreshold,
		Material->RaiseMassToPower, int32(Material->SoftCollisionMode), Material->SoftCollisionThickness, Material->BaseFrictionImpulse);
	Key += RecoveryRules.SignatureKey();
	Key += FString::Printf(TEXT(":GravityWallV1:%.9g:%.9g:%.9g:%.9g"),Definition->GravityScale,VehicleGravityZ,
		Definition->WallDropNormalDeltaSpeed,Definition->MinimumWallSlopeDegrees);
	Signature = FCrc::StrCrc32(*Key);
	if (!Signature) { Signature = 1; } // Zero means no authority snapshot received yet.
	bValid = true;
	UE_LOG(LogTemp, Display, TEXT("VehicleStabilityRules Snapshot=%u Asset=%s Restitution=%.3f SupportSlope=%.1f SuppressSupport=%d"),
		Signature, *Definition->GetPathName(), Material->Restitution, Definition->MaximumSupportSlopeDegrees, ContactRules.bSuppressSupportSupplement);
	UE_LOG(LogTemp,Display,TEXT("VehicleGravity Scale=%.3f GravityZ=%.3f Group=%d WallDropDV=%.3f WallSlope=%.1f"),
		Definition->GravityScale,VehicleGravityZ,VehicleGravityGroup,ContactRules.WallDropNormalDeltaSpeed,Definition->MinimumWallSlopeDegrees);
	return true;
}

bool FVehicleContactStabilityRules::IsOrdinaryWall(bool bOrdinaryStationaryEnvironment,const FVector& Normal) const
{
	return bOrdinaryStationaryEnvironment && !Normal.ContainsNaN() && !Normal.IsNearlyZero()
		&& FMath::Abs(Normal.GetSafeNormal().Z)<=FMath::Max(0.f,MaximumWallUpDot)+1.e-6f;
}
bool FVehicleContactStabilityRules::AllowsWallDrop(bool bOrdinaryStationaryEnvironment,const FVector& Normal,float NormalDeltaSpeed) const
{
	return !IsOrdinaryWall(bOrdinaryStationaryEnvironment,Normal)
		|| (FMath::IsFinite(NormalDeltaSpeed) && NormalDeltaSpeed>=WallDropNormalDeltaSpeed);
}
void UVehicleStabilitySubsystem::ConfigureVehicleGravity(UPrimitiveComponent* Body) const
{
	check(IsInGameThread());
	if (bValid && bUseVehicleGravity && Body && Body->GetOwner() && Body->GetWorld()==GetWorld())
	{ Body->BodyInstance.SetGravityGroupIndex(VehicleGravityGroup); }
}
