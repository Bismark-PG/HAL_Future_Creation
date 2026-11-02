#include "HALPracticeRoomGameMode.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "HALRoomConfig.h"
#include "HALVehicleCatalog.h"
#include "HALPlayerController.h"
#include "HALPlayerState.h"
#include "HALPlayerStart.h"
#include "HALNetworkGameInstance.h"
#include "HALAbilitySystemComponent.h"
#include "HALVehicleLifecycleTypes.h"
#include "TestVehiclePawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"

namespace
{
	bool SpawnBodyBlocked(ATestVehiclePawn* Pawn)
	{
		auto* Body=Cast<UPrimitiveComponent>(Pawn->GetRootComponent());
		if (!Body || !Body->IsQueryCollisionEnabled()) { return true; }
		FComponentQueryParams Params(SCENE_QUERY_STAT(HALPracticeSpawn),Pawn);
		TArray<FOverlapResult> Hits;
		const bool Blocked=Pawn->GetWorld()->ComponentOverlapMultiByChannel(Hits,Body,Body->GetComponentLocation(),Body->GetComponentQuat(),Body->GetCollisionObjectType(),Params);
		if (Blocked)
		{
			for (const auto& Hit:Hits)
			{ if (Hit.bBlockingHit) { UE_LOG(LogTemp,Log,TEXT("Practice spawn blocked: Body=%s At=%s Other=%s Component=%s"),*GetNameSafe(Body),*Body->GetComponentLocation().ToCompactString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent())); } }
		}
		return Blocked;
	}
}

void AHALPracticeRoomGameMode::InitGame(const FString& Map, const FString& Options, FString& Error)
{
	Super::InitGame(Map,Options,Error);
	// Server startup URL only; never read PreLogin/client options to configure the room.
	const FString ConfigPath = UGameplayStatics::ParseOption(Options,TEXT("RoomConfig"));
	if (!RoomConfig && !ConfigPath.IsEmpty()) { RoomConfig = LoadObject<UHALRoomConfig>(nullptr,*ConfigPath); }
	if (!RoomConfig) { Error = TEXT("Practice room needs a server Room Config."); }
}
void AHALPracticeRoomGameMode::InitGameState()
{
	Super::InitGameState();
	if (auto* S = GetGameState<AHALMatchGameState>(); S && RoomConfig) { S->GetRoom()->EnablePracticeRoom(); }
}
void AHALPracticeRoomGameMode::SetSelectionView(AHALPlayerController* PC)
{
	if (!PC) { return; }
	const auto* PS = PC->GetPlayerState<AHALPlayerState>();
	for (TActorIterator<AHALPlayerStart> It(GetWorld()); It && PS; ++It)
	{
		if (It->SpawnSlot == PS->GetRoomData().Seat)
		{ PC->ClientRoomSelectionView(It->GetActorLocation()+FVector(0,0,1200),FRotator(-75,0,0)); break; }
	}
}
void AHALPracticeRoomGameMode::PostLogin(APlayerController* Player)
{
	Super::PostLogin(Player); SetSelectionView(Cast<AHALPlayerController>(Player));
}
void AHALPracticeRoomGameMode::RemovePracticeAvatar(AHALPlayerController* PC)
{
	if (!PC) { return; }
	auto* Old = Cast<ATestVehiclePawn>(PC->GetPawn());
	if (Old) { Old->RetireForVehicleTransition(); }
	PC->UnPossess(); PC->StartSpot.Reset();
	if (Old) { Old->Destroy(); }
	SetSelectionView(PC);
}
EHALRoomResult AHALPracticeRoomGameMode::SpawnSelectedVehicle(AHALPlayerController* PC, uint32 Revision)
{
	auto* S = GetGameState<AHALMatchGameState>(); auto* PS = PC ? PC->GetPlayerState<AHALPlayerState>() : nullptr;
	auto* Room = S ? S->GetRoom() : nullptr;
	if (!HasAuthority() || !PS || !Room || !RoomConfig || !RoomConfig->Catalog) { return EHALRoomResult::InvalidConfiguration; }
	const auto P = PS->GetRoomData();
	if (P.Phase != EHALRoomPlayerPhase::PreparingAvatar || P.SelectionRevision != Revision) { return EHALRoomResult::StaleSelection; }
	FString Error; UClass* Class = RoomConfig->Catalog->Resolve(P.VehicleId,Error);
	if (!Class) { return Room->FailAvatar(PC,Revision,EHALRoomResult::InvalidVehicle); }
	AHALPlayerStart* Start = nullptr; bool Slots[4] = {};
	for (TActorIterator<AHALPlayerStart> It(GetWorld()); It; ++It)
	{
		const int32 Slot = It->SpawnSlot;
		if (Slot < 0 || Slot > 3 || Slots[Slot]) { return Room->FailAvatar(PC,Revision,EHALRoomResult::MissingSpawn); }
		Slots[Slot] = true; if (Slot == P.Seat) { Start = *It; }
	}
	if (!Start) { return Room->FailAvatar(PC,Revision,EHALRoomResult::MissingSpawn); }
	if (PS->GetVehicleLifecycle().ChangeSerial == MAX_uint32) { return Room->FailAvatar(PC,Revision,EHALRoomResult::SpawnFailed); }
	const FTransform Pose(Start->GetActorRotation(),Start->GetActorLocation());
	auto* Next = GetWorld()->SpawnActorDeferred<ATestVehiclePawn>(Class,Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Next) { return Room->FailAvatar(PC,Revision,EHALRoomResult::SpawnFailed); }
	Next->FinishSpawning(Pose);
	// Definition initializes the real convex body during FinishSpawning. Test that body, not a guessed capsule/CDO bound.
	EHALRoomResult Failure = EHALRoomResult::Accepted;
	if (!Next->IsConfigurationValid()) { Failure = EHALRoomResult::InvalidVehicle; }
	else if (SpawnBodyBlocked(Next)) { Failure = EHALRoomResult::SpawnBlocked; }
	else if (PS->GetRoomData().SelectionRevision != Revision || PS->GetRoomData().Phase != EHALRoomPlayerPhase::PreparingAvatar)
	{ Failure = EHALRoomResult::StaleSelection; }
	else
	{
		auto* Old = Cast<ATestVehiclePawn>(PC->GetPawn());
		if (!PS->ChangeVehicleAvatar(Next,PS->GetVehicleLifecycle().ChangeSerial+1))
		{
			auto* ASC=PS->GetHALAbilitySystemComponent(); FHALVehicleLifecycleSeed Seed;
			UE_LOG(LogTemp,Log,TEXT("Practice lifecycle refused: Owner=%s Avatar=%s Current=%s CandidateController=%s Seed=%d RoomCandidate=%d Serial=%u"),
				*GetNameSafe(ASC->GetOwnerActor()),*GetNameSafe(ASC->GetAvatarActor()),*GetNameSafe(PC->GetPawn()),*GetNameSafe(Next->GetController()),Next->GetLifecycleSeed(Seed),Room->CanInitializeAvatar(PC,Next),PS->GetVehicleLifecycle().ChangeSerial);
			Failure = EHALRoomResult::SpawnFailed;
		}
		else
		{
			if (Old) { Old->Destroy(); }
			PC->StartSpot = Start;
			Failure = Room->CompleteAvatar(PC,Revision);
			if (Failure == EHALRoomResult::Accepted)
			{
				UE_LOG(LogTemp,Log,TEXT("Practice Avatar ready: Seat=%d Vehicle=%s Pawn=%s Serial=%u"),P.Seat,*P.VehicleId.ToString(),*GetNameSafe(Next),PS->GetVehicleLifecycle().ChangeSerial);
				return Failure;
			}
		}
	}
	if (PC->GetPawn() == Next) { RemovePracticeAvatar(PC); }
	else { Next->Destroy(); }
	UE_LOG(LogTemp,Log,TEXT("Practice Avatar rejected: Seat=%d Vehicle=%s Result=%d"),P.Seat,*P.VehicleId.ToString(),int32(Failure));
	return Room->FailAvatar(PC,Revision,Failure);
}
EHALRoomResult AHALPracticeRoomGameMode::HandleRoomAction(AHALPlayerController* PC, EHALRoomAction Action, FName Id, uint32 Revision)
{
	if (!HasAuthority() || bReturningToEntry) { return EHALRoomResult::WrongPhase; }
	const auto Result = Super::HandleRoomAction(PC,Action,Id,Revision);
	if (Result != EHALRoomResult::Accepted) { return Result; }
	if (Action == EHALRoomAction::ConfirmVehicle)
	{ return SpawnSelectedVehicle(PC,PC->GetPlayerState<AHALPlayerState>()->GetRoomData().SelectionRevision); }
	if (Action == EHALRoomAction::CancelConfirmation) { RemovePracticeAvatar(PC); }
	return Result;
}
void AHALPracticeRoomGameMode::ReturnRoomToEntry(AHALPlayerController* Host)
{
	auto* S = GetGameState<AHALMatchGameState>();
	if (!HasAuthority() || bReturningToEntry || !S || !S->GetRoom()->IsEnabled()
		|| !Host || S->GetRoom()->GetData().Host != Host->GetPlayerState<AHALPlayerState>()) { return; }
	bReturningToEntry = true;
	S->GetRoom()->CloseForReturn();
	const FText Reason = FText::FromString(TEXT("Host closed the room."));
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		auto* PC = Cast<AHALPlayerController>(It->Get());
		if (PC && PC != Host) { PC->ClientReturnToMainMenuWithTextReason(Reason); }
	}
	FTimerHandle ExitTimer;
	GetWorldTimerManager().SetTimer(ExitTimer,FTimerDelegate::CreateWeakLambda(this,[this,Reason]
	{
		if (auto* GI = GetGameInstance<UHALNetworkGameInstance>()) { GI->ReturnToEntry(Reason); }
	}),.5f,false);
}
void AHALPracticeRoomGameMode::Logout(AController* Exiting)
{
	auto* PC = Cast<AHALPlayerController>(Exiting);
	auto* S = GetGameState<AHALMatchGameState>();
	if (S && S->GetRoom()->GetData().Host == (PC ? PC->GetPlayerState<AHALPlayerState>() : nullptr)) { ReturnRoomToEntry(PC); }
	if (auto* Pawn = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr) { Pawn->RetireForVehicleTransition(); }
	Super::Logout(Exiting);
}
