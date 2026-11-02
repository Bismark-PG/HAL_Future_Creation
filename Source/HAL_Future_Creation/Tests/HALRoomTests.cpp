#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALRoomTestSupport.h"
#include "../HALGameFlowPermissions.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALAbilitySystemComponent.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	struct FRoomWorld
	{
		UWorld* World;
		AHALMatchGameState* Match;
		UHALRoomComponent* Room;
		FRoomWorld()
		{
			const auto V = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&V);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Match = World->SpawnActor<AHALMatchGameState>(); World->SetGameState(Match); Room = Match->GetRoom();
		}
		AHALPlayerController* Player()
		{
			auto* PC = World->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
			auto* PS = World->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS);
			PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay(); return PC;
		}
		ATestVehiclePawn* Avatar(AHALPlayerController* PC)
		{
			auto* Class = LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Pawn = World->SpawnActor<ATestVehiclePawn>(Class,FVector(0,0,1000),FRotator::ZeroRotator,P);
			Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents(); PC->Possess(Pawn); Pawn->DispatchBeginPlay(); return Pawn;
		}
		EHALRoomResult Act(AHALPlayerController* PC, EHALRoomAction Action, FName Id = NAME_None)
		{ return Room->Request(PC,Action,Id,PC->GetPlayerState<AHALPlayerState>()->GetRoomData().SelectionRevision); }
		ATestVehiclePawn* Ready(AHALPlayerController* PC)
		{
			Room->AddMember(PC); Act(PC,EHALRoomAction::SelectVehicle,TEXT("Car_01")); Act(PC,EHALRoomAction::ConfirmVehicle);
			auto* Pawn = Avatar(PC); Room->CompleteAvatar(PC,PC->GetPlayerState<AHALPlayerState>()->GetRoomData().SelectionRevision); return Pawn;
		}
		~FRoomWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomCatalogTest,"HAL.FutureCreation.GameFlow.CatalogAndExplicitRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALRoomCatalogTest::RunTest(const FString&)
{
	auto* C = FHALRoomTestAccess::Config(GetTransientPackage(),EHALRoomProfile::Exhibition); FString Error;
	TestTrue(TEXT("Existing BP and Definition valid without display icon/name"),C->Validate(Error));
	TestEqual(TEXT("Exhibition requires four including host"),C->Rules.MinimumPlayers(),4);
	C->Rules.Profile = EHALRoomProfile::Development; TestEqual(TEXT("Development requires one"),C->Rules.MinimumPlayers(),1);
	TestEqual(TEXT("Both cap at four"),C->Rules.MaximumPlayers(),4);
	C->Rules.Profile = static_cast<EHALRoomProfile>(255); TestFalse(TEXT("Unknown profile rejected"),C->Validate(Error));
	C->Rules.Profile = EHALRoomProfile::Development;
	const auto Duplicate = C->Catalog->Vehicles[0]; C->Catalog->Vehicles.Add(Duplicate); C->Catalog->Vehicles.Last().VehicleId = TEXT("car_01");
	TestFalse(TEXT("Duplicate case-insensitive IDs rejected"),C->Validate(Error)); C->Catalog->Vehicles.Pop();
	TestNull(TEXT("Client asset path is not an allowed ID"),C->Catalog->Resolve(TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn"),Error));
	C->Catalog->Vehicles[0].VehicleClass = ATestVehiclePawn::StaticClass();
	TestFalse(TEXT("Unconfigured native class rejected"),C->Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomSeatTest,"HAL.FutureCreation.GameFlow.SeatsOwnershipAndCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALRoomSeatTest::RunTest(const FString&)
{
	FRoomWorld F; TestTrue(TEXT("Old rooms dormant"),!F.Room->IsEnabled());
	TestEqual(TEXT("Opt-in initialization"),F.Room->Initialize(FHALRoomTestAccess::Config(F.Room,EHALRoomProfile::Development)),EHALRoomResult::Accepted);
	TArray<AHALPlayerController*> PCs;
	for (int32 I = 0; I < 4; ++I)
	{
		auto* PC = F.Player(); PCs.Add(PC); TestEqual(TEXT("Unique seat accepted"),F.Room->AddMember(PC),EHALRoomResult::Accepted);
		const auto& Data = PC->GetPlayerState<AHALPlayerState>()->GetRoomData();
		TestEqual(TEXT("Deterministic seat"),Data.Seat,I); TestEqual(TEXT("Display number"),Data.Number,I+1);
		TestEqual(TEXT("Same model allowed for all"),F.Act(PC,EHALRoomAction::SelectVehicle,TEXT("Car_01")),EHALRoomResult::Accepted);
	}
	auto* Fifth = F.Player(); TestEqual(TEXT("Capacity hard cap"),F.Room->AddMember(Fifth),EHALRoomResult::Full);
	TestEqual(TEXT("Nonmember cannot target someone else"),F.Room->Request(Fifth,EHALRoomAction::SelectVehicle,TEXT("Car_01"),2),EHALRoomResult::NotMember);
	auto* PS = PCs[1]->GetPlayerState<AHALPlayerState>(); const auto Original = PS->GetRoomData();
	F.Act(PCs[1],EHALRoomAction::ConfirmVehicle); TestFalse(TEXT("Confirmation does not create avatar"),PCs[1]->GetPawn()!=nullptr);
	TestEqual(TEXT("Cannot declare ready without actual Avatar/ASC"),F.Room->CompleteAvatar(PCs[1],PS->GetRoomData().SelectionRevision),EHALRoomResult::NotReady);
	F.Act(PCs[1],EHALRoomAction::CancelConfirmation);
	TestEqual(TEXT("Cancel returns to selection"),PS->GetRoomData().Phase,EHALRoomPlayerPhase::Selecting);
	TestTrue(TEXT("Color and number survive cancel"),PS->GetRoomData().Color == Original.Color && PS->GetRoomData().Number == Original.Number);
	F.Room->RemoveMember(PCs[2]); TestEqual(TEXT("Open room frees seat"),F.Room->AddMember(Fifth),EHALRoomResult::Accepted);
	TestEqual(TEXT("Only freed seat reused"),Fifth->GetPlayerState<AHALPlayerState>()->GetRoomData().Seat,2);
	PCs[3]->GetPlayerState<AHALPlayerState>()->Destroy(); F.Room->RemoveMember(PCs[3]);
	TestEqual(TEXT("PlayerState EndPlay cannot leak an occupied seat"),F.Room->GetData().Members.Num(),3);
	F.Room->RemoveMember(PCs[0]); TestEqual(TEXT("Host exit signals recovery; no migration"),F.Room->GetData().Phase,EHALRoomPhase::Returning);
	TestFalse(TEXT("Recovery closes admission"),F.Room->CanAdmitPlayer()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomStaleTest,"HAL.FutureCreation.GameFlow.RequestVersionsAndInvalidStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALRoomStaleTest::RunTest(const FString&)
{
	FRoomWorld F; F.Room->Initialize(FHALRoomTestAccess::Config(F.Room,EHALRoomProfile::Development)); auto* PC = F.Player(); F.Room->AddMember(PC);
	TestEqual(TEXT("Invalid ID rejected"),F.Act(PC,EHALRoomAction::SelectVehicle,TEXT("Invalid")),EHALRoomResult::InvalidVehicle);
	F.Act(PC,EHALRoomAction::SelectVehicle,TEXT("Car_01"));
	TestEqual(TEXT("Old revision rejected"),F.Room->Request(PC,EHALRoomAction::ConfirmVehicle,NAME_None,1),EHALRoomResult::StaleSelection);
	TestEqual(TEXT("Unknown action rejected"),F.Act(PC,static_cast<EHALRoomAction>(255)),EHALRoomResult::InvalidRequest);
	TestEqual(TEXT("Not ready cannot start"),F.Act(PC,EHALRoomAction::Start),EHALRoomResult::NotReady);
	TestEqual(TEXT("Rejected start does not lock"),F.Room->GetData().Phase,EHALRoomPhase::Open);
	F.Act(PC,EHALRoomAction::ConfirmVehicle); const uint32 Preparing = PC->GetPlayerState<AHALPlayerState>()->GetRoomData().SelectionRevision;
	F.Act(PC,EHALRoomAction::CancelConfirmation);
	TestTrue(TEXT("Late ready after cancel cannot promote player"),F.Room->CompleteAvatar(PC,Preparing)!=EHALRoomResult::Accepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomPermissionsTest,"HAL.FutureCreation.GameFlow.AvatarPermissionsAndPhysicsSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALRoomPermissionsTest::RunTest(const FString&)
{
	FRoomWorld F; auto* PC = F.Player(); auto* Pawn = F.Avatar(PC);
	auto Old = HALGameFlow::Permissions(F.World,Pawn);
	TestTrue(TEXT("Legacy waiting driving/ball unchanged"),Old.bDrive && Old.bBallControl);
	TestFalse(TEXT("Legacy waiting ability rejected"),Old.bAbilities);
	F.Room->Initialize(FHALRoomTestAccess::Config(F.Room,EHALRoomProfile::Development)); F.Room->AddMember(PC);
	TestFalse(TEXT("Selecting cannot drive actual existing pawn"),HALGameFlow::Permissions(F.World,Pawn).bDrive);
	auto* Bridge = Pawn->FindComponentByClass<UHALMovementEffectBridge>();
	TestTrue(TEXT("Selection gate queues through existing authoritative physics snapshot"),FHALRoomTestAccess::Flush(Bridge).bControlsBlocked);
	F.Act(PC,EHALRoomAction::SelectVehicle,TEXT("Car_01")); F.Act(PC,EHALRoomAction::ConfirmVehicle);
	TestEqual(TEXT("Authority verifies actual matching Avatar/ASC"),F.Room->CompleteAvatar(PC,PC->GetPlayerState<AHALPlayerState>()->GetRoomData().SelectionRevision),EHALRoomResult::Accepted);
	const auto P = HALGameFlow::Permissions(F.World,Pawn);
	TestTrue(TEXT("Practice actions independently permitted"),P.bDrive && P.bBallControl && P.bAbilities && P.bRecovery && P.bObserve && P.bWorldInteractions);
	TestFalse(TEXT("Practice damage policy query is false; resolver integration is GF-2"),P.bDamage);
	TestTrue(TEXT("No simulated frame: preserve previous publication until physics acknowledges it"),FHALRoomTestAccess::Flush(Bridge).bControlsBlocked);
	TestTrue(TEXT("Ready unblock remains pending for the next actual authority frame"),FHALRoomTestAccess::HasPendingPublication(Bridge));
	F.Act(PC,EHALRoomAction::CancelConfirmation);
	TestTrue(TEXT("Cancel blocks even before future GF-3 destroys old pawn"),FHALRoomTestAccess::Flush(Bridge).bControlsBlocked);
	F.Act(PC,EHALRoomAction::ConfirmVehicle);
	F.Room->CompleteAvatar(PC,PC->GetPlayerState<AHALPlayerState>()->GetRoomData().SelectionRevision);
	auto* Replacement = F.Avatar(PC);
	TestFalse(TEXT("Replacement Avatar never inherits previous readiness"),HALGameFlow::Permissions(F.World,Replacement).bDrive);
	TestEqual(TEXT("Replaced Avatar invalidates start qualification"),F.Room->CheckStart(PC),EHALRoomResult::NotReady);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomLockTest,"HAL.FutureCreation.GameFlow.FrozenRosterAndPhasePermissions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALRoomLockTest::RunTest(const FString&)
{
	FRoomWorld F; F.Room->Initialize(FHALRoomTestAccess::Config(F.Room,EHALRoomProfile::Development));
	auto* Host = F.Player(); auto* Client = F.Player(); auto* Pawn = F.Ready(Host); F.Ready(Client);
	TestEqual(TEXT("Nonhost cannot lock"),F.Act(Client,EHALRoomAction::Start),EHALRoomResult::NotHost);
	TestEqual(TEXT("Host atomically locks"),F.Act(Host,EHALRoomAction::Start),EHALRoomResult::Accepted);
	TestEqual(TEXT("Frozen roster contains both players"),F.Room->GetData().LockedRoster.Num(),2);
	TestEqual(TEXT("First round has nonzero identity"),F.Room->GetData().RoundId,1u);
	TestFalse(TEXT("Joining closed"),F.Room->CanAdmitPlayer());
	TestEqual(TEXT("Cancel after lock rejected"),F.Act(Client,EHALRoomAction::CancelConfirmation),EHALRoomResult::WrongPhase);
	TestFalse(TEXT("Locked forbids driving"),HALGameFlow::Permissions(F.World,Pawn).bDrive);
	TestFalse(TEXT("Locked forbids interaction settlement"),HALGameFlow::Permissions(F.World).bWorldInteractions);
	TestEqual(TEXT("No phase skipping"),F.Room->AdvanceRound(1,EHALRoomPhase::Playing),EHALRoomResult::WrongPhase);
	TestEqual(TEXT("Old round message rejected"),F.Room->AdvanceRound(9,EHALRoomPhase::Loading),EHALRoomResult::StaleSelection);
	F.Room->AdvanceRound(1,EHALRoomPhase::Loading); F.Room->AdvanceRound(1,EHALRoomPhase::Countdown);
	TestFalse(TEXT("Countdown remains gated"),HALGameFlow::Permissions(F.World,Pawn).bAbilities);
	F.Room->AdvanceRound(1,EHALRoomPhase::Playing);
	TestTrue(TEXT("Playing enables policy and own actions"),HALGameFlow::Permissions(F.World,Pawn).bDrive && HALGameFlow::Permissions(F.World).bDamage);
	F.Room->RemoveMember(Client); TestEqual(TEXT("Locked/playing disconnect requests recovery"),F.Room->GetData().Phase,EHALRoomPhase::Returning);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomFourTest,"HAL.FutureCreation.GameFlow.ExhibitionFourAndDevelopmentOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALRoomFourTest::RunTest(const FString&)
{
	{
		FRoomWorld F; F.Room->Initialize(FHALRoomTestAccess::Config(F.Room,EHALRoomProfile::Exhibition)); auto* Host = F.Player(); F.Ready(Host);
		TestEqual(TEXT("Exhibition cannot silently become solo"),F.Room->CheckStart(Host),EHALRoomResult::NotReady);
		for (int32 I = 0; I < 3; ++I) { F.Ready(F.Player()); }
		TestEqual(TEXT("Exhibition four actual initialized avatars"),F.Room->CheckStart(Host),EHALRoomResult::Accepted);
	}
	{
		FRoomWorld F; F.Room->Initialize(FHALRoomTestAccess::Config(F.Room,EHALRoomProfile::Development)); auto* Host = F.Player(); F.Ready(Host);
		TestEqual(TEXT("Explicit development solo starts in Standalone native world"),F.Act(Host,EHALRoomAction::Start),EHALRoomResult::Accepted);
	}
	return true;
}
#endif
