#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "../HALSkillTestDummy.h"
#include "../HALPolicePursuitAbility.h"
#include "../HALPursuitHUDWidget.h"
#include "../HALPursuitTargetComponent.h"
#include "../HALMatchGameMode.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALAbilitySystemComponent.h"
#include "../VehicleHealthComponent.h"
#include "../CombatResolver.h"
#include "Components/PrimitiveComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	struct FSkillDummyFixture
	{
		FSkillDummyFixture()
		{
			FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* W,const UWorld::InitializationValues&)
			{
				if(!W->IsGameWorld()||!FParse::Param(FCommandLine::Get(),TEXT("HALSkillDummyFixture"))) { return; }
				auto* Ability=GetMutableDefault<UHALPolicePursuitAbility>(); auto* C=NewObject<UHALPolicePursuitConfig>(Ability);
				if(auto* Saved=LoadObject<UHALPolicePursuitConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car06_PolicePursuit"))) { C->Rules=Saved->Rules; C->FX=Saved->FX; }
				C->FX.SirenSound=nullptr; Ability->SkillConfig=C;
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* A)
				{
					if(auto* M=Cast<AHALMatchGameMode>(A)) { M->DefaultPawnClass=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_06.BP_VehiclePawn_Car_06_C")); }
					if(auto* V=Cast<ATestVehiclePawn>(A);V&&!V->IsA<AHALSkillTestDummy>()&&V->GetVehicleDefinition()) { FHALVehicleAbilityTestAccess::Configure(V,UHALPolicePursuitAbility::StaticClass()); }
				}));
			});
		}
	} SkillDummyFixture;
	class FSkillDummyLive : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test; double Deadline=FPlatformTime::Seconds()+90,Start=-1,Done=-1; bool Announced=false,TargetAnnounced=false,Spawned=false,Casted=false,Ready=false,Damaged=false,Wanted=false,Arrested=false,Tail=false,HUD=false;
	public:
		explicit FSkillDummyLive(FAutomationTestBase* T):Test(T){}
		bool Update() override
		{
			if(Done>=0) { return FPlatformTime::Seconds()-Done>3; }
			if(FPlatformTime::Seconds()>Deadline) { Test->AddError(FString::Printf(TEXT("Dummy live timeout spawn=%d cast=%d ready=%d damage=%d wanted=%d arrest=%d tail=%d hud=%d"),Spawned,Casted,Ready,Damaged,Wanted,Arrested,Tail,HUD)); return true; }
			UWorld* W=nullptr; for(const auto& C:GEngine->GetWorldContexts()) if(auto* X=C.World();X&&X->IsGameWorld()&&(X->GetNetMode()==NM_Client||X->GetNetMode()==NM_ListenServer)) { W=X; break; }
			if(!W) { return false; } const bool Host=W->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC=nullptr; for(auto It=W->GetPlayerControllerIterator();It;++It) if(auto* C=Cast<AHALPlayerController>(It->Get());C&&C->IsLocalController()) { PC=C; break; }
			auto* V=PC?Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr; auto* Match=W->GetGameState<AHALMatchGameState>();
			if(!V||!Match||Match->PlayerArray.Num()<2) { return false; }
			if(!Host&&!Announced)
			{
				// Warm the full dummy asset graph before telling the server to start the timed cast.
				LoadObject<UHALSkillTestDummyConfig>(nullptr,TEXT("/Game/Data/Testing/DA_SkillTestDummy_Default"));
				PC->SetName(TEXT("HAL_Dummy_Client_Ready")); Announced=true;
			}
			if(Host&&!Match->PlayerArray.ContainsByPredicate([&](const auto& P){return P&&P!=PC->PlayerState&&(P->GetPlayerName()==TEXT("HAL_Dummy_Client_Ready")||P->GetPlayerName()==TEXT("HAL_Dummy_Target_Ready"));})) { return false; }
			if(Host&&Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers) { PC->HALStartMatch(); }
			if(Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
			const double Now=Match->GetServerWorldTimeSeconds();
			if(Host&&!Spawned)
			{
				auto* Config=LoadObject<UHALSkillTestDummyConfig>(nullptr,TEXT("/Game/Data/Testing/DA_SkillTestDummy_Default"));
				if(!Config) { Test->AddError(TEXT("Missing native dummy config asset")); return true; }
				const FTransform Pose(V->GetActorLocation()+FVector(0,450,0));
				auto* D=W->SpawnActorDeferred<AHALSkillTestDummy>(AHALSkillTestDummy::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				FindFProperty<FObjectProperty>(D->GetClass(),TEXT("TestConfig"))->SetObjectPropertyValue_InContainer(D,Config); D->FinishSpawning(Pose); Spawned=true;
				FHALAttackSourceSnapshot Snapshot; if(FCombatResolver::CaptureVehicleAttack(*V,*V,1,0,Snapshot))
				{
					Snapshot.SourceTag=HALGameplayTags::Combat_Source_Ball; FHALCombatAction Action; Action.Begin(*V,Snapshot);
					FHALCombatHitRequest R; R.AttackId=Action.GetSnapshot().AttackId; R.PulseSequence=Action.GetPulseSequence(); R.bApplyDamage=true; R.bApplyImpact=false; R.Hit.TargetActor=D; R.Hit.TargetPhysicsBody=CastChecked<UPrimitiveComponent>(D->GetRootComponent()); FCombatResolver::ResolveCombatHit(Action,R);
				}
			}
			AHALSkillTestDummy* D=nullptr; for(TActorIterator<AHALSkillTestDummy> It(W);It;++It) { if(!It->IsActorBeingDestroyed()) { D=*It; break; } }
			if(D)
			{
				if(Start<0) { Start=Now; } auto* A=Cast<UHALAbilitySystemComponent>(D->GetAbilitySystemComponent()); const auto* H=D->FindComponentByClass<UVehicleHealthComponent>();
				Ready|=D->CanReceiveSkillEffects()&&A&&A->GetAvatarActor()==D&&!D->GetController()&&!D->GetPlayerState();
				Damaged|=H&&H->IsHealthReady()&&H->GetCurrentHP()<H->GetMaxHP();
				const auto* C=D->FindComponentByClass<UHALPursuitTargetComponent>(); Arrested|=C&&C->GetState().bArrested&&A&&A->IsStunned(); Tail|=C&&!C->GetState().bArrested&&C->GetState().bProtected;
				for(const auto& View:UHALPursuitHUDWidget::GatherViews(W)) { Wanted|=View.Target==D&&View.Fraction>0; }
				if(auto* Component=D->FindComponentByClass<UWidgetComponent>()) if(auto* Widget=Cast<UHALSkillTestDummyWidget>(Component->GetUserWidgetObject())) { HUD|=Widget->GetDummyView().bReady; }
				if(!Host&&Ready&&HUD&&!TargetAnnounced) { PC->SetName(TEXT("HAL_Dummy_Target_Ready")); TargetAnnounced=true; }
				const bool RemoteReady=Match->PlayerArray.ContainsByPredicate([](const auto& P){return P&&P->GetPlayerName()==TEXT("HAL_Dummy_Target_Ready");});
				if(Host&&!Casted&&Ready&&RemoteReady) { PC->HALUseSkill(); Casted=true; Start=Now; }
				if(Host&&Casted&&Now-Start>12) { D->Destroy(); }
			}
			else if(Start>=0)
			{
				Test->TestTrue(TEXT("Replicated instance config initializes unpossessed dummy ASC"),Ready);
				Test->TestTrue(TEXT("Damage and wanted replicate"),Damaged&&Wanted);
				Test->TestTrue(TEXT("Common Stun and protection replicate"),Arrested&&Tail);
				Test->TestTrue(TEXT("Native fallback health widget bound"),HUD);
				Test->TestEqual(TEXT("Still exactly two real players"),Match->PlayerArray.Num(),2);
				Test->TestTrue(TEXT("Destroyed dummy disappears from pursuit views"),UHALPursuitHUDWidget::GatherViews(W).IsEmpty());
				Test->AddInfo(FString::Printf(TEXT("Skill dummy role=%s ready=%d damage=%d wanted=%d arrest=%d tail=%d hud=%d"),Host?TEXT("Host"):TEXT("Client"),Ready,Damaged,Wanted,Arrested,Tail,HUD)); Done=FPlatformTime::Seconds();
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSkillDummyLiveTest,"HAL.NetworkSmoke.SkillDummy.ReplicationDamagePursuitAndCleanup",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALSkillDummyLiveTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSkillDummyLive>(this)); return true; }
#endif
