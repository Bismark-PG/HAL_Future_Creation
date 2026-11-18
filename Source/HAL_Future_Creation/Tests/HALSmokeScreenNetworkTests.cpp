#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "../HALSmokeScreen.h"
#include "../HALSmokeScreenAbility.h"
#include "../HALSmokeScreenView.h"
#include "../HALMatchGameMode.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALAbilitySystemComponent.h"
#include "../VehicleHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"

namespace
{
	struct FSmokeFixture
	{
		FSmokeFixture()
		{
			FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* W,const UWorld::InitializationValues&)
			{
				if(!W->IsGameWorld()||!FParse::Param(FCommandLine::Get(),TEXT("HALSmokeScreenFixture"))) { return; }
				GetMutableDefault<UHALSmokeScreenAbility>()->SkillConfig=LoadObject<UHALSmokeScreenConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car08_SmokeScreen"));
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* Actor)
				{
					if(auto* Mode=Cast<AHALMatchGameMode>(Actor)) { Mode->DefaultPawnClass=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_08.BP_VehiclePawn_Car_08_C")); }
					if(auto* V=Cast<ATestVehiclePawn>(Actor);V&&V->GetVehicleDefinition()) { FHALVehicleAbilityTestAccess::Configure(V,UHALSmokeScreenAbility::StaticClass()); }
				}));
			});
		}
	} SmokeFixture;
	class FSmokeLive : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test; double Deadline=FPlatformTime::Seconds()+100,LastStart=-1,Attempt=-1,Done=-1;
		bool Own=false,Remote=false,Immune=false,Obscured=false,Tag=false,Tail=false,FX=false,Screen=false,Swapped=false,Shot=false,ImmuneShot=false;
		float InitialHP=-1;
	public:
		explicit FSmokeLive(FAutomationTestBase* T):Test(T) {}
		bool Update() override
		{
			if(Done>=0) { return FPlatformTime::Seconds()-Done>3; }
			if(FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Smoke fixture timed out; requires paired -HALSmokeScreenFixture processes.")); return true; }
			UWorld* W=nullptr; for(const auto& C:GEngine->GetWorldContexts()) if(auto* Candidate=C.World();Candidate&&Candidate->IsGameWorld()&&(Candidate->GetNetMode()==NM_Client||Candidate->GetNetMode()==NM_ListenServer)) { W=Candidate; break; }
			if(!W) { return false; } const bool Host=W->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC=nullptr; for(auto It=W->GetPlayerControllerIterator();It;++It) { auto* P=Cast<AHALPlayerController>(It->Get()); if(P&&P->IsLocalController()) { PC=P; break; } }
			auto* V=PC?Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr; auto* ASC=V?Cast<UHALAbilitySystemComponent>(V->GetAbilitySystemComponent()):nullptr;
			auto* Match=W->GetGameState<AHALMatchGameState>(); ATestVehiclePawn* Other=nullptr;
			for(TActorIterator<ATestVehiclePawn> It(W);It;++It) if(*It!=V&&It->GetPlayerState()&&It->GetAbilitySystemComponent()) { Other=*It; break; }
			if(!ASC||!ASC->IsHealthInitialized()||!Match||!Other) { return false; }
			if(InitialHP<0) { InitialHP=V->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(); }
			if(Host&&Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers) { PC->HALStartMatch(); }
			if(Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
			const double Now=HALSmoke::ServerTime(W); int32 Count=0; AHALSmokeScreen* Local=nullptr; AHALSmokeScreen* OtherSmoke=nullptr;
			for(TActorIterator<AHALSmokeScreen> It(W);It;++It)
			{
				if(It->GetStartTime()<0||It->IsActorBeingDestroyed()) { continue; } ++Count;
				if(It->GetOwner()==V) { Local=*It; Own=true; } else { OtherSmoke=*It; Remote=true; }
				LastStart=FMath::Max(LastStart,It->GetStartTime());
				Tail|=Now-It->GetStartTime()>4.5&&!It->IsEmitting()&&!It->GetPuffs().IsEmpty();
				Obscured|=It->GetOwner()!=V&&It->Influence(*V,true,Now)>.5f;
			}
			if(!Own&&(Attempt<0||Now-Attempt>1)) { PC->HALUseSkill(); Attempt=Now; }
			if(Local&&Now-Local->GetStartTime()>.4&&Now-Local->GetStartTime()<1.1)
			{ Immune|=Local->Influence(*V,true,Now)==0&&!ASC->HasMatchingGameplayTag(HALGameplayTags::State_Vision_SmokeObscured); }
			if(FApp::CanEverRender()&&Immune&&Local&&!ImmuneShot)
			{ FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Car08Smoke/20261117/Immune_%s.png"),Host?TEXT("Host"):TEXT("Client")),true,false); ImmuneShot=true; }
			// Test-only placement after both genuine requests; all smoke generation and region effects stay on production paths.
			if(Host&&Local&&OtherSmoke&&!Swapped&&Now-FMath::Max(Local->GetStartTime(),OtherSmoke->GetStartTime())>1.2)
			{
				const auto A=V->GetActorTransform(),B=Other->GetActorTransform(); V->SetActorTransform(B,false,nullptr,ETeleportType::TeleportPhysics); Other->SetActorTransform(A,false,nullptr,ETeleportType::TeleportPhysics);
				for(auto* Pawn:{V,Other}) if(auto* Body=Cast<UPrimitiveComponent>(Pawn->GetRootComponent())) { Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); }
				Swapped=true;
			}
			Tag|=ASC->HasMatchingGameplayTag(HALGameplayTags::State_Vision_SmokeObscured);
			if(FApp::CanEverRender())
			{
				float ViewStrength=0;
				for(TObjectIterator<UHALSmokeScreenView> It;It;++It) if(It->GetOwningPlayer()==PC)
				{ ViewStrength=FMath::Max(ViewStrength,It->GetObscuration()); Screen|=It->GetObscuration()>.2f&&It->IsInViewport()&&!It->IsFocusable(); }
				for(TObjectIterator<UNiagaraComponent> It;It;++It) if(It->GetWorld()==W&&Local&&It->GetAsset()==Local->GetFX().JetSystem)
				{
					const auto Controller=It->GetSystemInstanceController(); const auto* Instance=Controller?Controller->GetSystemInstance_Unsafe():nullptr;
					if(Instance) for(const auto& E:Instance->GetEmitters()) { FX|=E->GetNumParticles()>0; }
				}
				if(Screen&&Obscured&&ViewStrength>.65f&&!Shot)
				{ FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Car08Smoke/20261117/Live_%s.png"),Host?TEXT("Host"):TEXT("Client")),true,false); Shot=true; }
			}
			const auto* Config=GetDefault<UHALSmokeScreenAbility>()->SkillConfig.Get();
			if(Own&&Remote&&Count==0&&Config&&Now>=LastStart+Config->CooldownSeconds+2&&!ASC->HasMatchingGameplayTag(HALGameplayTags::Cooldown_SmokeScreen))
			{
				Test->TestTrue(TEXT("Owner and observer see independently ageing replicated clouds"),Own&&Remote&&Tail);
				Test->TestTrue(TEXT("Caster starts immune to own screen effect"),Immune);
				Test->TestTrue(TEXT("Both players receive the other smoke's authority GE"),Obscured&&Tag);
				if(FApp::CanEverRender()) { Test->TestTrue(TEXT("Real Niagara particles and local input-transparent view"),FX&&Screen); }
				Test->TestFalse(TEXT("Final field cleanup removes vision Tag"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Vision_SmokeObscured));
				Test->TestEqual(TEXT("Smoke caused no HP damage"),V->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(),InitialHP);
				for(TObjectIterator<UHALSmokeScreenView> It;It;++It) if(It->GetOwningPlayer()==PC) { Test->TestTrue(TEXT("No persistent local veil"),It->GetObscuration()<.01f); }
				Test->AddInfo(FString::Printf(TEXT("Smoke role=%s own=%d remote=%d immune=%d tag=%d obscured=%d tail=%d particles=%d view=%d"),Host?TEXT("Host"):TEXT("Client"),Own,Remote,Immune,Tag,Obscured,Tail,FX,Screen)); Done=FPlatformTime::Seconds();
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSmokeLiveTest,"HAL.NetworkSmoke.SmokeScreen.OwnerImmunityReplicatedRegionsLocalViewAndCleanup",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALSmokeLiveTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSmokeLive>(this)); return true; }
#endif
