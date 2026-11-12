#include "HALBlackHole.h"

#include "ArcadeVehicleMovementComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "HALGameFlowPermissions.h"
#include "Net/UnrealNetwork.h"
#include "TestVehiclePawn.h"
#include "TimerManager.h"

bool FHALBlackHoleConfig::IsValid() const
{
	const auto Range = [](float V, float Min, float Max) { return FMath::IsFinite(V) && V >= Min && V <= Max; };
	return Range(ContinuousAcceleration, 0, 10000) && Range(PulseDeltaSpeed, 0, 10000)
		&& Range(ContinuousSpeedLimit, 1, 2000) && Range(CenterSofteningRadius, 1, 1000)
		&& Range(EscapeRecoverySpeed, 1, 2000) && Range(LowSpeedAttractionScale, 0, 1)
		&& Range(EntrySpeedMultiplier, 0, 1) && Range(EntryRetriggerDelay, 0, 10)
		&& Range(PulseInterval, .1f, 120) && Range(ExpansionSeconds, 0, 60)
		&& Range(DurationSeconds, .01f, 120) && Range(MaximumRadius, 1, 10000) && Range(HalfHeight, 1, 1000);
}

float FHALBlackHoleConfig::RadiusAt(double Age) const
{
	return ExpansionSeconds <= 0 ? MaximumRadius
		: FMath::Lerp(1.f, MaximumRadius, float(FMath::Clamp(Age / ExpansionSeconds, 0., 1.)));
}

EHALBlackHoleStage FHALBlackHoleConfig::StageAt(double Age) const
{
	return Age >= double(ExpansionSeconds) + DurationSeconds ? EHALBlackHoleStage::Ended
		: Age < ExpansionSeconds ? EHALBlackHoleStage::Expanding : EHALBlackHoleStage::PreparingPulse;
}

int32 FHALBlackHoleConfig::PulseIndexAt(double Age) const
{
	if (Age < ExpansionSeconds || StageAt(Age) == EHALBlackHoleStage::Ended || !IsValid()) { return 0; }
	return FMath::FloorToInt((Age - ExpansionSeconds) / PulseInterval);
}

FVector FHALBlackHoleConfig::InwardDirection(const FVector& Center, const FVector& Position, double Age) const
{
	if (Age < 0 || StageAt(Age) == EHALBlackHoleStage::Ended || Center.ContainsNaN() || Position.ContainsNaN()) { return FVector::ZeroVector; }
	const FVector Offset = Center - Position;
	return FMath::Abs(Offset.Z) <= HalfHeight && Offset.SizeSquared2D() <= FMath::Square(double(RadiusAt(Age)))
		? Offset.GetSafeNormal2D() : FVector::ZeroVector;
}

FVector FHALBlackHoleConfig::ContinuousDeltaVelocity(const FVector& Center, const FVector& Position,
	const FVector& Velocity, double Age, float StepSeconds, const FVector& Forward, float Throttle) const
{
	if (!IsValid() || Velocity.ContainsNaN() || !FMath::IsFinite(StepSeconds) || StepSeconds <= 0) { return FVector::ZeroVector; }
	const FVector Direction = InwardDirection(Center, Position, Age);
	if (Direction.IsNearlyZero()) { return FVector::ZeroVector; }
	const double Softening = FMath::Clamp(FVector::Dist2D(Center, Position) / CenterSofteningRadius, 0., 1.);
	const double InwardSpeed = FVector::DotProduct(Velocity, Direction);
	double EscapeScale = 1.;
	if (!Forward.ContainsNaN() && FMath::IsFinite(Throttle))
	{
		const double Alignment = FMath::Clamp(FVector::DotProduct(Forward.GetSafeNormal2D(), -Direction), 0., 1.);
		const double Progress = FMath::Clamp(-InwardSpeed / EscapeRecoverySpeed, 0., 1.);
		const double Recovery = Progress * Progress * (3. - 2. * Progress);
		const double Assistance = Alignment * FMath::Clamp(double(Throttle), 0., 1.) * (1. - Recovery);
		EscapeScale = FMath::Lerp(1., double(LowSpeedAttractionScale), Assistance);
	}
	const double Headroom = FMath::Max(0., ContinuousSpeedLimit * Softening - InwardSpeed);
	// Bound only the weak contribution; never brake a yellow pulse, lateral motion or Z velocity.
	return Direction * FMath::Min(double(ContinuousAcceleration) * FMath::Min(StepSeconds, .1f) * Softening * EscapeScale, Headroom);
}

AHALBlackHole::AHALBlackHole()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("BlackHoleCenter")));
	PrimaryActorTick.bCanEverTick = false;
#if WITH_EDITORONLY_DATA
	for (const TCHAR* Name : {TEXT("DebugCenterRadius"), TEXT("DebugUpperBound"), TEXT("DebugLowerBound")})
	{
		auto* Bounds = CreateEditorOnlyDefaultSubobject<USplineComponent>(Name);
		if (!Bounds) { continue; }
		Bounds->SetupAttachment(RootComponent);
		Bounds->SetAbsolute(false, true, true);
		Bounds->SetHiddenInGame(true);
		Bounds->SetDrawDebug(true);
		Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DebugBounds.Add(Bounds);
	}
#endif
}

void AHALBlackHole::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITORONLY_DATA
	for (int32 Layer = 0; Layer < DebugBounds.Num(); ++Layer)
	{
		auto* Bounds = DebugBounds[Layer].Get();
		if (!Bounds) { continue; }
		Bounds->ClearSplinePoints(false);
		Bounds->SetVisibility(bDebugDraw && Settings.IsValid());
		Bounds->SetRelativeLocation(FVector::ZeroVector);
		Bounds->SetWorldRotation(FRotator::ZeroRotator);
		Bounds->SetWorldScale3D(FVector::OneVector);
		const float Height = Layer == 0 ? 0.f : Layer == 1 ? Settings.HalfHeight : -Settings.HalfHeight;
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const double Angle = Index * 2. * UE_DOUBLE_PI / 64.;
			Bounds->AddSplinePoint(FVector(FMath::Cos(Angle) * Settings.MaximumRadius, FMath::Sin(Angle) * Settings.MaximumRadius, Height), ESplineCoordinateSpace::Local, false);
			Bounds->SetSplinePointType(Index, ESplinePointType::Linear, false);
		}
		Bounds->SetClosedLoop(true, false);
		Bounds->UpdateSpline();
	}
#endif
}

bool AHALBlackHole::ConfigureBlackHole(const FHALBlackHoleConfig& Config)
{
	if (!HasAuthority() || HasActorBegunPlay() || !Config.IsValid()) { return false; }
	Settings = Config;
	return true;
}

double AHALBlackHole::Age() const
{
	if (!GetWorld() || StartedAt < 0) { return -1; }
	const auto* State = GetWorld()->GetGameState();
	const double Now = !HasAuthority() && State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return Now - StartedAt;
}

float AHALBlackHole::GetCurrentRadius() const { const double A = Age(); return A < 0 || GetStage() == EHALBlackHoleStage::Ended ? 0.f : Settings.RadiusAt(A); }
EHALBlackHoleStage AHALBlackHole::GetStage() const { const double A = Age(); return A < 0 ? EHALBlackHoleStage::Ended : Settings.StageAt(A); }
float AHALBlackHole::GetPreparationProgress() const
{
	const double A = Age();
	return GetStage() != EHALBlackHoleStage::PreparingPulse || !Settings.IsValid() ? 0.f
		: float(FMath::Fmod(FMath::Max(0., A - Settings.ExpansionSeconds), double(Settings.PulseInterval)) / Settings.PulseInterval);
}

void AHALBlackHole::BeginPlay()
{
	Super::BeginPlay();
	if (!Settings.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Black hole %s: invalid settings."), *GetName());
		if (HasAuthority()) { Destroy(); }
		return;
	}
	if (HasAuthority()) { StartedAt = GetWorld()->GetTimeSeconds(); ForceNetUpdate(); }
	UpdateField();
	FTimerManagerTimerParameters Timer;
	Timer.bLoop = true;
	Timer.bMaxOncePerFrame = true;
	GetWorldTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdateField, .02f, Timer);
}

void AHALBlackHole::ApplyAttraction(double Elapsed, float StepSeconds, bool bPulse)
{
	if (!HasAuthority() || Elapsed < 0 || Settings.StageAt(Elapsed) == EHALBlackHoleStage::Ended) { return; }
	TSet<TWeakObjectPtr<ATestVehiclePawn>> Inside;
	for (TActorIterator<ATestVehiclePawn> It(GetWorld()); It; ++It)
	{
		if (!It->IsConfigurationValid() || !HALGameFlow::Permissions(GetWorld(), *It).bWorldInteractions) { continue; }
		const FVector Offset = It->GetActorLocation() - GetActorLocation();
		if (FMath::Abs(Offset.Z) > Settings.HalfHeight || Offset.SizeSquared2D() > FMath::Square(double(Settings.RadiusAt(Elapsed)))) { continue; }
		const FVector Direction = Settings.InwardDirection(GetActorLocation(), It->GetActorLocation(), Elapsed);
		if (auto* Movement = It->FindComponentByClass<UArcadeVehicleMovementComponent>())
		{
			const auto* Body = Cast<UPrimitiveComponent>(It->GetRootComponent());
			if (!Body || !Body->IsSimulatingPhysics()) { continue; }
			const TWeakObjectPtr<ATestVehiclePawn> Vehicle(*It);
			Inside.Add(Vehicle);
			const double* LastEntry = LastEntryAt.Find(Vehicle);
			const bool bEntering = !Occupants.Contains(Vehicle) && (!LastEntry || Elapsed - *LastEntry >= Settings.EntryRetriggerDelay);
			const FVector Velocity = Body->GetPhysicsLinearVelocity();
			const FVector EntryDelta = bEntering ? FVector(Velocity.X,Velocity.Y,0) * (Settings.EntrySpeedMultiplier - 1.f) : FVector::ZeroVector;
			const FVehicleInputCmd Input = Movement->GetAcceptedDrivingInput();
			const float OutwardThrottle = Input.Brake <= KINDA_SMALL_NUMBER ? Input.Throttle : 0.f;
			const FVector Continuous = Settings.ContinuousDeltaVelocity(GetActorLocation(), It->GetActorLocation(),
				Velocity + EntryDelta, Elapsed, StepSeconds, Body->GetForwardVector(), OutwardThrottle);
			// Resolve entry first, then add the complete yellow impulse; entry never attenuates that pulse.
			const FVector DeltaVelocity = EntryDelta + Continuous + (bPulse ? Direction * Settings.PulseDeltaSpeed : FVector::ZeroVector);
			if (DeltaVelocity.IsNearlyZero()) { if (bEntering) { LastEntryAt.Add(Vehicle,Elapsed); } continue; }
			FVehicleImpactAction Action;
			if (++ActionSequence == 0) { ++ActionSequence; }
			Action.EventSequence = ActionSequence;
			Action.DeltaVelocity = DeltaVelocity;
			// Existing authority physics action stores the resolved delta for replay; no GAS/HP/drop rules.
			if (Movement->QueueImpact(Action)) { if (bEntering) { LastEntryAt.Add(Vehicle,Elapsed); } }
			else { Inside.Remove(Vehicle); }
		}
	}
	Occupants = MoveTemp(Inside);
	for (auto It = LastEntryAt.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || (!Occupants.Contains(It.Key()) && Elapsed - It.Value() >= Settings.EntryRetriggerDelay)) { It.RemoveCurrent(); }
	}
}

void AHALBlackHole::UpdateField()
{
	if (!HasActorBegunPlay() || StartedAt < 0 || !Settings.IsValid()) { return; }
	const double Elapsed = Age();
	if (Elapsed < 0) { return; }
	const EHALBlackHoleStage Stage = Settings.StageAt(Elapsed);
	if (!bPresented || PresentedStage != Stage)
	{
		bPresented = true;
		PresentedStage = Stage;
		if (!IsNetMode(NM_DedicatedServer)) { OnStageChanged(Stage); }
	}
	if (Stage == EHALBlackHoleStage::Ended)
	{
		if (HasAuthority()) { Destroy(); }
		return;
	}
	if (HasAuthority())
	{
		const int32 NextPulse = Settings.PulseIndexAt(Elapsed);
		const bool bPulse = NextPulse > PulseIndex;
		if (bPulse) { PulseIndex = NextPulse; ForceNetUpdate(); }
		// Never stack missed pulses or accumulate an unbounded force after a stalled server frame.
		ApplyAttraction(Elapsed, float(FMath::Clamp(Elapsed - LastUpdateAge, 0., .1)), bPulse);
		LastUpdateAge = Elapsed;
		if (bPulse && !IsNetMode(NM_DedicatedServer)) { OnAttractionPulse(PulseIndex); }
	}
	if (!bPresentationReady) { PresentedPulse = PulseIndex; bPresentationReady = true; }
	if (!IsNetMode(NM_DedicatedServer))
	{
		OnFieldUpdated(Settings.RadiusAt(Elapsed), GetPreparationProgress());
#if ENABLE_DRAW_DEBUG
		DrawFieldDebug(Elapsed);
#endif
	}
}

void AHALBlackHole::DrawFieldDebug(double Elapsed) const
{
#if ENABLE_DRAW_DEBUG
	if (!bDebugDraw || !Settings.IsValid() || Elapsed < 0 || Settings.StageAt(Elapsed) == EHALBlackHoleStage::Ended) { return; }
	FVector Center = GetActorLocation();
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BlackHoleDebugGround), false, this);
	if (GetInstigator()) { Query.AddIgnoredActor(GetInstigator()); }
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Center + FVector(0,0,Settings.HalfHeight),
		Center - FVector(0,0,Settings.HalfHeight), Objects, Query) && Hit.ImpactNormal.Z > .1f)
	{
		Center.Z = Hit.ImpactPoint.Z;
	}
	Center.Z += 3;
	const double Radius = Settings.RadiusAt(Elapsed);
	const bool bPulse = Settings.PulseIndexAt(Elapsed) > 0
		&& FMath::Fmod(Elapsed - Settings.ExpansionSeconds, double(Settings.PulseInterval)) < .15;
	const FColor Color = bPulse ? FColor::Yellow : Settings.StageAt(Elapsed) == EHALBlackHoleStage::Expanding ? FColor::Red : FColor::Purple;
	constexpr int32 Segments = 64;
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	Vertices.Add(Center);
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const double Angle = Index * 2. * UE_DOUBLE_PI / Segments;
		Vertices.Add(Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Radius);
		Indices.Append({0, (Index + 1) % Segments + 1, Index + 1});
	}
	DrawDebugMesh(GetWorld(), Vertices, Indices, FColor(Color.R,Color.G,Color.B,35), false, .04f);
	DrawDebugCircle(GetWorld(), Center, Radius, Segments, Color, false, .04f, 0, 3,
		FVector::ForwardVector, FVector::RightVector, false);
	for (int32 Ring = 1; Ring <= 3; ++Ring)
	{
		DrawDebugCircle(GetWorld(), Center, Radius * Ring / 4., Segments, Color, false, .04f, 0, 1,
			FVector::ForwardVector, FVector::RightVector, false);
	}
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const double Angle = Index * 2. * UE_DOUBLE_PI / 8.;
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0);
		DrawDebugDirectionalArrow(GetWorld(), Center + Direction * Radius * .85, Center + Direction * Radius * .25,
			20, Color, false, .04f, 0, 2);
	}
#endif
}

void AHALBlackHole::OnRep_Lifecycle() { UpdateField(); }
void AHALBlackHole::OnRep_Pulse()
{
	if (bPresentationReady && HasActorBegunPlay() && GetStage() != EHALBlackHoleStage::Ended && PulseIndex > PresentedPulse)
	{
		if (!IsNetMode(NM_DedicatedServer)) { OnAttractionPulse(PulseIndex); }
	}
	PresentedPulse = PulseIndex;
}

void AHALBlackHole::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(UpdateTimer);
	Occupants.Reset(); LastEntryAt.Reset();
	if (bPresented && PresentedStage != EHALBlackHoleStage::Ended && !IsNetMode(NM_DedicatedServer)) { OnStageChanged(EHALBlackHoleStage::Ended); }
	Super::EndPlay(Reason);
}

void AHALBlackHole::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALBlackHole, Settings);
	DOREPLIFETIME(AHALBlackHole, StartedAt);
	DOREPLIFETIME(AHALBlackHole, PulseIndex);
	DOREPLIFETIME(AHALBlackHole, bDebugDraw);
}
