#include "HALSpecialBallPad.h"

#include "HALBlackHoleBall.h"
#include "HALGameFlowPermissions.h"
#include "HALMatchBallLifecycle.h"
#include "HALMatchGameState.h"
#include "HALMeteorBall.h"
#include "Algo/RandomShuffle.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHALSpecialBallSupply, Log, All);

namespace
{
	UClass* SupplyBallClass(EHALPadBall Kind)
	{
		switch (Kind)
		{
		case EHALPadBall::Explosive: return AHALExplosiveBall::StaticClass();
		case EHALPadBall::BlackHole: return AHALBlackHoleBall::StaticClass();
		case EHALPadBall::Meteor: return AHALMeteorBall::StaticClass();
		default: return nullptr;
		}
	}

	constexpr float RetrySeconds = .25f;
}

AHALSpecialBallPad::AHALSpecialBallPad()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	auto* Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("FloorAnchor"));
	Anchor->SetMobility(EComponentMobility::Static);
	SetRootComponent(Anchor);
	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadSurface"));
	Surface->SetupAttachment(Anchor);
	Surface->SetMobility(EComponentMobility::Movable);
	Surface->SetCollisionProfileName(TEXT("NoCollision"));
	Surface->SetGenerateOverlapEvents(false);
	Surface->SetCanEverAffectNavigation(false);
	Surface->SetCastShadow(false);
	SetActorEnableCollision(false);
}

void AHALSpecialBallPad::ApplyMesh()
{
	const auto* Config = GetDefault<UHALMatchBallSettings>()->SpecialBallSupply.LoadSynchronous();
	if (!Config || !Config->PadMesh) { return; }
	Surface->SetStaticMesh(Config->PadMesh);
	const FBox Box = Config->PadMesh->GetBoundingBox();
	const FVector Size = Box.GetSize();
	if (Size.GetMin() < UE_SMALL_NUMBER) { return; }
	const double Lateral = Config->PadDiameter / FMath::Max(Size.Y, Size.Z);
	const FVector Scale(Config->PadThickness / Size.X, Lateral, Lateral);
	const FRotator FaceUp(90., 0., 0.);
	// The factory mesh has its six lenses on +X and an off-centre origin behind the casting.
	const FVector LocalFace(Box.Max.X, Box.GetCenter().Y, Box.GetCenter().Z);
	constexpr float SurfaceGap = .2f;
	const FVector Offset = FVector::UpVector * (DeckFinishOffset + SurfaceGap + Config->PadThickness) - FaceUp.RotateVector(Scale * LocalFace);
	const FTransform Relative(FaceUp, Offset, Scale);
	if (!Surface->GetRelativeTransform().Equals(Relative)) { Surface->SetRelativeTransform(Relative); }
}

void AHALSpecialBallPad::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMesh();
}

void AHALSpecialBallPad::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer) { ApplyMesh(); }
	OnRep_Warning();
}

void AHALSpecialBallPad::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALSpecialBallPad, Warning);
}

void AHALSpecialBallPad::SetWarning(EHALPadBall Ball, float Duration)
{
	check(HasAuthority());
	Warning.Ball = Ball;
	Warning.StartedAt = GetWorld()->GetTimeSeconds();
	Warning.Duration = Duration;
	OnRep_Warning();
	ForceNetUpdate();
}

void AHALSpecialBallPad::OnRep_Warning()
{
	if (GetNetMode() == NM_DedicatedServer) { return; }
	const auto* State = GetWorld()->GetGameState();
	const double LocalNow = GetWorld()->GetTimeSeconds();
	const double Age = State ? State->GetServerWorldTimeSeconds() - Warning.StartedAt : 0.;
	// The shader uses these values and game time; even a client joining mid-warning sees the current beat.
	Surface->SetCustomPrimitiveDataFloat(0, static_cast<float>(Warning.Ball));
	Surface->SetCustomPrimitiveDataFloat(1, LocalNow - FMath::Max(0., Age));
	Surface->SetCustomPrimitiveDataFloat(2, Warning.Duration);
}

bool AHALSpecialBallPad::FindSpawnCenter(float Radius, float Clearance, FVector& Center) const
{
	if (!(Radius > 0.f)) { return false; }
	const FVector Anchor = GetActorLocation();
	const FVector Probe = FVector::UpVector * 40.;
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(HALBallSupplyPad), false, this);
	FHitResult Ground;
	if (!GetWorld()->LineTraceSingleByObjectType(Ground, Anchor + Probe, Anchor - Probe,
		FCollisionObjectQueryParams(ECC_WorldStatic), Query)
		|| Ground.ImpactNormal.Z < .9 || FMath::Abs(Ground.ImpactPoint.Z - Anchor.Z) > 8.) { return false; }
	Center = Ground.ImpactPoint + FVector::UpVector * (Radius + Clearance);
	if (GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_PhysicsBody,
		FCollisionShape::MakeSphere(Radius + 2.f), Query)) { return false; }
	// Reserve the actual ball volumes, including any using a non-blocking collision response.
	for (TActorIterator<ABasicBallActor> It(GetWorld()); It; ++It)
	{
		if (FVector::DistSquared(It->GetActorLocation(), Center) <= FMath::Square(Radius + It->GetPhysicsRoot()->GetScaledSphereRadius() + 2.f)) { return false; }
	}
	return true;
}

void UHALSpecialBallSupply::SetPlaying(bool bPlaying)
{
	if (!GetOwner()->HasAuthority()) { return; }
	GetWorld()->GetTimerManager().ClearTimer(Timer);
	if (AnnouncedPad.IsValid()) { AnnouncedPad->SetWarning(EHALPadBall::None); }
	AnnouncedPad.Reset();
	Pads.Reset();
	Bag.Reset();
	Settings = nullptr;
	LastDelivered = EHALPadBall::None;
	NextPad = 0;
	if (!bPlaying) { return; }
	TArray<AHALSpecialBallPad*> Sorted;
	for (TActorIterator<AHALSpecialBallPad> It(GetWorld()); It; ++It) { Sorted.Add(*It); }
	if (Sorted.IsEmpty()) { return; }
	Sorted.Sort([](const AHALSpecialBallPad& A, const AHALSpecialBallPad& B)
	{
		return A.RotationOrder == B.RotationOrder ? A.GetFName().LexicalLess(B.GetFName()) : A.RotationOrder < B.RotationOrder;
	});
	for (auto* Pad : Sorted) { Pads.Add(Pad); }
	Settings = GetDefault<UHALMatchBallSettings>()->SpecialBallSupply.LoadSynchronous();
	if (Settings) { ScheduleCycle(Settings->FirstDropSeconds); }
	else { UE_LOG(LogHALSpecialBallSupply, Warning, TEXT("Placed special-ball pads have no configured supply DA.")); }
}

void UHALSpecialBallSupply::EndPlay(EEndPlayReason::Type Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(Timer);
	Super::EndPlay(Reason);
}

bool UHALSpecialBallSupply::CanSupply() const
{
	const auto* Match = Cast<AHALMatchGameState>(GetOwner());
	return Settings && Match && Match->HasAuthority() && Match->GetMatchPhase() == EHALMatchPhase::Playing
		&& !GetWorld()->bIsTearingDown && !GetWorld()->IsInSeamlessTravel() && HALGameFlow::Permissions(GetWorld()).bWorldInteractions;
}

void UHALSpecialBallSupply::ScheduleCycle(float Delay)
{
	Delay = FMath::Max(.1f, Delay);
	WarningDuration = FMath::Clamp(Settings->WarningSeconds, .1f, Delay);
	GetWorld()->GetTimerManager().SetTimer(Timer, this, &ThisClass::BeginCycle, FMath::Max(.001f, Delay - WarningDuration));
}

void UHALSpecialBallSupply::BeginCycle()
{
	if (!CanSupply()) { if (Settings) { ScheduleCycle(Settings->DropInterval); } return; }
	if (Bag.IsEmpty())
	{
		for (EHALPadBall Kind : Settings->BallPool)
		{
			UClass* Class = SupplyBallClass(Kind);
			const auto* Type = Class ? Class->GetDefaultObject<AHALSpecialBall>()->GetSpawnDefinition() : nullptr;
			FString Error;
			if (Type && Type->bReadyForUse && Type->Validate(Error)) { Bag.AddUnique(Kind); }
			else { UE_LOG(LogHALSpecialBallSupply, Warning, TEXT("Skipping unavailable supply type %s. %s"), *UEnum::GetValueAsString(Kind), *Error); }
		}
		if (Bag.IsEmpty()) { UE_LOG(LogHALSpecialBallSupply, Warning, TEXT("No ready ball types in the supply pool.")); return; }
		Algo::RandomShuffle(Bag);
		if (Bag.Num() > 1 && Bag.Last() == LastDelivered) { Bag.Swap(0, Bag.Num() - 1); }
	}
	const auto* Type = SupplyBallClass(Bag.Last())->GetDefaultObject<AHALSpecialBall>()->GetSpawnDefinition();
	SpawnRadius = Type->GetScaledPhysicsRadius();
	RetryUntil = GetWorld()->GetTimeSeconds() + Settings->BlockedGrace;
	Announce();
}

void UHALSpecialBallSupply::Announce()
{
	if (!CanSupply()) { FinishCycle(); return; }
	for (int32 Step = 0; Step < Pads.Num(); ++Step)
	{
		const int32 Index = (NextPad + Step) % Pads.Num();
		auto* Pad = Pads[Index].Get();
		FVector Center;
		if (!Pad || !Pad->FindSpawnCenter(SpawnRadius, Settings->SpawnClearance, Center)) { continue; }
		AnnouncedPad = Pad;
		NextPad = (Index + 1) % Pads.Num();
		Pad->SetWarning(Bag.Last(), WarningDuration);
		RetryUntil = GetWorld()->GetTimeSeconds() + WarningDuration + Settings->BlockedGrace;
		GetWorld()->GetTimerManager().SetTimer(Timer, this, &ThisClass::Deliver, WarningDuration);
		return;
	}
	if (GetWorld()->GetTimeSeconds() < RetryUntil)
	{
		GetWorld()->GetTimerManager().SetTimer(Timer, this, &ThisClass::Announce, RetrySeconds);
		return;
	}
	NextPad = (NextPad + 1) % Pads.Num();
	UE_LOG(LogHALSpecialBallSupply, Verbose, TEXT("Skipping blocked turn; the bag keeps its next ball."));
	// No warning could start. Include this cycle's unplayed warning in the interval between due dates.
	ScheduleCycle(Settings->DropInterval + WarningDuration);
}

void UHALSpecialBallSupply::Deliver()
{
	auto* Pad = AnnouncedPad.Get();
	if (!CanSupply() || !Pad) { FinishCycle(); return; }
	FVector Center;
	if (Pad->FindSpawnCenter(SpawnRadius, Settings->SpawnClearance, Center))
	{
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
		auto* Ball = GetWorld()->SpawnActor<AHALSpecialBall>(SupplyBallClass(Bag.Last()), Center, FRotator::ZeroRotator, Spawn);
		if (IsValid(Ball) && Ball->IsConfigurationValid())
		{
			LastDelivered = Bag.Pop();
			UE_LOG(LogHALSpecialBallSupply, Log, TEXT("Drop %s at pad %d, center %s."), *Ball->GetName(), Pad->RotationOrder, *Center.ToCompactString());
			FinishCycle();
			return;
		}
	}
	if (GetWorld()->GetTimeSeconds() < RetryUntil)
	{
		GetWorld()->GetTimerManager().SetTimer(Timer, this, &ThisClass::Deliver, RetrySeconds);
		return;
	}
	UE_LOG(LogHALSpecialBallSupply, Verbose, TEXT("Announced pad stayed occupied; skipping this turn."));
	FinishCycle();
}

void UHALSpecialBallSupply::FinishCycle()
{
	if (AnnouncedPad.IsValid()) { AnnouncedPad->SetWarning(EHALPadBall::None); }
	AnnouncedPad.Reset();
	if (Settings) { ScheduleCycle(Settings->DropInterval); }
}
