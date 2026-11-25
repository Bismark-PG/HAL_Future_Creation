#include "HALBallComicFXComponent.h"
#include "HALBallComicFXSettings.h"
#include "HALMagnetBeamPresentationComponent.h"
#include "HALIncomingBallWidget.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

static TAutoConsoleVariable<int32> CVarHALBallComicFXLog(TEXT("hal.BallComicFXLog"), 0,
	TEXT("Log ball ink trail and aim arrow starts and stops (0 off, 1 on). Presentation only."));

FHALBallComicFXTransition HALBallComicFX::ResolveTransition(const FHALBallComicFXSeen& Previous, const FBallRepState& Next)
{
	FHALBallComicFXTransition Out;
	const bool bTrailWas = Previous.bValid && Previous.State == EBasicBallState::Launched;
	const bool bTrailWanted = Next.State == EBasicBallState::Launched;
	// A new sequence is a new shot even when the state reads Launched on both sides.
	Out.bStartTrail = bTrailWanted && (!bTrailWas || Previous.Sequence != Next.StateSequence || Previous.LauncherPlayerId != Next.LastLauncherPlayerId);
	Out.bStopTrail = bTrailWas && (!bTrailWanted || Out.bStartTrail);
	const bool bAimWas = Previous.bValid && Previous.State == EBasicBallState::Controlled;
	const bool bAimWanted = Next.State == EBasicBallState::Controlled && Next.Holder != nullptr;
	Out.bStartAim = bAimWanted && (!bAimWas || Previous.Holder.Get() != Next.Holder.Get());
	Out.bStopAim = bAimWas && (!bAimWanted || Out.bStartAim);
	return Out;
}

namespace
{
	/** 0 -> overshoot -> 1 over the pop time; exactly 1 afterwards. Shared by the aim arrow and the hold rings. */
	float HALComicPop(float Seconds, float PopSeconds, float Overshoot)
	{
		if (!FMath::IsFinite(Seconds) || PopSeconds <= 0 || Seconds >= PopSeconds) { return 1.f; }
		const float Alpha = FMath::Clamp(Seconds / PopSeconds, 0.f, 1.f);
		// Share of the pop time spent rising to the overshoot; the rest settles back to full size.
		constexpr float RiseShare = .6f;
		return Alpha < RiseShare ? FMath::Lerp(0.f, Overshoot, Alpha / RiseShare)
			: FMath::Lerp(Overshoot, 1.f, (Alpha - RiseShare) / (1.f - RiseShare));
	}
}

float HALBallComicFX::AimPopScale(const FHALBallComicFXConfig& Config, float AimSeconds)
{
	return HALComicPop(AimSeconds, Config.AimPopSeconds, Config.AimPopOvershoot);
}

float HALBallComicFX::HoldRingScale(const FHALBallComicFXConfig& Config, EHALBallHoldRingPhase Phase, float PhaseSeconds)
{
	if (Phase == EHALBallHoldRingPhase::Shown) { return HALComicPop(PhaseSeconds, Config.HoldPopSeconds, Config.HoldPopOvershoot); }
	if (Phase != EHALBallHoldRingPhase::Leaving || !FMath::IsFinite(PhaseSeconds) || Config.HoldShrinkSeconds <= 0) { return 0.f; }
	return FMath::Clamp(1.f - PhaseSeconds / Config.HoldShrinkSeconds, 0.f, 1.f);
}

bool HALBallComicFX::HoldMarkerQuad(const FHALBallComicFXConfig& Config, const FVector& RoofCentre, const FVector& HolderForward,
	float BodyLengthCm, float BodyWidthCm, const FVector& ViewForward, const FVector& ViewUp, float Scale, FTransform& OutQuad)
{
	if (RoofCentre.ContainsNaN() || HolderForward.ContainsNaN() || ViewForward.ContainsNaN() || ViewUp.ContainsNaN() || !FMath::IsFinite(BodyLengthCm)
		|| BodyLengthCm < 0 || !FMath::IsFinite(BodyWidthCm) || BodyWidthCm <= 0 || !FMath::IsFinite(Scale) || Scale <= 0) { return false; }
	const FVector Normal = -ViewForward.GetSafeNormal();
	if (Normal.IsNearlyZero()) { return false; }
	// Over the rear half, measured along the ground: a car standing on its nose keeps the marker straight overhead.
	const FVector Back = -HolderForward.GetSafeNormal2D();
	const FVector Anchor = RoofCentre + Back * (double(BodyLengthCm) * Config.HoldMarkerBackLengthFraction)
		+ FVector(0, 0, double(BodyWidthCm) * Config.HoldMarkerLiftWidthFraction);
	// The arrow (+V of the quad) points from there at the middle of the car, as that line lies on the screen.
	FVector Down = FVector::VectorPlaneProject(RoofCentre - Anchor, Normal);
	// Seen exactly along that line there is no direction on screen: point down the screen.
	if (!Down.Normalize(1.)) { Down = FVector::VectorPlaneProject(-ViewUp, Normal); }
	if (!Down.Normalize()) { return false; }
	// The quad mesh is 100 cm square with U along its X and V along its Y.
	const double Size = double(BodyWidthCm) * Config.HoldRoofSizeWidthFraction * .01 * Scale;
	OutQuad = FTransform(FMatrix(Down ^ Normal, Down, Normal, FVector::ZeroVector).ToQuat(), Anchor, FVector(Size, Size, 1.));
	return true;
}

bool HALBallComicFX::TrailWedgeQuad(const FHALBallComicFXConfig& Config, const FVector& Centre, float DrawnRadiusCm, const FVector& Velocity,
	FTransform& OutQuad, float& OutAspect)
{
	if (Centre.ContainsNaN() || Velocity.ContainsNaN() || !FMath::IsFinite(DrawnRadiusCm) || DrawnRadiusCm <= 0) { return false; }
	// The quad lies level: only the travel along the ground gives it a direction.
	const FVector Ground(Velocity.X, Velocity.Y, 0.);
	const double Speed = Ground.Size();
	if (Speed <= KINDA_SMALL_NUMBER || Speed < Config.TrailWedgeMinSpeed) { return false; }
	const double Diameter = 2. * DrawnRadiusCm;
	const double Width = Diameter * Config.TrailWidthDiameterFraction;
	const double Length = FMath::Clamp(Speed * Config.TrailWedgeLengthSeconds, Diameter * Config.TrailWedgeMinLengthDiameters,
		FMath::Max(Diameter * Config.TrailWedgeMaxLengthDiameters, Diameter * Config.TrailWedgeMinLengthDiameters));
	if (Width <= KINDA_SMALL_NUMBER || Length <= KINDA_SMALL_NUMBER) { return false; }
	const FVector Back = -Ground / Speed;
	// The quad mesh is 100 cm square with U along its X and V along its Y; the material draws base at V = 0, tip at V = 1.
	OutQuad = FTransform(FMatrix(Back ^ FVector::UpVector, Back, FVector::UpVector, FVector::ZeroVector).ToQuat(),
		Centre + Back * (DrawnRadiusCm * Config.TrailWedgeStartRadiusFraction + .5 * Length), FVector(Width * .01, Length * .01, 1.));
	OutAspect = float(Length / Width);
	return true;
}

float HALBallComicFX::HoldBallHalfSize(const FHALBallComicFXConfig& Config, float DrawnRadiusCm, float Scale)
{
	if (!FMath::IsFinite(DrawnRadiusCm) || !FMath::IsFinite(Scale)) { return 0.f; }
	return FMath::Max(DrawnRadiusCm, 0.f) * Config.HoldBallSizeDiameterFraction * FMath::Max(Scale, 0.f);
}

bool HALBallComicFX::HoldBallQuad(const FVector& Centre, float HalfSizeCm, const FVector& HolderRight, const FVector& ViewForward,
	const FVector& ViewUp, FTransform& OutQuad)
{
	if (Centre.ContainsNaN() || HolderRight.ContainsNaN() || ViewForward.ContainsNaN() || ViewUp.ContainsNaN() || !FMath::IsFinite(HalfSizeCm) || HalfSizeCm <= 0)
	{ return false; }
	// The quad faces the camera; only its roll follows the car.
	const FVector Normal = -ViewForward.GetSafeNormal();
	if (Normal.IsNearlyZero()) { return false; }
	// Long axis of the rings: the holder's right, as it lies on the screen.
	FVector Across = FVector::VectorPlaneProject(HolderRight, Normal);
	// A car seen exactly side-on has no right on screen: fall back to screen right rather than spin on noise.
	if (!Across.Normalize(.05)) { Across = FVector::VectorPlaneProject(ViewUp ^ ViewForward, Normal); }
	if (!Across.Normalize()) { return false; }
	FVector Down = Normal ^ Across;
	// The half of each ring that passes behind the ball is the one further up the screen, whichever way the car drives.
	if ((Down | ViewUp) > 0) { Across = -Across; Down = -Down; }
	// The quad mesh is 100 cm square with U along its X and V along its Y.
	const double Size = 2. * HalfSizeCm * .01;
	OutQuad = FTransform(FMatrix(Across, Down, Normal, FVector::ZeroVector).ToQuat(), Centre, FVector(Size, Size, 1.));
	return true;
}

float HALBallComicFX::DrawnBallRadius(float PhysicsRadiusCm, const FVector& VisualBoundsExtent)
{
	const float Physics = FMath::IsFinite(PhysicsRadiusCm) ? FMath::Max(PhysicsRadiusCm, 0.f) : 0.f;
	if (VisualBoundsExtent.ContainsNaN()) { return Physics; }
	// The box of a sphere has its radius on every axis; the horizontal ones are what a top-down camera sees.
	return FMath::Max(Physics, float(FMath::Max(VisualBoundsExtent.X, VisualBoundsExtent.Y)));
}

FVector HALBallComicFX::UnrotatedExtent(const UPrimitiveComponent& Visual)
{
	// World bounds are the axis aligned box of the rotated local box: once the ball has rolled, that box is up to
	// the square root of three times wider than the ball. Size comes from the local box and the scale alone.
	return Visual.CalcBounds(FTransform(FQuat::Identity, FVector::ZeroVector, Visual.GetComponentScale().GetAbs())).BoxExtent;
}

bool HALBallComicFX::AimAnchor(const FHALBallComicFXConfig& Config, const FTransform& HolderBody, const FVector& HolderExtent,
	const FVector& BallCentre, float BallRadiusCm, float PopScale, FTransform& OutQuad)
{
	FVector Forward = HolderBody.GetUnitAxis(EAxis::X);
	Forward.Z = 0;
	if (HolderBody.GetLocation().ContainsNaN() || HolderExtent.ContainsNaN() || BallCentre.ContainsNaN() || !FMath::IsFinite(BallRadiusCm)
		|| HolderExtent.Y <= 0 || !Forward.Normalize(.05f)) { return false; }
	const float Scale = FMath::IsFinite(PopScale) ? FMath::Max(PopScale, 0.f) : 1.f;
	const double BodyWidth = 2. * HolderExtent.Y;
	const double Length = BodyWidth * Config.AimLengthWidthFraction;
	// Past the nose and past the far edge of the ball as drawn, then a clear gap: the arrow never sits under the ball.
	const double BallFarEdge = FVector::DotProduct(BallCentre - HolderBody.GetLocation(), Forward) + FMath::Max(BallRadiusCm, 0.f);
	const double Distance = FMath::Max(FMath::Max(HolderExtent.X, 0.), BallFarEdge) + BodyWidth * Config.AimGapWidthFraction + Length * .5;
	FVector Location = HolderBody.GetLocation() + Forward * Distance;
	Location.Z = HolderBody.GetLocation().Z - FMath::Max(HolderExtent.Z, 0.) + Config.AimGroundLiftCm;
	// The quad mesh is 100 cm square, so a scale of one hundredth of the size keeps real centimetres.
	OutQuad = FTransform(Forward.Rotation(), Location, FVector(Length * .01 * Scale, BodyWidth * Config.AimWidthWidthFraction * .01 * Scale, 1.));
	return true;
}

UHALBallComicFXComponent::UHALBallComicFXComponent()
{
	// Ticks only while an arrow or a hold ring is on screen; the trail is a Niagara component and needs none.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

const FHALBallComicFXConfig* UHALBallComicFXComponent::GetComicConfig() const
{
	return bReady ? &GetDefault<UHALBallComicFXSettings>()->Comic : nullptr;
}

void UHALBallComicFXComponent::BeginPlay()
{
	Super::BeginPlay();
	Ball = Cast<ABasicBallActor>(GetOwner());
	const FHALBallComicFXConfig& Settings = GetDefault<UHALBallComicFXSettings>()->Comic;
	FString Error;
	if (!Ball.IsValid() || !Settings.bEnabled || GetNetMode() == NM_DedicatedServer || !FApp::CanEverRender()) { return; }
	if (!Settings.Validate(Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("HAL BallComicFX: disabled, settings invalid: %s"), *Error);
		return;
	}
	MagnetBeam = Ball->FindComponentByClass<UHALMagnetBeamPresentationComponent>();
	// Resolved once outside the state path; a state change never loads an asset synchronously.
	if (Settings.bTrail && !Settings.TrailSystem.IsNull()) { TrailSystem = Settings.TrailSystem.LoadSynchronous(); }
	if (Settings.bTrail && !Settings.TrailWedgeMaterial.IsNull())
	{
		TrailWedgeMaterial = Settings.TrailWedgeMaterial.LoadSynchronous();
		TrailWedgeMesh = Settings.TrailWedgeMesh.LoadSynchronous();
	}
	if (Settings.bAim && !Settings.AimMaterial.IsNull())
	{
		AimMaterial = Settings.AimMaterial.LoadSynchronous();
		AimMesh = Settings.AimMesh.LoadSynchronous();
	}
	if (Settings.bHoldRings)
	{
		if (!Settings.HoldBallMaterial.IsNull()) { HoldBallMaterial = Settings.HoldBallMaterial.LoadSynchronous(); }
		if (!Settings.HoldRoofMaterial.IsNull())
		{
			HoldRoofMaterial = Settings.HoldRoofMaterial.LoadSynchronous();
			HoldRoofMesh = Settings.HoldRoofMesh.LoadSynchronous();
		}
		// The rings say whose ball it is, so the beam arc stops saying it too. Only once a ring really loaded:
		// a missing asset must not leave the held ball with no ownership mark at all.
		if (Settings.bHoldReplacesBeam && MagnetBeam.IsValid() && IsValid(HoldRoofMesh) && (IsValid(HoldBallMaterial) || IsValid(HoldRoofMaterial)))
		{ MagnetBeam->SetBeamRibbonSuppressed(true); }
	}
	bReady = true;
	if (Settings.bIncomingWarning) { UHALIncomingBallHUDSubsystem::EnsureForWorld(GetWorld()); }
	SyncToBallState(Ball->GetBallRepState());
}

void UHALBallComicFXComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopTrail(true);
	StopAim();
	StopHold(nullptr, true);
	Super::EndPlay(Reason);
}

FLinearColor UHALBallComicFXComponent::ResolveColor(const AActor* Pawn, int32 PlayerId) const
{
	// One identity colour for holder, beam, trail and arrow: all of them ask the magnet beam.
	return MagnetBeam.IsValid() ? MagnetBeam->ResolveIdentityColor(Pawn, PlayerId) : FLinearColor::White;
}

void UHALBallComicFXComponent::SyncToBallState(const FBallRepState& State)
{
	const FHALBallComicFXConfig* ComicConfig = GetComicConfig();
	if (!ComicConfig || !Ball.IsValid() || !Ball->AllowsNormalLaunch()) { return; }
	const FHALBallComicFXTransition Transition = HALBallComicFX::ResolveTransition(Seen, State);
	Seen.bValid = true;
	Seen.State = State.State;
	Seen.Sequence = State.StateSequence;
	Seen.Holder = State.Holder.Get();
	Seen.LauncherPlayerId = State.LastLauncherPlayerId;
	if (Transition.bStopTrail) { StopTrail(ComicConfig->bCutTrailImmediately || Transition.bStartTrail); }
	if (Transition.bStartTrail) { StartTrail(*ComicConfig, State); }
	if (Transition.bStopAim)
	{
		StopAim();
		// A steal restarts on the new holder in this same call, so the old marks are cut instead of shrinking.
		StopHold(ComicConfig, Transition.bStartAim);
	}
	if (Transition.bStartAim)
	{
		StartAim(*ComicConfig, State);
		StartHold(*ComicConfig, State);
	}
	if (CVarHALBallComicFXLog.GetValueOnGameThread() && (Transition.bStartTrail || Transition.bStopTrail || Transition.bStartAim || Transition.bStopAim))
	{
		UE_LOG(LogTemp, Display, TEXT("HAL BallComicFX: %s Seq=%u State=%d Trail=%d Aim=%d Hold=%d"), *GetNameSafe(GetOwner()),
			State.StateSequence, int32(State.State), bTrailActive ? 1 : 0, bAimActive ? 1 : 0, int32(HoldPhase));
	}
}

void UHALBallComicFXComponent::StartTrail(const FHALBallComicFXConfig& ComicConfig, const FBallRepState& State)
{
	USphereComponent* Root = Ball.IsValid() ? Ball->GetPhysicsRoot() : nullptr;
	if (!Root) { return; }
	const FLinearColor Colour = ResolveColor(State.LastLauncherPawn, State.LastLauncherPlayerId);
	if (IsValid(TrailWedgeMaterial) && IsValid(TrailWedgeMesh))
	{
		if (!IsValid(TrailWedgeComponent))
		{
			TrailWedgeComponent = NewObject<UStaticMeshComponent>(GetOwner());
			if (TrailWedgeComponent)
			{
				TrailWedgeComponent->SetupAttachment(Root);
				// Placed in world space every frame from the ball's velocity, never from the spinning ball.
				TrailWedgeComponent->SetUsingAbsoluteLocation(true);
				TrailWedgeComponent->SetUsingAbsoluteRotation(true);
				TrailWedgeComponent->SetUsingAbsoluteScale(true);
				TrailWedgeComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				TrailWedgeComponent->SetGenerateOverlapEvents(false);
				TrailWedgeComponent->SetCastShadow(false);
				TrailWedgeComponent->bReceivesDecals = false;
				TrailWedgeComponent->SetCanEverAffectNavigation(false);
				TrailWedgeComponent->SetStaticMesh(TrailWedgeMesh);
				TrailWedgeComponent->SetVisibility(false);
				TrailWedgeComponent->RegisterComponent();
				TrailWedgeInstance = TrailWedgeComponent->CreateDynamicMaterialInstance(0, TrailWedgeMaterial);
			}
		}
		if (IsValid(TrailWedgeInstance))
		{
			TrailWedgeInstance->SetVectorParameterValue(ComicConfig.TrailWedgeColorParameter, Colour);
			bTrailWedgeActive = true;
			bTrailActive = true;
			RefreshTick();
			UpdateTrailWedge(ComicConfig);
		}
	}
	if (!IsValid(TrailSystem) || !GetWorld() || !GetWorld()->FXSystem.IsValid()) { return; }
	if (!IsValid(TrailComponent))
	{
		TrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(TrailSystem, Root, NAME_None, FVector::ZeroVector,
			FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, false, false, ENCPoolMethod::None, false);
		if (!TrailComponent) { return; }
		// The ball spins; the ribbon must only follow its position.
		TrailComponent->SetUsingAbsoluteRotation(true);
		TrailComponent->SetUsingAbsoluteScale(true);
	}
	TrailComponent->SetVariableLinearColor(ComicConfig.TrailColorParameter, Colour);
	FVector DrawnCentre;
	float DrawnRadius = Root->GetScaledSphereRadius();
	ResolveDrawnBall(DrawnCentre, DrawnRadius);
	TrailComponent->SetVariableFloat(ComicConfig.TrailWidthParameter, 2.f * DrawnRadius * ComicConfig.TrailWidthDiameterFraction);
	TrailComponent->Activate(true);
	bTrailActive = true;
}

void UHALBallComicFXComponent::StopTrail(bool bImmediate)
{
	if (IsValid(TrailComponent))
	{
		// A trail that outlives the Launched state would keep calling a harmless ball dangerous.
		if (bImmediate) { TrailComponent->DeactivateImmediate(); } else { TrailComponent->Deactivate(); }
	}
	// The triangle has no tail to play out: it goes with the state.
	if (IsValid(TrailWedgeComponent)) { TrailWedgeComponent->SetVisibility(false); }
	bTrailWedgeActive = false;
	bTrailActive = false;
	RefreshTick();
}

void UHALBallComicFXComponent::UpdateTrailWedge(const FHALBallComicFXConfig& ComicConfig)
{
	if (!IsValid(TrailWedgeComponent) || !IsValid(TrailWedgeInstance) || !Ball.IsValid()) { return; }
	FVector DrawnCentre;
	float DrawnRadius = 0;
	FTransform Quad;
	float Aspect = 1;
	const bool bPlaced = ResolveDrawnBall(DrawnCentre, DrawnRadius)
		&& HALBallComicFX::TrailWedgeQuad(ComicConfig, DrawnCentre, DrawnRadius, Ball->GetVelocity(), Quad, Aspect);
	if (bPlaced)
	{
		TrailWedgeComponent->SetWorldTransform(Quad);
		TrailWedgeInstance->SetScalarParameterValue(ComicConfig.TrailWedgeAspectParameter, Aspect);
	}
	// Too slow to have a direction: nothing, rather than a triangle pointing somewhere arbitrary.
	TrailWedgeComponent->SetVisibility(bPlaced);
}

void UHALBallComicFXComponent::StartAim(const FHALBallComicFXConfig& ComicConfig, const FBallRepState& State)
{
	USceneComponent* Root = Ball.IsValid() ? Ball->GetRootComponent() : nullptr;
	if (!IsValid(AimMaterial) || !IsValid(AimMesh) || !Root || !IsValid(State.Holder)) { return; }
	if (!IsValid(AimComponent))
	{
		AimComponent = NewObject<UStaticMeshComponent>(GetOwner());
		if (!AimComponent) { return; }
		AimComponent->SetupAttachment(Root);
		// Placed in world space every frame from the holder's physics body, never from the spinning ball.
		AimComponent->SetUsingAbsoluteLocation(true);
		AimComponent->SetUsingAbsoluteRotation(true);
		AimComponent->SetUsingAbsoluteScale(true);
		AimComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AimComponent->SetGenerateOverlapEvents(false);
		AimComponent->SetCastShadow(false);
		AimComponent->bReceivesDecals = false;
		AimComponent->SetCanEverAffectNavigation(false);
		AimComponent->SetStaticMesh(AimMesh);
		AimComponent->SetVisibility(false);
		AimComponent->RegisterComponent();
		AimInstance = AimComponent->CreateDynamicMaterialInstance(0, AimMaterial);
	}
	if (!IsValid(AimInstance)) { return; }
	AimInstance->SetVectorParameterValue(ComicConfig.AimColorParameter, ResolveColor(State.Holder, INDEX_NONE));
	AimHolder = State.Holder;
	AimSeconds = 0;
	bAimActive = true;
	RefreshTick();
	UpdateAim(ComicConfig);
}

void UHALBallComicFXComponent::StopAim()
{
	if (IsValid(AimComponent)) { AimComponent->SetVisibility(false); }
	AimHolder.Reset();
	bAimActive = false;
	RefreshTick();
}

void UHALBallComicFXComponent::RefreshTick()
{
	SetComponentTickEnabled(bAimActive || bTrailWedgeActive || HoldPhase != EHALBallHoldRingPhase::Off);
}

void UHALBallComicFXComponent::StartHold(const FHALBallComicFXConfig& ComicConfig, const FBallRepState& State)
{
	USceneComponent* Root = Ball.IsValid() ? Ball->GetRootComponent() : nullptr;
	const bool bRoof = IsValid(HoldRoofMaterial) && IsValid(HoldRoofMesh);
	const bool bBall = IsValid(HoldBallMaterial) && IsValid(HoldRoofMesh);
	if (!ComicConfig.bHoldRings || !Root || !IsValid(State.Holder) || (!bBall && !bRoof)) { return; }
	// Both marks are owned by the ball and placed in world space every frame: every vehicle class gets them
	// without wiring, and each ball owns its own pair once there is more than one ball in play.
	const auto Detach = [](UPrimitiveComponent* Mark)
	{
		Mark->SetUsingAbsoluteLocation(true);
		Mark->SetUsingAbsoluteRotation(true);
		Mark->SetUsingAbsoluteScale(true);
		Mark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mark->SetGenerateOverlapEvents(false);
		Mark->SetCastShadow(false);
		Mark->bReceivesDecals = false;
		Mark->SetCanEverAffectNavigation(false);
		Mark->SetVisibility(false);
	};
	if (bBall && !IsValid(HoldBallComponent))
	{
		// A quad rather than a billboard: it faces the camera too, but its roll has to follow the holder's heading.
		HoldBallComponent = NewObject<UStaticMeshComponent>(GetOwner());
		if (HoldBallComponent)
		{
			HoldBallComponent->SetupAttachment(Root);
			Detach(HoldBallComponent);
			HoldBallComponent->SetStaticMesh(HoldRoofMesh);
			HoldBallComponent->RegisterComponent();
			HoldBallInstance = HoldBallComponent->CreateDynamicMaterialInstance(0, HoldBallMaterial);
		}
	}
	if (bRoof && !IsValid(HoldRoofComponent))
	{
		HoldRoofComponent = NewObject<UStaticMeshComponent>(GetOwner());
		if (HoldRoofComponent)
		{
			HoldRoofComponent->SetupAttachment(Root);
			Detach(HoldRoofComponent);
			HoldRoofComponent->SetStaticMesh(HoldRoofMesh);
			HoldRoofComponent->RegisterComponent();
			HoldRoofInstance = HoldRoofComponent->CreateDynamicMaterialInstance(0, HoldRoofMaterial);
		}
	}
	const FLinearColor Colour = ResolveColor(State.Holder, INDEX_NONE);
	if (IsValid(HoldBallInstance))
	{
		HoldBallInstance->SetVectorParameterValue(ComicConfig.HoldColorParameter, Colour);
		// The ball as drawn fills one over the size fraction of the sprite's half extent.
		HoldBallInstance->SetScalarParameterValue(ComicConfig.HoldBallRadiusParameter, 1.f / ComicConfig.HoldBallSizeDiameterFraction);
	}
	if (IsValid(HoldRoofInstance)) { HoldRoofInstance->SetVectorParameterValue(ComicConfig.HoldColorParameter, Colour); }
	HoldHolder = State.Holder;
	HoldSeconds = 0;
	HoldPhase = EHALBallHoldRingPhase::Shown;
	RefreshTick();
	UpdateHold(ComicConfig);
}

void UHALBallComicFXComponent::StopHold(const FHALBallComicFXConfig* ComicConfig, bool bImmediate)
{
	if (HoldPhase == EHALBallHoldRingPhase::Shown && !bImmediate && ComicConfig && ComicConfig->HoldShrinkSeconds > 0)
	{
		HoldPhase = EHALBallHoldRingPhase::Leaving;
		HoldSeconds = 0;
		return;
	}
	if (bImmediate || HoldPhase == EHALBallHoldRingPhase::Shown) { HideHold(); }
}

void UHALBallComicFXComponent::HideHold()
{
	if (IsValid(HoldBallComponent)) { HoldBallComponent->SetVisibility(false); }
	if (IsValid(HoldRoofComponent)) { HoldRoofComponent->SetVisibility(false); }
	HoldHolder.Reset();
	HoldPhase = EHALBallHoldRingPhase::Off;
	RefreshTick();
}

void UHALBallComicFXComponent::UpdateHold(const FHALBallComicFXConfig& ComicConfig)
{
	const float Scale = HALBallComicFX::HoldRingScale(ComicConfig, HoldPhase, HoldSeconds);
	if (HoldPhase == EHALBallHoldRingPhase::Off || (HoldPhase == EHALBallHoldRingPhase::Leaving && Scale <= 0)) { HideHold(); return; }
	const AActor* Holder = HoldHolder.Get();
	const UPrimitiveComponent* Body = Holder ? Cast<UPrimitiveComponent>(Holder->GetRootComponent()) : nullptr;
	// This machine's own view; a top-down one stands in where there is none (nothing is drawn there anyway).
	FVector ViewLocation;
	FRotator ViewRotation(-90, 0, 0);
	if (const APlayerController* Viewer = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr) { Viewer->GetPlayerViewPoint(ViewLocation, ViewRotation); }
	const FRotationMatrix View(ViewRotation);
	const FVector ViewForward = View.GetUnitAxis(EAxis::X);
	const FVector ViewUp = View.GetUnitAxis(EAxis::Z);
	if (IsValid(HoldBallComponent))
	{
		FVector DrawnCentre;
		float DrawnRadius = 0;
		FTransform Quad;
		// The rings stay square to the holder's heading: their long axis is the car's right as seen on screen.
		const bool bPlaced = Body && ResolveDrawnBall(DrawnCentre, DrawnRadius) && HALBallComicFX::HoldBallQuad(DrawnCentre,
			HALBallComicFX::HoldBallHalfSize(ComicConfig, DrawnRadius, Scale), Body->GetComponentTransform().GetUnitAxis(EAxis::Y), ViewForward, ViewUp, Quad);
		if (bPlaced) { HoldBallComponent->SetWorldTransform(Quad); }
		HoldBallComponent->SetVisibility(bPlaced);
	}
	if (IsValid(HoldRoofComponent))
	{
		FTransform Quad;
		bool bPlaced = false;
		if (Body)
		{
			// Local bounds times scale are the car's own length and width whichever way it faces; world bounds give the roof.
			const FVector Extent = Body->CalcBounds(FTransform::Identity).BoxExtent * Body->GetComponentScale().GetAbs();
			bPlaced = HALBallComicFX::HoldMarkerQuad(ComicConfig, Body->Bounds.Origin + FVector(0, 0, FMath::Max(Body->Bounds.BoxExtent.Z, 0.)),
				Body->GetComponentTransform().GetUnitAxis(EAxis::X), float(2. * Extent.X), float(2. * Extent.Y), ViewForward, ViewUp, Scale, Quad);
		}
		if (bPlaced) { HoldRoofComponent->SetWorldTransform(Quad); }
		// Holder gone: no marker, rather than one left hanging where the car used to be.
		HoldRoofComponent->SetVisibility(bPlaced);
	}
}

void UHALBallComicFXComponent::UpdateAim(const FHALBallComicFXConfig& ComicConfig)
{
	const AActor* Holder = AimHolder.Get();
	const UPrimitiveComponent* Body = Holder ? Cast<UPrimitiveComponent>(Holder->GetRootComponent()) : nullptr;
	FTransform Quad;
	FVector DrawnCentre;
	float DrawnRadius = 0;
	if (!IsValid(AimComponent) || !Body || !ResolveDrawnBall(DrawnCentre, DrawnRadius) || !HALBallComicFX::AimAnchor(ComicConfig, Body->GetComponentTransform(),
		Body->CalcBounds(FTransform::Identity).BoxExtent * Body->GetComponentScale().GetAbs(), DrawnCentre, DrawnRadius,
		HALBallComicFX::AimPopScale(ComicConfig, AimSeconds), Quad))
	{
		// Holder gone or standing on its nose: draw nothing rather than an arrow that lies about the shot.
		if (IsValid(AimComponent)) { AimComponent->SetVisibility(false); }
		return;
	}
	AimComponent->SetWorldTransform(Quad);
	AimComponent->SetVisibility(true);
}

bool UHALBallComicFXComponent::ResolveDrawnBall(FVector& OutCentre, float& OutRadius) const
{
	const USphereComponent* Root = Ball.IsValid() ? Ball->GetPhysicsRoot() : nullptr;
	if (!Root) { return false; }
	OutCentre = Root->GetComponentLocation();
	OutRadius = Root->GetScaledSphereRadius();
	// The visual mesh is what the player sees: it may be bigger than the physics sphere, and on a client a held
	// ball is drawn from a holder-relative sample rather than at the physics root.
	if (const UStaticMeshComponent* Visual = Ball->GetVisualMesh(); Visual && Visual->GetStaticMesh() && Visual->IsVisible())
	{
		OutCentre = Visual->Bounds.Origin;
		OutRadius = HALBallComicFX::DrawnBallRadius(OutRadius, HALBallComicFX::UnrotatedExtent(*Visual));
	}
	return true;
}

void UHALBallComicFXComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	const FHALBallComicFXConfig* ComicConfig = GetComicConfig();
	if (!ComicConfig) { SetComponentTickEnabled(false); return; }
	if (bTrailWedgeActive) { UpdateTrailWedge(*ComicConfig); }
	if (bAimActive)
	{
		AimSeconds += DeltaTime;
		UpdateAim(*ComicConfig);
	}
	if (HoldPhase != EHALBallHoldRingPhase::Off)
	{
		HoldSeconds += DeltaTime;
		UpdateHold(*ComicConfig);
	}
	RefreshTick();
}
