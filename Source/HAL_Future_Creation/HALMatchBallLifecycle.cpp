#include "HALMatchBallLifecycle.h"
#include "HALSpecialBallCue.h"

#include "BasicBallActor.h"
#include "HALGameFlowPermissions.h"
#include "HALMatchGameState.h"
#include "HALPracticeRoomGameMode.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SphereComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Generators/DiscMeshGenerator.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHALMatchBall, Log, All);

namespace
{
	constexpr float CheckInterval = .15f;
	constexpr double SpawnClearance = 8.;
}

bool FHALMatchBallArena::Contains(const FVector& Position) const
{
	return !Position.ContainsNaN() && Position.Z >= FloorCenter.Z - FallDepth
		&& FVector::DistSquared2D(Position, FloorCenter) <= FMath::Square(double(PlayableRadius));
}

bool FHALMatchBallArena::IsValid() const
{
	return !MatchMap.IsNull() && !PrimaryBallClass.IsNull() && !FloorCenter.ContainsNaN()
		&& FMath::IsFinite(PlayableRadius) && PlayableRadius >= 100.f
		&& FMath::IsFinite(FallDepth) && FallDepth >= 10.f
		&& FMath::IsWithinInclusive(OutsideGraceSeconds, 0.f, 5.f)
		&& FMath::IsWithinInclusive(ReturnWarningSeconds, .25f, 5.f);
}

UHALMatchBallLifecycle::UHALMatchBallLifecycle()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHALMatchBallLifecycle::MulticastBallCue_Implementation(UHALSpecialBallDefinition* Type, EHALBallCue Kind, FVector_NetQuantize10 Origin,
	float Radius, FVector_NetQuantizeNormal Normal, double ServerStarted)
{
	if (Type && GetWorld() && GetNetMode() != NM_DedicatedServer)
	{
		AHALSpecialBallCue::CreateCue(*GetWorld(), Kind, Type, Origin, Radius, Normal, ServerStarted, false);
	}
}

void UHALMatchBallLifecycle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHALMatchBallLifecycle, ReturnCue);
}

void UHALMatchBallLifecycle::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority()) { OnRep_ReturnCue(); return; }
	const auto* Mode = GetWorld()->GetAuthGameMode<AHALPracticeRoomGameMode>();
	if (Mode && !Mode->IsRoundWorld()) { return; }
	const FString Map = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	const auto* Settings = GetDefault<UHALMatchBallSettings>()->Arenas.FindByPredicate(
		[&Map](const FHALMatchBallArena& Entry) { return Entry.MatchMap.ToSoftObjectPath().GetLongPackageName() == Map; });
	if (!Settings)
	{
		if (Mode) { UE_LOG(LogHALMatchBall, Warning, TEXT("Competition map %s has no Match Ball arena settings."), *Map); }
		return;
	}
	PrepareArena(*Settings);
	SetPlaying(CastChecked<AHALMatchGameState>(GetOwner())->GetMatchPhase() == EHALMatchPhase::Playing);
}

void UHALMatchBallLifecycle::PrepareArena(const FHALMatchBallArena& Settings)
{
	if (!Settings.IsValid() || !(BallClass = Settings.PrimaryBallClass.LoadSynchronous()))
	{
		UE_LOG(LogHALMatchBall, Error, TEXT("Invalid Match Ball arena or primary class for %s."), *GetNameSafe(GetWorld()));
		return;
	}
	Arena = Settings;
	Home = Arena.FloorCenter;
	BallRadius = BallClass->GetDefaultObject<ABasicBallActor>()->GetConfiguredSphereRadius();
	TArray<ABasicBallActor*> Marked;
	for (TActorIterator<ABasicBallActor> It(GetWorld()); It; ++It)
	{
		if (!It->IsPrimaryMatchBall()) { continue; }
		if (It->GetClass() != BallClass.Get())
		{
			UE_LOG(LogHALMatchBall, Warning, TEXT("Clearing extra primary marker on non-primary class %s."), *It->GetPathName());
			It->SetPrimaryMatchBall(false);
			continue;
		}
		Marked.Add(*It);
	}
	if (!Marked.IsEmpty())
	{
		// A duplicated level marker must resolve identically on every server.
		Marked.Sort([](const ABasicBallActor& A, const ABasicBallActor& B) { return A.GetPathName() < B.GetPathName(); });
		PrimaryBall = Marked[0];
		if (Arena.Contains(Marked[0]->GetActorLocation())) { Home = Marked[0]->GetActorLocation(); }
		for (int32 Index = 1; Index < Marked.Num(); ++Index)
		{
			UE_LOG(LogHALMatchBall, Warning, TEXT("Removing duplicate placed primary %s."), *Marked[Index]->GetPathName());
			Marked[Index]->Destroy();
		}
	}
	UE_LOG(LogHALMatchBall, Log, TEXT("Arena prepared. Primary=%s Home=%s Radius=%.1f"), *GetNameSafe(PrimaryBall.Get()), *Home.ToCompactString(), Arena.PlayableRadius);
}

void UHALMatchBallLifecycle::SetPlaying(bool bPlaying)
{
	if (!GetOwner()->HasAuthority() || !BallClass) { return; }
	auto& Timers = GetWorld()->GetTimerManager();
	if (!bPlaying)
	{
		Timers.ClearTimer(CheckTimer);
		OutsideSince = -1.;
		NextSearchAt = 0.;
		SetReturnCue(false);
	}
	else if (!Timers.IsTimerActive(CheckTimer))
	{
		Timers.SetTimer(CheckTimer, this, &ThisClass::CheckPrimaryBall, CheckInterval, true);
		CheckPrimaryBall();
	}
}

bool UHALMatchBallLifecycle::IsSafeGround(const FVector& XY, FVector& OutGround) const
{
	const FVector OnPlane(XY.X, XY.Y, Arena.FloorCenter.Z);
	if (FVector::Dist2D(OnPlane, Arena.FloorCenter) + BallRadius > Arena.PlayableRadius) { return false; }
	FHitResult Floor;
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(HALPrimaryBallReturn), false);
	const FVector Probe(0, 0, FMath::Max(120., 2. * BallRadius));
	if (!GetWorld()->LineTraceSingleByObjectType(Floor, OnPlane + Probe, OnPlane - Probe,
		FCollisionObjectQueryParams(ECC_WorldStatic), Query) || Floor.ImpactNormal.Z < .8
		|| FMath::Abs(Floor.ImpactPoint.Z - OnPlane.Z) > FMath::Max(30.f, BallRadius * .5f)) { return false; }
	OutGround = Floor.ImpactPoint;
	const FVector Center = OutGround + FVector(0, 0, BallRadius + SpawnClearance);
	return !GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_PhysicsBody,
		FCollisionShape::MakeSphere(BallRadius + 2.f), Query);
}

bool UHALMatchBallLifecycle::FindReturnGround(const FVector& Around, FVector& OutGround) const
{
	if (IsSafeGround(Around, OutGround)) { return true; }
	const double Step = FMath::Max(350., 5. * BallRadius);
	for (int32 Ring = 1; Ring <= 2; ++Ring)
	{
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const double Angle = Index * UE_TWO_PI / 8.;
			const FVector Offset(FMath::Cos(Angle), FMath::Sin(Angle), 0.);
			if (IsSafeGround(Around + Offset * (Ring * Step), OutGround)) { return true; }
		}
	}
	// A permanent obstacle can cover the entire kickoff area. Continue over the playable deck.
	for (double Fraction : { .45, .78 })
	{
		for (int32 Index = 0; Index < 12; ++Index)
		{
			const double Angle = Index * UE_TWO_PI / 12.;
			const FVector Offset(FMath::Cos(Angle), FMath::Sin(Angle), 0.);
			if (IsSafeGround(Arena.FloorCenter + Offset * (Arena.PlayableRadius * Fraction), OutGround)) { return true; }
		}
	}
	return false;
}

void UHALMatchBallLifecycle::CheckPrimaryBall()
{
	UWorld* World = GetWorld();
	if (!GetOwner()->HasAuthority() || !World || World->bIsTearingDown || World->IsInSeamlessTravel()
		|| CastChecked<AHALMatchGameState>(GetOwner())->GetMatchPhase() != EHALMatchPhase::Playing
		|| !HALGameFlow::Permissions(World).bWorldInteractions)
	{
		SetPlaying(false);
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (ABasicBallActor* Ball = PrimaryBall.Get(); IsValid(Ball) && !Ball->IsActorBeingDestroyed())
	{
		if (Arena.Contains(Ball->GetActorLocation())) { OutsideSince = -1.; return; }
		if (OutsideSince < 0.) { OutsideSince = Now; }
		if (Now - OutsideSince < Arena.OutsideGraceSeconds) { return; }
		UE_LOG(LogHALMatchBall, Log, TEXT("Primary out of play: %s at %s."), *Ball->GetName(), *Ball->GetActorLocation().ToCompactString());
		if (!Ball->Destroy()) { return; }
	}
	PrimaryBall.Reset();
	OutsideSince = -1.;
	FVector Ground;
	if (ReturnCue.bVisible)
	{
		// The floor or a vehicle can move after the announcement. Never spawn at a different point silently.
		if (IsSafeGround(ReturnCue.Ground, Ground) && Ground.Equals(ReturnCue.Ground, 1.))
		{
			if (Now < ReturnAt) { return; }
			const FVector Center = Ground + FVector(0, 0, BallRadius + SpawnClearance);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
			if (auto* NewBall = World->SpawnActor<ABasicBallActor>(BallClass, Center, FRotator::ZeroRotator, Params))
			{
				if (NewBall->IsConfigurationValid())
				{
					NewBall->SetPrimaryMatchBall(true);
					PrimaryBall = NewBall;
					UE_LOG(LogHALMatchBall, Log, TEXT("Returned primary %s at %s."), *NewBall->GetName(), *Center.ToCompactString());
				}
				else { NewBall->Destroy(); }
			}
			SetReturnCue(false);
			return;
		}
		SetReturnCue(false);
	}
	if (Now < NextSearchAt) { return; }
	if (FindReturnGround(Home, Ground))
	{
		ReturnAt = Now + Arena.ReturnWarningSeconds;
		SetReturnCue(true, Ground);
	}
	else { NextSearchAt = Now + .5; }
}

void UHALMatchBallLifecycle::SetReturnCue(bool bVisible, const FVector& Ground)
{
	if (ReturnCue.bVisible == bVisible && (!bVisible || Ground.Equals(ReturnCue.Ground, .1))) { return; }
	ReturnCue.bVisible = bVisible;
	ReturnCue.Ground = Ground;
	GetOwner()->ForceNetUpdate();
	OnRep_ReturnCue();
}

void UHALMatchBallLifecycle::OnRep_ReturnCue()
{
	if (!HasBegunPlay() || GetNetMode() == NM_DedicatedServer) { return; }
	if (ReturnCue.bVisible && !WarningRing)
	{
		// GameState inherits AInfo's hidden actor flag. Give the cue a local, visible owner.
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		auto* Visual = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), ReturnCue.Ground, FRotator::ZeroRotator, Params);
		if (!Visual) { return; }
		Visual->SetActorEnableCollision(false);
		WarningRing = NewObject<UDynamicMeshComponent>(Visual);
		Visual->AddInstanceComponent(WarningRing);
		Visual->SetRootComponent(WarningRing);
		WarningRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WarningRing->SetGenerateOverlapEvents(false);
		WarningRing->SetCanEverAffectNavigation(false);
		WarningRing->SetCastShadow(false);
		auto* Material = GetDefault<UHALMatchBallSettings>()->ReturnRingMaterial.LoadSynchronous();
		auto* Tint = UMaterialInstanceDynamic::Create(Material ? Material : UMaterial::GetDefaultMaterial(MD_Surface), WarningRing);
		Tint->SetVectorParameterValue(TEXT("Color"), FLinearColor(.12f, .85f, 1.f));
		WarningRing->SetMaterial(0, Tint);
		UE::Geometry::FPuncturedDiscMeshGenerator Disc;
		Disc.Radius = 145.f;
		Disc.HoleRadius = 128.f;
		Disc.AngleSamples = 64;
		Disc.RadialSamples = 2;
		Disc.Generate();
		WarningRing->SetMesh(UE::Geometry::FDynamicMesh3(&Disc));
		WarningRing->RegisterComponent();
	}
	if (WarningRing)
	{
		if (ReturnCue.bVisible) { WarningRing->SetWorldLocation(ReturnCue.Ground + FVector(0, 0, 3)); }
		WarningRing->SetVisibility(ReturnCue.bVisible);
	}
}

void UHALMatchBallLifecycle::EndPlay(EEndPlayReason::Type Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(CheckTimer);
	if (IsValid(WarningRing)) { WarningRing->GetOwner()->Destroy(); }
	WarningRing = nullptr;
	Super::EndPlay(Reason);
}
