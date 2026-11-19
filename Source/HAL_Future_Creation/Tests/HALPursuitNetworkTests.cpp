#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "../HALPolicePursuit.h"
#include "../HALPolicePursuitAbility.h"
#include "../HALPursuitTargetComponent.h"
#include "../HALPursuitHUDWidget.h"
#include "../HALMatchGameMode.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALAbilitySystemComponent.h"
#include "../VehicleHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
struct FHALPursuitNetworkAccess
{
	static bool Mounted(const AHALPolicePursuit& P)
	{ return P.Range&&P.Range->IsVisible()&&P.Beacons.Num()==2&&P.Lights.Num()==2&&P.Materials.Num()==3&&P.Materials[0]&&P.Materials[1]&&P.Materials[2]; }
};
namespace
{
	struct FPursuitFixture
	{
		FPursuitFixture()
		{
			FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* W,const UWorld::InitializationValues&)
			{
				if(!W->IsGameWorld()||!FParse::Param(FCommandLine::Get(),TEXT("HALPursuitFixture"))) { return; }
				auto* Ability=GetMutableDefault<UHALPolicePursuitAbility>(); auto* C=NewObject<UHALPolicePursuitConfig>(Ability);
				if(const auto* Saved=LoadObject<UHALPolicePursuitConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car06_PolicePursuit.DA_Ability_Car06_PolicePursuit"))) { C->Rules=Saved->Rules; C->FX=Saved->FX; }
				C->FX.SirenSound=nullptr; Ability->SkillConfig=C;
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* A)
				{
					if(auto* M=Cast<AHALMatchGameMode>(A)) { M->DefaultPawnClass=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_06.BP_VehiclePawn_Car_06_C")); }
					if(auto* V=Cast<ATestVehiclePawn>(A);V&&V->GetVehicleDefinition()) { FHALVehicleAbilityTestAccess::Configure(V,UHALPolicePursuitAbility::StaticClass()); }
				}));
			});
		}
	} PursuitFixture;
	class FPursuitLive : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test; double Deadline=FPlatformTime::Seconds()+85,LastAttempt=-1,Done=-1,OwnStunObserved=-1;
		bool Placed=false,Own=false,OtherSource=false,Wanted=false,Arrested=false,ProtectedTail=false,Continued=false,FX=false;
		float InitialHP=-1;
	public:
		explicit FPursuitLive(FAutomationTestBase* T):Test(T){}
		bool Update() override
		{
			if(Done>=0) { return FPlatformTime::Seconds()-Done>3; }
			if(FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Pursuit live fixture timed out")); return true; }
			UWorld* W=nullptr; for(const auto& C:GEngine->GetWorldContexts()) if(auto* Candidate=C.World();Candidate&&Candidate->IsGameWorld()&&(Candidate->GetNetMode()==NM_Client||Candidate->GetNetMode()==NM_ListenServer)) { W=Candidate; break; }
			if(!W) { return false; } const bool Host=W->GetNetMode()==NM_ListenServer; AHALPlayerController* PC=nullptr;
			for(auto It=W->GetPlayerControllerIterator();It;++It) if(auto* C=Cast<AHALPlayerController>(It->Get());C&&C->IsLocalController()) { PC=C; break; }
			auto* V=PC?Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr; auto* ASC=V?Cast<UHALAbilitySystemComponent>(V->GetAbilitySystemComponent()):nullptr;
			auto* Match=W->GetGameState<AHALMatchGameState>(); ATestVehiclePawn* Other=nullptr;
			for(TActorIterator<ATestVehiclePawn> It(W);It;++It) if(*It!=V&&It->GetAbilitySystemComponent()) { Other=*It; break; }
			if(!V||!Other||!ASC||!ASC->IsHealthInitialized()||!Match) { return false; }
			if(Host&&Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers) { PC->HALStartMatch(); }
			if(Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
			if(Host&&!Placed)
			{
				Other->SetActorLocation(V->GetActorLocation()+FVector(0,450,0),false,nullptr,ETeleportType::TeleportPhysics);
				for(auto* A:{V,Other}) { auto* B=CastChecked<UPrimitiveComponent>(A->GetRootComponent()); B->SetPhysicsLinearVelocity(FVector::ZeroVector); B->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector); } Placed=true;
			}
			if(InitialHP<0) { InitialHP=V->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(); }
			const double Now=Match->GetServerWorldTimeSeconds(); int32 Count=0;
			for(TActorIterator<AHALPolicePursuit> It(W);It;++It)
			{
				++Count; if(It->GetOwner()==V) { Own=true; } else { OtherSource=true; }
				FX|=FHALPursuitNetworkAccess::Mounted(**It);
				if(ASC->IsStunned()&&It->GetOwner()==V)
				{
					if(OwnStunObserved<0) { OwnStunObserved=Now; }
					// Two independent casts can start seconds apart. Measure persistence
					// from observed Stun, rather than assuming this cast is already 4s old.
					Continued|=Now-OwnStunObserved>.35;
				}
			}
			if(!Own&&(LastAttempt<0||Now-LastAttempt>1)) { PC->HALUseSkill(); LastAttempt=Now; }
			for(const auto& View:UHALPursuitHUDWidget::GatherViews(W)) { Wanted|=View.Fraction>0&&View.StarFill(0)>=View.StarFill(1); }
			const auto* Receiver=V->FindComponentByClass<UHALPursuitTargetComponent>();
			Arrested|=Receiver->GetState().bArrested&&ASC->IsStunned(); ProtectedTail|=!Receiver->GetState().bArrested&&Receiver->GetState().bProtected;
			if(Own&&OtherSource&&Count==0&&ASC->GetUltimateEnergyView().bHasEnergy)
			{
				Test->TestTrue(TEXT("Replicated independent wanted and left-to-right progress"),Wanted);
				Test->TestTrue(TEXT("Both ends see arrest, shared Stun and post-Stun protection"),Arrested&&ProtectedTail);
				Test->TestTrue(TEXT("Already active siren continues while caster is stunned"),Continued);
				Test->TestTrue(TEXT("Roof lights and scanning range actually mount"),FX);
				Test->TestEqual(TEXT("No skill HP damage"),V->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(),InitialHP);
				Test->TestTrue(TEXT("No lingering wanted or arrest presentation data"),UHALPursuitHUDWidget::GatherViews(W).IsEmpty());
				Test->AddInfo(FString::Printf(TEXT("Pursuit role=%s own=%d observer=%d wanted=%d arrest=%d tail=%d continued=%d fx=%d"),Host?TEXT("Host"):TEXT("Client"),Own,OtherSource,Wanted,Arrested,ProtectedTail,Continued,FX)); Done=FPlatformTime::Seconds();
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPursuitLiveTest,"HAL.NetworkSmoke.Pursuit.WantedArrestProtectionAndSirenContinuity",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALPursuitLiveTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPursuitLive>(this)); return true; }
#endif
