#include "HALVehicleTestGameMode.h"

#include "HALVehicleTestPlayerController.h"
#include "HALMatchGameState.h"
#include "HALPlayerState.h"
#include "HALAbilitySystemComponent.h"
#include "HALVehicleLifecycleTypes.h"
#include "TestVehiclePawn.h"
#include "PassiveTestVehicle.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

DEFINE_LOG_CATEGORY_STATIC(LogHALVehicleTest, Log, All);

bool AHALVehicleTestGameMode::IsTestWorld(const UWorld* World)
{ return World && World->GetAuthGameMode<AHALVehicleTestGameMode>()!=nullptr; }

AHALVehicleTestGameMode::AHALVehicleTestGameMode()
{
	GameStateClass=AHALMatchGameState::StaticClass();
	PlayerStateClass=AHALPlayerState::StaticClass();
	PlayerControllerClass=AHALVehicleTestPlayerController::StaticClass();
	DefaultPawnClass=nullptr; // BP_VehicleTestGameMode selects the tuned Car_02 Blueprint.
	bUseSeamlessTravel=false;
}

void AHALVehicleTestGameMode::InitGameState()
{
	Super::InitGameState();
	if (auto* State=GetGameState<AHALMatchGameState>()) { State->SetMatchPhase(EHALMatchPhase::Playing); }
	TestStatus=TEXT("Local test: Playing / no time limit");
}

void AHALVehicleTestGameMode::PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& Id,FString& Error)
{
	Super::PreLogin(Options,Address,Id,Error);
	if (!Address.IsEmpty()) { Error=TEXT("SingleVehicleTest accepts only its local player. Use Play Standalone."); }
}

TArray<TSubclassOf<ATestVehiclePawn>> AHALVehicleTestGameMode::GetTestVehicleClasses() const
{
	TArray<TSubclassOf<ATestVehiclePawn>> Result;
	const auto Add=[&](UClass* Class)
	{
		if (Class && Class->IsChildOf(ATestVehiclePawn::StaticClass())
			&& !Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)) { Result.AddUnique(Class); }
	};
	Add(DefaultPawnClass);
	for (const auto& Class:TestVehicleClasses) { Add(Class); }
	return Result;
}

void AHALVehicleTestGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	auto* PC=Cast<AHALVehicleTestPlayerController>(NewPlayer);
	if (!PC || (TestController.IsValid() && TestController.Get()!=PC))
	{
		TestStatus=TEXT("Use HALVehicleTestPlayerController and exactly one local player.");
		UE_LOG(LogHALVehicleTest,Error,TEXT("%s"),*TestStatus);
		return;
	}
	TestController=PC;
	ChangeTestVehicle(PC,DefaultPawnClass.Get());
}

bool AHALVehicleTestGameMode::ChangeTestVehicle(AHALVehicleTestPlayerController* PC,TSubclassOf<ATestVehiclePawn> Class)
{
	const auto Fail=[&](const FString& Reason)
	{ TestStatus=Reason; UE_LOG(LogHALVehicleTest,Warning,TEXT("%s"),*Reason); return false; };
	if (!HasAuthority() || !PC || PC->GetWorld()!=GetWorld() || !PC->IsLocalController()
		|| (TestController.IsValid() && TestController.Get()!=PC)) { return Fail(TEXT("Vehicle test requires its local authoritative player.")); }
	const auto Classes=GetTestVehicleClasses();
	if (!Class || !Classes.Contains(Class)) { return Fail(TEXT("Set Default Pawn Class to BP_VehiclePawn_Car_02; add alternatives to Test Vehicle Classes.")); }
	auto* PS=PC->GetPlayerState<AHALPlayerState>();
	if (!PS || PS->GetVehicleLifecycle().ChangeSerial==MAX_uint32) { return Fail(TEXT("HALPlayerState or a valid vehicle lifecycle serial is unavailable.")); }
	AActor* Start=FindPlayerStart(PC);
	if (!Start) { return Fail(TEXT("Add a PlayerStart to SingleVehicleTest.")); }
	const FTransform Pose(Start->GetActorRotation(),Start->GetActorLocation());
	auto* Next=GetWorld()->SpawnActorDeferred<ATestVehiclePawn>(Class,Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Next) { return Fail(TEXT("Could not spawn the selected vehicle.")); }
	Next->FinishSpawning(Pose);
	FHALVehicleLifecycleSeed Seed;
	FString Error;
	auto* Old=Cast<ATestVehiclePawn>(PC->GetPawn());
	if (!Next->GetLifecycleSeed(Seed) || !PS->ValidateVehicleAbilities(Next,Error,Old))
	{ Next->Destroy(); return Fail(TEXT("Selected vehicle configuration/abilities are invalid. ")+Error); }

	// Inspect the actual configured Chaos body. Copied test vehicles are retired after a successful swap.
	auto* Body=Cast<UPrimitiveComponent>(Next->GetRootComponent());
	FComponentQueryParams Params(SCENE_QUERY_STAT(HALVehicleTestSpawn),Next);
	for (TActorIterator<ATestVehiclePawn> It(GetWorld());It;++It) { Params.AddIgnoredActor(*It); }
	for (TActorIterator<APassiveTestVehicle> It(GetWorld());It;++It) { Params.AddIgnoredActor(*It); }
	TArray<FOverlapResult> Hits;
	if (!Body || !Body->IsQueryCollisionEnabled()) { Next->Destroy(); return Fail(TEXT("Selected vehicle has no queryable physics body.")); }
	GetWorld()->ComponentOverlapMultiByChannel(Hits,Body,Body->GetComponentLocation(),Body->GetComponentQuat(),Body->GetCollisionObjectType(),Params);
	if (Hits.ContainsByPredicate([](const FOverlapResult& Hit){ return Hit.bBlockingHit; }))
	{ Next->Destroy(); return Fail(TEXT("PlayerStart is blocked. Raise it above the floor or clear surrounding geometry.")); }
	PC->FinishCameraNavigation();
	if (!PS->ChangeVehicleAvatar(Next,PS->GetVehicleLifecycle().ChangeSerial+1))
	{ Next->Destroy(); return Fail(TEXT("Vehicle lifecycle transition failed. Select again or restart Play; see Output Log.")); }
	PC->StartSpot=Start;
	TestController=PC;
	TArray<AActor*> Retire;
	for (TActorIterator<ATestVehiclePawn> It(GetWorld());It;++It) { if (*It!=Next) { It->RetireForVehicleTransition(); Retire.Add(*It); } }
	for (TActorIterator<APassiveTestVehicle> It(GetWorld());It;++It) { Retire.Add(*It); }
	for (auto* Actor:Retire) { Actor->Destroy(); }
	PC->RefreshTestView();
	TestStatus=FString::Printf(TEXT("Playing / %s / GAS ready / no time limit"),*Class->GetName());
	UE_LOG(LogHALVehicleTest,Log,TEXT("%s"),*TestStatus);
	return true;
}
