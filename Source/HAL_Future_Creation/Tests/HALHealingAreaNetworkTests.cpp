#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALExpandingHealingArea.h"
#include "../HALHealingAreaReceiver.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALGameplayTags.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace
{
	class FHealingNetworkProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds()+100, Complete = -1, EndObserved = -1;
		int32 Stage = 0;
		float EndHP = -1;
		AHALExpandingHealingArea *A = nullptr, *B = nullptr;
	public:
		explicit FHealingNetworkProbe(FAutomationTestBase* InTest) : Test(InTest) {}
		bool Update() override
		{
			if(Complete>0) { return FPlatformTime::Seconds()-Complete>5; }
			if(FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Healing area network timeout: paired listen host/client processes required.")); return true; }
			UWorld* World=nullptr;
			for(const auto& Context:GEngine->GetWorldContexts())
			{ auto* W=Context.World(); if(W && W->IsGameWorld() && (W->GetNetMode()==NM_ListenServer || W->GetNetMode()==NM_Client)) { World=W; break; } }
			if(!World) { return false; }
			const bool Host=World->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC=nullptr;
			for(auto It=World->GetPlayerControllerIterator();It;++It)
			{ if(auto* C=Cast<AHALPlayerController>(It->Get()); C && C->IsLocalController()) { PC=C; break; } }
			auto* V=PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			auto* ASC=V ? Cast<UHALAbilitySystemComponent>(V->GetAbilitySystemComponent()) : nullptr;
			ATestVehiclePawn* Other=nullptr; UHALAbilitySystemComponent* OtherASC=nullptr;
			for(TActorIterator<ATestVehiclePawn> It(World);It;++It)
			{ if(*It!=V) { Other=*It; OtherASC=Cast<UHALAbilitySystemComponent>(Other->GetAbilitySystemComponent()); if(OtherASC) { break; } } }
			auto* Match=World->GetGameState<AHALMatchGameState>();
			if(!ASC || !OtherASC || !Match || !ASC->IsHealthInitialized() || !OtherASC->IsHealthInitialized()) { return false; }
			auto* OtherState=Cast<APlayerState>(OtherASC->GetOwnerActor());
			const auto Ack=[&](const TCHAR* Name) { return OtherState && OtherState->GetPlayerName()==Name; };
			if(Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers)
			{
				if(!Host && Stage==0) { PC->SetName(TEXT("HAL_Healing_Ready")); Stage=-1; }
				if(Host && Ack(TEXT("HAL_Healing_Ready"))) { PC->HALStartMatch(); }
				return false;
			}
			if(Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
			const float HP=ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute());
			const float OtherHP=OtherASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute());
			const bool Active=ASC->HasMatchingGameplayTag(HALGameplayTags::State_Healing_Area);
			const bool OtherActive=OtherASC->HasMatchingGameplayTag(HALGameplayTags::State_Healing_Area);
			if(Host)
			{
				if(Stage==0)
				{
					FPacketSimulationSettings Lag; Lag.PktLag=80; World->GetNetDriver()->SetPacketSimulationSettings(Lag);
					ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),50);
					OtherASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),50); Stage=1;
				}
				else if(Stage==1 && Ack(TEXT("HAL_Healing_Damaged")))
				{
					const auto Spawn=[&]()
					{
						FHALExpandingHealingAreaConfig C; C.ExpansionSeconds=.25f; C.DurationSeconds=8; C.MaximumRadius=10000; C.HalfHeight=1000;
						const FTransform Pose((V->GetActorLocation()+Other->GetActorLocation())*.5);
						auto* Area=World->SpawnActorDeferred<AHALExpandingHealingArea>(AHALExpandingHealingArea::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
						Area->ConfigureArea(C); Area->FinishSpawning(Pose); return Area;
					};
					A=Spawn(); B=Spawn();
					Test->TestEqual(TEXT("Host receives no immediate heal"),HP,50.f);
					Test->TestEqual(TEXT("Remote owner receives no immediate heal"),OtherHP,50.f); Stage=2;
				}
				else if(Stage==2 && HP>=55 && OtherHP>=55 && Ack(TEXT("HAL_Healing_Pulse")))
				{
					Test->TestEqual(TEXT("Host nonstacked first heal"),HP,55.f);
					Test->TestEqual(TEXT("Client authority nonstacked first heal"),OtherHP,55.f);
					A->Destroy();
					Test->TestEqual(TEXT("Remaining host source retained"),V->FindComponentByClass<UHALHealingAreaReceiver>()->GetAreaCount(),1);
					Test->TestEqual(TEXT("Remaining client source retained"),Other->FindComponentByClass<UHALHealingAreaReceiver>()->GetAreaCount(),1); Stage=3;
				}
				else if(Stage==3 && HP>=60 && OtherHP>=60 && Ack(TEXT("HAL_Healing_Continued"))) { Stage=4; }
				else if(Stage==4 && !Active && !OtherActive && (!IsValid(B) || B->IsActorBeingDestroyed()))
				{ EndHP=HP; EndObserved=World->GetTimeSeconds(); Stage=5; }
				else if(Stage==5 && World->GetTimeSeconds()-EndObserved>1.5 && Ack(TEXT("HAL_Healing_Ended")))
				{
					Test->TestEqual(TEXT("Host has no pulses after final area expires"),HP,EndHP);
					Test->TestEqual(TEXT("Both HP values agree on authority"),OtherHP,HP);
					Test->TestEqual(TEXT("Host receiver cleared"),V->FindComponentByClass<UHALHealingAreaReceiver>()->GetAreaCount(),0);
					PC->SetName(TEXT("HAL_Healing_Complete")); Complete=FPlatformTime::Seconds();
				}
			}
			else
			{
				if(Stage==-1 && HP==50 && OtherHP==50 && !Active) { PC->SetName(TEXT("HAL_Healing_Damaged")); Stage=1; }
				else if(Stage==1 && Active && OtherActive && HP>=55 && OtherHP>=55)
				{
					Test->TestEqual(TEXT("Owning client receives one five-HP pulse"),HP,55.f);
					int32 Areas=0; for(TActorIterator<AHALExpandingHealingArea> It(World);It;++It)
					{
						if(It->IsActorBeingDestroyed()) { continue; }
						++Areas; Test->TestEqual(TEXT("Area configuration replicated"),It->GetAreaConfig().MaximumRadius,10000.f);
						Test->TestEqual(TEXT("Client reconstructs radius from server clock"),It->GetCurrentRadius(),10000.f);
					}
					Test->TestEqual(TEXT("Both authority fields exist on client"),Areas,2);
					Test->TestEqual(TEXT("Client never authors healing sources"),V->FindComponentByClass<UHALHealingAreaReceiver>()->GetAreaCount(),0);
					PC->SetName(TEXT("HAL_Healing_Pulse")); Stage=2;
				}
				else if(Stage==2 && HP>=60 && Active && OtherActive) { PC->SetName(TEXT("HAL_Healing_Continued")); Stage=3; }
				else if(Stage==3 && !Active && !OtherActive) { EndHP=HP; EndObserved=World->GetTimeSeconds(); Stage=4; }
				else if(Stage==4 && World->GetTimeSeconds()-EndObserved>1.5)
				{
					Test->TestEqual(TEXT("Client HP stays fixed after replicated removal"),HP,EndHP);
					PC->SetName(TEXT("HAL_Healing_Ended")); Stage=5;
				}
				else if(Stage==5 && Ack(TEXT("HAL_Healing_Complete"))) { Complete=FPlatformTime::Seconds(); }
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHealingAreaNetworkTest,"HAL.NetworkSmoke.HealingAreas",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALHealingAreaNetworkTest::RunTest(const FString& Parameters)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHealingNetworkProbe>(this)); return true; }
#endif
