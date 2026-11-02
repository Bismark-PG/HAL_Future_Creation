#include "HALMagnetBeamPresentationComponent.h"
#include "HALBallPresentationComponent.h"
#include "HALPlayerState.h"

#include "Components/MeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"

#include "HAL/IConsoleManager.h"

// Off by default. Like the shocked overlay, every layer here fails silently when a name or a
// component does not line up, so the only cheap way to tell "never ran" from "ran and drew
// nothing" is to say so on request: hal.MagnetBeamLog 1
static TAutoConsoleVariable<int32> CVarHALMagnetBeamLog(TEXT("hal.MagnetBeamLog"), 0,
	TEXT("Log magnet beam mounting and phase changes (0 off, 1 on)."), ECVF_Default);

namespace HALMagnetBeam
{
	static bool WantsLog() { return CVarHALMagnetBeamLog.GetValueOnGameThread() != 0; }

	// User parameters the beam asset reads. Names are the contract with that asset: one
	// mismatch and this layer silently draws nothing, which is what hal.MagnetBeamLog is for.
	static const FName PositionsParam(TEXT("BeamPositions"));
	static const FName WidthsParam(TEXT("BeamWidths"));
	static const FName PointsPerRibbonParam(TEXT("BeamPointsPerRibbon"));
	static const FName RibbonCountParam(TEXT("BeamRibbonCount"));
	static const FName AlphaParam(TEXT("BeamAlpha"));
	static const FName ColorParam(TEXT("BeamColor"));

	// Material parameters on the ball shell. Absent parameters make these no-ops, which is
	// why a placeholder ball material costs nothing and never has to be edited first.
	static const FName ShellColorParam(TEXT("HolderColor"));
	static const FName ShellPulseParam(TEXT("HolderPulse"));

	static const TCHAR* LexToString(const EHALMagnetBeamPhase Phase)
	{
		switch (Phase)
		{
		case EHALMagnetBeamPhase::Idle: return TEXT("Idle");
		case EHALMagnetBeamPhase::Attach: return TEXT("Attach");
		case EHALMagnetBeamPhase::Hold: return TEXT("Hold");
		case EHALMagnetBeamPhase::Release: return TEXT("Release");
		case EHALMagnetBeamPhase::Launch: return TEXT("Launch");
		default: return TEXT("Unknown");
		}
	}
}

UHALMagnetBeamPresentationComponent::UHALMagnetBeamPresentationComponent()
{
	// Ticks only while a beam is on screen; a free ball costs nothing. PostPhysics so the
	// anchors read this frame's solved poses instead of last frame's.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	// Four placeholder planet colours, none of them blue: blue already means "being shocked"
	// on a vehicle, and a second blue state would be unreadable in a melee.
	HolderPalette = {
		FLinearColor(0.98f, 0.62f, 0.12f, 1.f),
		FLinearColor(0.11f, 0.78f, 0.52f, 1.f),
		FLinearColor(0.92f, 0.28f, 0.52f, 1.f),
		FLinearColor(0.58f, 0.45f, 0.95f, 1.f)
	};
}

void UHALMagnetBeamPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	Ball = Cast<ABasicBallActor>(GetOwner());
	if (Ball.IsValid())
	{
		BallPresentation = Ball->FindComponentByClass<UHALBallPresentationComponent>();
		SyncToBallState(Ball->GetBallState(), Ball->GetControlledBy());
	}
}

void UHALMagnetBeamPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopBeam();
	Super::EndPlay(Reason);
}

void UHALMagnetBeamPresentationComponent::SyncToBallState(EBasicBallState State, AActor* Holder)
{
	// Headless, null RHI and automation never build the effect, but the phase bookkeeping
	// still has to stay consistent so diagnostics and tests behave the same everywhere.
	const EBasicBallState Previous = bSynced ? LastSeenState : EBasicBallState::Free;
	bSynced = true;
	LastSeenState = State;
	if (!FApp::CanEverRender()) { return; }

	if (State == EBasicBallState::Controlled)
	{
		const bool bNewHolder = BeamHolder.Get() != Holder;
		const bool bNotRunning = Phase == EHALMagnetBeamPhase::Idle
			|| Phase == EHALMagnetBeamPhase::Release || Phase == EHALMagnetBeamPhase::Launch;
		if (bNewHolder || bNotRunning)
		{
			BeamHolder = Holder;
			ResolvedColor = ResolveHolderColor(Holder);
			BeginPhase(EHALMagnetBeamPhase::Attach);
		}
		return;
	}

	const bool bWasHeld = Previous == EBasicBallState::Controlled
		|| Phase == EHALMagnetBeamPhase::Attach || Phase == EHALMagnetBeamPhase::Hold;
	if (bWasHeld)
	{
		// Launched means the holder spent the ball, anything else means the grip broke.
		BeginPhase(State == EBasicBallState::Launched
			? EHALMagnetBeamPhase::Launch : EHALMagnetBeamPhase::Release);
	}
}

void UHALMagnetBeamPresentationComponent::BeginPhase(EHALMagnetBeamPhase Next)
{
	Phase = Next;
	PhaseSeconds = 0.f;
	if (Next == EHALMagnetBeamPhase::Attach)
	{
		HeldSeconds = 0.f;
		EnsureBeam();
	}
	if (Next == EHALMagnetBeamPhase::Idle) { StopBeam(); }
	SetComponentTickEnabled(Next != EHALMagnetBeamPhase::Idle);
	if (HALMagnetBeam::WantsLog())
	{
		UE_LOG(LogTemp, Log, TEXT("MagnetBeam: %s phase=%s holder=%s beam=%d shell=%d"),
			*GetNameSafe(GetOwner()), HALMagnetBeam::LexToString(Phase),
			*GetNameSafe(BeamHolder.Get()), IsValid(BeamComponent) ? 1 : 0, ShellInstances.Num());
	}
}

void UHALMagnetBeamPresentationComponent::EnsureBeam()
{
	USceneComponent* Attach = Ball.IsValid() ? Ball->GetRootComponent() : nullptr;
	if (!Attach || !GetWorld() || !GetWorld()->FXSystem.IsValid()) { return; }

	if (bPulseBallShell && ShellInstances.IsEmpty())
	{
		if (UStaticMeshComponent* Shell = Ball->GetVisualMesh())
		{
			const int32 Slots = Shell->GetNumMaterials();
			for (int32 Slot = 0; Slot < Slots; ++Slot)
			{
				if (UMaterialInstanceDynamic* Instance = Shell->CreateDynamicMaterialInstance(Slot))
				{
					ShellInstances.Add(Instance);
				}
			}
		}
	}

	if (!BeamSystem) { return; }
	if (!IsValid(BeamComponent))
	{
		BeamComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(BeamSystem, Attach, NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
			false, false, ENCPoolMethod::None, false);
		if (!BeamComponent) { return; }
		// The ball is a spinning sphere and the positions pushed below are world space, so the
		// component must not inherit its rotation or scale; it follows the ball only so the
		// bounds stay near what it actually draws.
		BeamComponent->SetUsingAbsoluteRotation(true);
		BeamComponent->SetUsingAbsoluteScale(true);
	}
	// Layout is fixed for the session, so the asset can size its spawn burst from these.
	BeamComponent->SetVariableInt(HALMagnetBeam::PointsPerRibbonParam,
		HALMagnetBeamGeometry::PointsPerRibbon(BeamSettings));
	BeamComponent->SetVariableInt(HALMagnetBeam::RibbonCountParam,
		HALMagnetBeamGeometry::RibbonCount(BeamSettings));
	BeamComponent->SetVariableLinearColor(HALMagnetBeam::ColorParam, ResolvedColor);
	BeamComponent->Activate(true);
}

void UHALMagnetBeamPresentationComponent::StopBeam()
{
	if (IsValid(BeamComponent))
	{
		// Deactivate rather than destroy, so strands already spawned play out their own
		// lifetime and a vehicle that regains the ball reuses the same component.
		BeamComponent->SetVariableFloat(HALMagnetBeam::AlphaParam, 0.f);
		BeamComponent->Deactivate();
	}
	PushShell(0.f);
	BeamHolder.Reset();
}

FLinearColor UHALMagnetBeamPresentationComponent::ResolveHolderColor(const AActor* Holder) const
{
	const APawn* Pawn = Cast<APawn>(Holder);
	if (const auto* RoomPlayer = Pawn ? Pawn->GetPlayerState<AHALPlayerState>() : nullptr; RoomPlayer && RoomPlayer->GetRoomData().Seat != INDEX_NONE)
	{ return RoomPlayer->GetRoomData().Color; }
	if (HolderPalette.IsEmpty()) { return FallbackColor; }
	const APlayerState* State = Pawn ? Pawn->GetPlayerState() : nullptr;
	if (!State || State->GetPlayerId() < 0) { return FallbackColor; }
	return HolderPalette[State->GetPlayerId() % HolderPalette.Num()];
}

bool UHALMagnetBeamPresentationComponent::ResolveAnchors(FVector& OutRoof, FVector& OutBall,
	float& OutRestLengthCm) const
{
	const AActor* Holder = BeamHolder.Get();
	const USphereComponent* Root = Ball.IsValid() ? Ball->GetPhysicsRoot() : nullptr;
	if (!Holder || !Root) { return false; }
	// A mesh component specifically, not any primitive: local bounds are what place the roof
	// anchor, and both vehicle classes already root themselves on a static mesh body.
	const UMeshComponent* Body = Cast<UMeshComponent>(Holder->GetRootComponent());
	if (!Body) { return false; }

	const FVector Extent = Body->GetLocalBounds().BoxExtent;
	const FVector RoofLocal(-Extent.X * RoofBackFraction, 0.f, Extent.Z + RoofLiftCm);
	OutRoof = Body->GetComponentTransform().TransformPosition(RoofLocal);

	// The displayed pose is the only correct ball end on a client: while Controlled the ball
	// is shown from the holder-relative sample, so the physics root can sit somewhere else
	// entirely and a beam built from it would miss the ball the player can see.
	const UHALBallPresentationComponent* Presentation = BallPresentation.Get();
	const bool bUseDisplayed = Presentation
		&& Presentation->GetVisualMode() != EHALBallVisualMode::Physics;
	const FVector Centre = bUseDisplayed
		? Presentation->GetDisplayedRootPose().GetLocation()
		: Root->GetComponentLocation();

	const float Radius = Root->GetScaledSphereRadius();
	OutBall = Centre + FVector::UpVector * (Radius * BallEntryHeightFraction);
	// Flush contact: the nose reaches Extent.X from the body origin and the ball centre sits
	// one radius beyond it, so this is the shortest the pair can plausibly be.
	OutRestLengthCm = Extent.X + Radius;
	return (OutBall - OutRoof).SizeSquared() <= FMath::Square(MaximumBeamLengthCm);
}

void UHALMagnetBeamPresentationComponent::PushBeam(float Visible, bool bRetractFromBall)
{
	if (!IsValid(BeamComponent)) { return; }
	FVector Roof = FVector::ZeroVector;
	FVector BallEnd = FVector::ZeroVector;
	float RestLength = 0.f;
	if (!ResolveAnchors(Roof, BallEnd, RestLength))
	{
		// Holder gone, body missing or implausibly far: draw nothing rather than a beam
		// stretched across the arena.
		BeamComponent->SetVariableFloat(HALMagnetBeam::AlphaParam, 0.f);
		return;
	}

	const float Length = (BallEnd - Roof).Size();
	const float Strain = FMath::Clamp((Length - RestLength)
		/ FMath::Max(1.f, TensionReferenceCm - RestLength), 0.f, 1.f);
	const float Gain = FMath::Lerp(1.f, MaximumTensionGain, Strain);

	// Attach overshoots the authored apex and spread, then settles: the beam snaps onto the
	// ball instead of easing on, which is what sells the capture.
	const float Overshoot = (Phase == EHALMagnetBeamPhase::Attach)
		? AttachOvershoot * (1.f - FMath::Clamp(Visible, 0.f, 1.f)) : 0.f;
	FHALMagnetBeamSettings Frame = BeamSettings;
	Frame.ApexHeightCm *= 1.f + Overshoot * 0.5f;
	Frame.SpreadCm *= 1.f + Overshoot;
	Frame.WidthCm *= Gain;

	// Rebuilt every frame rather than on a step clock: the shock arcs jump because electricity
	// does, while the beam has to slide continuously or it reads as broken.
	HALMagnetBeamGeometry::Build(Roof, BallEnd, FVector::UpVector, Frame, HelixPhase,
		BeamPositions, BeamBaseWidths);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(BeamComponent,
		HALMagnetBeam::PositionsParam, BeamPositions);

	const int32 PerRibbon = FMath::Max(2, HALMagnetBeamGeometry::PointsPerRibbon(Frame));
	const float Feather = FMath::Max(0.01f, RetractFeather);
	const float Retract = 1.f - FMath::Clamp(Visible, 0.f, 1.f);
	// Front starts behind the tail and ends past the head, so Retract 0 and 1 are exactly
	// "whole strand" and "nothing left" instead of clipping either end early.
	const float Front = Retract * (1.f + Feather) - Feather;
	const float Step01 = 1.f / float(PerRibbon - 1);
	BeamWidths.SetNumUninitialized(BeamBaseWidths.Num());
	for (int32 Index = 0; Index < BeamBaseWidths.Num(); ++Index)
	{
		const float Along = float(Index % PerRibbon) * Step01;
		// The surviving end differs by phase: a broken grip loses the ball end first, a launch
		// loses the roof end first, so the same mask is read from one end or the other.
		const float Measured = bRetractFromBall ? 1.f - Along : Along;
		BeamWidths[Index] = BeamBaseWidths[Index] * FMath::SmoothStep(Front, Front + Feather, Measured);
	}
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(BeamComponent,
		HALMagnetBeam::WidthsParam, BeamWidths);

	// Retraction already removes the strand, so brightness carries strain instead of fading
	// twice over the same transition.
	BeamComponent->SetVariableLinearColor(HALMagnetBeam::ColorParam, ResolvedColor);
	BeamComponent->SetVariableFloat(HALMagnetBeam::AlphaParam, Gain);
}

void UHALMagnetBeamPresentationComponent::PushShell(float Visible)
{
	if (!bPulseBallShell || ShellInstances.IsEmpty()) { return; }
	// Same colour and same clock as the beam: synchronised blinking is what ties the ball to
	// one specific vehicle once the cars are too far away to trace a beam by eye.
	const float Wave = 0.5f + 0.5f * FMath::Sin(HeldSeconds * UE_TWO_PI * ShellPulseHz);
	const float Pulse = FMath::Clamp(Visible, 0.f, 1.f)
		* FMath::Lerp(FMath::Clamp(ShellPulseFloor, 0.f, 1.f), 1.f, Wave);
	for (const TObjectPtr<UMaterialInstanceDynamic>& Instance : ShellInstances)
	{
		if (!Instance) { continue; }
		Instance->SetVectorParameterValue(HALMagnetBeam::ShellColorParam, ResolvedColor);
		Instance->SetScalarParameterValue(HALMagnetBeam::ShellPulseParam, Pulse);
	}
}

void UHALMagnetBeamPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (Phase == EHALMagnetBeamPhase::Idle) { SetComponentTickEnabled(false); return; }
	PhaseSeconds += DeltaTime;
	HelixPhase += DeltaTime * DriftTurnsPerSecond * UE_TWO_PI;

	switch (Phase)
	{
	case EHALMagnetBeamPhase::Attach:
	{
		HeldSeconds += DeltaTime;
		const float Alpha = AttachSeconds > 0.f
			? FMath::Clamp(PhaseSeconds / AttachSeconds, 0.f, 1.f) : 1.f;
		// Sweeps in from the roof, so the ball end is the part still missing.
		PushBeam(Alpha, true);
		PushShell(Alpha);
		if (Alpha >= 1.f) { BeginPhase(EHALMagnetBeamPhase::Hold); }
		return;
	}
	case EHALMagnetBeamPhase::Hold:
		HeldSeconds += DeltaTime;
		PushBeam(1.f, true);
		PushShell(1.f);
		return;
	case EHALMagnetBeamPhase::Release:
	{
		const float Remaining = ReleaseSeconds > 0.f
			? FMath::Max(0.f, 1.f - PhaseSeconds / ReleaseSeconds) : 0.f;
		PushBeam(Remaining, true);
		PushShell(Remaining);
		if (Remaining <= 0.f) { BeginPhase(EHALMagnetBeamPhase::Idle); }
		return;
	}
	case EHALMagnetBeamPhase::Launch:
	{
		const float Remaining = LaunchSeconds > 0.f
			? FMath::Max(0.f, 1.f - PhaseSeconds / LaunchSeconds) : 0.f;
		// Collapses into the ball, so the roof end is the part that goes first.
		PushBeam(Remaining, false);
		PushShell(Remaining);
		if (Remaining <= 0.f) { BeginPhase(EHALMagnetBeamPhase::Idle); }
		return;
	}
	default:
		return;
	}
}
