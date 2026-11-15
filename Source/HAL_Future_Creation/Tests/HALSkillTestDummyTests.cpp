#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "../HALSkillTestDummy.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALMovementAreaReceiver.h"
#include "../HALMovementAttributeSet.h"
#include "../HALGameFlowPermissions.h"
#include "../HALPolicePursuit.h"
#include "../HALPolicePursuitAbility.h"
#include "../HALPursuitTargetComponent.h"
#include "../HALPursuitHUDWidget.h"
#include "../HALTornado.h"
#include "../HALTornadoAbility.h"
#include "../HALSlipperyTrail.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../VehicleHealthComponent.h"
#include "../CombatResolver.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../HALVehicleTestGameMode.h"
#include "../HALVehicleTestPlayerController.h"
#include "../HALPlayerStart.h"
#include "../PassiveTestVehicle.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"

struct FHALSkillTestDummyTestAccess
{
	static void PursuitStep(AHALPolicePursuit& P,float Dt) { P.LastScan=P.ServerTime()-Dt; P.Scan(); }
	static void TornadoContact(AHALTornado& T) { T.State.StartServerTime=T.ServerTime()-.7; T.LastAge=.7; T.Contacts(T.State.Ground,T.State.Ground,0); }
	static void TrailReceivers(AHALSlipperyTrail& T) { T.UpdateReceivers(1); }
	static void Grounded(ATestVehiclePawn& D)
	{ auto* M=D.FindComponentByClass<UArcadeVehicleMovementComponent>(); M->PublishedStability.bValid=M->PublishedStability.bGrounded=true; }
};
namespace
{
	void Begin(AActor* A)
	{ if(!A->IsActorInitialized()) { A->PreInitializeComponents(); A->InitializeComponents(); A->PostInitializeComponents(); } A->DispatchBeginPlay(); }
	UHALSkillTestDummyConfig* DummyConfig(UObject* Outer)
	{
		auto* C=NewObject<UHALSkillTestDummyConfig>(Outer);
		auto* D=LoadObject<UVehicleDefinition>(nullptr,TEXT("/Game/Data/MVP_B/DA_Vehicle_Car_02.DA_Vehicle_Car_02"));
		C->VehicleDefinition=DuplicateObject<UVehicleDefinition>(D,C);
		C->VehicleDefinition->Health.Health.MaxHP=1000;
		C->bShowHealth=false; return C;
	}
	AHALSkillTestDummy* Dummy(UWorld* W,FVector At)
	{
		auto* D=W->SpawnActorDeferred<AHALSkillTestDummy>(AHALSkillTestDummy::StaticClass(),FTransform(At),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		FindFProperty<FObjectProperty>(D->GetClass(),TEXT("TestConfig"))->SetObjectPropertyValue_InContainer(D,DummyConfig(D));
		D->FinishSpawning(FTransform(At));
		if(!D->IsActorInitialized()) { D->PreInitializeComponents(); D->InitializeComponents(); D->PostInitializeComponents(); }
		if(!D->HasActorBegunPlay()) { D->DispatchBeginPlay(); } return D;
	}
	struct FWorld
	{
		UWorld* W;
		FWorld()
		{
			const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
			auto* S=W->SpawnActor<AHALMatchGameState>(); W->SetGameState(S); S->SetMatchPhase(EHALMatchPhase::Playing);
		}
		~FWorld() { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); }
		ATestVehiclePawn* Player()
		{
			auto* Class=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_06.BP_VehiclePawn_Car_06_C"));
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* V=W->SpawnActor<ATestVehiclePawn>(Class,FVector(0,0,100),FRotator::ZeroRotator,P);
			auto* D=FHALVehicleAbilityTestAccess::Configure(V,UHALPolicePursuitAbility::StaticClass()); FHALVehicleAbilityTestAccess::RuntimeConfig(V,D->Abilities);
			V->PreInitializeComponents(); V->InitializeComponents(); V->PostInitializeComponents();
			auto* PC=W->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController(); auto* PS=W->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS); PS->SetPlayerId(1);
			PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay(); PS->ChangeVehicleAvatar(V,1); V->DispatchBeginPlay(); return V;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSkillDummyLifecycle,"HAL.FutureCreation.SkillDummy.IndependentGASReceiversAndDamage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSkillDummyLifecycle::RunTest(const FString&)
{
	FWorld F; auto* V=F.Player(); const int32 Players=F.W->GetGameState()->PlayerArray.Num(); auto* D=Dummy(F.W,FVector(500,0,100));
	auto* A=CastChecked<UHALAbilitySystemComponent>(D->GetAbilitySystemComponent()); auto* H=D->FindComponentByClass<UVehicleHealthComponent>();
	TestTrue(TEXT("Configured unpossessed dummy is a valid skill receiver"),D->CanReceiveSkillEffects());
	TestNull(TEXT("No controller"),D->GetController()); TestNull(TEXT("No PlayerState"),D->GetPlayerState()); TestEqual(TEXT("No match participant added"),F.W->GetGameState()->PlayerArray.Num(),Players);
	TestTrue(TEXT("Own ASC avatar and all seeds initialized"),A->GetOwnerActor()==D&&A->GetAvatarActor()==D&&A->IsCombatInitialized()&&A->IsImpactInitialized()&&A->GetSet<UHALMovementAttributeSet>());
	TestEqual(TEXT("Definition HP seed"),H->GetCurrentHP(),1000.f); TestEqual(TEXT("No active grants"),A->GetActivatableAbilities().Num(),0);
	const auto P=HALGameFlow::Permissions(F.W,D); TestFalse(TEXT("Dummy has no cast or auto pickup permission"),P.bAbilities||P.bBallControl); TestTrue(TEXT("Neutral movement receiver stays active"),P.bDrive);
	TestFalse(TEXT("Caster eligibility does not use receiver eligibility"),AHALPolicePursuit::VehicleReady(D));
	FHALAttackSourceSnapshot Snapshot; TestTrue(TEXT("Real player attack snapshot"),FCombatResolver::CaptureVehicleAttack(*V,*V,1,0,Snapshot));
	Snapshot.SourceTag=HALGameplayTags::Combat_Source_Ball;
	FHALCombatAction Action; TestTrue(TEXT("Attack starts"),Action.Begin(*V,Snapshot));
	FHALCombatHitRequest Request; Request.AttackId=Action.GetSnapshot().AttackId; Request.PulseSequence=Action.GetPulseSequence(); Request.bApplyDamage=true; Request.bApplyImpact=false;
	Request.Hit.TargetActor=D; Request.Hit.TargetPhysicsBody=CastChecked<UPrimitiveComponent>(D->GetRootComponent());
	const auto Result=FCombatResolver::ResolveCombatHit(Action,Request); TestTrue(TEXT("Standard resolver damages dummy"),Result.bHitAccepted&&Result.ActualHPRemoved>0); TestTrue(TEXT("HP actually decreased"),H->GetCurrentHP()<1000);
	auto* Region=F.W->SpawnActor<AActor>(); Begin(Region); FHALMovementAreaConfig Area; Area.Kind=EHALMovementAreaKind::Slippery; Area.LateralGripMultiplier=.2f;
	auto* Receiver=D->FindComponentByClass<UHALMovementAreaReceiver>(); TestTrue(TEXT("Actual area receiver registers"),Receiver->RegisterArea(Region,Area));
	TestTrue(TEXT("Area GE changes GAS grip"),A->GetSet<UHALMovementAttributeSet>()->GetAreaGripDelta()<-.7f); Region->Destroy();
	TestEqual(TEXT("Source destruction clears area"),Receiver->GetAreaCount(),0);
	auto* Floor=F.W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Floor); Floor->SetRootComponent(Box); Box->SetBoxExtent(FVector(5000,5000,50)); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionResponseToAllChannels(ECR_Block); Box->SetWorldLocation(FVector(0,0,-50)); Box->RegisterComponent();
	D->SetActorLocation(FVector(500,0,60)); FHALSkillTestDummyTestAccess::Grounded(*D);
	auto* Trail=F.W->SpawnActor<AHALSlipperyTrail>(); Trail->SetOwner(V); Begin(Trail); FHALSlipperyTrailHandling Handling; Handling.LateralGripMultiplier=.2f;
	TestTrue(TEXT("Real trail starts from player source"),Trail->Start({}, {}, Handling,FVector(500,0,0),FVector::UpVector,{}));
	FHALSkillTestDummyTestAccess::TrailReceivers(*Trail); TestTrue(TEXT("Trail membership includes unpossessed dummy"),Receiver->GetAreaCount()>0);
	Trail->Destroy(); TestEqual(TEXT("Trail destruction restores receiver"),Receiver->GetAreaCount(),0);
	auto* Widget=NewObject<UHALSkillTestDummyWidget>(F.W); Widget->SetDummy(D); const auto View=Widget->GetDummyView(); TestTrue(TEXT("WBP/native view exposes authoritative HP"),View.bReady&&View.HP==H->GetCurrentHP());
	TestTrue(TEXT("Shared Stun can affect neutral dummy"),A->ApplyStun(2,V));
	FHALAttackSourceSnapshot Lethal; FCombatResolver::CaptureVehicleAttack(*V,*V,1000,0,Lethal); FHALCombatAction Finish; Finish.Begin(*V,Lethal);
	Request.AttackId=Finish.GetSnapshot().AttackId; Request.PulseSequence=Finish.GetPulseSequence(); FCombatResolver::ResolveCombatHit(Finish,Request);
	TestEqual(TEXT("Defeated dummy keeps readable zero HP"),H->GetCurrentHP(),0.f); TestFalse(TEXT("Defeat clears Avatar Stun"),A->IsStunned()); TestFalse(TEXT("Defeated dummy is no longer a skill target"),D->CanReceiveSkillEffects());
	D->Destroy(); TestNull(TEXT("Destroyed dummy unbinds its ASC avatar"),A->GetAvatarActor()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSkillDummyPursuit,"HAL.FutureCreation.SkillDummy.PursuitStarsArrestAndTornado",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSkillDummyPursuit::RunTest(const FString&)
{
	FWorld F; auto* V=F.Player(); auto* D=Dummy(F.W,FVector(500,0,100)); auto* A=CastChecked<UHALAbilitySystemComponent>(D->GetAbilitySystemComponent());
	auto* Config=NewObject<UHALPolicePursuitConfig>(); auto* P=F.W->SpawnActor<AHALPolicePursuit>(); P->SetOwner(V);
	if(!TestTrue(TEXT("Player police starts"),P->Start(*Config))) { return false; }
	FHALSkillTestDummyTestAccess::PursuitStep(*P,2); const auto Views=UHALPursuitHUDWidget::GatherViews(F.W);
	const auto* View=Views.FindByPredicate([&](const auto& X){return X.Target==D;}); if(!TestNotNull(TEXT("Unpossessed dummy gets six-star view"),View)) { return false; }
	TestEqual(TEXT("Half wanted progress"),View->Fraction,.5f); FHALSkillTestDummyTestAccess::PursuitStep(*P,2);
	TestTrue(TEXT("Actual shared Stun and arrest"),A->IsStunned()&&D->FindComponentByClass<UHALPursuitTargetComponent>()->GetState().bArrested);
	P->Destroy(); TestTrue(TEXT("Caster disappearance keeps accepted arrest"),A->IsStunned());
	auto* T=F.W->SpawnActor<AHALTornado>(); T->SetOwner(V); auto* TC=NewObject<UHALTornadoConfig>(); TC->Rules.Speed=0;
	TestTrue(TEXT("Player tornado starts"),T->Start(*TC,FVector(500,0,0))); FHALSkillTestDummyTestAccess::TornadoContact(*T);
	TestTrue(TEXT("Tornado recognizes dummy as vehicle and applies launch"),T->GetVehicleHitCount()>0);
	D->Destroy(); TestNull(TEXT("Cleanup clears target ASC"),A->GetAvatarActor()); TestTrue(TEXT("No stale target HUD"),UHALPursuitHUDWidget::GatherViews(F.W).IsEmpty()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSkillDummyPreservation,"HAL.FutureCreation.SkillDummy.SingleVehicleSwapPreservesOnlyNewDummy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSkillDummyPreservation::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W); W->SetGameInstance(NewObject<UGameInstance>(GEngine));
	FURL URL; URL.AddOption(TEXT("game=/Script/HAL_Future_Creation.HALVehicleTestGameMode")); W->SetGameMode(URL); W->InitializeActorsForPlay(URL);
	auto* Mode=W->GetAuthGameMode<AHALVehicleTestGameMode>(); auto* Class=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_06.BP_VehiclePawn_Car_06_C")); Mode->DefaultPawnClass=Class;
	W->SpawnActor<AHALPlayerStart>(FVector(0,0,1000),FRotator::ZeroRotator); auto* D=Dummy(W,FVector(1000,0,100)); auto* Passive=W->SpawnActor<APassiveTestVehicle>(LoadClass<APassiveTestVehicle>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_PassiveTestVehicle.BP_PassiveTestVehicle_C")));
	auto* PC=W->SpawnActor<AHALVehicleTestPlayerController>(); PC->SetAsLocalPlayerController(); Mode->HandleStartingNewPlayer(PC);
	TestTrue(TEXT("Player spawned"),PC->GetPawn()!=nullptr); TestFalse(TEXT("New dummy survives initial spawn"),D->IsActorBeingDestroyed()); TestTrue(TEXT("Legacy copied Passive cleanup unchanged"),Passive->IsActorBeingDestroyed());
	TestTrue(TEXT("Swap succeeds"),Mode->ChangeTestVehicle(PC,Class)); TestFalse(TEXT("New dummy survives repeated swaps"),D->IsActorBeingDestroyed());
	W->DestroyWorld(false); GEngine->DestroyWorldContext(W); return true;
}
#endif
