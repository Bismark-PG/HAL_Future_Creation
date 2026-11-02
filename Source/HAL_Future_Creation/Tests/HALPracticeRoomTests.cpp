#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALPracticeRoomGameMode.h"
#include "../HALRoomConfig.h"
#include "../HALRoomComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALPlayerStart.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALGameFlowPermissions.h"
#include "../TestVehiclePawn.h"
#include "../VehicleHealthComponent.h"
#include "../VehicleDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Components/BoxComponent.h"

namespace
{
	struct FPracticeWorld
	{
		UWorld* World; AHALPracticeRoomGameMode* Mode; UHALRoomComponent* Room;
		FPracticeWorld()
		{
			const auto V=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&V);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			FURL URL; URL.AddOption(TEXT("game=/Script/HAL_Future_Creation.HALPracticeRoomGameMode"));
			URL.AddOption(TEXT("RoomConfig=/Game/Data/Gameflow/DA_Room_Development.DA_Room_Development"));
			World->SetGameMode(URL); World->InitializeActorsForPlay(URL);
			Mode=World->GetAuthGameMode<AHALPracticeRoomGameMode>(); Room=World->GetGameState<AHALMatchGameState>()->GetRoom();
		}
		AHALPlayerController* Player()
		{
			auto* PC=World->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
			Room->AddMember(PC); return PC;
		}
		AHALPlayerStart* Start(int32 Slot)
		{
			auto* S=World->SpawnActor<AHALPlayerStart>(FVector(Slot*1800,0,1000),FRotator::ZeroRotator); S->SpawnSlot=Slot; return S;
		}
		EHALRoomResult Act(AHALPlayerController* PC,EHALRoomAction A,FName Id=NAME_None)
		{ return Mode->HandleRoomAction(PC,A,Id,PC->GetPlayerState<AHALPlayerState>()->GetRoomData().SelectionRevision); }
		~FPracticeWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPracticeSelectionTest,"HAL.FutureCreation.GameFlow.GF3.SelectedSpawnCancelAndReselect",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALPracticeSelectionTest::RunTest(const FString&)
{
	FPracticeWorld F; F.Start(0); auto* PC=F.Player(); auto* PS=PC->GetPlayerState<AHALPlayerState>();
	TestTrue(TEXT("Saved Development profile corrected and production mode enabled"),F.Room->GetData().bPracticeRoom && F.Room->GetData().Rules.Profile==EHALRoomProfile::Development);
	TestNull(TEXT("Connection/registration creates no driving Pawn"),PC->GetPawn());
	TestEqual(TEXT("Engine initialized ASC to owner-only PlayerState before first car"),PS->GetHALAbilitySystemComponent()->GetAvatarActor(),static_cast<AActor*>(PS));
	const auto Identity=PS->GetRoomData();
	for (const FName Id : { FName(TEXT("Car_01")),FName(TEXT("Car_02")),FName(TEXT("Car_01")) })
	{
		TestEqual(TEXT("Server allowlisted selection"),F.Act(PC,EHALRoomAction::SelectVehicle,Id),EHALRoomResult::Accepted);
		const uint32 ConfirmRevision=PS->GetRoomData().SelectionRevision;
		TestEqual(TEXT("Production confirm spawns, binds, and completes"),F.Act(PC,EHALRoomAction::ConfirmVehicle),EHALRoomResult::Accepted);
		auto* Pawn=Cast<ATestVehiclePawn>(PC->GetPawn()); if (!TestNotNull(TEXT("Actual selected vehicle"),Pawn)) { return false; }
		TestEqual(TEXT("Correct model Definition seeds HP"),PS->GetHALAbilitySystemComponent()->GetSet<UHALHealthAttributeSet>()->GetMaxHP(),Pawn->GetVehicleDefinition()->Health.Health.MaxHP);
		TestTrue(TEXT("Practice is permitted; HP damage is independently blocked"),HALGameFlow::Permissions(F.World,Pawn).bDrive && !HALGameFlow::Permissions(F.World,Pawn).bDamage);
		const auto Serial=PS->GetVehicleLifecycle().ChangeSerial;
		TestEqual(TEXT("Old confirm cannot produce another vehicle"),F.Mode->HandleRoomAction(PC,EHALRoomAction::ConfirmVehicle,NAME_None,ConfirmRevision),EHALRoomResult::StaleSelection);
		TestTrue(TEXT("Duplicate request preserves Pawn and lifecycle serial"),PC->GetPawn()==Pawn && PS->GetVehicleLifecycle().ChangeSerial==Serial);
		TestEqual(TEXT("Cancel revokes readiness before retiring Pawn"),F.Act(PC,EHALRoomAction::CancelConfirmation),EHALRoomResult::Accepted);
		TestNull(TEXT("Cancel returns to non-driving selection"),PC->GetPawn());
		TestTrue(TEXT("Old Pawn retired and destroyed"),Pawn->IsActorBeingDestroyed() && !Pawn->IsConfigurationValid());
		TestNull(TEXT("ASC no longer binds old Avatar"),PS->GetHALAbilitySystemComponent()->GetAvatarActor());
		TestTrue(TEXT("Stable room identity across models"),PS->GetRoomData().Seat==Identity.Seat && PS->GetRoomData().Color==Identity.Color);
	}
	TestEqual(TEXT("Three fresh lifecycle operations"),PS->GetVehicleLifecycle().ChangeSerial,3u);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPracticeFailureTest,"HAL.FutureCreation.GameFlow.GF3.SpawnFailuresStayRetryable",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALPracticeFailureTest::RunTest(const FString&)
{
	FPracticeWorld F; auto* PC=F.Player(); auto* PS=PC->GetPlayerState<AHALPlayerState>();
	TestEqual(TEXT("Invalid ID cannot cause a spawn"),F.Act(PC,EHALRoomAction::SelectVehicle,TEXT("Unknown")),EHALRoomResult::InvalidVehicle);
	F.Act(PC,EHALRoomAction::SelectVehicle,TEXT("Car_01"));
	TestEqual(TEXT("Missing authored seat rejected"),F.Act(PC,EHALRoomAction::ConfirmVehicle),EHALRoomResult::MissingSpawn);
	TestTrue(TEXT("Failure retains selection, clears readiness"),PS->GetRoomData().Phase==EHALRoomPlayerPhase::Selecting && !PC->GetPawn() && PS->GetRoomData().AvatarResult==EHALRoomResult::MissingSpawn);
	F.Start(0);
	auto* Blocker=F.World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Blocker);
	Blocker->SetRootComponent(Box); Box->SetBoxExtent(FVector(500)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Blocker->SetActorLocation(FVector(0,0,1000));
	TestEqual(TEXT("Actual configured convex body cannot spawn inside solid"),F.Act(PC,EHALRoomAction::ConfirmVehicle),EHALRoomResult::SpawnBlocked);
	TestNull(TEXT("Blocked candidate does not become Pawn"),PC->GetPawn());
	TestEqual(TEXT("Blocked candidate does not consume lifecycle serial"),PS->GetVehicleLifecycle().ChangeSerial,0u);
	Blocker->Destroy();
	auto* Foreign=F.World->SpawnActor<AActor>(); PS->GetHALAbilitySystemComponent()->InitAbilityActorInfo(PS,Foreign);
	TestEqual(TEXT("Foreign initial ASC Avatar still rejects transition"),F.Act(PC,EHALRoomAction::ConfirmVehicle),EHALRoomResult::SpawnFailed);
	PS->GetHALAbilitySystemComponent()->InitAbilityActorInfo(PS,PS); Foreign->Destroy();
	TestEqual(TEXT("Same selection can retry after clearing obstacle"),F.Act(PC,EHALRoomAction::ConfirmVehicle),EHALRoomResult::Accepted);
	TestTrue(TEXT("Retry creates real ready Avatar"),PS->GetRoomData().ReadyAvatar==PC->GetPawn() && PC->GetPawn()!=nullptr);
	const auto Current=PS->GetRoomData().SelectionRevision;
	TestEqual(TEXT("Late failure callback cannot undo success"),F.Room->FailAvatar(PC,Current-1,EHALRoomResult::SpawnFailed),EHALRoomResult::WrongPhase);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPracticeRosterTest,"HAL.FutureCreation.GameFlow.GF3.SameModelsReadinessAndLock",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALPracticeRosterTest::RunTest(const FString&)
{
	FPracticeWorld F; F.Start(0); F.Start(1); auto* Host=F.Player();
	F.Act(Host,EHALRoomAction::SelectVehicle,TEXT("Car_01"));
	if (!TestEqual(TEXT("Host confirmation succeeds"),F.Act(Host,EHALRoomAction::ConfirmVehicle),EHALRoomResult::Accepted)) { return false; }
	auto* Client=F.Player();
	TestTrue(TEXT("Late selection does not stop existing practice"),HALGameFlow::Permissions(F.World,Host->GetPawn()).bDrive && !Client->GetPawn());
	TestEqual(TEXT("Unconfirmed newcomer blocks start"),F.Act(Host,EHALRoomAction::Start),EHALRoomResult::NotReady);
	F.Act(Client,EHALRoomAction::SelectVehicle,TEXT("Car_01"));
	if (!TestEqual(TEXT("Client confirmation succeeds"),F.Act(Client,EHALRoomAction::ConfirmVehicle),EHALRoomResult::Accepted)) { return false; }
	TestTrue(TEXT("Same model has distinct identities and instances"),Host->GetPawn()->GetClass()==Client->GetPawn()->GetClass()
		&& Host->GetPawn()!=Client->GetPawn() && Host->GetPlayerState<AHALPlayerState>()->GetRoomData().Color!=Client->GetPlayerState<AHALPlayerState>()->GetRoomData().Color);
	TestEqual(TEXT("Nonhost cannot lock room"),F.Act(Client,EHALRoomAction::Start),EHALRoomResult::NotHost);
	TestEqual(TEXT("Host locks only truly ready roster"),F.Act(Host,EHALRoomAction::Start),EHALRoomResult::Accepted);
	APawn* Old=Host->GetPawn(); TestEqual(TEXT("Cancel after lock rejected"),F.Act(Host,EHALRoomAction::CancelConfirmation),EHALRoomResult::WrongPhase);
	TestTrue(TEXT("Rejected cancel cannot retire existing vehicle"),Old==Host->GetPawn() && Cast<ATestVehiclePawn>(Old)->IsConfigurationValid());
	TestFalse(TEXT("Late admission closed"),F.Room->CanAdmitPlayer());
	TestFalse(TEXT("Lock reaches physics permission gate"),HALGameFlow::Permissions(F.World,Old).bDrive);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPracticeSingleTest,"HAL.FutureCreation.GameFlow.GF3.SingleHostAndEarlyReturn",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALPracticeSingleTest::RunTest(const FString&)
{
	{
		FPracticeWorld F; F.Start(0); auto* Host=F.Player();
		F.Act(Host,EHALRoomAction::SelectVehicle,TEXT("Car_01"));
		TestEqual(TEXT("Standalone native room confirms one real player"),F.Act(Host,EHALRoomAction::ConfirmVehicle),EHALRoomResult::Accepted);
		TestEqual(TEXT("Development single host may lock without fake players"),F.Act(Host,EHALRoomAction::Start),EHALRoomResult::Accepted);
		TestEqual(TEXT("Exactly one real roster member"),F.Room->GetData().LockedRoster.Num(),1);
	}
	{
		FPracticeWorld F; auto* Host=F.Player();
		TestEqual(TEXT("Before first match round remains zero"),F.Room->GetData().RoundId,0u);
		F.Room->CloseForReturn();
		TestEqual(TEXT("Host may close before any round exists"),F.Room->GetData().Phase,EHALRoomPhase::Returning);
		TestFalse(TEXT("Closing at round zero rejects arrivals"),F.Room->CanAdmitPlayer());
		TestEqual(TEXT("Closing rejects later confirmation"),F.Act(Host,EHALRoomAction::ConfirmVehicle),EHALRoomResult::WrongPhase);
	}
	return true;
}
#endif
