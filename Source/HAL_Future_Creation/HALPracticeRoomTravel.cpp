#include "HALPracticeRoomGameMode.h"
#include "HALNetworkGameInstance.h"
#include "HALRoomConfig.h"
#include "HALRoomComponent.h"
#include "HALMatchGameState.h"
#include "HALPlayerController.h"
#include "HALPlayerState.h"
#include "TestVehiclePawn.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

AHALPracticeRoomGameMode::AHALPracticeRoomGameMode() { bUseSeamlessTravel=true; }
void AHALPracticeRoomGameMode::PreLogin(const FString& O,const FString& A,const FUniqueNetIdRepl& Id,FString& Error)
{
	if (bRoundWorld || bTravelDeparting) { Error=TEXT("The participant list is locked."); return; }
	Super::PreLogin(O,A,Id,Error);
}
bool AHALPracticeRoomGameMode::PreflightRound() const
{
	FString Error;
	if (!bUseSeamlessTravel) { return false; }
	const auto* PIE=IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel"));
	if (GetWorld()->WorldType==EWorldType::PIE && (!PIE || !PIE->GetInt())) { return false; }
	return RoomConfig && RoomConfig->Validate(Error) && GetGameInstance<UHALNetworkGameInstance>()
		&& RoomConfig->MatchMap.GetLongPackageName()!=UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName())
		&& FPackageName::DoesPackageExist(RoomConfig->MatchMap.GetLongPackageName());
}
bool AHALPracticeRoomGameMode::StartRoundTravel(bool ToPractice,const FString& Reason)
{
	if (!HasAuthority() || bTravelDeparting || bReturningToEntry || !RoomConfig) { return false; }
	auto* GI=GetGameInstance<UHALNetworkGameInstance>(); auto* Room=GetGameState<AHALMatchGameState>()->GetRoom();
	if (!GI || (!ToPractice && !PreflightRound())) { return false; }
	FHALRoundTravel T; T.FlowId=Room->GetData().FlowId; T.RoundId=Room->GetData().RoundId; T.bToPractice=ToPractice;
	T.HostKey=Room->GetData().Host ? Room->GetData().Host->GetRoomData().PlayerKey : Travel.HostKey;
	T.Config=DuplicateObject<UHALRoomConfig>(RoomConfig,GI); T.Config->SetFlags(RF_Transient); T.Config->ClearFlags(RF_Standalone|RF_Public);
	T.Config->Rules=Room->GetData().Rules;
	T.PracticeMap=bRoundWorld?Travel.PracticeMap:UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	T.Destination=ToPractice?T.PracticeMap:RoomConfig->MatchMap.GetLongPackageName(); T.StartedAt=FPlatformTime::Seconds(); T.Reason=Reason;
	for (auto It=GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		auto* PC=Cast<AHALPlayerController>(It->Get()); auto* PS=PC?PC->GetPlayerState<AHALPlayerState>():nullptr;
		if (!PS || PC->IsActorBeingDestroyed() || !PS->GetRoomData().PlayerKey.IsValid()) { continue; }
		FHALTravelPlayer P; P.Identity=PS->GetRoomData(); P.Identity.ReadyAvatar=nullptr; P.Identity.LoadToken.Invalidate(); P.Identity.bClientLoaded=false;
		P.ChangeSerial=PS->GetVehicleLifecycle().ChangeSerial; P.LifecycleRevision=PS->GetVehicleLifecycle().Revision; P.PreviousRound=PS->GetVehicleLifecycle().RoundId;
		T.Players.Add(P);
	}
	if (!T.IsValid() || !FPackageName::DoesPackageExist(T.Destination)) { return false; }
	bTravelDeparting=true; GetWorldTimerManager().ClearTimer(RoundBarrierTimer);
	if (ToPractice) { Room->CloseForReturn(); }
	GI->StoreRoundTravel(T);
	for (auto It=GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		auto* PC=Cast<AHALPlayerController>(It->Get()); auto* PS=PC?PC->GetPlayerState<AHALPlayerState>():nullptr;
		if (!PS) { continue; }
		APawn* Old=PC->GetPawn(); PS->PrepareForWorldExit(); PC->UnPossess(); PC->StartSpot.Reset();
		if (Old) { Old->Destroy(); }
	}
	const FString URL=T.Destination+TEXT("?game=")+GetClass()->GetPathName();
	UE_LOG(LogTemp,Display,TEXT("GF4 travel: Round=%u ToPractice=%d Players=%d Destination=%s"),T.RoundId,ToPractice,T.Players.Num(),*T.Destination);
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this,URL]
	{
		if (!GetWorld()->ServerTravel(URL,true))
		{ if (auto* GI=GetGameInstance<UHALNetworkGameInstance>()) { GI->ReturnToEntry(FText::FromString(TEXT("Could not travel to match. Please create the room again."))); } }
	}));
	return true;
}
void AHALPracticeRoomGameMode::HandleStartingNewPlayer_Implementation(APlayerController* Player)
{
	if (!bRestoringTravel) { Super::HandleStartingNewPlayer_Implementation(Player); return; }
	auto* PC=Cast<AHALPlayerController>(Player); auto* PS=PC?PC->GetPlayerState<AHALPlayerState>():nullptr;
	if (!PS || bTravelDeparting) { return; }
	const auto* P=Travel.Players.FindByPredicate([PS](const FHALTravelPlayer& E){ return E.Identity.PlayerKey==PS->GetRoomData().PlayerKey; });
	auto* Room=GetGameState<AHALMatchGameState>()->GetRoom();
	if (!P || !Room->ImportTravelMember(PC,*P,Travel.HostKey))
	{ PC->ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("Your room identity could not be restored."))); return; }
	if (bRoundWorld && !PC->GetPawn())
	{
		if (SpawnSelectedVehicle(PC,PS->GetRoomData().SelectionRevision,Travel.RoundId)!=EHALRoomResult::Accepted)
		{ StartRoundTravel(true,TEXT("Could not prepare every vehicle. Please confirm again.")); }
	}
	else if (!bRoundWorld) { SetSelectionView(PC); }
}
void AHALPracticeRoomGameMode::PollRoundBarrier()
{
	if (bTravelDeparting || bReturningToEntry) { return; }
	auto* S=GetGameState<AHALMatchGameState>(); auto* Room=S->GetRoom(); const auto& D=Room->GetData();
	if (D.Phase==EHALRoomPhase::Loading)
	{
		if (Room->AllClientsLoaded())
		{ Room->SetCountdown(S->GetServerWorldTimeSeconds()+RoomConfig->CountdownSeconds); UE_LOG(LogTemp,Display,TEXT("GF4 all loaded: Round=%u End=%.3f"),D.RoundId,D.CountdownEndServerTime); }
		else if (FPlatformTime::Seconds()-Travel.StartedAt>RoomConfig->LoadTimeoutSeconds)
		{
			if (!StartRoundTravel(true,TEXT("Loading timed out. Please confirm again.")))
			{ if (auto* GI=GetGameInstance<UHALNetworkGameInstance>()) { GI->ReturnToEntry(FText::FromString(TEXT("Could not recover the room after loading timed out."))); } }
		}
	}
	else if (D.Phase==EHALRoomPhase::Countdown && S->GetServerWorldTimeSeconds()>=D.CountdownEndServerTime)
	{
		Room->AdvanceRound(D.RoundId,EHALRoomPhase::Playing); S->SetMatchPhase(EHALMatchPhase::Playing);
		GetWorldTimerManager().ClearTimer(RoundBarrierTimer);
		UE_LOG(LogTemp,Display,TEXT("GF4 Playing: Round=%u Players=%d"),D.RoundId,D.Members.Num());
	}
}
