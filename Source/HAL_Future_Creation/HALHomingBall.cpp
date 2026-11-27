#include "HALHomingBall.h"

#include "TestVehiclePawn.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace
{
	FVector VehicleLaunchDirection(const AActor& Vehicle)
	{
		return FRotator(0., Vehicle.GetActorRotation().Yaw, 0.).Vector();
	}

	ATestVehiclePawn* NearestOpponent(AActor& Launcher, const FHALAttackSourceSnapshot& Source)
	{
		ATestVehiclePawn* Best = nullptr;
		double BestDistance = MAX_dbl;
		for (TActorIterator<ATestVehiclePawn> It(Launcher.GetWorld()); It; ++It)
		{
			if (*It == &Launcher || !HALSpecialBall::IsLiveOpponent(*It)) { continue; }
			const APlayerState* Player = It->GetPlayerState();
			if (Player && Source.InstigatorPlayerId != INDEX_NONE && Player->GetPlayerId() == Source.InstigatorPlayerId) { continue; }
			const double Distance = FVector::DistSquared2D(Launcher.GetActorLocation(), It->GetActorLocation());
			if (Distance < BestDistance || (Distance == BestDistance && Best && It->GetFName().LexicalLess(Best->GetFName())))
			{
				Best = *It;
				BestDistance = Distance;
			}
		}
		return Best;
	}
}

float UHALHomingBallDefinition::SpeedFraction(double Age) const
{
	return FMath::SmoothStep(0.f, AccelerationSeconds, float(Age));
}

bool UHALHomingBallDefinition::Validate(FString& Error) const
{
	if (!Super::Validate(Error)) { return false; }
	if (!FMath::IsWithinInclusive(InitialSpeed, 100.f, 10000.f) || !FMath::IsWithinInclusive(CruiseSpeed, InitialSpeed, 12000.f)
		|| !FMath::IsWithinInclusive(AccelerationSeconds, .1f, 15.f) || !FMath::IsWithinInclusive(FlightSeconds, .1f, 30.f)
		|| !FMath::IsWithinInclusive(InitialTurnRate, 0.f, 720.f) || !FMath::IsWithinInclusive(CruiseTurnRate, 0.f, InitialTurnRate)
		|| !FMath::IsWithinInclusive(BreakLockSpeedFraction, 0.f, 1.f) || !FMath::IsWithinInclusive(PassDistance, 50.f, 2500.f)
		|| !FMath::IsWithinInclusive(ExplosionRadius, 1.f, 5000.f) || !FMath::IsWithinInclusive(ExhaustLength, .25f, 5.f)
		|| !FMath::IsWithinInclusive(WarningScreenFraction, .015f, .12f) || !FMath::IsWithinInclusive(WarningBottomInset, 0.f, .30f)
		|| !FMath::IsWithinInclusive(WarningApproachSeconds, .2f, 5.f)
		|| !ExhaustMesh || Gameplay.Impact.Version != EHALImpactVersion::TieredV1)
	{
		Error = TEXT("Check homing ball speed and turn ranges, pass window, flight lifetime, blast, HUD, exhaust and tiered impact.");
		return false;
	}
	return true;
}

AHALHomingBall::AHALHomingBall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Exhaust = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CruiseExhaust"));
	Exhaust->SetupAttachment(VisualMesh);
	Exhaust->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Exhaust->SetGenerateOverlapEvents(false);
	Exhaust->SetCanEverAffectNavigation(false);
	Exhaust->SetCastShadow(false);
	Exhaust->SetVisibility(false);
}

UHALSpecialBallDefinition* AHALHomingBall::DefaultType() const
{
	return GetDefault<UHALMatchBallSettings>()->HomingBallType.LoadSynchronous();
}

const ATestVehiclePawn* AHALHomingBall::GetLockedVehicle() const
{
	return GetBallState() == EBasicBallState::Launched ? Flight.Target.Get() : nullptr;
}

FVector AHALHomingBall::GetVisualFacingDirection() const
{
	if (GetBallState() == EBasicBallState::Controlled && IsValid(GetControlledBy()))
	{
		return VehicleLaunchDirection(*GetControlledBy());
	}
	if (GetBallState() == EBasicBallState::Launched)
	{
		if (HasAuthority()) { return Heading; }
		const FVector Direction = PhysicsRoot->GetPhysicsLinearVelocity().GetSafeNormal2D();
		if (Flight.Started >= 0. && !Direction.IsNearlyZero()) { return Direction; }
		if (IsValid(GetLaunchedBy())) { return VehicleLaunchDirection(*GetLaunchedBy()); }
	}
	return FVector::ZeroVector;
}

void AHALHomingBall::BeginPlay()
{
	Super::BeginPlay();
	if (const auto* Type = GetHomingType(); Type && GetNetMode() != NM_DedicatedServer)
	{
		Exhaust->SetStaticMesh(Type->ExhaustMesh);
		if (auto* Jet = Exhaust->CreateDynamicMaterialInstance(0))
		{
			Jet->SetVectorParameterValue(TEXT("CurrentColor"), Type->CurrentColor);
			Jet->SetVectorParameterValue(TEXT("CoronaColor"), Type->CoronaColor);
			Jet->SetScalarParameterValue(TEXT("Glow"), Type->Glow);
		}
		Exhaust->SetRelativeLocation(FVector(-38., 0., 0.));
		RefreshFlightVisual();
	}
}

bool AHALHomingBall::DeliverLaunch(const FHALAttackSourceSnapshot& Attack, AActor* Launcher, const FVector& Velocity, const AActor* FrameSource)
{
	const auto* Type = GetHomingType();
	if (!HasAuthority() || !IsValid(Launcher) || !Type || bConsumed) { return false; }
	const FVector Forward = VehicleLaunchDirection(*Launcher);
	const FVector Direction = GetBallState() == EBasicBallState::Controlled ? Forward : Velocity.GetSafeNormal2D(UE_SMALL_NUMBER, Forward);
	if (!Super::DeliverLaunch(Attack, Launcher, Direction * Type->InitialSpeed, FrameSource)) { return false; }
	Heading = Direction;
	Flight.Started = LastUpdate = ServerNow();
	Flight.Target = NearestOpponent(*Launcher, Attack);
	PreviousOffset = Flight.Target ? Flight.Target->GetActorLocation() - GetActorLocation() : FVector::ZeroVector;
	PreviousOffset.Z = 0.;
	bCrossedTarget = false;
	RefreshFlightVisual();
	ForceNetUpdate();
	return true;
}

void AHALHomingBall::OnAuthorityBallStateChanged(EBasicBallState Previous)
{
	Super::OnAuthorityBallStateChanged(Previous);
	if (Previous == EBasicBallState::Launched && GetBallState() != Previous)
	{
		Flight = {};
		RefreshFlightVisual();
	}
}

void AHALHomingBall::UpdateFlightMovement()
{
	const auto& Type = *GetHomingType();
	const double Now = ServerNow();
	const double Age = Now - Flight.Started;
	if (Age >= Type.FlightSeconds) { Expire(); return; }
	const double Delta = Now - LastUpdate;
	if (Delta <= UE_SMALL_NUMBER) { return; }
	LastUpdate = Now;
	const float Gain = Type.SpeedFraction(Age);
	if (Flight.Target)
	{
		bool bLost = !HALSpecialBall::IsLiveOpponent(Flight.Target);
		if (!bLost)
		{
			FVector Offset = Flight.Target->GetActorLocation() - GetActorLocation();
			Offset.Z = 0.;
			const bool bAhead = FVector::DotProduct(Offset, Heading) >= 0.;
			if (bAhead) { bCrossedTarget = false; }
			else if (Gain >= Type.BreakLockSpeedFraction && FVector::DotProduct(PreviousOffset, Heading) >= 0.)
			{
				// Check the whole relative-motion segment: a fast car can cross between two samples.
				bCrossedTarget = FMath::PointDistToSegmentSquared(FVector::ZeroVector, PreviousOffset, Offset) <= FMath::Square(Type.PassDistance);
			}
			bLost = bCrossedTarget && !bAhead && Offset.SizeSquared() > PreviousOffset.SizeSquared();
			if (!bLost && !Offset.IsNearlyZero())
			{
				const double MaxTurn = FMath::Lerp(Type.InitialTurnRate, Type.CruiseTurnRate, Gain) * Delta;
				const double Yaw = Heading.Rotation().Yaw;
				Heading = FRotator(0., Yaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(Yaw, Offset.Rotation().Yaw), -MaxTurn, MaxTurn), 0.).Vector();
			}
			PreviousOffset = Offset;
		}
		if (bLost) { Flight.Target = nullptr; ForceNetUpdate(); }
	}
	PhysicsRoot->SetPhysicsLinearVelocity(Heading * FMath::Lerp(Type.InitialSpeed, Type.CruiseSpeed, Gain));
}

bool AHALHomingBall::ApplyContact(AActor*, const FVector& Origin, const FVector& Normal, const FVector& Travel)
{
	const auto& Type = *GetHomingType();
	// A physics callback can precede the flight timer in the frame the lifetime ends.
	if (ServerNow() - Flight.Started >= Type.FlightSeconds)
	{
		PlayCue(EHALBallCue::Expiration, GetActorLocation(), PhysicsRoot->GetScaledSphereRadius());
	}
	else { ApplyContactBlast(Type.ExplosionRadius, Origin, Normal, Travel); }
	return true;
}

void AHALHomingBall::RefreshFlightVisual()
{
	if (GetNetMode() == NM_DedicatedServer) { return; }
	const bool bFlying = Flight.Started >= 0.;
	Exhaust->SetVisibility(bFlying);
	SetActorTickEnabled(bFlying);
}

void AHALHomingBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const auto* Type = GetHomingType();
	if (!Type || Flight.Started < 0.) { return; }
	const double Age = FMath::Max(0., ServerNow() - Flight.Started);
	const float Gain = Type->SpeedFraction(Age);
	const float Flutter = 1.f + .07f * FMath::Sin(float(Age) * 31.f);
	Exhaust->SetRelativeScale3D(FVector(Type->ExhaustLength * (.24f + .76f * Gain) * Flutter, Flutter, Flutter));
}

void AHALHomingBall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALHomingBall, Flight);
}
