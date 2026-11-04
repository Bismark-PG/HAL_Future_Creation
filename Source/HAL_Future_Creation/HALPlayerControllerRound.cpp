#include "HALPlayerController.h"
#include "HALNetworkGameInstance.h"
#include "HALPlayerState.h"
#include "HALRoomComponent.h"
#include "HALMatchGameState.h"
#include "HALAbilitySystemComponent.h"
#include "HALDebugHUDWidget.h"
#include "HALCameraDebugWidget.h"
#include "ArcadeVehicleMovementComponent.h"
#include "VehicleHealthComponent.h"
#include "TestVehiclePawn.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<float> CVarHALRoundDelay(TEXT("hal.RoundReadyDelay"),0,TEXT("Development only: delay this local client's round-ready acknowledgement in seconds."),ECVF_Cheat);
#endif
void AHALPlayerController::TickActor(float DeltaSeconds,ELevelTick TickType,FActorTickFunction& ThisTickFunction)
{
	// Seamless replacement can retain AutonomousProxy briefly before its owning connection is attached.
	if (GetNetMode()==NM_Client && GetLocalRole()==ROLE_AutonomousProxy && (!GetLocalPlayer() || !GetNetConnection())) { return; }
	Super::TickActor(DeltaSeconds,TickType,ThisTickFunction);
}
void AHALPlayerController::PreClientTravel(const FString& URL,ETravelType Type,bool Seamless)
{
	if (IsLocalController())
	{
		if (auto* Vehicle=Cast<ATestVehiclePawn>(GetPawn())) { Vehicle->NeutralizeInputForMenu(); }
		if (auto* GI=GetGameInstance<UHALNetworkGameInstance>()) { GI->HideTravelWidgets(); }
		if (DebugHUD) { DebugHUD->RemoveFromParent(); DebugHUD=nullptr; }
		if (CameraDebugHUD) { CameraDebugHUD->RemoveFromParent(); CameraDebugHUD=nullptr; }
	}
	Super::PreClientTravel(URL,Type,Seamless);
}
void AHALPlayerController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel(); bRoomMenuOpen=false;
	if (IsLocalController())
	{ GetWorldTimerManager().SetTimer(RoundReadyTimer,this,&ThisClass::PollRoundReadiness,.1f,true); }
}
void AHALPlayerController::HALReturnToRoom() { SubmitRoomAction(EHALRoomAction::ReturnToRoom); }
void AHALPlayerController::PollRoundReadiness()
{
	if (!IsLocalController() || !GetLocalPlayer() || IsActorBeingDestroyed() || GetWorld()->bIsTearingDown
		|| (GetNetMode()==NM_Client && !GetNetConnection())) { return; }
	CreateLocalDebugWidgets();
	auto* S=GetWorld()->GetGameState<AHALMatchGameState>(); if (!S) { return; }
	auto* R=S->GetRoom();
	if (BoundRoom.Get()!=R)
	{
		if (BoundRoom.IsValid()) { BoundRoom->OnRoomChanged.RemoveDynamic(this,&ThisClass::RefreshRoomUI); }
		BoundRoom=R; R->OnRoomChanged.AddUniqueDynamic(this,&ThisClass::RefreshRoomUI);
	}
	RefreshRoomUI();
	if (R->GetData().Phase!=EHALRoomPhase::Loading) { return; }
	auto* PS=GetPlayerState<AHALPlayerState>(); auto* Vehicle=Cast<ATestVehiclePawn>(GetPawn());
	if (!PS || !Vehicle) { return; }
	const auto& P=PS->GetRoomData(); auto* ASC=PS->GetHALAbilitySystemComponent();
	const auto* M=Vehicle->FindComponentByClass<UArcadeVehicleMovementComponent>();
	const auto* H=Vehicle->FindComponentByClass<UVehicleHealthComponent>();
	if (P.bClientLoaded || !P.LoadToken.IsValid() || P.ReadyAvatar!=Vehicle || Vehicle->GetPlayerState()!=PS || !Vehicle->IsLocallyControlled()
		|| !Vehicle->InputComponent || ASC->GetOwnerActor()!=PS || ASC->GetAvatarActor()!=Vehicle || !ASC->IsHealthInitialized()
		|| !ASC->IsCombatInitialized() || !ASC->IsImpactInitialized() || !PS->AreVehicleAbilitiesReady(Vehicle) || !H || !H->IsHealthReady()
		|| PS->GetVehicleLifecycle().RoundId!=R->GetData().RoundId || PS->GetVehicleLifecycle().Avatar!=Vehicle
		|| !M || !M->GetMovementEffectStatus().bReady || !M->GetMovementEffectStatus().Value.Epoch.IsValid()) { return; }
	if (ObservedLoadToken!=P.LoadToken) { ObservedLoadToken=P.LoadToken; ReadyObservedAt=FPlatformTime::Seconds(); }
	float Delay=0;
#if !UE_BUILD_SHIPPING
	Delay=FMath::Clamp(CVarHALRoundDelay.GetValueOnGameThread(),0.f,120.f);
#endif
	const double Now=FPlatformTime::Seconds();
	if (Now-ReadyObservedAt<Delay || Now<NextReadyRequest) { return; }
	NextReadyRequest=Now+.25;
	ServerRoundLoaded(R->GetData().FlowId,R->GetData().RoundId,P.LoadToken,Vehicle);
}
void AHALPlayerController::ServerRoundLoaded_Implementation(FGuid Flow,uint32 Round,FGuid Token,APawn* Vehicle)
{
	if (auto* S=GetWorld()->GetGameState<AHALMatchGameState>())
	{ S->GetRoom()->AcknowledgeLoaded(this,Flow,Round,Token,Vehicle); }
}
