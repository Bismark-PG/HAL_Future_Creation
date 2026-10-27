#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALMovementArea.h"
#include "../HALMovementAreaReceiver.h"
#include "../HALMovementBoostEffect.h"
#include "../HALMovementAttributeSet.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace
{
	class FAreaNetworkProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds()+100, Complete = -1;
		int32 Stage = 0;
		bool bWeakMid = false, bExitMid = false, bRemoteArea = false;
		AHALMovementArea *SlowA = nullptr, *SlowB = nullptr, *Ice = nullptr;
		FActiveGameplayEffectHandle LocalBoost, RemoteBoost;
	public:
		explicit FAreaNetworkProbe(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (Complete > 0) { return FPlatformTime::Seconds()-Complete > 5; }
			if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Movement area smoke timeout: matching host/client processes required.")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* Candidate=Context.World();
				if (Candidate && Candidate->IsGameWorld() && (Candidate->GetNetMode()==NM_Client || Candidate->GetNetMode()==NM_ListenServer)) { World=Candidate; break; }
			}
			if (!World) { return false; }
			const bool bHost=World->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC = nullptr;
			for (auto It=World->GetPlayerControllerIterator();It;++It)
			{ if (auto* Candidate=Cast<AHALPlayerController>(It->Get()); Candidate && Candidate->IsLocalController()) { PC=Candidate; break; } }
			auto* Vehicle=PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			auto* ASC=Vehicle ? Cast<UHALAbilitySystemComponent>(Vehicle->GetAbilitySystemComponent()) : nullptr;
			auto* Movement=Vehicle ? Vehicle->FindComponentByClass<UArcadeVehicleMovementComponent>() : nullptr;
			auto* Match=World->GetGameState<AHALMatchGameState>();
			ATestVehiclePawn* OtherVehicle=nullptr; UHALAbilitySystemComponent* OtherASC=nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World);It;++It)
			{ if (*It!=Vehicle) { OtherVehicle=*It; OtherASC=Cast<UHALAbilitySystemComponent>(It->GetAbilitySystemComponent()); if (OtherASC) { break; } } }
			if (!ASC || !OtherASC || !Movement || !Match || !ASC->IsHealthInitialized() || !OtherASC->IsHealthInitialized()) { return false; }
			const auto Status=Movement->GetMovementEffectStatus();
			if (!Status.bReady) { return false; }
			auto* OtherState=Cast<APlayerState>(OtherASC->GetOwnerActor());
			if (Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers)
			{
				if (!bHost && Stage==0) { PC->SetName(TEXT("HAL_Areas_Ready")); Stage=-1; }
				if (bHost && OtherState && OtherState->GetPlayerName()==TEXT("HAL_Areas_Ready")) { PC->HALStartMatch(); }
				return false;
			}
			if (Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
			const FVector Current(Status.Value.AccelerationScale,Status.Value.SpeedScale,Status.Value.GripScale);
			const bool bSlow=ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_SlowArea);
			const bool bSlip=ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_SlipperyArea);
			bRemoteArea |= OtherASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_SlowArea)
				&& OtherASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_SlipperyArea);
			bWeakMid |= Current.X>.905 && Current.X<1.195 && Status.Definition.AreaAcceleration.Target>.69f;
			bExitMid |= Current.Z>.205 && Current.Z<.995;
			const auto Matches=[&](FVector Expected) { return Current.Equals(Expected,.003); };
			const auto Acknowledged=[&](const TCHAR* Name) { return OtherState && OtherState->GetPlayerName()==Name; };
			if (bHost)
			{
				const auto RefreshBoosts=[&]()
				{
					LocalBoost=ASC->ApplyGameplayEffectToSelf(GetDefault<UHALMovementBoostEffect>(),1,ASC->MakeEffectContext());
					RemoteBoost=OtherASC->ApplyGameplayEffectToSelf(GetDefault<UHALMovementBoostEffect>(),1,OtherASC->MakeEffectContext());
				};
				if (Stage==0)
				{
					FPacketSimulationSettings Network; Network.PktLag=80; World->GetNetDriver()->SetPacketSimulationSettings(Network);
					RefreshBoosts();
					const auto SpawnArea=[&](FHALMovementAreaConfig Config)
					{
						auto* Area=World->SpawnActorDeferred<AHALMovementArea>(AHALMovementArea::StaticClass(),FTransform::Identity);
						Area->ConfigureArea(Config); Area->GetAreaBounds()->SetBoxExtent(FVector(10000,10000,5000));
						Area->FinishSpawning(FTransform::Identity); return Area;
					};
					FHALMovementAreaConfig A,B,C; A.AccelerationMultiplier=.7f; A.SpeedLimitMultiplier=.3f;
					B.AccelerationMultiplier=.4f; B.SpeedLimitMultiplier=.8f; C.Kind=EHALMovementAreaKind::Slippery;
					SlowA=SpawnArea(A); SlowB=SpawnArea(B); Ice=SpawnArea(C); Stage=1;
					FVehicleInputCmd Input; Input.Throttle=.1f; Movement->SetInputCommand(Input);
				}
				else if (Stage==1 && Matches(FVector(.9f,.55f,.2f)) && Acknowledged(TEXT("HAL_Areas_Full")))
				{
					Test->TestEqual(TEXT("Both authority targets have three actual overlap sources"),OtherVehicle->FindComponentByClass<UHALMovementAreaReceiver>()->GetAreaCount(),3);
					SlowB->SetAreaEnabled(false); RefreshBoosts(); Stage=2;
				}
				else if (Stage==2 && Matches(FVector(1.2f,.55f,.2f)) && Acknowledged(TEXT("HAL_Areas_Weak")))
				{ SlowA->SetAreaEnabled(false); Ice->SetAreaEnabled(false); RefreshBoosts(); Stage=3; }
				else if (Stage==3 && Matches(FVector(1.5f,1.25f,1)) && Acknowledged(TEXT("HAL_Areas_Exit")))
				{ SlowB->SetAreaEnabled(true); SlowB->SetAreaEnabled(true); Ice->SetAreaEnabled(true); RefreshBoosts(); Stage=4; }
				else if (Stage==4 && Matches(FVector(.9f,1.05f,.2f)) && Acknowledged(TEXT("HAL_Areas_Reenter")))
				{ SlowA->Destroy(); SlowB->Destroy(); Ice->Destroy(); RefreshBoosts(); Stage=5; }
				else if (Stage==5 && Matches(FVector(1.5f,1.25f,1)) && Acknowledged(TEXT("HAL_Areas_Destroyed")))
				{
					Test->TestEqual(TEXT("All source bookkeeping cleared"),Vehicle->FindComponentByClass<UHALMovementAreaReceiver>()->GetAreaCount(),0);
					ASC->RemoveActiveGameplayEffect(LocalBoost); OtherASC->RemoveActiveGameplayEffect(RemoteBoost); Stage=6;
				}
				else if (Stage==6 && Matches(FVector::OneVector) && Acknowledged(TEXT("HAL_Areas_Complete")))
				{
					Test->TestTrue(TEXT("Authority observed weak handoff and exit recovery"),bWeakMid && bExitMid && bRemoteArea);
					Test->TestFalse(TEXT("No selected area tag after cleanup"),bSlow || bSlip);
					Movement->ResetInputCommand(); Complete=FPlatformTime::Seconds();
					UE_LOG(LogTemp,Display,TEXT("HAL Area smoke: Role=Host Complete=1 WeakMid=%d ExitMid=%d Revision=%u"),bWeakMid,bExitMid,Status.Value.Revision);
				}
				return false;
			}
			if (Stage==-1 && bSlow && bSlip && Matches(FVector(.9f,.55f,.2f)))
			{ PC->SetName(TEXT("HAL_Areas_Full")); Stage=1; FVehicleInputCmd Input; Input.Throttle=.1f; Movement->SetInputCommand(Input); }
			else if (Stage==1 && Matches(FVector(1.2f,.55f,.2f)))
			{
				Test->TestTrue(TEXT("Owner used an authority-stamped recovery with the original .4 regional origin"),
					Status.Definition.AreaAcceleration.DurationFrames>0 && FMath::IsNearlyEqual(Status.Definition.AreaAcceleration.From,.4f,.0001f));
				PC->SetName(TEXT("HAL_Areas_Weak")); Stage=2;
			}
			else if (Stage==2 && !bSlow && !bSlip && Matches(FVector(1.5f,1.25f,1)))
			{
				Test->TestTrue(TEXT("Simultaneous slow/slippery exits preserve all three recovery curves"),
					Status.Definition.AreaAcceleration.DurationFrames>0 && Status.Definition.AreaSpeed.DurationFrames>0 && Status.Definition.AreaGrip.DurationFrames>0);
				Test->TestTrue(TEXT("Exit retains unrelated boost"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost));
				PC->SetName(TEXT("HAL_Areas_Exit")); Stage=3;
			}
			else if (Stage==3 && bSlow && bSlip && Matches(FVector(.9f,1.05f,.2f)))
			{ PC->SetName(TEXT("HAL_Areas_Reenter")); Stage=4; }
			else if (Stage==4 && !bSlow && !bSlip && Matches(FVector(1.5f,1.25f,1)))
			{ PC->SetName(TEXT("HAL_Areas_Destroyed")); Stage=5; }
			else if (Stage==5 && Matches(FVector::OneVector) && !ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost))
			{
				Test->TestTrue(TEXT("Client observes smooth handoff and exit, not only final target values"),bWeakMid && bExitMid && bRemoteArea);
				Test->TestTrue(TEXT("Real owning vehicle resimulation occurred"),Movement->GetResimulationRunCount()>0);
				PC->SetName(TEXT("HAL_Areas_Complete")); Stage=6; Movement->ResetInputCommand(); Complete=FPlatformTime::Seconds();
				UE_LOG(LogTemp,Display,TEXT("HAL Area smoke: Role=Client Complete=1 WeakMid=%d ExitMid=%d Revision=%u Resims=%d Rewinds=%d"),
					bWeakMid,bExitMid,Status.Value.Revision,Movement->GetResimulationRunCount(),Status.RewindRequests);
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAreaNetworkTest,"HAL.NetworkSmoke.MovementAreas",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALAreaNetworkTest::RunTest(const FString& Parameters)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAreaNetworkProbe>(this)); return true; }
#endif
