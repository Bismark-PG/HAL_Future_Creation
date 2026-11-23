#include "HALMeteorBall.h"

#include "HALBombActor.h"
#include "HALPlayerState.h"
#include "HALAbilitySystemComponent.h"
#include "HALHealthAttributeSet.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHALMeteorBall, Log, All);

namespace
{
bool IsMeteorOpponent(const ATestVehiclePawn* Vehicle)
{
	if (!IsValid(Vehicle) || !Vehicle->CanReceiveSkillEffects()) { return false; }
	const auto* Player = Vehicle->GetPlayerState<AHALPlayerState>();
	if (!Player) { return true; } // Practice vehicles and skill dummies have no round seat.
	const auto Life = Player->GetRoomData().RoundLife;
	return Life != EHALRoundLife::Eliminated && Life != EHALRoundLife::Forfeited;
}

bool MeteorGround(const ATestVehiclePawn& Vehicle, FVector& Ground)
{
	UWorld* World = Vehicle.GetWorld();
	const FVector Position = Vehicle.GetActorLocation();
	const FString Map = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	const auto* Arena = GetDefault<UHALMatchBallSettings>()->Arenas.FindByPredicate([&Map](const FHALMatchBallArena& Item)
	{ return Item.MatchMap.ToSoftObjectPath().GetLongPackageName() == Map; });
	if (Arena && !Arena->Contains(Position)) { return false; }
	FCollisionObjectQueryParams Surfaces;
	Surfaces.AddObjectTypesToQuery(ECC_WorldStatic);
	Surfaces.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FHitResult> Hits;
	World->LineTraceMultiByObjectType(Hits, Position + FVector::UpVector * 10., Position - FVector::UpVector * 20000.,
		Surfaces, FCollisionQueryParams(SCENE_QUERY_STAT(MeteorGround), false, &Vehicle));
	for (const auto& Hit : Hits)
	{
		const auto* Body = Hit.GetComponent();
		if (Body && !Hit.bStartPenetrating && Hit.ImpactNormal.Z >= .55 && Body->IsPhysicsCollisionEnabled()
			&& !Body->IsSimulatingPhysics() && Body->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Block)
		{
			Ground = Hit.ImpactPoint;
			return true;
		}
	}
	return false;
}
}

bool UHALMeteorBallDefinition::Validate(FString& Error) const
{
	if (!Super::Validate(Error)) { return false; }
	if (!FMath::IsWithinInclusive(TrackingSeconds,.15f,15.f) || !FMath::IsWithinInclusive(FallSeconds,.2f,8.f)
		|| !FMath::IsWithinInclusive(DamageRadius,50.f,5000.f) || !FMath::IsWithinInclusive(SkyHeight,500.f,20000.f)
		|| !FMath::IsWithinInclusive(RiseSeconds,.08f,2.f) || !FMath::IsWithinInclusive(FallingRockRadius,15.f,600.f)
		|| !FMath::IsWithinInclusive(TrailLength,0.f,8.f) || !FMath::IsWithinInclusive(MarkerSize,16.f,160.f)
		|| !FMath::IsWithinInclusive(SmokeLifetime,.08f,1.f) || !FMath::IsWithinInclusive(SmokeSpread,0.f,4.f)
		|| !FMath::IsWithinInclusive(RockSpinRate,0.f,1440.f) || !FMath::IsWithinInclusive(AirflowStrength,0.f,1.f)
		|| !FMath::IsWithinInclusive(AirflowSpeed,0.f,8.f)
		|| !FMath::IsWithinInclusive(CloudCoverage,.35f,.94f) || !FMath::IsWithinInclusive(CloudLift,0.f,.65f)
		|| !FMath::IsWithinInclusive(CloudCurl,0.f,2.f) || !FMath::IsWithinInclusive(ShockRingWidth,2.f,100.f)
		|| !FMath::IsWithinInclusive(CloudExpansionFraction,.2f,.95f) || !FMath::IsWithinInclusive(CloudTailSeconds,0.f,1.5f)
		|| !FMath::IsWithinInclusive(ShockRingIntensity,0.f,4.f)
		|| !FMath::IsWithinInclusive(MarkerClosedScale,.55f,1.f) || !FMath::IsWithinInclusive(MarkerLift,0.f,200.f)
		|| !FMath::IsWithinInclusive(SmokeColor.R,0.f,32.f) || !FMath::IsWithinInclusive(SmokeColor.G,0.f,32.f)
		|| !FMath::IsWithinInclusive(SmokeColor.B,0.f,32.f)
		|| !FlameMesh || !GroundPlane || !WarningMaterial || !PlumeMaterial || !MarkerCore || !MarkerJaws
		|| Gameplay.Impact.Version != EHALImpactVersion::TieredV1)
	{
		Error = TEXT("Check meteor timing, range, flight and impact FX, marker assets, and tiered impact.");
		return false;
	}
	return true;
}

UHALSpecialBallDefinition* AHALMeteorBall::DefaultType() const
{
	return GetDefault<UHALMatchBallSettings>()->MeteorBallType.LoadSynchronous();
}

bool AHALMeteorBall::DeliverLaunch(const FHALAttackSourceSnapshot& Attack, AActor* Launcher, const FVector&, const AActor*)
{
	auto* Type = Cast<UHALMeteorBallDefinition>(Definition);
	if (!Type || !IsValid(Launcher) || bConsumed || !AHALMeteorStrike::Start(*this, *Launcher, Attack, *Type)) { return false; }
	bConsumed = true;
	SetActorEnableCollision(false);
	Destroy();
	return true;
}

bool AHALMeteorStrike::Start(AHALMeteorBall& Ball, AActor& Launcher, const FHALAttackSourceSnapshot& Attack, UHALMeteorBallDefinition& Type)
{
	if (!Ball.HasAuthority() || Ball.GetNetMode() == NM_Client) { return false; }
	// Reservoir sampling makes each eligible opponent equally likely, independent of actor order.
	ATestVehiclePawn* Chosen = nullptr;
	FVector Ground = FVector::ZeroVector;
	int32 Count = 0;
	for (TActorIterator<ATestVehiclePawn> It(Ball.GetWorld()); It; ++It)
	{
		if (*It == &Launcher || !IsMeteorOpponent(*It)) { continue; }
		const auto* Player = It->GetPlayerState();
		if (Player && Attack.InstigatorPlayerId != INDEX_NONE && Player->GetPlayerId() == Attack.InstigatorPlayerId) { continue; }
		FVector CandidateGround;
		if (MeteorGround(**It, CandidateGround) && FMath::RandHelper(++Count) == 0)
		{
			Chosen = *It;
			Ground = CandidateGround;
		}
	}
	if (!Chosen) { return false; }
	const FTransform Transform(Ball.GetActorLocation());
	auto* Strike = Ball.GetWorld()->SpawnActorDeferred<AHALMeteorStrike>(StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Strike) { return false; }
	FHALAttackSourceSnapshot Snapshot = Attack;
	Snapshot.SourceActor = Strike;
	Snapshot.bCanHitInstigator = true;
	if (!Strike->Action.Begin(*Strike, Snapshot)) { Strike->Destroy(); return false; }
	Strike->State.Type = &Type;
	Strike->State.Target = Chosen;
	Strike->State.LaunchPoint = Transform.GetLocation();
	Strike->State.Started = Strike->ServerNow();
	Strike->State.Ground = Strike->LastGround = Ground;
	Strike->FinishSpawning(Transform);
	UE_LOG(LogHALMeteorBall, Log, TEXT("Launched %s instigator=%s target=%s tracking=%.2f fall=%.2f radius=%.0f"),
		*Strike->GetName(), *Launcher.GetName(), *Chosen->GetName(), Type.TrackingSeconds, Type.FallSeconds, Type.DamageRadius);
	return true;
}

double AHALMeteorStrike::ServerNow() const
{
	const auto* Game = GetWorld()->GetGameState();
	return Game ? Game->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void AHALMeteorStrike::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && State.Type && State.Target)
	{
		if (auto* Match = GetWorld()->GetGameState<AHALMatchGameState>())
		{ Match->OnMatchPhaseChanged.AddUniqueDynamic(this, &ThisClass::OnMatchPhase); }
		TargetAbilitySystem = State.Target->FindComponentByClass<UVehicleHealthComponent>()->GetBoundAbilitySystem();
		if (TargetAbilitySystem.IsValid())
		{
			TargetHealthHandle = TargetAbilitySystem->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute())
				.AddUObject(this, &ThisClass::OnTargetHealth);
		}
		GetWorldTimerManager().SetTimer(SampleTimer, this, &ThisClass::SampleTarget, .04f, true);
		GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::LockGround, State.Type->TrackingSeconds);
		ForceNetUpdate();
	}
	RefreshPresentation();
}

void AHALMeteorStrike::OnTargetHealth(const FOnAttributeChangeData& Change)
{
	if (Change.NewValue > 0 || State.Phase != EHALMeteorPhase::Tracking) { return; }
	if (IsValid(State.Target)) { MeteorGround(*State.Target, LastGround); }
	ReleaseTarget();
}

void AHALMeteorStrike::ReleaseTarget()
{
	if (TargetAbilitySystem.IsValid())
	{
		TargetAbilitySystem->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute()).Remove(TargetHealthHandle);
	}
	TargetHealthHandle.Reset();
	TargetAbilitySystem.Reset();
	State.Target = nullptr;
	State.Ground = LastGround;
	GetWorldTimerManager().ClearTimer(SampleTimer);
	ForceNetUpdate();
}

void AHALMeteorStrike::SampleTarget()
{
	if (IsMeteorOpponent(State.Target)) { MeteorGround(*State.Target, LastGround); }
	else { ReleaseTarget(); }
}

void AHALMeteorStrike::LockGround()
{
	if (State.Phase != EHALMeteorPhase::Tracking) { return; }
	if (IsMeteorOpponent(State.Target)) { MeteorGround(*State.Target, LastGround); }
	ReleaseTarget();
	State.Phase = EHALMeteorPhase::Falling;
	UE_LOG(LogHALMeteorBall, Log, TEXT("Locked %s ground=%s"), *GetName(), *LastGround.ToCompactString());
	const double Remaining = State.Started + State.Type->TrackingSeconds + State.Type->FallSeconds - ServerNow();
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::Land, FMath::Max(.001, Remaining));
	RefreshPresentation();
}

void AHALMeteorStrike::Land()
{
	if (State.Phase != EHALMeteorPhase::Falling || !Action.IsActive()) { return; }
	State.Phase = EHALMeteorPhase::Impact;
	ForceNetUpdate();
	FHALBlastArea Area{LastGround + FVector::UpVector * 2., State.Type->DamageRadius};
	Area.Apply(*this, Action);
	Action.Close();
	UE_LOG(LogHALMeteorBall, Log, TEXT("Impact %s ground=%s radius=%.0f"), *GetName(), *LastGround.ToCompactString(), Area.Radius);
	RefreshPresentation();
	// Keep the completed state relevant through the render tail, including a lethal final pulse.
	SetLifeSpan(FMath::Max(State.Type->GetCloudDuration(), State.Type->SmokeLifetime) + 1.f);
}

void AHALMeteorStrike::OnMatchPhase(EHALMatchPhase Phase)
{
	if (Phase == EHALMatchPhase::Finished && State.Phase != EHALMeteorPhase::Impact) { Destroy(); }
}

void AHALMeteorStrike::EndPlay(EEndPlayReason::Type Reason)
{
	if (auto* Match = GetWorld()->GetGameState<AHALMatchGameState>())
	{ Match->OnMatchPhaseChanged.RemoveDynamic(this, &ThisClass::OnMatchPhase); }
	if (HasAuthority()) { ReleaseTarget(); }
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	Action.Close();
	Super::EndPlay(Reason);
}

void AHALMeteorStrike::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALMeteorStrike, State);
}
