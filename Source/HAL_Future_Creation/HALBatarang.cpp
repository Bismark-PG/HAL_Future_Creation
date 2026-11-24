#include "HALBatarang.h"
#include "BasicBallActor.h"
#include "CombatResolver.h"
#include "Components/DynamicMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "HALAbilitySystemComponent.h"
#include "HALBatarangAbility.h"
#include "HALBatarangWidget.h"
#include "HALGameFlowPermissions.h"
#include "HALGameplayTags.h"
#include "HALHookComponent.h"
#include "Net/UnrealNetwork.h"
#include "TestVehiclePawn.h"
#include "VehicleRecoveryComponent.h"

namespace
{
	constexpr double BurstSeconds = .45;
	// Five tangent-matched cubics in the horizontal throw frame. Finish behind the original car.
	const FVector2D ArcPoints[] = {
		{0,0},{26,0},{44,65},{99,70},{154,75},{184,39},{184,-1},
		{184,-48},{149,-81},{102,-88},{55,-95},{3,-94},{-26,-83},{-55,-72},{-79,-47},{-83,-15}
	};
}
FVector HALBatarang::Arc(float Fraction, float Reach, float Width, int8 Mirror)
{
	const double Index = FMath::Clamp(double(Fraction),0.,1.) * 5.;
	const int32 Segment = FMath::Min(4,FMath::FloorToInt(Index));
	const double T = Index - Segment, S = 1. - T;
	const FVector2D* P = &ArcPoints[Segment*3];
	const FVector2D XY = S*S*S*P[0] + 3.*S*S*T*P[1] + 3.*S*T*T*P[2] + T*T*T*P[3];
	return FVector(XY.X/184.*Reach,XY.Y/92.*Width*Mirror,0);
}
EHALDartSide HALBatarang::HitSide(FVector Offset, FVector Extent, FVector Normal)
{
	// A long bonnet and a short flank still have equally wide sectors. An exact diagonal favours the contacted face.
	const double X = Offset.X/FMath::Max(1.,Extent.X), Y = Offset.Y/FMath::Max(1.,Extent.Y);
	if (FMath::IsNearlyEqual(FMath::Abs(X),FMath::Abs(Y),.015))
	{
		return FMath::Abs(Normal.X) >= FMath::Abs(Normal.Y)
			? (Normal.X >= 0 ? EHALDartSide::Front : EHALDartSide::Rear)
			: (Normal.Y >= 0 ? EHALDartSide::Right : EHALDartSide::Left);
	}
	if (FMath::Abs(X) > FMath::Abs(Y)) { return X >= 0 ? EHALDartSide::Front : EHALDartSide::Rear; }
	return Y >= 0 ? EHALDartSide::Right : EHALDartSide::Left;
}
FVector HALBatarang::PushDirection(EHALDartSide Side, double Yaw)
{
	const FRotator Heading(0,Yaw,0);
	switch (Side)
	{
	case EHALDartSide::Rear: return Heading.Vector();
	case EHALDartSide::Left: return Heading.RotateVector(FVector::RightVector);
	case EHALDartSide::Right: return Heading.RotateVector(FVector::LeftVector);
	default: return -Heading.Vector();
	}
}
AHALBatarang::AHALBatarang()
{
	bReplicates = bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(30);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DartRoot")));
	Shape = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DartWings"));
	Shape->SetupAttachment(GetRootComponent());
	Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shape->SetGenerateOverlapEvents(false);
	Shape->SetCanEverAffectNavigation(false);
	Shape->SetCastShadow(false);
	Shape->SetVisibility(false);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}
double AHALBatarang::ServerTime() const
{
	const auto* GS = GetWorld()->GetGameState();
	return GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}
bool AHALBatarang::ParticipantsReady() const
{
	const auto* Car = Cast<ATestVehiclePawn>(GetOwner());
	return IsValid(Car) && Car->CanReceiveSkillEffects() && HALGameFlow::Permissions(GetWorld(),Car).bAbilities
		&& (State.Phase != EHALDartPhase::Pinned || (IsValid(State.Target) && State.Target->CanReceiveSkillEffects()));
}
bool AHALBatarang::Start(UHALBatarangConfig& Config, int8 Mirror)
{
	auto* Car = Cast<ATestVehiclePawn>(GetOwner());
	auto* Body = Car ? Cast<UPrimitiveComponent>(Car->GetRootComponent()) : nullptr;
	if (!HasAuthority() || State.Config || !Body || Car->ActiveBatarang || !Config.HasValidRules() || !ParticipantsReady()) { return false; }
	FHALAttackSourceSnapshot Source;
	if (!FCombatResolver::CaptureVehicleAttack(*this,*Car,Config.DamageMultiplier,0,Source)) { return false; }
	Source.bCanHitInstigator = false;
	Source.SourceTag = HALGameplayTags::Combat_Source_Batarang;
	if (!Attack.Begin(*this,Source)) { return false; }
	const auto Bounds = Body->CalcBounds(FTransform::Identity);
	State.Config = &Config;
	State.Origin = Body->GetComponentTransform().TransformPosition(Bounds.Origin+FVector(Bounds.BoxExtent.X,0,0)+Config.LaunchOffset);
	State.Yaw = Car->GetActorRotation().Yaw;
	State.Mirror = Mirror < 0 ? -1 : 1;
	State.PhaseStart = ServerTime();
	Car->ActiveBatarang = this;
	Car->ForceNetUpdate();
	ForceNetUpdate();
	UE_CLOG(HALAbilities::ShouldLog(),LogHALAbility,Log,TEXT("Batarang release owner=%s mirror=%d"),*GetNameSafe(Car),State.Mirror);
	return true;
}
FVector AHALBatarang::FlightPoint(float Fraction) const
{
	return State.Origin + FRotator(0,State.Yaw,0).RotateVector(HALBatarang::Arc(Fraction,State.Config->ForwardReach,State.Config->SideReach,State.Mirror));
}
FVector AHALBatarang::PushDirection() const
{
	return IsValid(State.Target) ? HALBatarang::PushDirection(State.Side,State.Target->GetActorRotation().Yaw) : FVector(State.BurstDirection);
}
bool AHALBatarang::Sweep(const FVector& From,const FVector& To, FCollisionQueryParams& Query, double Now)
{
	// Object queries can end at an irrelevant blocking hit. Ignore it and repeat the same span before advancing.
	for (int32 Retry=0; Retry<32; ++Retry)
	{
		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByObjectType(Hits,From,To,FQuat::Identity,FCollisionObjectQueryParams::AllObjects,
			FCollisionShape::MakeSphere(State.Config->SweepRadius),Query);
		Hits.StableSort([](const FHitResult& A,const FHitResult& B)
		{
			if (A.Time != B.Time) { return A.Time < B.Time; }
			const bool bVehicleA = Cast<ATestVehiclePawn>(A.GetActor()) != nullptr;
			const bool bVehicleB = Cast<ATestVehiclePawn>(B.GetActor()) != nullptr;
			if (bVehicleA != bVehicleB) { return !bVehicleA; } // A solid wall wins an exactly coincident contact.
			return GetPathNameSafe(A.GetComponent()) < GetPathNameSafe(B.GetComponent());
		});
		bool bRetry = false;
		for (const FHitResult& Hit : Hits)
		{
			auto* Part = Hit.GetComponent();
			auto* Actor = Hit.GetActor();
			if (!Part) { continue; }
			auto* Vehicle = Cast<ATestVehiclePawn>(Actor);
			if (Vehicle && Vehicle->CanReceiveSkillEffects() && Part == Vehicle->GetRootComponent())
			{
				const FTransform Frame = Part->GetComponentTransform();
				const FVector Point = Hit.bStartPenetrating ? Hit.Location - Hit.Normal*State.Config->SweepRadius : FVector(Hit.ImpactPoint);
				const FVector Normal = Hit.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER,(From-To).GetSafeNormal());
				State.Target = Vehicle;
				State.LocalAnchor = Frame.InverseTransformPosition(Point);
				State.LocalNormal = Frame.InverseTransformVectorNoScale(Normal);
				const auto Bounds = Part->CalcBounds(FTransform::Identity);
				State.Side = HALBatarang::HitSide(State.LocalAnchor-Bounds.Origin,Bounds.BoxExtent,State.LocalNormal);
				State.PhaseStart = Now;
				State.Phase = EHALDartPhase::Pinned;
				ForceNetUpdate();
				UE_CLOG(HALAbilities::ShouldLog(),LogHALAbility,Log,TEXT("Batarang pin target=%s side=%d"),*GetNameSafe(Vehicle),int32(State.Side));
				return true;
			}
			if (!Vehicle && !Cast<ABasicBallActor>(Actor) && Part->IsQueryCollisionEnabled()
				&& (Part->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block || Part->GetCollisionResponseToChannel(ECC_PhysicsBody)==ECR_Block))
			{
				Destroy();
				return true;
			}
			Query.AddIgnoredComponent(Part);
			bRetry |= Hit.bBlockingHit;
		}
		if (!bRetry) { return false; }
	}
	Destroy();
	return true;
}
void AHALBatarang::Advance(double Now)
{
	if (State.Phase == EHALDartPhase::Burst)
	{
		if (Now-State.PhaseStart >= BurstSeconds) { Destroy(); }
		return;
	}
	if (!ParticipantsReady()) { Destroy(); return; }
	if (State.Phase == EHALDartPhase::Pinned)
	{
		if (Now-State.PhaseStart >= State.Config->FuseSeconds) { Detonate(); }
		return;
	}
	const float Fraction = FMath::Clamp(float((Now-State.PhaseStart)/State.Config->FlightSeconds),0.f,1.f);
	if (Fraction <= LastFlightFraction) { return; }
	// Bound chord lengths even after a server hitch. Outer control spans are shorter than 6 x the larger radius per unit time.
	const float MaxTravel = 6.f*FMath::Max(State.Config->ForwardReach,State.Config->SideReach);
	const int32 Steps = FMath::Max(1,FMath::CeilToInt((Fraction-LastFlightFraction)*MaxTravel/FMath::Max(12.f,State.Config->SweepRadius*1.5f)));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BatarangFlight),false,GetOwner());
	Query.AddIgnoredActor(this);
	FVector From = FlightPoint(LastFlightFraction);
	for (int32 I=1; I<=Steps; ++I)
	{
		const FVector To = FlightPoint(FMath::Lerp(LastFlightFraction,Fraction,float(I)/Steps));
		if (Sweep(From,To,Query,Now)) { return; }
		From = To;
	}
	LastFlightFraction = Fraction;
	if (Fraction >= 1) { Destroy(); }
}
bool AHALBatarang::IsManualInputOpen() const
{
	const auto* OwnerCar = Cast<ATestVehiclePawn>(GetOwner());
	if (!IsValid(OwnerCar) || State.Phase != EHALDartPhase::Pinned) { return false; }
	const auto* Recovery = OwnerCar->FindComponentByClass<UVehicleRecoveryComponent>();
	return !UHALAbilitySystemComponent::IsActorStunned(OwnerCar) && !UHALHookComponent::BlocksActionsOn(OwnerCar)
		&& !(Recovery && Recovery->IsExecuting());
}
bool AHALBatarang::RequestDetonation()
{
	return HasAuthority() && IsManualInputOpen() && Detonate();
}
bool AHALBatarang::Detonate()
{
	if (State.Phase != EHALDartPhase::Pinned || !ParticipantsReady() || !Attack.IsActive()) { return false; }
	auto* Target = State.Target.Get();
	State.BurstPoint = Target->GetActorTransform().TransformPosition(State.LocalAnchor);
	State.BurstDirection = PushDirection();
	State.PhaseStart = ServerTime();
	State.Phase = EHALDartPhase::Burst; // Publish the terminal state before combat callbacks can retire either car.
	FHALCombatHitRequest Request;
	Request.AttackId = Attack.GetSnapshot().AttackId;
	Request.PulseSequence = Attack.GetPulseSequence();
	Request.bApplyImpact = true;
	Request.Impact.Mode = EHALImpactMode::ExplicitTier;
	Request.Impact.Tier = State.Config->ImpactTier;
	Request.Impact.PushPreset = State.Config->PushDistance;
	Request.Hit.TargetActor = Target;
	Request.Hit.TargetPhysicsBody = Cast<UPrimitiveComponent>(Target->GetRootComponent());
	Request.Hit.ImpactPoint = State.BurstPoint;
	Request.Hit.ImpactNormal = -State.BurstDirection;
	const auto Result = FCombatResolver::ResolveCombatHit(Attack,Request);
	Attack.Close();
	ForceNetUpdate();
	UE_CLOG(HALAbilities::ShouldLog(),LogHALAbility,Log,TEXT("Batarang burst target=%s damage=%.2f tier=%d direction=%s"),
		*GetNameSafe(Target),Result.ActualHPRemoved,int32(Result.ImpactSummary.EffectiveTier),*State.BurstDirection.ToString());
	return true;
}
void AHALBatarang::Tick(float Delta)
{
	Super::Tick(Delta);
	if (!State.Config) { return; }
	const double Now = ServerTime();
	if (HasAuthority()) { Advance(Now); }
	if (!IsActorBeingDestroyed() && !IsNetMode(NM_DedicatedServer)) { Present(Now); }
}
void AHALBatarang::EndPlay(EEndPlayReason::Type Reason)
{
	Attack.Close();
	if (OwnerView) { OwnerView->RemoveFromParent(); }
	if (auto* Car = Cast<ATestVehiclePawn>(GetOwner()); HasAuthority() && Car && Car->ActiveBatarang == this)
	{
		Car->ActiveBatarang = nullptr;
		Car->ForceNetUpdate();
	}
	Super::EndPlay(Reason);
}
void AHALBatarang::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALBatarang,State);
}
