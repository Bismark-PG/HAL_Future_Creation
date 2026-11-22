#include "HALSpecialBall.h"
#include "HALSpecialBallCue.h"
#include "HALBombActor.h"
#include "HALElectricWallGimmick.h"
#include "HALMatchGameState.h"
#include "HALArenaEffectGenerator.h"
#include "SpringPunchGimmick.h"
#include "VehicleHealthComponent.h"
#include "VehicleStabilityRules.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	void FindSpecialBallContactFace(UPrimitiveComponent& Body, const FHitResult& Hit, const FVector& IncomingCenter,
		float BallRadius, FVector& Point, FVector& Normal)
	{
		// Chaos may report its solver contact inside a wall. Start on the incoming side and
		// trace only the struck component, so both the blast and its cover rays start at the real face.
		if (FVector::DotProduct(IncomingCenter - Point, Normal) < 0.) { Normal = -Normal; }
		const double Reach = 2. * BallRadius + FMath::Max(0.f, Hit.PenetrationDepth);
		const FVector Outside = Point + Normal * Reach;
		FHitResult Face;
		if (Body.LineTraceComponent(Face, Outside, Point - Normal * Reach, FCollisionQueryParams(SCENE_QUERY_STAT(SolarImpactFace), false))
			&& !Face.bStartPenetrating)
		{
			Point = Face.ImpactPoint;
			Normal = Face.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, Normal);
		}
	}
}

bool UHALSpecialBallDefinition::Validate(FString& Error) const
{
	if (!Super::Validate(Error)) { return false; }
	auto Between = [](float Value, float Low, float High) { return FMath::IsFinite(Value) && Value >= Low && Value <= High; };
	auto ValidColor = [&Between](const FLinearColor& Color)
	{
		return Between(Color.R, 0, 32) && Between(Color.G, 0, 32) && Between(Color.B, 0, 32);
	};
	if (!Between(FreeLifeSeconds, .1f, 600) || !Between(WarningSeconds, 0, 60)
		|| !Between(ArrivalSeconds, .12f, 3) || !Between(ExpirationSeconds, .12f, 3) || !Between(ImpactSeconds, .12f, 3)
		|| !Between(CoronaSize, 1, 6) || !Between(SurfaceFlowSpeed, 0, 10) || !Between(Glow, .1f, 8)
		|| !Between(BodyCoronaWidth, 0, 1) || !Between(BodyCoronaGlow, 0, 8)
		|| !ValidColor(CoreColor) || !ValidColor(CurrentColor) || !ValidColor(CoronaColor) || !CueMaterial)
	{
		Error = TEXT("Check special ball lifetime, cue material, timing and solar colours.");
		return false;
	}
	return true;
}

float UHALSpecialBallDefinition::CueSeconds(EHALBallCue Cue) const
{
	switch (Cue)
	{
	case EHALBallCue::Arrival: return ArrivalSeconds;
	case EHALBallCue::Expiration: return ExpirationSeconds;
	default: return ImpactSeconds;
	}
}

float UHALExplosiveBallDefinition::CueSeconds(EHALBallCue Cue) const
{
	return Cue == EHALBallCue::Impact && EruptionMaterial ? ImpactSeconds + ProminenceTailSeconds : Super::CueSeconds(Cue);
}

bool UHALExplosiveBallDefinition::Validate(FString& Error) const
{
	if (!Super::Validate(Error)) { return false; }
	if (!FMath::IsFinite(ExplosionRadius) || ExplosionRadius < 1 || ExplosionRadius > 5000 || FlareCount < 4 || FlareCount > 20
		|| !FMath::IsWithinInclusive(ProminenceHeight, .15f, .9f) || !FMath::IsWithinInclusive(ProminenceWidth, .04f, .35f)
		|| !FMath::IsWithinInclusive(ProminenceFlowSpeed, 0.f, 5.f) || !FMath::IsWithinInclusive(ProminenceFlowStrength, 0.f, 1.f)
		|| !FMath::IsWithinInclusive(ProminenceTailSeconds, 0.f, 1.5f)
		|| !FMath::IsWithinInclusive(CoreSmokeSize, 0.f, .45f)
		|| Gameplay.Impact.Version != EHALImpactVersion::TieredV1)
	{
		Error = TEXT("Check explosion radius, tiered impact, flare count, prominence shape, flow, tail duration and central smoke size.");
		return false;
	}
	return true;
}

AHALSpecialBall::AHALSpecialBall()
{
	ConfigurationSource = EVehicleConfigurationSource::Definition;
	bAlwaysRelevant = true;
}

void AHALSpecialBall::SelectDefinition()
{
	ConfigurationSource = EVehicleConfigurationSource::Definition;
	bPrimaryCameraTarget = false;
	if (!Definition) { Definition = DefaultType(); }
}

void AHALSpecialBall::OnConstruction(const FTransform& Transform)
{
	SelectDefinition();
	Super::OnConstruction(Transform);
}

void AHALSpecialBall::PreInitializeComponents()
{
	SelectDefinition();
	if (HasAuthority()) { SpawnType = Cast<UHALSpecialBallDefinition>(Definition); }
	Super::PreInitializeComponents();
}

void AHALSpecialBall::PostNetInit()
{
	// Initial network properties are available before Super dispatches the first BeginPlay.
	if (SpawnType && Definition != SpawnType && !HasActorBegunPlay())
	{
		Definition = SpawnType;
		bConfigurationValid = ApplyDefinition(false);
	}
	Super::PostNetInit();
}

void AHALSpecialBall::BeginPlay()
{
	Super::BeginPlay();
	const auto* Type = GetTypeDefinition();
	if (!Type || !IsConfigurationValid())
	{
		if (HasAuthority()) { Destroy(); }
		return;
	}
	bPresentationReady = true;
	if (GetNetMode() != NM_DedicatedServer && VisualMesh->GetMaterial(0))
	{
		Surface = VisualMesh->CreateDynamicMaterialInstance(0);
		if (Surface)
		{
			Surface->SetVectorParameterValue(TEXT("CoreColor"), Type->CoreColor);
			Surface->SetVectorParameterValue(TEXT("CurrentColor"), Type->CurrentColor);
			Surface->SetVectorParameterValue(TEXT("CoronaColor"), Type->CoronaColor);
			Surface->SetScalarParameterValue(TEXT("Glow"), Type->Glow);
			Surface->SetScalarParameterValue(TEXT("FlowSpeed"), Type->SurfaceFlowSpeed);
		}
		if (Type->BodyCoronaMaterial && Type->BodyCoronaWidth > 0.f && Type->BodyCoronaGlow > 0.f)
		{
			BodyCorona = UMaterialInstanceDynamic::Create(Type->BodyCoronaMaterial, this);
			BodyCorona->SetVectorParameterValue(TEXT("CurrentColor"), Type->CurrentColor);
			BodyCorona->SetVectorParameterValue(TEXT("CoronaColor"), Type->CoronaColor);
			BodyCorona->SetScalarParameterValue(TEXT("RadiusCm"), PhysicsRoot->GetScaledSphereRadius());
			BodyCorona->SetScalarParameterValue(TEXT("Width"), Type->BodyCoronaWidth);
			BodyCorona->SetScalarParameterValue(TEXT("Intensity"), Type->BodyCoronaGlow);
			BodyCorona->SetScalarParameterValue(TEXT("FlowSpeed"), Type->SurfaceFlowSpeed);
			VisualMesh->SetOverlayMaterial(BodyCorona);
			VisualMesh->SetBoundsScale(FMath::Max(VisualMesh->BoundsScale, 1.f + Type->BodyCoronaWidth));
		}
	}
	if (HasAuthority())
	{
		const FString Map = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
		const auto* Entry = GetDefault<UHALMatchBallSettings>()->Arenas.FindByPredicate(
			[&Map](const FHALMatchBallArena& Item) { return Item.MatchMap.ToSoftObjectPath().GetLongPackageName() == Map; });
		if (Entry && Entry->IsValid()) { Arena = *Entry; }
		if (auto* State = GetWorld()->GetGameState<AHALMatchGameState>())
		{
			State->OnMatchPhaseChanged.AddUniqueDynamic(this, &ThisClass::OnMatchPhaseChanged);
		}
		if (Arena) { GetWorldTimerManager().SetTimer(ArenaTimer, this, &ThisClass::CheckArena, .1f, true); }
		CheckArena();
	}
	OnRep_Clock();
}

double AHALSpecialBall::ServerNow() const
{
	const auto* State = GetWorld()->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void AHALSpecialBall::CheckArena()
{
	if (bConsumed) { return; }
	const auto* State = GetWorld()->GetGameState<AHALMatchGameState>();
	if (State && State->GetMatchPhase() == EHALMatchPhase::Finished) { bConsumed = true; Destroy(); return; }
	if (Arena)
	{
		if (!State || State->GetMatchPhase() != EHALMatchPhase::Playing) { return; }
		if (!Arena->Contains(GetActorLocation()))
		{
			if (OutsideSince < 0.) { OutsideSince = ServerNow(); }
			if (ServerNow() - OutsideSince >= Arena->OutsideGraceSeconds) { Expire(); return; }
		}
		else { OutsideSince = -1.; }
	}
	if (!Clock.bStarted)
	{
		Clock.Remaining = GetTypeDefinition()->FreeLifeSeconds;
		Clock.bStarted = true;
		OnAuthorityBallStateChanged(GetBallState());
		PlayCue(EHALBallCue::Arrival, GetActorLocation(), PhysicsRoot->GetScaledSphereRadius());
	}
}

void AHALSpecialBall::OnMatchPhaseChanged(EHALMatchPhase Phase)
{
	if (HasAuthority()) { CheckArena(); }
}

float AHALSpecialBall::RemainingFreeSeconds() const
{
	return Clock.FreeUntil < 0. ? Clock.Remaining : FMath::Max(0., Clock.FreeUntil - ServerNow());
}

bool AHALSpecialBall::CanUseBall() const
{
	return Clock.bStarted && !bConsumed && RemainingFreeSeconds() > 0.f;
}

void AHALSpecialBall::OnAuthorityBallStateChanged(EBasicBallState Previous)
{
	if (!Clock.bStarted || bConsumed) { return; }
	if (Clock.FreeUntil >= 0.)
	{
		Clock.Remaining = RemainingFreeSeconds();
		Clock.FreeUntil = -1.;
	}
	GetWorldTimerManager().ClearTimer(ExpiryTimer);
	if (GetBallState() == EBasicBallState::Free)
	{
		Clock.FreeUntil = ServerNow() + Clock.Remaining;
		GetWorldTimerManager().SetTimer(ExpiryTimer, this, &ThisClass::Expire, FMath::Max(.001f, Clock.Remaining), false);
	}
	OnRep_Clock();
	ForceNetUpdate();
}

void AHALSpecialBall::OnRep_Clock()
{
	if (!bPresentationReady) { return; }
	UpdateStaging();
	VisualMesh->SetVisibility(Clock.bStarted);
	if (Surface || BodyCorona)
	{
		const auto* Type = GetTypeDefinition();
		const float WarnAt = Clock.FreeUntil >= 0. && Type && Type->WarningSeconds > 0
			? GetWorld()->GetTimeSeconds() + RemainingFreeSeconds() - Type->WarningSeconds : 1.e9f;
		if (Surface) { Surface->SetScalarParameterValue(TEXT("WarnAt"), WarnAt); }
		if (BodyCorona) { BodyCorona->SetScalarParameterValue(TEXT("WarnAt"), WarnAt); }
	}
}

void AHALSpecialBall::UpdateStaging()
{
	const bool bWait = !Clock.bStarted;
	if (bStaged == bWait) { return; }
	bStaged = bWait;
	if (bWait)
	{
		SpawnResponses = PhysicsRoot->GetCollisionResponseToChannels();
		bSpawnGravity = PhysicsRoot->IsGravityEnabled();
		// Keep the same Chaos body for network physics while it waits at the placement transform.
		PhysicsRoot->SetCollisionResponseToAllChannels(ECR_Ignore);
		PhysicsRoot->SetEnableGravity(false);
		if (PhysicsRoot->IsSimulatingPhysics())
		{
			PhysicsRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
			PhysicsRoot->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		}
	}
	else if (HasAuthority() || GetBallState() != EBasicBallState::Controlled)
	{
		PhysicsRoot->SetCollisionResponseToChannels(SpawnResponses);
		PhysicsRoot->SetEnableGravity(bSpawnGravity);
		PhysicsRoot->WakeAllRigidBodies();
	}
}

void AHALSpecialBall::PlayCue(EHALBallCue Cue, const FVector& Location, float Radius, const FVector& Normal)
{
	AHALSpecialBallCue::Spawn(*GetWorld(), Cue, CastChecked<UHALSpecialBallDefinition>(Definition), Location, Radius, Normal);
}

void AHALSpecialBall::Expire()
{
	if (!HasAuthority() || bConsumed) { return; }
	bConsumed = true;
	PlayCue(EHALBallCue::Expiration, GetActorLocation(), PhysicsRoot->GetScaledSphereRadius());
	Destroy();
}

void AHALSpecialBall::OnAcceptedVehicleHit()
{
	if (bConsumed) { return; }
	bConsumed = true;
	PlayCue(EHALBallCue::Impact, GetActorLocation(), PhysicsRoot->GetScaledSphereRadius());
	Destroy();
}

void AHALSpecialBall::EndPlay(EEndPlayReason::Type Reason)
{
	if (auto* State = GetWorld()->GetGameState<AHALMatchGameState>())
	{ State->OnMatchPhaseChanged.RemoveDynamic(this, &ThisClass::OnMatchPhaseChanged); }
	GetWorldTimerManager().ClearTimer(ExpiryTimer);
	GetWorldTimerManager().ClearTimer(ArenaTimer);
	Super::EndPlay(Reason);
}

void AHALSpecialBall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AHALSpecialBall, SpawnType, COND_InitialOnly);
	DOREPLIFETIME(AHALSpecialBall, Clock);
}

UHALSpecialBallDefinition* AHALExplosiveBall::DefaultType() const
{
	return GetDefault<UHALMatchBallSettings>()->ExplosiveBallType.LoadSynchronous();
}

void AHALContactSpecialBall::OnAuthorityBallStateChanged(EBasicBallState Previous)
{
	Super::OnAuthorityBallStateChanged(Previous);
	GetWorldTimerManager().ClearTimer(FlightTimer);
	if (!bConsumed && GetBallState() == EBasicBallState::Launched)
	{
		LastFlightCenter = GetActorLocation();
		GetWorldTimerManager().SetTimer(FlightTimer, this, &ThisClass::CheckFlight, .025f, true);
	}
}

bool AHALContactSpecialBall::FindFieldHit(const FVector& Start, const FVector& End, FHitResult& Out) const
{
	bool bFound = false;
	const float Radius = PhysicsRoot->GetScaledSphereRadius();
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
	for (TActorIterator<AHALElectricWallGimmick> It(GetWorld()); It; ++It)
	{
		UPrimitiveComponent* Field = It->GetEnergizedSurface();
		if (!Field || !Field->IsQueryCollisionEnabled()) { continue; }
		FHitResult Hit;
		FVector Closest;
		const float Gap = Field->GetClosestPointOnCollision(Start, Closest);
		if (Gap >= 0 && Gap <= Radius)
		{
			Hit = FHitResult(*It, Field, Closest, (Start - Closest).GetSafeNormal());
			Hit.Time = 0;
		}
		else if (End.Equals(Start, .01) ||
			!Field->SweepComponent(Hit, Start, End, FQuat::Identity, Sphere, false)) { continue; }
		if (!bFound || Hit.Time < Out.Time) { bFound = true; Out = Hit; }
	}
	return bFound;
}

void AHALContactSpecialBall::CheckFlight()
{
	if (bConsumed || GetBallState() != EBasicBallState::Launched) { return; }
	const FVector Center = GetActorLocation();
	FHitResult Hit;
	if (FindFieldHit(LastFlightCenter, Center, Hit)) { ConsumeContact(Hit.GetActor(), Hit.ImpactPoint, Hit.ImpactNormal); return; }
	LastFlightCenter = Center;
}

void AHALContactSpecialBall::ResolveLaunchedContact(AActor* Other, UPrimitiveComponent* Body, FVector Impulse, const FHitResult& Hit)
{
	if (bConsumed || !Body || !IsValid(Other)) { return; }
	// A query-only electric curtain can precede the rigid contact in the same physics step.
	FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	FVector ImpactPoint = Hit.ImpactPoint;
	const FVector ContactCenter = Hit.ImpactPoint + Normal * PhysicsRoot->GetScaledSphereRadius();
	FHitResult FieldHit;
	if (FindFieldHit(LastFlightCenter, ContactCenter, FieldHit)) { ConsumeContact(FieldHit.GetActor(), FieldHit.ImpactPoint, FieldHit.ImpactNormal); return; }
	if (Other->IsA<ABasicBallActor>() || Other == GetLaunchedBy()) { LastFlightCenter = ContactCenter; return; }
	if (const auto* Health = Other->FindComponentByClass<UVehicleHealthComponent>())
	{
		const APawn* Pawn = Cast<APawn>(Other);
		const APlayerState* State = Pawn ? Pawn->GetPlayerState() : nullptr;
		if (!Health->IsHealthReady() || Health->GetCurrentHP() <= 0 ||
			(State && State->GetPlayerId() != INDEX_NONE && State->GetPlayerId() == GetBallRepState().LastLauncherPlayerId)) { return; }
	}
	else
	{
		const bool bGimmick = Other->IsA<ASpringPunchGimmick>() || Other->IsA<AHALElectricWallGimmick>() || Other->IsA<AHALArenaEffectGenerator>();
		const bool bWorld = Body->GetCollisionObjectType() == ECC_WorldStatic || Body->GetCollisionObjectType() == ECC_WorldDynamic;
		const auto* Stability = GetWorld()->GetSubsystem<UVehicleStabilitySubsystem>();
		if (!Body->IsPhysicsCollisionEnabled() || (!bGimmick && (!Stability ||
			!Stability->GetContactRules().IsOrdinaryWall(bWorld && !Body->IsSimulatingPhysics(), Normal))))
		{
			LastFlightCenter = ContactCenter;
			return;
		}
		if (!Normal.IsNearlyZero())
		{
			FindSpecialBallContactFace(*Body, Hit, LastFlightCenter, PhysicsRoot->GetScaledSphereRadius(), ImpactPoint, Normal);
		}
	}
	ConsumeContact(Other, ImpactPoint, Normal);
}

void AHALContactSpecialBall::ConsumeContact(AActor* Other, const FVector& Origin, const FVector& Normal)
{
	if (!HasAuthority() || bConsumed || GetBallState() != EBasicBallState::Launched || !GetLaunchAction().IsActive()) { return; }
	bConsumed = true;
	const FVector Travel = (Origin - LastFlightCenter).GetSafeNormal2D();
	if (ApplyContact(Other, Origin, Normal, Travel)) { Destroy(); }
	else { bConsumed = false; }
}

bool AHALExplosiveBall::ApplyContact(AActor* Other, const FVector& Origin, const FVector& Normal, const FVector& Travel)
{
	const auto* Type = Cast<UHALExplosiveBallDefinition>(Definition);
	if (!Type) { return false; }
	FHALBlastArea Area{Origin, Type->ExplosionRadius};
	Area.FallbackDirection = Travel.IsNearlyZero() ? -Normal.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::BackwardVector) : Travel;
	Area.CoverStart = Origin + Normal.GetSafeNormal() * 1.5f;
	Area.Apply(*this, GetLaunchAction());
	PlayCue(EHALBallCue::Impact, Origin, Area.Radius, Normal.GetSafeNormal());
	return true;
}

void AHALContactSpecialBall::EndPlay(EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(FlightTimer);
	Super::EndPlay(Reason);
}
