#include "HALExpandingSlipperyArea.h"

#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "HALGroundSurfacePresentation.h"
#include "HALMovementAreaReceiver.h"
#include "HALSlipperyTrailAbility.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Net/UnrealNetwork.h"
#include "TestVehiclePawn.h"
#include "TimerManager.h"

FHALExpandingSlipperyAreaConfig::FHALExpandingSlipperyAreaConfig()
{
	Handling = GetDefault<UHALSlipperyTrailConfig>()->Handling;
}

bool FHALExpandingSlipperyAreaConfig::IsValid() const
{
	const auto Range = [](float V, float Min, float Max) { return FMath::IsFinite(V) && V >= Min && V <= Max; };
	return Range(ExpansionSeconds, 0, 60) && Range(DurationSeconds, .01f, 120)
		&& Range(MaximumRadius, 1, 10000) && Range(HalfHeight, 1, 1000)
		&& Range(Handling.LateralGripMultiplier, 0, 1) && Range(Handling.RecoverySeconds, 0, 10)
		&& Handling.Sway.IsValid();
}

float FHALExpandingSlipperyAreaConfig::RadiusAt(double Age) const
{
	// A nonzero seed makes the center effective on the initial registration pass.
	return ExpansionSeconds <= 0 ? MaximumRadius
		: FMath::Lerp(1.f, MaximumRadius, float(FMath::Clamp(Age / ExpansionSeconds, 0., 1.)));
}

AHALExpandingSlipperyArea::AHALExpandingSlipperyArea()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("AreaCenter")));
	// Ticks only on rendering peers: ground surface presentation and optional debug drawing.
	PrimaryActorTick.bCanEverTick = FApp::CanEverRender();
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
}

bool AHALExpandingSlipperyArea::ConfigureArea(const FHALExpandingSlipperyAreaConfig& Config)
{
	if (!HasAuthority() || HasActorBegunPlay() || !Config.IsValid()) { return false; }
	Settings = Config;
	return true;
}

double AHALExpandingSlipperyArea::Age() const
{
	const auto* State = GetWorld()->GetGameState();
	return (State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) - StartedAt;
}

void AHALExpandingSlipperyArea::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(PrimaryActorTick.bCanEverTick && !IsNetMode(NM_DedicatedServer));
	if (!HasAuthority()) { return; }
	if (!Settings.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: invalid expanding slippery area settings."), *GetNameSafe(this));
		Destroy();
		return;
	}
	const auto* State = GetWorld()->GetGameState();
	StartedAt = State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	UpdateArea();
	// Gameplay ends at GameplaySeconds; the remaining life is only the visual dissolve on every peer.
	SetLifeSpan(Settings.GameplaySeconds() + (Presentation.HasUsableMaterial() ? Presentation.DissolveSeconds : 0.f));
	GetWorldTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdateArea, .02f, true);
	ForceNetUpdate();
}

void AHALExpandingSlipperyArea::UpdateArea()
{
	if (!HasAuthority() || StartedAt < 0 || bGameplayFinished) { return; }
	const double Elapsed = Age();
	if (Settings.IsExpired(Elapsed))
	{
		if (Presentation.HasUsableMaterial() && Presentation.DissolveSeconds > 0) { FinishGameplay(); }
		else { Destroy(); }
		return;
	}
	FHALMovementAreaConfig Effect;
	Effect.Kind = EHALMovementAreaKind::Slippery;
	Effect.LateralGripMultiplier = Settings.Handling.LateralGripMultiplier;
	Effect.RecoverySeconds = Settings.Handling.RecoverySeconds;
	Effect.Sway = Settings.Handling.Sway;
	const double RadiusSquared = FMath::Square(double(Settings.RadiusAt(Elapsed)));
	TSet<TWeakObjectPtr<UHALMovementAreaReceiver>> Inside;
	for (TActorIterator<ATestVehiclePawn> It(GetWorld()); It; ++It)
	{
		const FVector Offset = It->GetActorLocation() - GetActorLocation();
		if (FMath::Abs(Offset.Z) > Settings.HalfHeight || Offset.SizeSquared2D() > RadiusSquared) { continue; }
		auto* Receiver = It->FindComponentByClass<UHALMovementAreaReceiver>();
		if (Receiver && (Occupants.Contains(Receiver) || Receiver->RegisterArea(this, Effect))) { Inside.Add(Receiver); }
	}
	for (const auto& Entry : Occupants)
	{
		if (auto* Receiver = Entry.Get(); Receiver && !Inside.Contains(Entry)) { Receiver->UnregisterArea(this); }
	}
	Occupants = MoveTemp(Inside);
}

void AHALExpandingSlipperyArea::FinishGameplay()
{
	// Gameplay is over; the actor stays alive only so every peer can play the dissolve.
	bGameplayFinished = true;
	GetWorldTimerManager().ClearTimer(UpdateTimer);
	for (const auto& Entry : Occupants) { if (auto* Receiver = Entry.Get()) { Receiver->UnregisterArea(this); } }
	Occupants.Reset();
}

void AHALExpandingSlipperyArea::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (StartedAt < 0) { return; }
	const double Elapsed = Age();
	if (Elapsed < 0) { return; }
	UpdateGroundSurface(Elapsed);
#if ENABLE_DRAW_DEBUG
	if (!bDebugDraw || Settings.IsExpired(Elapsed)) { return; }
	DrawGroundDebug(Elapsed);
#endif
}

bool AHALExpandingSlipperyArea::FindGround(FVector& Center, FVector& Normal) const
{
	// Presentation only: use a supporting surface below the center, without moving the authority volume.
	Center = GetActorLocation();
	Normal = FVector::UpVector;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SlipperyAreaDebugGround), false, this);
	if (GetInstigator()) { Query.AddIgnoredActor(GetInstigator()); }
	TArray<FHitResult> Hits;
	GetWorld()->LineTraceMultiByObjectType(Hits, Center + FVector::UpVector * Settings.HalfHeight,
		Center - FVector::UpVector * Settings.HalfHeight, Objects, Query);
	for (const auto& Hit : Hits)
	{
		const auto* Surface = Hit.GetComponent();
		if (!Surface || Surface->IsSimulatingPhysics() || !Surface->IsPhysicsCollisionEnabled()
			|| Hit.bStartPenetrating || Hit.ImpactNormal.Z < .1f) { continue; }
		Center = Hit.ImpactPoint;
		Normal = Hit.ImpactNormal;
		return true;
	}
	return false;
}

void AHALExpandingSlipperyArea::UpdateGroundSurface(double Elapsed)
{
	const double GameplayEnd = Settings.GameplaySeconds();
	if (!Presentation.HasUsableMaterial() || Elapsed >= GameplayEnd + Presentation.DissolveSeconds) { HideGroundSurface(); return; }
	const float Radius = Settings.RadiusAt(FMath::Min(Elapsed, GameplayEnd));
	// Rebuild only while the radius visibly changes; the hold phase just advances material time.
	const bool bRebuild = !GroundSurface || !GroundSurface->IsVisible()
		|| FMath::Abs(Radius - LastSurfaceRadius) >= FMath::Max(1.f, Settings.MaximumRadius * .01f)
		|| (Radius >= Settings.MaximumRadius && LastSurfaceRadius < Settings.MaximumRadius);
	if (bRebuild)
	{
		FHALGroundSurfacePoint Center;
		FindGround(Center.Ground, Center.Normal);
		Center.HalfWidth = Radius;
		auto Mesh = HALGroundSurface::BuildDisc(Center, Center.Ground, Presentation.GroundLift, Presentation.TextureTileSize,
			Presentation.Deformation, Presentation.PaintEdge);
		if (Mesh.TriangleCount() == 0) { HideGroundSurface(); return; }
		const bool bNewComponent = !GroundSurface;
		if (bNewComponent)
		{
			GroundSurface = NewObject<UHALGroundSurfaceComponent>(this, TEXT("SlipperyGroundSurface"));
			GroundSurface->SetUsingAbsoluteLocation(true);
			GroundSurface->SetUsingAbsoluteRotation(true);
			GroundSurface->SetUsingAbsoluteScale(true);
			GroundSurface->SetupAttachment(GetRootComponent());
		}
		GroundSurface->SetWorldLocationAndRotation(Center.Ground, FQuat::Identity);
		GroundSurface->SetMesh(MoveTemp(Mesh));
		if (bNewComponent) { GroundSurface->RegisterComponent(); }
		GroundSurface->SetVisibility(true);
		LastSurfaceRadius = Radius;
	}
	GroundSurface->ConfigureSurface(Presentation.GroundMaterial, Presentation.TextureTileSize, Presentation.Deformation, Presentation.PaintEdge);
	GroundSurface->SetSurfaceAge(float(Elapsed));
	if (Presentation.DissolveSeconds > 0)
	{
		GroundSurface->SetDissolveProgress(Presentation.DissolveProgress(float(Elapsed), float(GameplayEnd)));
	}
	// Ripple travel contract for ground materials: during expansion the rings advance exactly with the gameplay
	// radius; afterwards the material adds HoldSeconds x its own slow speed, so the hand-off never jumps.
	// The MID exists whenever deformation, paint edge or dissolve is enabled; otherwise ripples simply stay static.
	if (auto* Ripple = Cast<UMaterialInstanceDynamic>(GroundSurface->GetMaterial(0)))
	{
		const double Expansion = Settings.ExpansionSeconds;
		Ripple->SetScalarParameterValue(TEXT("HAL_GroundRippleTravel"), Settings.RadiusAt(FMath::Min(Elapsed, Expansion)));
		Ripple->SetScalarParameterValue(TEXT("HAL_GroundRippleHoldSeconds"), float(FMath::Max(0., Elapsed - Expansion)));
	}
}

void AHALExpandingSlipperyArea::HideGroundSurface()
{
	if (GroundSurface) { GroundSurface->SetVisibility(false); }
	LastSurfaceRadius = -1.f;
}

void AHALExpandingSlipperyArea::DrawGroundDebug(double Elapsed) const
{
#if ENABLE_DRAW_DEBUG
	FVector Center, Normal;
	FindGround(Center, Normal);
	const auto GroundPoint = [&](double X, double Y)
	{
		// Keep world-XY radius identical to the gameplay cylinder, including on slopes.
		return Center + FVector(X, Y, -(Normal.X * X + Normal.Y * Y) / Normal.Z + 3.);
	};
	const double Radius = Settings.RadiusAt(Elapsed);
	const FColor Color = Elapsed < Settings.ExpansionSeconds ? FColor::Red : FColor::Blue;
	constexpr int32 Segments = 64;
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	Vertices.Reserve(Segments + 1);
	Indices.Reserve(Segments * 3);
	Vertices.Add(GroundPoint(0, 0));
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const double Angle = 2. * UE_DOUBLE_PI * Index / Segments;
		Vertices.Add(GroundPoint(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius));
		Indices.Append({0, (Index + 1) % Segments + 1, Index + 1});
	}
	DrawDebugMesh(GetWorld(), Vertices, Indices, FColor(Color.R, Color.G, Color.B, 45), false, 0);
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		DrawDebugLine(GetWorld(), Vertices[Index + 1], Vertices[(Index + 1) % Segments + 1], Color, false, 0, 0, 3);
	}
	// At most 33 lines per direction, even for the largest authored area.
	const double Spacing = FMath::Max(100., Radius / 16.);
	const int32 Count = FMath::FloorToInt(Radius / Spacing);
	for (int32 Index = -Count; Index <= Count; ++Index)
	{
		const double Offset = Index * Spacing;
		const double Reach = FMath::Sqrt(FMath::Max(0., Radius * Radius - Offset * Offset));
		DrawDebugLine(GetWorld(), GroundPoint(Offset, -Reach), GroundPoint(Offset, Reach), Color, false, 0, 0, 1.5f);
		DrawDebugLine(GetWorld(), GroundPoint(-Reach, Offset), GroundPoint(Reach, Offset), Color, false, 0, 0, 1.5f);
	}
#endif
}

void AHALExpandingSlipperyArea::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(UpdateTimer);
	for (const auto& Entry : Occupants) { if (auto* Receiver = Entry.Get()) { Receiver->UnregisterArea(this); } }
	Occupants.Reset();
	HideGroundSurface();
	Super::EndPlay(Reason);
}

void AHALExpandingSlipperyArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALExpandingSlipperyArea, Settings);
	DOREPLIFETIME(AHALExpandingSlipperyArea, StartedAt);
	DOREPLIFETIME(AHALExpandingSlipperyArea, bDebugDraw);
	DOREPLIFETIME_CONDITION(AHALExpandingSlipperyArea, Presentation, COND_InitialOnly);
}
