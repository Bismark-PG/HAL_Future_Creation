#include "HALSlipperyTrail.h"

#include "ArcadeVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "HALGameFlowPermissions.h"
#include "HALMovementAreaReceiver.h"
#include "Misc/App.h"
#include "Net/UnrealNetwork.h"
#include "TestVehiclePawn.h"
#include "TimerManager.h"
#include "VehicleHealthComponent.h"
#include "VehicleRecoveryComponent.h"

namespace
{
	bool IsActiveVehicle(const ATestVehiclePawn& Vehicle)
	{
		const auto* Health = Vehicle.FindComponentByClass<UVehicleHealthComponent>();
		return Vehicle.IsConfigurationValid() && Vehicle.GetController() && Vehicle.GetController()->GetPawn() == &Vehicle
			&& Health && Health->IsHealthReady() && Health->GetCurrentHP() > 0;
	}

	bool ProjectGround(const ATestVehiclePawn& Vehicle, const FHALSlipperyTrailEmission& Rules,
		const FVector& Probe, FVector& Out, FVector* OutNormal = nullptr)
	{
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(SlipperyTrailGround), false, &Vehicle);
		const auto* Body = Cast<UPrimitiveComponent>(Vehicle.GetRootComponent());
		if (!Body) { return false; }
		TArray<FHitResult> Hits;
		Vehicle.GetWorld()->LineTraceMultiByObjectType(Hits, Probe, Probe - FVector::UpVector * Rules.GroundProbeDepth,
			Objects, Params);
		for (const FHitResult& Hit : Hits)
		{
			const auto* Floor = Hit.GetComponent();
			if (!Floor || Floor->IsSimulatingPhysics() || !Floor->IsPhysicsCollisionEnabled()
				|| Floor->GetCollisionResponseToChannel(Body->GetCollisionObjectType()) != ECR_Block
				|| Body->GetCollisionResponseToChannel(Floor->GetCollisionObjectType()) != ECR_Block) { continue; }
			// Do not project through a solid but too-steep surface to a lower floor.
			if (Hit.bStartPenetrating || Hit.ImpactNormal.Z < Rules.MinimumGroundNormalZ) { return false; }
			Out = Hit.ImpactPoint;
			if (OutNormal) { *OutNormal = Hit.ImpactNormal; }
			return true;
		}
		return false;
	}

	bool HasGroundContact(const ATestVehiclePawn& Vehicle)
	{
		const auto* Movement = Vehicle.FindComponentByClass<UArcadeVehicleMovementComponent>();
		const auto Pose = Movement ? Movement->GetStabilityObservation() : FVehicleStabilityObservation{};
		return Pose.bValid && Pose.bGrounded;
	}

	// Clip the swept vehicle center to a linear inequality (positive means inside).
	bool Clip(double AtStart, double AtEnd, double& Min, double& Max)
	{
		if (AtStart < 0 && AtEnd < 0) { return false; }
		if (AtStart < 0) { Min = FMath::Max(Min, -AtStart / (AtEnd - AtStart)); }
		if (AtEnd < 0) { Max = FMath::Min(Max, AtStart / (AtStart - AtEnd)); }
		return Min <= Max;
	}

	bool ClipHeight(double StartZ, double EndZ, double Tolerance, double& Min, double& Max)
	{
		return Clip(Tolerance + StartZ, Tolerance + EndZ, Min, Max)
			&& Clip(Tolerance - StartZ, Tolerance - EndZ, Min, Max);
	}

	bool SweepsDisk(const FVector& From, const FVector& To, const FVector& Center, double Radius, double Height)
	{
		double Min = 0, Max = 1;
		if (!ClipHeight(From.Z - Center.Z, To.Z - Center.Z, Height, Min, Max)) { return false; }
		const FVector2D Relative(From - Center);
		const FVector2D Step(To - From);
		const double Along = Step.IsNearlyZero() ? Min
			: FMath::Clamp(-FVector2D::DotProduct(Relative, Step) / Step.SizeSquared(), Min, Max);
		return (Relative + Along * Step).SizeSquared() <= FMath::Square(Radius);
	}

	bool SweepsSection(const FVector& From, const FVector& To, const FVector& A, const FVector& B,
		double WidthA, double WidthB, double Height)
	{
		const FVector2D Segment(B - A);
		const double Length = Segment.Size();
		if (Length <= UE_SMALL_NUMBER) { return false; }
		const FVector2D Direction = Segment / Length;
		const FVector2D Across(-Direction.Y, Direction.X);
		const FVector2D Start(From - A), End(To - A);
		const double AlongStart = FVector2D::DotProduct(Start, Direction) / Length;
		const double AlongEnd = FVector2D::DotProduct(End, Direction) / Length;
		double Min = 0, Max = 1;
		if (!Clip(AlongStart, AlongEnd, Min, Max) || !Clip(1 - AlongStart, 1 - AlongEnd, Min, Max)) { return false; }
		const double StartWidth = FMath::Lerp(WidthA, WidthB, AlongStart);
		const double EndWidth = FMath::Lerp(WidthA, WidthB, AlongEnd);
		const double StartAcross = FVector2D::DotProduct(Start, Across);
		const double EndAcross = FVector2D::DotProduct(End, Across);
		return Clip(StartWidth + StartAcross, EndWidth + EndAcross, Min, Max)
			&& Clip(StartWidth - StartAcross, EndWidth - EndAcross, Min, Max)
			&& ClipHeight(From.Z - FMath::Lerp(A.Z, B.Z, AlongStart), To.Z - FMath::Lerp(A.Z, B.Z, AlongEnd), Height, Min, Max);
	}
}

float FHALSlipperyTrailShape::HalfWidthAt(float PointAge) const
{
	return FMath::Lerp(InitialHalfWidth, MaximumHalfWidth, FMath::Clamp(PointAge / ExpansionSeconds, 0.f, 1.f));
}

AHALSlipperyTrail::AHALSlipperyTrail()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = FApp::CanEverRender();
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
	SetNetUpdateFrequency(25);
}

bool AHALSlipperyTrail::CanLay(const ATestVehiclePawn& Vehicle)
{
	const auto* Movement = Vehicle.FindComponentByClass<UArcadeVehicleMovementComponent>();
	const auto* Recovery = Vehicle.FindComponentByClass<UVehicleRecoveryComponent>();
	const auto Pose = Movement ? Movement->GetStabilityObservation() : FVehicleStabilityObservation{};
	return Pose.bValid && Pose.bGrounded && Pose.bDrivingAllowed && !Pose.Recovery.bActive
		&& (!Recovery || !Recovery->IsExecuting())
		&& FVector::DotProduct(Pose.Up, Pose.GroundNormal) > 0;
}

bool AHALSlipperyTrail::FindTailGround(const ATestVehiclePawn& Vehicle, const FHALSlipperyTrailEmission& Rules,
	const FHALSlipperyTrailShape& InShape, FVector& Out, FVector* OutNormal)
{
	const auto* Body = Cast<UPrimitiveComponent>(Vehicle.GetRootComponent());
	if (!Body || !CanLay(Vehicle)) { return false; }
	const FBoxSphereBounds Bounds = Body->CalcBounds(FTransform::Identity);
	const FVector LocalTail(Bounds.Origin.X - Bounds.BoxExtent.X, Bounds.Origin.Y, Bounds.Origin.Z);
	const FVector Rear = Body->GetComponentTransform().TransformPosition(LocalTail)
		- Body->GetForwardVector() * Rules.TailOffset;
	FVector CenterGround;
	if (!ProjectGround(Vehicle, Rules, Body->Bounds.Origin, CenterGround)) { return false; }
	if (!ProjectGround(Vehicle, Rules, Rear, Out, OutNormal)) { return false; }
	// A rear point may follow a slope, but should not snap through a ledge to another level.
	const double ExpectedRise = (Rear - Body->Bounds.Origin).Z;
	return FMath::Abs(Out.Z - CenterGround.Z - ExpectedRise) <= InShape.GroundHeightTolerance;
}

double AHALSlipperyTrail::ServerTime() const
{
	const auto* State = GetWorld()->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

bool AHALSlipperyTrail::Start(const FHALSlipperyTrailShape& InShape, const FHALSlipperyTrailEmission& InEmission,
	const FHALSlipperyTrailHandling& Handling, const FVector& FirstPoint, const FVector& FirstNormal,
	const FHALSlipperyTrailPresentation& InPresentation)
{
	const auto* Vehicle = Cast<ATestVehiclePawn>(GetOwner());
	if (!HasAuthority() || !Vehicle || !IsActiveVehicle(*Vehicle) || StartTime >= 0
		|| !(InPresentation.DissolveSeconds >= 0 && InPresentation.DissolveSeconds <= 5)) { return false; }
	Shape = InShape;
	Presentation = InPresentation;
	Emission = InEmission;
	Area.Kind = EHALMovementAreaKind::Slippery;
	Area.LateralGripMultiplier = Handling.LateralGripMultiplier;
	Area.RecoverySeconds = Handling.RecoverySeconds;
	Area.Sway = Handling.Sway;
	StartTime = ServerTime();
	AddPoint(FirstPoint, FirstNormal, 0);
	UpdateReceivers(0);
	GetWorldTimerManager().SetTimer(GameplayEndHandle, this, &ThisClass::FinishGameplay, Shape.LifetimeSeconds, false);
	SetLifeSpan(Shape.LifetimeSeconds + Presentation.DissolveSeconds);
	FTimerManagerTimerParameters Timer;
	Timer.bLoop = true;
	Timer.bMaxOncePerFrame = true;
	GetWorldTimerManager().SetTimer(UpdateHandle, this, &ThisClass::UpdateTrail, Emission.UpdateInterval, Timer);
	return true;
}

void AHALSlipperyTrail::AddPoint(const FVector& Ground, const FVector& Normal, float Age)
{
	const double Distance = Points.IsEmpty() ? 0 : FVector::Dist2D(Points.Last().Ground, Ground);
	const bool bLinked = bLinkNext && Distance <= Emission.MaximumLinkDistance
		&& FMath::Abs(Ground.Z - Points.Last().Ground.Z) <= Shape.GroundHeightTolerance;
	if (bLinked && Distance < Emission.SampleSpacing) { return; }
	auto& Point = Points.AddDefaulted_GetRef();
	Point.Ground = Ground;
	Point.GroundNormal = Normal;
	Point.EmissionAge = Age;
	Point.bLinked = bLinked;
	bLinkNext = true;
	OnRep_PresentationInput();
	ForceNetUpdate();
}

void AHALSlipperyTrail::UpdateTrail()
{
	const auto* Vehicle = Cast<ATestVehiclePawn>(GetOwner());
	const float Age = float(ServerTime() - StartTime);
	if (!Vehicle || !IsActiveVehicle(*Vehicle)
		|| !HALGameFlow::Permissions(GetWorld(), Vehicle).bAbilities)
	{
		Destroy();
		return;
	}
	if (Age >= Shape.LifetimeSeconds) { FinishGameplay(); return; }
	if (bGameplayEnded) { return; }
	if (Age < Emission.EmissionSeconds && Points.Num() < Emission.MaximumPoints)
	{
		FVector Ground;
		FVector Normal;
		if (FindTailGround(*Vehicle, Emission, Shape, Ground, &Normal)) { AddPoint(Ground, Normal, Age); }
		else { bLinkNext = false; }
	}
	UpdateReceivers(Age);
}

void AHALSlipperyTrail::FinishGameplay()
{
	if (!HasAuthority() || bGameplayEnded) { return; }
	bGameplayEnded = true;
	GetWorldTimerManager().ClearTimer(GameplayEndHandle);
	ClearReceivers();
	OnGameplayEnded.Broadcast(this);
}

void AHALSlipperyTrail::ClearReceivers()
{
	for (const auto& Occupant : Occupants)
	{
		if (auto* Receiver = Occupant.Get()) { Receiver->UnregisterArea(this); }
	}
	Occupants.Reset();
	LastGround.Reset();
	bLinkNext = false;
}

bool AHALSlipperyTrail::Intersects(TConstArrayView<FHALSlipperyTrailPoint> Path, const FHALSlipperyTrailShape& InShape,
	float TrailAge, const FVector& From, const FVector& To)
{
	if (TrailAge < 0 || TrailAge >= InShape.LifetimeSeconds) { return false; }
	for (int32 Index = 0; Index < Path.Num(); ++Index)
	{
		const auto& Point = Path[Index];
		if (TrailAge < Point.EmissionAge) { continue; }
		const double Width = InShape.HalfWidthAt(TrailAge - Point.EmissionAge);
		if (SweepsDisk(From, To, Point.Ground, Width, InShape.GroundHeightTolerance)) { return true; }
		if (Index > 0 && Point.bLinked)
		{
			const auto& Previous = Path[Index - 1];
			if (SweepsSection(From, To, Previous.Ground, Point.Ground, InShape.HalfWidthAt(TrailAge - Previous.EmissionAge),
				Width, InShape.GroundHeightTolerance)) { return true; }
		}
	}
	return false;
}

void AHALSlipperyTrail::UpdateReceivers(float Age)
{
	TSet<TWeakObjectPtr<UHALMovementAreaReceiver>> Inside;
	TMap<TWeakObjectPtr<ATestVehiclePawn>, FVector> NewGround;
	for (TActorIterator<ATestVehiclePawn> It(GetWorld()); It; ++It)
	{
		auto* Vehicle = *It;
		if (Vehicle == GetOwner() || !IsActiveVehicle(*Vehicle) || !HasGroundContact(*Vehicle)
			|| !HALGameFlow::Permissions(GetWorld(), Vehicle).bWorldInteractions) { continue; }
		auto* Receiver = Vehicle->FindComponentByClass<UHALMovementAreaReceiver>();
		const auto* Body = Cast<UPrimitiveComponent>(Vehicle->GetRootComponent());
		FVector Ground;
		if (!Receiver || !Body || !ProjectGround(*Vehicle, Emission, Body->Bounds.Origin, Ground)) { continue; }
		const FVector* Last = LastGround.Find(Vehicle);
		const bool bSweep = Last && FVector::Dist2D(*Last, Ground) <= Emission.MaximumLinkDistance
			&& FMath::Abs(Last->Z - Ground.Z) <= Shape.GroundHeightTolerance;
		NewGround.Add(Vehicle, Ground);
		if (Intersects(Points, Shape, Age, bSweep ? *Last : Ground, Ground)
			&& (Occupants.Contains(Receiver) || Receiver->RegisterArea(this, Area)))
		{
			Inside.Add(Receiver);
		}
	}
	for (const auto& Previous : Occupants)
	{
		if (auto* Receiver = Previous.Get(); Receiver && !Inside.Contains(Previous)) { Receiver->UnregisterArea(this); }
	}
	Occupants = MoveTemp(Inside);
	LastGround = MoveTemp(NewGround);
}

void AHALSlipperyTrail::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (StartTime < 0 || Points.IsEmpty()) { return; }
	const float Age = float(ServerTime() - StartTime);
	if (Age >= Shape.LifetimeSeconds + Presentation.DissolveSeconds)
	{
		HideGroundMesh();
		SetActorTickEnabled(false);
		return;
	}
	if (Age < 0) { return; }
	UpdateGroundMesh(Age);
	if (Age >= Shape.LifetimeSeconds) { return; }
#if ENABLE_DRAW_DEBUG
	if (!Shape.bDrawOutline) { return; }
	const FVector Lift = FVector::UpVector * Shape.OutlineLift;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const auto& Point = Points[Index];
		if (Age < Point.EmissionAge) { break; }
		const float Width = Shape.HalfWidthAt(Age - Point.EmissionAge);
		const FVector Center = Point.Ground + Lift;
		const bool bHasNext = Points.IsValidIndex(Index + 1) && Points[Index + 1].bLinked && Age >= Points[Index + 1].EmissionAge;
		if (!Point.bLinked || !bHasNext)
		{
			DrawDebugCircle(GetWorld(), Center, Width, 20, Shape.OutlineColor, false, 0, 0, Shape.OutlineThickness,
				FVector::ForwardVector, FVector::RightVector, false);
		}
		if (Point.bLinked && Index > 0)
		{
			const auto& Previous = Points[Index - 1];
			const FVector PreviousCenter = Previous.Ground + Lift;
			const float PreviousWidth = Shape.HalfWidthAt(Age - Previous.EmissionAge);
			const FVector Side = FVector::CrossProduct(FVector::UpVector, (Center - PreviousCenter).GetSafeNormal2D());
			for (const float Sign : {-1.f, 1.f})
			{
				DrawDebugLine(GetWorld(), PreviousCenter + Side * (PreviousWidth * Sign), Center + Side * (Width * Sign),
					Shape.OutlineColor, false, 0, 0, Shape.OutlineThickness);
			}
		}
	}
#endif
}

void AHALSlipperyTrail::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(UpdateHandle);
	GetWorldTimerManager().ClearTimer(GameplayEndHandle);
	ClearReceivers();
	HideGroundMesh();
	Super::EndPlay(Reason);
}

void AHALSlipperyTrail::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALSlipperyTrail, Shape);
	DOREPLIFETIME(AHALSlipperyTrail, StartTime);
	DOREPLIFETIME(AHALSlipperyTrail, Points);
	DOREPLIFETIME(AHALSlipperyTrail, Presentation);
}
