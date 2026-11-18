#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "../HALPolicePursuit.h"
#include "../HALPolicePursuitAbility.h"
#include "../HALPursuitTargetComponent.h"
#include "../HALPursuitHUDWidget.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../VehicleRecoveryComponent.h"
#include "../VehicleHealthComponent.h"
#include "../BasicBallActor.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/ScopeExit.h"
struct FHALPursuitTestAccess
{
	static void Step(AHALPolicePursuit& P,float Dt) { P.LastScan=P.ServerTime()-Dt; P.Scan(); }
	static bool Inside(AHALPolicePursuit& P,ATestVehiclePawn& V,bool Was) { return P.InRange(V,Was); }
	static float Value(AHALPolicePursuit& P,ATestVehiclePawn* V) { const auto* R=P.State.Records.FindByPredicate([&](const auto& X){return X.Target==V;}); return R?R->Value:0; }
	static void Recovery(ATestVehiclePawn& V,bool On) { V.FindComponentByClass<UVehicleRecoveryComponent>()->Status.Phase=On?EVehicleRecoveryPhase::Executing:EVehicleRecoveryPhase::Idle; }
	static void RefreshProtection(UHALPursuitTargetComponent& C) { C.UpdateProtection(); }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPursuitMathTest,"HAL.FutureCreation.Pursuit.CounterAndStars",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALPursuitMathTest::RunTest(const FString&)
{
	FHALPursuitRules R; TestTrue(TEXT("Approved defaults valid"),R.IsValid());
	float V=R.Integrate(0,2,true,false,false); TestEqual(TEXT("Two seconds accrue fifty"),V,50.f);
	V=R.Integrate(V,.5,false,false,false); TestEqual(TEXT("Exit decays without clearing"),V,40.f);
	TestEqual(TEXT("Reentry resumes"),R.Integrate(V,1,true,false,false),65.f);
	TestEqual(TEXT("Stunned target in range holds"),R.Integrate(65,1,true,true,false),65.f);
	TestEqual(TEXT("Protected target decays even inside"),R.Integrate(65,1,true,true,true),45.f);
	FHALPursuitView View; View.Fraction=.25f;
	TestEqual(TEXT("First star full, left to right"),View.StarFill(0),1.f); TestEqual(TEXT("Second star partially filled"),View.StarFill(1),.5f); TestEqual(TEXT("Third still empty"),View.StarFill(2),0.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPursuitAuthorityTest,"HAL.FutureCreation.Pursuit.AuthorityGASLifecycleArrestProtection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALPursuitAuthorityTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	auto* Ability=GetMutableDefault<UHALPolicePursuitAbility>(); auto* Old=Ability->SkillConfig.Get();
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); Ability->SkillConfig=Old; };
	auto* Config=NewObject<UHALPolicePursuitConfig>(Ability); Ability->SkillConfig=Config;
	auto* Match=W->SpawnActor<AHALMatchGameState>(); W->SetGameState(Match); Match->SetMatchPhase(EHALMatchPhase::Playing); int32 Id=0;
	const auto Car=[&]()
	{
		auto* Class=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_06.BP_VehiclePawn_Car_06_C"));
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* V=W->SpawnActor<ATestVehiclePawn>(Class,FVector(0,++Id*1800,60),FRotator::ZeroRotator,P);
		auto* D=FHALVehicleAbilityTestAccess::Configure(V,UHALPolicePursuitAbility::StaticClass()); FHALVehicleAbilityTestAccess::RuntimeConfig(V,D->Abilities);
		V->PreInitializeComponents(); V->InitializeComponents(); V->PostInitializeComponents();
		auto* PC=W->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController(); auto* PS=W->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS); PS->SetPlayerId(Id);
		PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay(); PS->ChangeVehicleAvatar(V,1); V->DispatchBeginPlay(); return V;
	};
	auto* A=Car(); auto* B=Car(); auto* C=Car(); A->SetActorLocation(FVector(0,0,100)); B->SetActorLocation(FVector(600,0,100)); C->SetActorLocation(FVector(0,2500,100));
	auto* ASC=CastChecked<UHALAbilitySystemComponent>(A->GetAbilitySystemComponent()); auto* BSC=CastChecked<UHALAbilitySystemComponent>(B->GetAbilitySystemComponent());
	A->SetActorRotation(FRotator(180,0,0));
	TestTrue(TEXT("Normal primary input permits inverted airborne siren"),ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	AHALPolicePursuit* P=nullptr; for(TActorIterator<AHALPolicePursuit> It(W);It;++It) { P=*It; } if(!TestNotNull(TEXT("Server pursuit actor"),P)) { return false; } P->DispatchBeginPlay();
	TestFalse(TEXT("No duplicate skill while active"),ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	FHALPursuitTestAccess::Step(*P,2); TestEqual(TEXT("Independent target wanted"),FHALPursuitTestAccess::Value(*P,B),50.f);
	ASC->ApplyStun(.5f,C); FHALPursuitTestAccess::Recovery(*A,true); FHALPursuitTestAccess::Step(*P,1);
	TestEqual(TEXT("Caster stun AND self-right do not pause active pursuit"),FHALPursuitTestAccess::Value(*P,B),75.f); FHALPursuitTestAccess::Recovery(*A,false);
	B->SetActorLocation(FVector(1200,0,100)); FHALPursuitTestAccess::Step(*P,.5f); TestEqual(TEXT("World exit decays"),FHALPursuitTestAccess::Value(*P,B),65.f);
	B->SetActorLocation(FVector(910,0,100)); TestTrue(TEXT("Exit hysteresis"),FHALPursuitTestAccess::Inside(*P,*B,true)); TestFalse(TEXT("Fresh entry respects base radius"),FHALPursuitTestAccess::Inside(*P,*B,false));
	B->SetActorLocation(FVector(600,0,500)); TestFalse(TEXT("Vertical separation excluded"),FHALPursuitTestAccess::Inside(*P,*B,false)); B->SetActorLocation(FVector(600,0,100));
	auto* Wall=W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(10,300,300)); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionResponseToAllChannels(ECR_Block); Box->SetWorldLocation(FVector(300,0,150)); Box->RegisterComponent();
	TestFalse(TEXT("Ordinary wall blocks pursuit"),FHALPursuitTestAccess::Inside(*P,*B,false)); Wall->Destroy();
	const auto* Health=B->FindComponentByClass<UVehicleHealthComponent>(); const float HP=Health->GetCurrentHP();
	BSC->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	FHALPursuitTestAccess::Step(*P,1.4f);
	auto* Receiver=B->FindComponentByClass<UHALPursuitTargetComponent>(); TestTrue(TEXT("Arrest enters shared Stun despite impact immunity"),BSC->IsStunned()&&Receiver->GetState().bArrested);
	TestEqual(TEXT("No direct damage"),Health->GetCurrentHP(),HP); TestEqual(TEXT("Arrest consumes wanted"),FHALPursuitTestAccess::Value(*P,B),0.f);
	FHALPursuitTestAccess::Step(*P,4); TestEqual(TEXT("One arrest per target per siren"),FHALPursuitTestAccess::Value(*P,B),0.f);
	C->SetActorLocation(FVector(0,600,100)); auto* Other=W->SpawnActor<AHALPolicePursuit>(); Other->SetOwner(C); TestTrue(TEXT("Second police starts"),Other->Start(*Config)); Other->DispatchBeginPlay();
	FHALPursuitTestAccess::Step(*Other,1); TestEqual(TEXT("Shared protection stops other police buildup"),FHALPursuitTestAccess::Value(*Other,B),0.f);
	P->Destroy(); TestTrue(TEXT("Caster cleanup does not remove accepted arrest"),BSC->IsStunned()&&Receiver->GetState().bArrested);
	BSC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FGameplayTagContainer(HALGameplayTags::Effect_Stun)));
	TestTrue(TEXT("Protection starts its tail on actual Stun removal"),!Receiver->GetState().bArrested&&Receiver->GetState().bProtected);
	TestTrue(TEXT("Car06 protection never blocks electric/ordinary Stun"),BSC->ApplyStun(.2f,C));
	for(int32 I=0;I<46;++I) { W->Tick(LEVELTICK_All,.05f); }
	// These synchronous ticks share one GFrameCounter, so UE's TimerManager only
	// dispatches once. Evaluate the receiver at the advanced world time explicitly;
	// the live network fixture separately verifies real frame/timer expiration.
	FHALPursuitTestAccess::RefreshProtection(*Receiver);
	TestFalse(TEXT("Protection eventually expires"),Receiver->GetState().bProtected);
	C->GetController()->UnPossess(); FHALPursuitTestAccess::Step(*Other,.1f); TestTrue(TEXT("Avatar exit removes source"),Other->IsActorBeingDestroyed());
	TestFalse(TEXT("No arrest tag leaked after expiry"),BSC->HasMatchingGameplayTag(HALGameplayTags::State_Arrested));
	return true;
}
#endif
