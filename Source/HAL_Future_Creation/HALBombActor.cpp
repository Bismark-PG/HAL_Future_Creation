#include "HALBombActor.h"
#include "BasicBallActor.h"
#include "CombatResolver.h"
#include "HALGameplayTags.h"
#include "VehicleHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"

bool FHALBombConfig::IsValid() const
{
	return FMath::IsFinite(FuseSeconds) && FuseSeconds >= 0.1f && FuseSeconds <= 60
		&& FMath::IsFinite(ExplosionRadius) && ExplosionRadius >= 1 && ExplosionRadius <= 5000
		&& FMath::IsFinite(Damage) && Damage >= 0 && Damage <= 10000 && HALImpact::IsValidTier(ImpactTier)
		&& HALImpact::IsValidPushPreset(ImpactPushPreset);
}

float AHALBombActor::GetBodyRadius() const { return Body->GetScaledSphereRadius(); }

AHALBombActor::AHALBombActor()
{
	ExplosionClass = AHALBombExplosion::StaticClass();
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	Body = CreateDefaultSubobject<USphereComponent>(TEXT("BombBody"));
	SetRootComponent(Body);
	Body->InitSphereRadius(30);
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true);
	Body->SetNotifyRigidBodyCollision(true);
	Body->SetGenerateOverlapEvents(false);
	Body->BodyInstance.bUseCCD = true;
	Body->SetLinearDamping(0.2f);
	Body->SetAngularDamping(0.5f);
	Body->OnComponentHit.AddDynamic(this, &ThisClass::OnBodyHit);
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BombVisual"));
	Visual->SetupAttachment(Body);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Countdown = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Countdown"));
	Countdown->SetupAttachment(Body);
	Countdown->SetRelativeLocation(FVector(0,0,60));
	Countdown->SetWorldSize(35);
	Countdown->SetTextRenderColor(FColor::Orange);
	Countdown->SetText(FText::FromString(TEXT("BOMB")));
}

void AHALBombActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && GetNetMode() != NM_Client)
	{
		if (!Config.IsValid() || !ExplosionClass || ExplosionClass->HasAnyClassFlags(CLASS_Abstract)
			|| !FMath::IsFinite(MinimumGroundNormalZ) || MinimumGroundNormalZ < 0.1f || MinimumGroundNormalZ > 1)
		{
			UE_LOG(LogTemp, Error, TEXT("Bomb %s has invalid configuration."), *GetName());
			Destroy(); return;
		}
		Attribution.InstigatorActor = GetInstigator();
		if (const APawn* Pawn = GetInstigator())
		{
			Attribution.InstigatorPlayerState = Pawn->GetPlayerState();
			if (const auto* State = Pawn->GetPlayerState()) { Attribution.InstigatorPlayerId = State->GetPlayerId(); }
		}
		ProbeTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ProbeInitialGround);
	}
	OnRep_Fuse();
}

void AHALBombActor::ProbeInitialGround()
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BombInitialGround), false, this);
	const FVector Start = GetActorLocation();
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0,0,Body->GetScaledSphereRadius()+3), ECC_Visibility, Params))
	{
		TryArmFromGround(Hit);
	}
}

bool AHALBombActor::TryArmFromGround(const FHitResult& Hit)
{
	AActor* Ground = Hit.GetActor();
	if (!HasAuthority() || GetNetMode() == NM_Client || bArmed || bDetonated || !Config.IsValid()
		|| !Hit.bBlockingHit || !IsValid(Ground) || Ground == this || Cast<APawn>(Ground)
		|| Cast<AHALBombActor>(Ground) || Cast<ABasicBallActor>(Ground) || Ground->FindComponentByClass<UVehicleHealthComponent>()
		|| Hit.ImpactNormal.ContainsNaN() || Hit.ImpactNormal.Z < MinimumGroundNormalZ) { return false; }
	return StartCountdown();
}

bool AHALBombActor::StartCountdown()
{
	if (!HasAuthority() || GetNetMode() == NM_Client || !HasActorBegunPlay() || IsActorBeingDestroyed()
		|| bArmed || bDetonated || !Config.IsValid()) { return false; }
	bArmed = true;
	FuseEndServerTime = GetWorld()->GetTimeSeconds() + Config.FuseSeconds;
	GetWorldTimerManager().SetTimer(FuseTimer, this, &ThisClass::Detonate, Config.FuseSeconds, false);
	ForceNetUpdate();
	OnRep_Fuse();
	return true;
}

void AHALBombActor::OnBodyHit(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	TryArmFromGround(Hit);
}

void AHALBombActor::OnRep_Fuse()
{
	UpdateCountdown();
	if (bArmed && GetNetMode() != NM_DedicatedServer && !GetWorldTimerManager().IsTimerActive(DisplayTimer))
	{
		GetWorldTimerManager().SetTimer(DisplayTimer, this, &ThisClass::UpdateCountdown, 0.1f, true);
	}
}

void AHALBombActor::UpdateCountdown()
{
	const AGameStateBase* State = GetWorld()->GetGameState();
	const float Now = State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	Countdown->SetText(FText::FromString(bArmed ? FString::Printf(TEXT("%.1f"), FMath::Max(0.f,FuseEndServerTime-Now)) : TEXT("BOMB")));
}

void AHALBombActor::Detonate()
{
	if (!HasAuthority() || GetNetMode() == NM_Client || !bArmed || bDetonated) { return; }
	bDetonated = true;
	GetWorldTimerManager().ClearTimer(FuseTimer);
	Body->SetSimulatePhysics(false);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	const FTransform Transform(FRotator::ZeroRotator, GetActorLocation());
	auto* Explosion = GetWorld()->SpawnActorDeferred<AHALBombExplosion>(ExplosionClass, Transform,
		GetOwner(), GetInstigator(), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Explosion)
	{
		Explosion->Initialize(Config, Attribution);
		Explosion->FinishSpawning(Transform);
	}
	Destroy();
}

void AHALBombActor::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(FuseTimer);
	GetWorldTimerManager().ClearTimer(DisplayTimer);
	GetWorldTimerManager().ClearTimer(ProbeTimer);
	Super::EndPlay(Reason);
}

void AHALBombActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALBombActor, bArmed);
	DOREPLIFETIME(AHALBombActor, FuseEndServerTime);
}

AHALBombExplosion::AHALBombExplosion()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	Range = CreateDefaultSubobject<USphereComponent>(TEXT("ExplosionRange"));
	SetRootComponent(Range);
	Range->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHALBombExplosion::Initialize(const FHALBombConfig& InConfig, const FHALAttackSourceSnapshot& Attribution)
{
	if (!HasAuthority() || GetNetMode() == NM_Client || bInitialized || HasActorBegunPlay() || !InConfig.IsValid()) { return; }
	Config = InConfig;
	FHALAttackSourceSnapshot Source = Attribution;
	Source.SourceActor = this;
	Source.SourceTag = HALGameplayTags::Combat_Source_Bomb;
	Source.FixedDamage = Config.Damage;
	Source.bCanHitInstigator = Config.bCanHitInstigator;
	Source.DamageMode = EHALDamageMode::SourceFixedDamage;
	Source.Impact.Version = EHALImpactVersion::TieredV1;
	Source.Impact.Tier = Config.ImpactTier;
	Source.Impact.PushPreset = Config.ImpactPushPreset;
	bInitialized = Action.Begin(*this, Source);
}

void AHALBombExplosion::BeginPlay()
{
	Super::BeginPlay();
	Range->SetSphereRadius(Config.ExplosionRadius);
	if (bDebugRange && GetNetMode() != NM_DedicatedServer)
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), Config.ExplosionRadius, 32, FColor::Orange, false, DisplaySeconds, 0, 3);
	}
	if (HasAuthority() && GetNetMode() != NM_Client)
	{
		if (!bInitialized) { Destroy(); return; }
		TArray<FOverlapResult> Hits;
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(BombExplosion),false,this);
		GetWorld()->OverlapMultiByObjectType(Hits, GetActorLocation(), FQuat::Identity,
			FCollisionObjectQueryParams(ECC_PhysicsBody), FCollisionShape::MakeSphere(Config.ExplosionRadius),Query);
		for (const FOverlapResult& Hit : Hits) { ResolveTarget(Hit.GetActor()); }
		SetLifeSpan(DisplaySeconds);
	}
}

FHALCombatHitResult AHALBombExplosion::ResolveTarget(AActor* Target)
{
	FHALCombatHitResult Rejected;
	Rejected.RejectReason = EHALHitRejectReason::InvalidTarget;
	if (!HasAuthority() || GetNetMode() == NM_Client || !bInitialized || !IsValid(Target)
		|| !Target->FindComponentByClass<UVehicleHealthComponent>()) { return Rejected; }
	auto* Body = Cast<UPrimitiveComponent>(Target->GetRootComponent());
	if (!Body || !Body->IsCollisionEnabled()) { return Rejected; }
	FVector Closest;
	const float Distance = Body->GetClosestPointOnCollision(GetActorLocation(), Closest);
	if (Distance < 0 || Distance > Config.ExplosionRadius) { return Rejected; }
	FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero()) { Direction = FVector::ForwardVector; }
	FHALCombatHitRequest Request;
	Request.AttackId = Action.GetSnapshot().AttackId;
	Request.bApplyImpact = true;
	Request.Impact.Mode = EHALImpactMode::ExplicitTier;
	Request.Impact.Tier = Action.GetSnapshot().Impact.Tier;
	Request.Impact.PushPreset = Action.GetSnapshot().Impact.PushPreset;
	Request.Hit.SourceActor = this;
	Request.Hit.TargetActor = Target;
	Request.Hit.TargetPhysicsBody = Body;
	Request.Hit.ImpactPoint = Closest;
	Request.Hit.ImpactNormal = -Direction;
	return FCombatResolver::ResolveCombatHit(Action, Request);
}

void AHALBombExplosion::EndPlay(const EEndPlayReason::Type Reason)
{
	Action.Close(); Super::EndPlay(Reason);
}

void AHALBombExplosion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALBombExplosion, Config);
}
