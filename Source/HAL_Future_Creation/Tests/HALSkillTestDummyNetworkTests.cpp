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
#include "../HALDamageFeedbackComponent.h"
#include "../HALDamageFeedbackSubsystem.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../HALImpactReceiverComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

struct FHALArrestChainNetworkAccess
{
	static bool Bound(const UHALPursuitTargetComponent& C)
	{ return C.Chains&&C.Chains->IsVisible()&&C.Chains->GetInstanceCount()==C.State.FX.Chains.LinksPerLoop*2
		&&C.Chains->GetCollisionEnabled()==ECollisionEnabled::NoCollision&&C.Chains->GetComponentQuat().Equals(C.GetOwner()->GetActorQuat(),.001f); }
	static bool Any(const UHALPursuitTargetComponent& C) { return C.Chains&&C.Chains->IsVisible()&&C.Chains->GetInstanceCount()>0; }
	static bool SampleFlow(const UHALPursuitTargetComponent& C,FVector& At)
	{
		if(!Bound(C)||!C.Material||!FMath::IsNearlyEqual(C.Material->K2_GetScalarParameterValue(TEXT("WireRadius")),C.State.FX.Chains.WireRadius)
			||C.State.FX.Chains.FlowSpeed==0||C.PresentationTime()-C.State.ArrestStart<.8) { return false; }
		FTransform T; C.Chains->GetInstanceTransform(0,T); At=T.GetLocation(); return true;
	}
};
struct FHALDamageFeedbackNetworkAccess
{
	static bool AccentVisible(const UHALDamageFeedbackSubsystem& FX,const AActor* Target)
	{
		const int32 Index=FX.Slots.IndexOfByPredicate([&](const auto& S) { return S.Target.Get()==Target; });
		if(Index==INDEX_NONE||!FX.Material) { return false; }
		const auto P=FX.Material->K2_GetVectorParameterValue(FName(*FString::Printf(TEXT("Target%d"),Index)));
		return P.B>.2&&P.A>.2;
	}
};

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
		bool ChainSeen=false,ChainRelease=false,ChainGoneDuringProtection=false;
		bool ChainFlowSeen=false; double ChainSampleTime=-1; FVector ChainSample=FVector::ZeroVector;
		bool FeedbackFired=false,FeedbackReceived=false,FeedbackRendered=false,FeedbackClean=false,FeedbackAccent=false; double FeedbackAt=-1;
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
			const bool DamageFixture=FParse::Param(FCommandLine::Get(),TEXT("HALDamageFeedbackFixture"));
			auto* DamageRenderer=W->GetSubsystem<UHALDamageFeedbackSubsystem>();
			if(DamageFixture&&(!DamageRenderer||!DamageRenderer->IsReady())) { return false; }
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
				if(C)
				{
					ChainSeen|=C->GetState().bArrested&&FHALArrestChainNetworkAccess::Bound(*C);
					FVector At;
					if(C->GetState().bArrested&&FHALArrestChainNetworkAccess::SampleFlow(*C,At))
					{
						if(ChainSampleTime<0) { ChainSampleTime=Now; ChainSample=At; }
						else if(Now-ChainSampleTime>.12) { ChainFlowSeen|=FVector::Distance(ChainSample,At)>1; }
					}
					if(!C->GetState().bArrested&&C->GetState().ReleaseStart>=0)
					{
						ChainRelease|=FHALArrestChainNetworkAccess::Any(*C);
						ChainGoneDuringProtection|=C->GetState().bProtected&&Now>C->GetState().ReleaseStart+C->GetState().FX.Chains.ReleaseSeconds&&!FHALArrestChainNetworkAccess::Any(*C);
					}
				}
				for(const auto& View:UHALPursuitHUDWidget::GatherViews(W)) { Wanted|=View.Target==D&&View.Fraction>0; }
				if(auto* Component=D->FindComponentByClass<UWidgetComponent>()) if(auto* Widget=Cast<UHALSkillTestDummyWidget>(Component->GetUserWidgetObject())) { HUD|=Widget->GetDummyView().bReady; }
				if(!Host&&Ready&&HUD&&!TargetAnnounced) { PC->SetName(TEXT("HAL_Dummy_Target_Ready")); TargetAnnounced=true; }
				const bool RemoteReady=Match->PlayerArray.ContainsByPredicate([](const auto& P){return P&&P->GetPlayerName()==TEXT("HAL_Dummy_Target_Ready");});
				if(Host&&DamageFixture&&Ready&&RemoteReady&&!FeedbackFired)
				{
					FHALAttackSourceSnapshot S; if(FCombatResolver::CaptureVehicleAttack(*V,*V,1,0,S))
					{
						S.SourceTag=HALGameplayTags::Combat_Source_Ball; FHALCombatAction Action; Action.Begin(*V,S);
						FHALCombatHitRequest R; R.AttackId=Action.GetSnapshot().AttackId; R.PulseSequence=Action.GetPulseSequence(); R.bApplyDamage=true; R.bApplyImpact=false;
						R.Hit.TargetActor=D; R.Hit.TargetPhysicsBody=CastChecked<UPrimitiveComponent>(D->GetRootComponent()); FCombatResolver::ResolveCombatHit(Action,R); FeedbackFired=true;
					}
				}
				if(DamageFixture)
				{
					const auto* Receiver=D->FindComponentByClass<UHALDamageFeedbackComponent>();
					FeedbackReceived|=Receiver&&Receiver->GetReceivedPulseCount()>0;
					FeedbackAccent|=FHALDamageFeedbackNetworkAccess::AccentVisible(*DamageRenderer,D);
					if(DamageRenderer->GetActiveTargetCount()>0&&FeedbackReceived) { FeedbackRendered=true; if(FeedbackAt<0) { FeedbackAt=Now; } }
					if(FeedbackAt>=0&&Now-FeedbackAt>1) { FeedbackClean|=DamageRenderer->GetActiveTargetCount()==0&&DamageRenderer->GetLeasedMeshCount()==0; }
				}
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
				if(FParse::Param(FCommandLine::Get(),TEXT("HALArrestChainFixture")))
				{
					Test->TestTrue(TEXT("Two non-colliding chains follow target orientation on this endpoint"),ChainSeen);
					Test->TestTrue(TEXT("Short release is observed after actual Stun ended"),ChainRelease);
					Test->TestTrue(TEXT("Chains finish before protection tail ends"),ChainGoneDuringProtection);
					Test->TestTrue(TEXT("Independent wire radius is bound and actual chain links circulate on this endpoint"),ChainFlowSeen);
					Test->AddInfo(FString::Printf(TEXT("Arrest chains role=%s active=%d release=%d hiddenWhileProtected=%d flowAndRadius=%d"),Host?TEXT("Host"):TEXT("Client"),ChainSeen,ChainRelease,ChainGoneDuringProtection,ChainFlowSeen));
				}
				if(DamageFixture)
				{
					Test->TestTrue(TEXT("Authority damage pulse arrives and is locally rendered"),FeedbackReceived&&FeedbackRendered);
					Test->TestTrue(TEXT("Damage pass and stencil leases expire independently from pursuit"),FeedbackClean);
					Test->TestTrue(TEXT("Short white/RGB accent survives actual packet lag"),FeedbackAccent);
					Test->AddInfo(FString::Printf(TEXT("Damage feedback role=%s received=%d rendered=%d cleanup=%d accent=%d"),Host?TEXT("Host"):TEXT("Client"),FeedbackReceived,FeedbackRendered,FeedbackClean,FeedbackAccent));
				}
				Test->AddInfo(FString::Printf(TEXT("Skill dummy role=%s ready=%d damage=%d wanted=%d arrest=%d tail=%d hud=%d"),Host?TEXT("Host"):TEXT("Client"),Ready,Damaged,Wanted,Arrested,Tail,HUD)); Done=FPlatformTime::Seconds();
			}
			return false;
		}
	};
	/** Resolver -> non-History queue -> actual server physics -> ordinary client body replication. */
	class FSkillDummyImpactLive : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline=FPlatformTime::Seconds()+90, SpawnAt=-1, StunAt=-1, SleepAt=-1, Done=-1;
		bool Ready=false, Spawned=false, Fired=false, Measured=false, HadTarget=false;
		FVector Rest=FVector::ZeroVector;
		float PeakRise=0, PeakDistance=0;
	public:
		explicit FSkillDummyImpactLive(FAutomationTestBase* T):Test(T){}
		bool Update() override
		{
			if(Done>=0) { return FPlatformTime::Seconds()-Done>3; }
			if(FPlatformTime::Seconds()>Deadline)
			{ Test->AddError(FString::Printf(TEXT("Dummy impact timeout ready=%d spawn=%d fired=%d measured=%d rise=%.2f distance=%.2f"),Ready,Spawned,Fired,Measured,PeakRise,PeakDistance)); return true; }
			UWorld* W=nullptr;
			for(const auto& Context:GEngine->GetWorldContexts()) if(auto* X=Context.World();X&&X->IsGameWorld()&&(X->GetNetMode()==NM_Client||X->GetNetMode()==NM_ListenServer)) { W=X; break; }
			if(!W) { return false; }
			const bool Host=W->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC=nullptr;
			for(auto It=W->GetPlayerControllerIterator();It;++It) if(auto* C=Cast<AHALPlayerController>(It->Get());C&&C->IsLocalController()) { PC=C; break; }
			auto* V=PC?Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr; auto* Match=W->GetGameState<AHALMatchGameState>();
			if(!V||!Match||Match->PlayerArray.Num()!=2) { return false; }
			FString Remote;
			for(const auto& P:Match->PlayerArray) if(P&&P!=PC->PlayerState) { Remote=P->GetPlayerName(); }
			if(!Ready)
			{ LoadObject<UHALSkillTestDummyConfig>(nullptr,TEXT("/Game/Data/Testing/DA_SkillTestDummy_Default")); PC->SetName(Host?TEXT("HAL_Wake_ReadyH"):TEXT("HAL_Wake_ReadyC")); Ready=true; }
			if(Host&&Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers&&Remote.StartsWith(TEXT("HAL_Wake_"))) { PC->HALStartMatch(); }
			if(Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
			const double Now=W->GetTimeSeconds();
			if(Host&&!Spawned)
			{
				auto* Config=LoadObject<UHALSkillTestDummyConfig>(nullptr,TEXT("/Game/Data/Testing/DA_SkillTestDummy_Default"));
				if(!Config) { Test->AddError(TEXT("Missing dummy config")); return true; }
				const FTransform Pose(V->GetActorLocation()+FVector(0,450,0));
				auto* D=W->SpawnActorDeferred<AHALSkillTestDummy>(AHALSkillTestDummy::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				FindFProperty<FObjectProperty>(D->GetClass(),TEXT("TestConfig"))->SetObjectPropertyValue_InContainer(D,Config); D->FinishSpawning(Pose);
				Spawned=true; SpawnAt=Now;
			}
			AHALSkillTestDummy* D=nullptr;
			for(TActorIterator<AHALSkillTestDummy> It(W);It;++It) if(!It->IsActorBeingDestroyed()) { D=*It; break; }
			if(D)
			{
				auto* Body=CastChecked<UPrimitiveComponent>(D->GetRootComponent());
				if(!HadTarget&&D->CanReceiveSkillEffects())
				{ HadTarget=true; Rest=D->GetActorLocation(); if(!Host) { PC->SetName(TEXT("HAL_Wake_TargetC")); } }
				if(Host&&!Fired&&HadTarget&&Remote==TEXT("HAL_Wake_TargetC")&&Now-SpawnAt>1.5)
				{
					// Continuous zero-input grip/steering can wake an upright vehicle. Use
					// the existing Stun state to suspend that work while arranging PT rest;
					// Stun does not grant impact immunity or disable the impact consumer.
					if(StunAt<0)
					{ Test->TestTrue(TEXT("Shared Stun arranges a resting target without disabling physics"),CastChecked<UHALAbilitySystemComponent>(D->GetAbilitySystemComponent())->ApplyStun(3,V)); StunAt=Now; return false; }
					if(Now-StunAt<.3) { return false; }
					if(SleepAt<0) { Body->PutAllRigidBodiesToSleep(); SleepAt=Now; return false; }
					if(Now-SleepAt<.25||Body->IsAnyRigidBodyAwake()) { return false; }
					Rest=D->GetActorLocation();
					FHALAttackSourceSnapshot S; S.SourceActor=V; S.SourceTag=HALGameplayTags::Combat_Source_Ball;
					S.FixedDamage=25; S.Impact.Version=EHALImpactVersion::TieredV1; S.Impact.Tier=EHALImpactTier::Heavy;
					FHALCombatAction Action; Action.Begin(*V,S);
					FHALCombatHitRequest Hit; Hit.AttackId=Action.GetSnapshot().AttackId; Hit.PulseSequence=Action.GetPulseSequence();
					Hit.bApplyDamage=true; Hit.bApplyImpact=true; Hit.Impact.Mode=EHALImpactMode::ExplicitTier; Hit.Impact.Tier=EHALImpactTier::Heavy;
					Hit.Hit.TargetActor=D; Hit.Hit.TargetPhysicsBody=Body; Hit.Hit.ImpactNormal=-FVector::ForwardVector;
					const auto Result=FCombatResolver::ResolveCombatHit(Action,Hit);
					Test->TestTrue(TEXT("Sleeping unowned target accepts authority damage and Heavy impact"),Result.bHitAccepted&&Result.bImpactApplied);
					Fired=true;
				}
				if(HadTarget)
				{
					const FVector Offset=D->GetActorLocation()-Rest;
					PeakRise=FMath::Max(PeakRise,float(Offset.Z)); PeakDistance=FMath::Max(PeakDistance,float(Offset.Size2D()));
					const auto* Health=D->FindComponentByClass<UVehicleHealthComponent>();
					if(!Measured&&Health&&Health->GetCurrentHP()<Health->GetMaxHP()&&PeakRise>80&&PeakDistance>80)
					{
						Measured=true;
						Test->TestTrue(TEXT("No input owner or player state was added to dummy"),!D->GetController()&&!D->GetPlayerState());
						Test->AddInfo(FString::Printf(TEXT("Dummy impact role=%s actualRise=%.3f actualHorizontal=%.3f HP=%.1f"),Host?TEXT("Host"):TEXT("Client"),PeakRise,PeakDistance,Health->GetCurrentHP()));
						PC->SetName(Host?TEXT("HAL_Wake_DoneH"):TEXT("HAL_Wake_DoneC"));
					}
				}
				if(Host&&Measured&&Remote==TEXT("HAL_Wake_DoneC")) { D->Destroy(); }
			}
			else if(HadTarget&&Measured)
			{ Test->TestEqual(TEXT("Only two real players after impact and dummy cleanup"),Match->PlayerArray.Num(),2); Done=FPlatformTime::Seconds(); }
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSkillDummyLiveTest,"HAL.NetworkSmoke.SkillDummy.ReplicationDamagePursuitAndCleanup",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALSkillDummyLiveTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSkillDummyLive>(this)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSkillDummyImpactLiveTest,"HAL.NetworkSmoke.SkillDummyImpact.SleepingAuthorityAndReplication",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALSkillDummyImpactLiveTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSkillDummyImpactLive>(this)); return true; }
#endif
