#include "HALDynamicCameraComponent.h"
#include "HALCameraConfig.h"
#include "BasicBallActor.h"
#include "TestVehiclePawn.h"
#include "HALMatchGameState.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"

namespace
{
	TAutoConsoleVariable<int32> CVarCameraStatic(TEXT("hal.CameraStatic"),0,TEXT("Force the configured profile into static base-camera mode: 0=profile, 1=static."));
	FVector CameraActorPoint(const AActor* Actor)
	{
		FVector Center, Extent; Actor->GetActorBounds(true,Center,Extent);
		return Extent.IsNearlyZero() ? Actor->GetActorLocation() : Center;
	}
}
UHALDynamicCameraComponent::UHALDynamicCameraComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UHALDynamicCameraComponent::Configure(UVehicleLocalConfig* InLocal) { Local = InLocal; ResetCamera(); PreviousProfile.Reset(); }
void UHALDynamicCameraComponent::AddMouseDelta(const FVector2D& Delta)
{
	if (!Delta.ContainsNaN()) { MouseDelta += Delta; }
}
void UHALDynamicCameraComponent::SetStickInput(const FVector2D& Value)
{
	StickInput = Value.ContainsNaN() ? FVector2D::ZeroVector : Value.GetClampedToMaxSize(1);
}
void UHALDynamicCameraComponent::ResetCamera()
{
	State = {}; Frame = {}; Weights.Reset(); MouseDelta = StickInput = FVector2D::ZeroVector; CalculatedFrame = MAX_uint64;
}
void UHALDynamicCameraComponent::EndPlay(const EEndPlayReason::Type Reason) { ResetCamera(); Super::EndPlay(Reason); }
bool UHALDynamicCameraComponent::CalculateView(float Dt, FMinimalViewInfo& Out)
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController() || !Pawn->IsLocallyControlled() || GetNetMode() == NM_DedicatedServer)
	{ ResetCamera(); return false; }
	int32 X = 0, Y = 0; PC->GetViewportSize(X,Y);
	if (X < 1 || Y < 1) { ResetCamera(); return false; }
	const auto* Player = PC->GetLocalPlayer();
	const auto* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
	const auto* Match = GetWorld()->GetGameState<AHALMatchGameState>();
	const bool bAccept = Viewport && Viewport->HasFocus() && Viewport->IsForegroundWindow() && !PC->bShowMouseCursor
		&& !PC->IsLookInputIgnored() && (!Match || Match->GetMatchPhase() == EHALMatchPhase::Playing) && !GetWorld()->IsPaused();
	if (PreviousController.Get() != PC) { ResetCamera(); PreviousController = PC; }
	if (CalculatedFrame == GFrameCounter && Frame.bValid) { Out = Frame.View; return true; }
	CalculatedFrame = GFrameCounter;
	return Compute(Dt,FVector2D(X,Y),bAccept,Out);
}
TArray<FHALCameraTarget> UHALDynamicCameraComponent::CollectTargets(const UHALCameraConfig& C, const FVector& Own, float Dt, int32& MainCount)
{
	TArray<TPair<AActor*,const FHALCameraTargetRule*>> Candidates;
	TArray<ABasicBallActor*> Primary;
	for (TActorIterator<ABasicBallActor> It(GetWorld()); It; ++It)
	{ if (!It->IsActorBeingDestroyed() && It->IsPrimaryCameraTarget()) { Primary.Add(*It); } }
	MainCount = Primary.Num();
	if (MainCount == 1) { Candidates.Add({Primary[0],&C.MainBall}); }
	for (TActorIterator<ATestVehiclePawn> It(GetWorld()); It; ++It)
	{ if (*It != GetOwner() && !It->IsActorBeingDestroyed() && It->GetPlayerState()) { Candidates.Add({*It,&C.OtherPlayers}); } }
	TArray<FHALCameraTarget> Result; TSet<TWeakObjectPtr<AActor>> Seen;
	const float Alpha = 1-FMath::Exp(-FMath::Clamp(Dt,0.f,C.MaximumSampleDelta)/C.WeightResponseSeconds);
	for (const auto& Candidate : Candidates)
	{
		AActor* Actor = Candidate.Key; const auto& Rule = *Candidate.Value; const FVector Position = CameraActorPoint(Actor);
		if (Position.ContainsNaN()) { continue; }
		Seen.Add(Actor); float& Weight = Weights.FindOrAdd(Actor);
		Weight = FMath::Lerp(Weight,Rule.Evaluate(float(FVector::Dist2D(Own,Position))),Alpha);
		const auto* Pawn = Cast<APawn>(Actor); const auto* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
		Result.Add({Position,Weight,Rule.ZoomInfluence,PS ? FString::Printf(TEXT("Player %d"),PS->GetPlayerId()) : TEXT("MainBall")});
	}
	for (auto It = Weights.CreateIterator(); It; ++It) { if (!It.Key().IsValid() || !Seen.Contains(It.Key())) { It.RemoveCurrent(); } }
	Result.Sort([](const FHALCameraTarget& A, const FHALCameraTarget& B) { return A.Label < B.Label; });
	return Result;
}
bool UHALDynamicCameraComponent::Compute(float Dt, const FVector2D& Viewport, bool bAccept, FMinimalViewInfo& Out)
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled()) { ResetCamera(); return false; }
	auto* Profile = Local ? Local->CameraProfile.Get() : nullptr; FString Error;
	if (!Profile || !Profile->Validate(Error))
	{
		if (Profile && LastError != Error) { UE_LOG(LogTemp,Warning,TEXT("%s: camera profile disabled: %s"),*GetNameSafe(Pawn),*Error); LastError = Error; }
		ResetCamera(); PreviousProfile.Reset(); return false;
	}
	const bool bDynamic = Profile->bDynamicEnabled && !CVarCameraStatic.GetValueOnGameThread();
	if (PreviousProfile.Get() != Profile || bPreviousDynamic != bDynamic)
	{ ResetCamera(); PreviousProfile = Profile; bPreviousDynamic = bDynamic; }
	LastError.Reset();
	int32 MainCount = 0; const FVector Own = CameraActorPoint(Pawn);
	const auto Targets = CollectTargets(*Profile,Own,Dt,MainCount);
	Frame = HALCamera::Advance(*Profile,Own,Targets,Viewport,Dt,MouseDelta,StickInput,bAccept,bDynamic,State);
	Frame.MainBallCandidates = MainCount;
	MouseDelta = FVector2D::ZeroVector;
	if (!bAccept) { StickInput = FVector2D::ZeroVector; }
	if (!Frame.bValid) { ResetCamera(); return false; }
	Out = Frame.View; return true;
}
