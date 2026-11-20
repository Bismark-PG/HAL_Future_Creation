#include "HALWaterCannon.h"

#include "BallControlComponent.h"
#include "BasicBallActor.h"
#include "CombatResolver.h"
#include "Components/AudioComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "HALAbilityTypes.h"
#include "HALGameFlowPermissions.h"
#include "HALGameplayTags.h"
#include "HALMovementEffectBridge.h"
#include "HALWaterCannonAbility.h"
#include "Net/UnrealNetwork.h"
#include "TestVehiclePawn.h"
#include "VehicleRecoveryComponent.h"

namespace
{
	bool BlocksWater(const FHitResult& Hit)
	{
		const auto* Body = Hit.GetComponent();
		return Body && Body->IsQueryCollisionEnabled() && (Body->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Block
			|| Body->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block);
	}
}
AHALWaterCannon::AHALWaterCannon()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(30);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Nozzle")));
	Mesh = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("Water"));
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetVisibility(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);
	Alarm = CreateDefaultSubobject<UAudioComponent>(TEXT("FireSiren"));
	Alarm->SetupAttachment(GetRootComponent());
	Alarm->bAutoActivate = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}
double AHALWaterCannon::ServerTime() const
{
	const auto* GameState = GetWorld()->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}
bool AHALWaterCannon::OwnerReady() const
{
	const auto* Car = Cast<ATestVehiclePawn>(GetOwner());
	return IsValid(Car) && Car->CanReceiveSkillEffects() && HALGameFlow::Permissions(GetWorld(),Car).bAbilities;
}
bool AHALWaterCannon::Start(UHALWaterCannonConfig& Settings, const FVector& Nozzle)
{
	if (!HasAuthority() || Config || !Settings.HasValidRules() || Nozzle.ContainsNaN() || !OwnerReady()) { return false; }
	auto* Bridge = GetOwner()->FindComponentByClass<UHALMovementEffectBridge>();
	if (!Bridge || !Bridge->GetBindingEpoch().IsValid()
		|| !FCombatResolver::CaptureVehicleAttack(*this,*GetOwner(),Settings.DamageMultiplier,0,Attack)) { return false; }
	Config = &Settings;
	LocalNozzle = Nozzle;
	StartTime = ServerTime();
	Activation = FGuid::NewGuid();
	Attack.SourceTag = HALGameplayTags::Combat_Source_WaterCannon;
	Attack.bCanHitInstigator = false;
	FHALWaterJetDrive Drive;
	Drive.Activation = Activation;
	Drive.Seconds = Config->Duration;
	Drive.RecoilAcceleration = Config->RecoilAcceleration;
	Drive.ForwardLimitScale = Config->ForwardLimitScale;
	Drive.ReverseLimit = Config->ReverseLimit;
	Bridge->SetWaterJet(Drive);
	UE_CLOG(HALAbilities::ShouldLog(),LogHALAbility,Log,TEXT("WaterCannon start owner=%s seconds=%.2f range=%.1f nozzle=%s recoil=%.1f"),
		*GetNameSafe(GetOwner()),Config->Duration,Config->Range,*LocalNozzle.ToString(),Config->RecoilAcceleration);
	ForceNetUpdate();
	return true;
}
FTransform AHALWaterCannon::Frame(bool bVisual) const
{
	const auto* Car = GetOwner();
	const FTransform Body = Car->GetActorTransform();
	FVector Origin = Body.TransformPositionNoScale(LocalNozzle);
	if (bVisual && !Config->MountBone.IsNone())
	{
		const auto* Skin = Car->FindComponentByClass<USkeletalMeshComponent>();
		if (Skin && Skin->DoesSocketExist(Config->MountBone))
		{
			Origin = Skin->GetSocketLocation(Config->MountBone) + Body.TransformVectorNoScale(Config->NozzleOffset);
		}
	}
	const FRotator Aim(Config->AimPitch, Body.Rotator().Yaw, 0);
	return FTransform(Aim,Origin);
}
FVector AHALWaterCannon::LanePoint(float Side, float Fraction) const
{
	const float X = Config->Range * Fraction;
	const float HalfWidth = Config->NozzleRadius + X * FMath::Tan(FMath::DegreesToRadians(Config->FanAngle * .5f));
	return FVector(X, Side * HalfWidth, 0);
}
void AHALWaterCannon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Config) { return; }
	const double Now = ServerTime();
	if (HasAuthority())
	{
		if (!OwnerReady() || Now >= StartTime + Config->Duration) { Destroy(); return; }
		SampleJet(Now);
	}
	if (!IsActorBeingDestroyed() && IsValid(GetOwner()) && !IsNetMode(NM_DedicatedServer)) { Present(); }
}
void AHALWaterCannon::SampleJet(double Now)
{
	FHALWaterJetView Sample;
	const auto* Recovery = GetOwner()->FindComponentByClass<UVehicleRecoveryComponent>();
	Sample.bFlowing = GetOwner()->GetActorUpVector().Z > .35 && !(Recovery && Recovery->IsExecuting());
	TMap<TWeakObjectPtr<ATestVehiclePawn>,FHitResult> Vehicles;
	TSet<TWeakObjectPtr<ABasicBallActor>> TouchedBalls;
	const FTransform Aim = Frame(false);
	const FVector Heading = Aim.GetUnitAxis(EAxis::X).GetSafeNormal2D();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WaterCannon), false, GetOwner());
	Query.AddIgnoredActor(this);
	const auto Objects = FCollisionObjectQueryParams::AllObjects;
	// A calibrated muzzle can project beyond a thin wall when the bumper touches it.
	FVector Rear = GetOwner()->GetActorLocation();
	Rear.Z = Aim.GetLocation().Z;
	TArray<FHitResult> Hits;
	bool bMuzzleBlocked = false;
	struct FLaneSurface { const UPrimitiveComponent* Body=nullptr; FVector Point=FVector::ZeroVector, Normal=FVector::ZeroVector; };
	FLaneSurface Surfaces[LaneCount];
	const auto AddContact=[&](const FVector& Point,const FVector& Normal,int32 First,int32 Last)
	{
		auto& C=Sample.Contacts.AddDefaulted_GetRef();
		C.Point=Aim.InverseTransformPosition(Point);
		C.Normal=Aim.InverseTransformVectorNoScale(Normal).GetSafeNormal(UE_SMALL_NUMBER,FVector::BackwardVector);
		C.FirstLane=uint8(First); C.LastLane=uint8(Last);
	};
	if (Sample.bFlowing)
	{
		GetWorld()->LineTraceMultiByObjectType(Hits, Rear, Aim.GetLocation(), Objects, Query);
		const FHitResult* MuzzleHit = Hits.FindByPredicate([](const FHitResult& Hit)
		{
			return BlocksWater(Hit) && !Cast<ATestVehiclePawn>(Hit.GetActor()) && !Cast<ABasicBallActor>(Hit.GetActor());
		});
		bMuzzleBlocked=MuzzleHit!=nullptr;
		if(MuzzleHit) { AddContact(MuzzleHit->ImpactPoint,MuzzleHit->ImpactNormal,LaneCount/2,LaneCount/2); }
	}
	if (Sample.bFlowing) { Sample.Reach.Init(bMuzzleBlocked ? 0 : 255,LaneCount); }
	const float LaneHalfWidth = LanePoint(1,1).Y / (LaneCount - 1);
	if (Sample.bFlowing && !bMuzzleBlocked)
	{
		for (int32 Lane = 0; Lane < LaneCount; ++Lane)
		{
			const float Side = 2.f * Lane / (LaneCount - 1) - 1.f;
			const FVector From = Aim.TransformPosition(LanePoint(Side,0)), To = Aim.TransformPosition(LanePoint(Side,1));
			Hits.Reset();
			GetWorld()->SweepMultiByObjectType(Hits,From,To,Aim.GetRotation(),Objects,
				FCollisionShape::MakeBox(FVector(1,LaneHalfWidth,Config->HalfHeight)),Query);
			const FHitResult* Stop = nullptr;
			for (const FHitResult& Hit : Hits)
			{
				const auto* Car = Cast<ATestVehiclePawn>(Hit.GetActor());
				if (Cast<ABasicBallActor>(Hit.GetActor()) || (!Car && !BlocksWater(Hit))
					|| (Car && Hit.GetComponent() != Car->GetRootComponent())) { continue; }
				if (!Stop || Hit.Time < Stop->Time || (Hit.Time == Stop->Time && !Car)) { Stop = &Hit; }
			}
			if (Stop)
			{
				Sample.Reach[Lane] = uint8(FMath::RoundToInt(255.f * Stop->Time));
				Sample.ContactMask |= 1u << Lane;
				Surfaces[Lane]={Stop->GetComponent(),Stop->ImpactPoint,Stop->ImpactNormal};
				auto* Target = Cast<ATestVehiclePawn>(Stop->GetActor());
				if (Target && Target->CanReceiveSkillEffects())
				{
					const auto* Prior = Vehicles.Find(Target);
					if (!Prior || Stop->Time < Prior->Time) { Vehicles.Add(Target,*Stop); }
				}
			}
			for (const FHitResult& Hit : Hits)
			{
				auto* Ball = Cast<ABasicBallActor>(Hit.GetActor());
				if (Ball && Hit.GetComponent() == Ball->GetRootComponent() && (!Stop || Hit.Time < Stop->Time)
					&& (Ball->GetBallState() != EBasicBallState::Controlled || Ball->GetControlledBy() == GetOwner())) { TouchedBalls.Add(Ball); }
			}
		}
		for(int32 First=0;First<LaneCount;)
		{
			if(!Surfaces[First].Body) { ++First; continue; }
			int32 Last=First;
			while(Last+1<LaneCount && Surfaces[Last+1].Body==Surfaces[First].Body
				&& FMath::Abs(int32(Sample.Reach[Last+1])-Sample.Reach[Last])<=14
				&& FVector::DotProduct(Surfaces[Last+1].Normal,Surfaces[First].Normal)>.75) { ++Last; }
			const auto& Middle=Surfaces[(First+Last)/2];
			AddContact(Middle.Point,Middle.Normal,First,Last);
			First=Last+1;
		}
	}
	if (Sample.bFlowing && !bMuzzleBlocked) { SampleCrossingBalls(Aim,Sample,TouchedBalls); }
	else { PreviousBalls.Reset(); }
	const bool bFlowChanged = Sample.bFlowing != View.bFlowing;
	View = MoveTemp(Sample);
	if (bFlowChanged) { ForceNetUpdate(); }
	// Capture exposure before any impact can drop a held ball. A drop never becomes a launch in the same sample.
	for (const auto& Pair : Vehicles)
	{
		if (auto* Vehicle = Pair.Key.Get()) { HitVehicle(*Vehicle, Pair.Value.ImpactPoint, Heading, Now); }
		if (IsActorBeingDestroyed() || !OwnerReady()) { return; }
	}
	for (auto It = LaunchedContacts.CreateIterator(); It; ++It)
	{
		if (!TouchedBalls.Contains(*It)) { It.RemoveCurrent(); }
	}
	for (const auto& Key : TouchedBalls)
	{
		auto* Ball = Key.Get();
		if (!Ball || LaunchedContacts.Contains(Key)) { continue; }
		PendingBall = Key;
		const FVector Velocity = Heading * Config->BallLaunchSpeed;
		bool bLaunched = false;
		if (Ball->GetBallState() == EBasicBallState::Controlled)
		{
			auto* Control = GetOwner()->FindComponentByClass<UBallControlComponent>();
			bLaunched = Control && Control->LaunchWaterJetHeldBall(*this,*Ball,Velocity);
		}
		else { bLaunched = Ball->LaunchFromWaterCannon(*this,Velocity); }
		PendingBall.Reset();
		if (bLaunched)
		{
			LaunchedContacts.Add(Key);
			UE_CLOG(HALAbilities::ShouldLog(),LogHALAbility,Log,TEXT("WaterCannon ball owner=%s ball=%s sequence=%u"),
				*GetNameSafe(GetOwner()),*Ball->GetName(),Ball->GetBallStateSequence());
		}
	}
}
void AHALWaterCannon::SampleCrossingBalls(const FTransform& Aim, const FHALWaterJetView& Sample, TSet<TWeakObjectPtr<ABasicBallActor>>& Contacts)
{
	// A launched ball can cross the entire narrow jet between two game-thread samples.
	// Clip its relative swept centre against the six planes of the finite, radius-expanded fan.
	TMap<TWeakObjectPtr<ABasicBallActor>, FBallSample> Current;
	const double Slope = FMath::Tan(FMath::DegreesToRadians(Config->FanAngle * .5));
	for (TActorIterator<ABasicBallActor> It(GetWorld()); It; ++It)
	{
		auto* Ball = *It;
		if (Ball->GetBallState() == EBasicBallState::Controlled || !Ball->GetPhysicsRoot()) { continue; }
		const FVector Center = Ball->GetPhysicsRoot()->Bounds.Origin;
		const uint32 Seq = Ball->GetBallStateSequence();
		Current.Add(Ball,{Center,Seq});
		const auto* Prev = PreviousBalls.Find(Ball);
		if (!Prev || Prev->Sequence != Seq || Contacts.Contains(Ball) || Center.Equals(Prev->Center,1)) { continue; }
		const FVector A = PreviousAim.InverseTransformPosition(Prev->Center), B = Aim.InverseTransformPosition(Center);
		const double Radius = Ball->GetPhysicsRoot()->GetScaledSphereRadius();
		double Entry = 0, Exit = 1;
		auto Clip = [&](double From, double To)
		{
			if (From >= 0 && To >= 0) { return true; }
			if (From < 0 && To < 0) { return false; }
			const double T = From / (From - To);
			if (From < 0) { Entry = FMath::Max(Entry,T); } else { Exit = FMath::Min(Exit,T); }
			return Entry <= Exit;
		};
		const double W = Config->NozzleRadius + Radius * FMath::Sqrt(1. + Slope*Slope), H = Config->HalfHeight + Radius;
		if (!Clip(A.X+Radius,B.X+Radius) || !Clip(Config->Range+Radius-A.X,Config->Range+Radius-B.X)
			|| !Clip(W+Slope*A.X-A.Y,W+Slope*B.X-B.Y) || !Clip(W+Slope*A.X+A.Y,W+Slope*B.X+B.Y)
			|| !Clip(H-A.Z,H-B.Z) || !Clip(H+A.Z,H+B.Z)) { continue; }
		const FVector Local = FMath::Lerp(A,B,Entry);
		const double Side = FMath::Clamp(Local.Y/(Config->NozzleRadius+Slope*FMath::Max(0.,Local.X)),-1.,1.);
		const int32 Lane = FMath::RoundToInt((Side+1)*.5*(LaneCount-1));
		if ((Sample.ContactMask & (1u<<Lane)) && Local.X-Radius >= Sample.Reach[Lane]*Config->Range/255.f) { continue; }
		const FVector Contact = FMath::Lerp(Prev->Center,Center,Entry);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WaterBallCrossing),false,Ball);
		Query.AddIgnoredActor(GetOwner());
		TArray<FHitResult> Path;
		GetWorld()->LineTraceMultiByObjectType(Path,Contact,Center,FCollisionObjectQueryParams::AllObjects,Query);
		if (!Path.ContainsByPredicate([](const FHitResult& Hit) { return !Cast<ABasicBallActor>(Hit.GetActor()) && BlocksWater(Hit); })) { Contacts.Add(Ball); }
	}
	PreviousAim = Aim;
	PreviousBalls = MoveTemp(Current);
}
bool AHALWaterCannon::AuthorizesBallContact(const ABasicBallActor& Ball) const
{
	return HasAuthority() && !IsActorBeingDestroyed() && Config && PendingBall.Get() == &Ball
		&& View.bFlowing && OwnerReady() && ServerTime() < StartTime + Config->Duration;
}
void AHALWaterCannon::HitVehicle(ATestVehiclePawn& Target, const FVector& Point, const FVector& Direction, double Now)
{
	if (const auto* Next = NextHit.Find(&Target); Next && Now < *Next) { return; }
	FHALCombatAction Action;
	if (!Action.Begin(*this,Attack)) { return; }
	NextHit.Add(&Target, Now + Config->HitInterval);
	FHALCombatHitRequest Request;
	Request.AttackId = Action.GetSnapshot().AttackId;
	Request.PulseSequence = Action.GetPulseSequence();
	Request.bApplyDamage = Request.bApplyImpact = true;
	Request.Impact.Mode = EHALImpactMode::ExplicitTier;
	Request.Impact.Tier = Config->ImpactTier;
	Request.Impact.PushPreset = Config->PushDistance;
	Request.Hit.TargetActor = &Target;
	Request.Hit.TargetPhysicsBody = Cast<UPrimitiveComponent>(Target.GetRootComponent());
	Request.Hit.ImpactPoint = Point;
	Request.Hit.ImpactNormal = -Direction;
	const auto Result = FCombatResolver::ResolveCombatHit(Action,Request);
	UE_CLOG(HALAbilities::ShouldLog(),LogHALAbility,Log,TEXT("WaterCannon pulse target=%s hpRemoved=%.1f tier=%d"),
		*GetNameSafe(&Target),Result.ActualHPRemoved,int32(Result.ImpactSummary.EffectiveTier));
}
void AHALWaterCannon::EndPlay(EEndPlayReason::Type Reason)
{
	if(Reason==EEndPlayReason::Destroyed && GetWorld() && !GetWorld()->bIsTearingDown) { ReleaseVisual(); }
	if (HasAuthority() && IsValid(GetOwner()))
	{
		if (auto* Bridge = GetOwner()->FindComponentByClass<UHALMovementEffectBridge>()) { Bridge->EndWaterJet(Activation); }
	}
	Alarm->Stop();
	Super::EndPlay(Reason);
}
void AHALWaterCannon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AHALWaterCannon,Config,COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AHALWaterCannon,LocalNozzle,COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AHALWaterCannon,StartTime,COND_InitialOnly);
	DOREPLIFETIME(AHALWaterCannon,View);
}
