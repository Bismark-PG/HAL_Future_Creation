#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALMovementTestEffect.h"
#include "../HALMovementAttributeSet.h"
#include "../HALAbilitySystemComponent.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	class FMovementLiveProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds() + 100;
		double Start = -1, StageStart = -1, Complete = -1;
		int32 Stage = 0, InitialResims = 0;
		bool bLocalBoost = false, bLocalRestored = false, bRemoteBoost = false, bRemoteRestored = false;
		bool bRecoverySeen = false, bInitialized = false;
		bool bLongLag = FParse::Param(FCommandLine::Get(), TEXT("HALMovementRecoveryTest"));
	public:
		explicit FMovementLiveProbe(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (Complete > 0) { return FPlatformTime::Seconds() - Complete > 3; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Movement network test timed out; requires matching host/client test processes.")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				if (auto* Candidate = Context.World(); Candidate && Candidate->IsGameWorld()
					&& (Candidate->GetNetMode() == NM_ListenServer || Candidate->GetNetMode() == NM_Client)) { World = Candidate; break; }
			}
			if (!World) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			AHALPlayerController* PC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (auto* Candidate = Cast<AHALPlayerController>(It->Get()); Candidate && Candidate->IsLocalController()) { PC = Candidate; break; }
			}
			auto* Pawn = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			auto* Local = Pawn ? Cast<UHALAbilitySystemComponent>(Pawn->GetAbilitySystemComponent()) : nullptr;
			auto* Movement = Pawn ? Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>() : nullptr;
			auto* Match = World->GetGameState<AHALMatchGameState>();
			UHALAbilitySystemComponent* Remote = nullptr;
			ATestVehiclePawn* RemotePawn = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It)
			{
				if (*It != Pawn) { RemotePawn = *It; Remote = Cast<UHALAbilitySystemComponent>(It->GetAbilitySystemComponent()); if (Remote) { break; } }
			}
			if (!Local || !Movement || !Local->IsHealthInitialized() || !Match || (!Remote && Complete < 0)) { return false; }
			if (bHost && Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers && Remote) { PC->HALStartMatch(); }
			if (Match->GetMatchPhase() != EHALMatchPhase::Playing) { return false; }
			const auto Status = Movement->GetMovementEffectStatus();
			const auto* OtherValues = Remote ? Remote->GetSet<UHALMovementAttributeSet>() : nullptr;
			if (!Status.Value.IsValid() || !OtherValues) { return false; }
			if (!bInitialized)
			{
				InitialResims = Movement->GetResimulationRunCount(); Start = World->GetTimeSeconds(); bInitialized = true;
				FVehicleInputCmd Input; Input.Throttle = 0.15f; Movement->SetInputCommand(Input);
			}
			const double Now = World->GetTimeSeconds();
			if (Status.bRecovery && !bRecoverySeen)
			{
				bRecoverySeen = true;
				UE_LOG(LogTemp, Display, TEXT("HAL Movement smoke: Role=%s RecoverySeen=1 Frame=%d Window=%d"), bHost ? TEXT("Host") : TEXT("Client"), Status.ServerFrame, Status.HistoryFrames);
			}
			if (FMath::IsNearlyEqual(Status.Value.AccelerationScale,1.5f) && FMath::IsNearlyEqual(Status.Value.SpeedScale,1.25f)) { bLocalBoost = true; }
			if (bLocalBoost && Status.Value.AccelerationScale == 1 && Status.Value.SpeedScale == 1) { bLocalRestored = true; }
			if (FMath::IsNearlyEqual(OtherValues->GetAccelerationMultiplierDelta(),0.5f)) { bRemoteBoost = true; }
			if (bRemoteBoost && OtherValues->GetAccelerationMultiplierDelta() == 0) { bRemoteRestored = true; }
			if (bHost)
			{
				if (Stage == 0 && Now - Start > 1)
				{
					FPacketSimulationSettings Settings; Settings.PktLag = bLongLag ? 1500 : 80;
					World->GetNetDriver()->SetPacketSimulationSettings(Settings);
					Test->TestTrue(TEXT("Authority applied client-target fixture GE"), Remote->ApplyGameplayEffectToSelf(GetDefault<UHALMovementBridgeTestEffect>(),1,Remote->MakeEffectContext()).IsValid());
					Stage = 1; StageStart = Now;
					UE_LOG(LogTemp, Display, TEXT("HAL Movement smoke: Host ClientTarget=1 OutgoingLag=%dms GameTime=%.2f"), Settings.PktLag, Now);
				}
				if (Stage == 1 && Now - StageStart > (bLongLag ? 4.5 : 3.5))
				{
					FPacketSimulationSettings Settings; World->GetNetDriver()->SetPacketSimulationSettings(Settings);
					const auto OtherStatus = RemotePawn->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetMovementEffectStatus();
					Test->TestEqual(TEXT("Client target naturally returned to neutral on authority"), OtherStatus.Value.AccelerationScale,1.f);
					Test->TestTrue(TEXT("Authority applied host-target fixture GE"), Local->ApplyGameplayEffectToSelf(GetDefault<UHALMovementBridgeTestEffect>(),1,Local->MakeEffectContext()).IsValid());
					Stage = 2; StageStart = Now;
				}
				if (Stage == 2 && Now - StageStart > 3.5)
				{
					Test->TestTrue(TEXT("Authority host boost and natural expiry observed"), bLocalBoost && bLocalRestored);
					Test->TestTrue(TEXT("Authority observed client boost and expiry"), bRemoteBoost && bRemoteRestored);
					Stage = 3; Complete = FPlatformTime::Seconds();
					Movement->ResetInputCommand();
					UE_LOG(LogTemp, Display, TEXT("HAL Movement smoke: Role=Host Complete=1 LocalRevision=%u Accel=%.2f Speed=%.2f"), Status.Value.Revision,Status.Value.AccelerationScale,Status.Value.SpeedScale);
				}
				return Complete > 0 && FPlatformTime::Seconds() - Complete > 4;
			}
			if (bRemoteBoost && bRemoteRestored && Status.Value.Revision >= 3 && Status.Value.AccelerationScale == 1 && !Status.bRecovery && Complete < 0)
			{
				if (!bLongLag) { Test->TestTrue(TEXT("Owning client physics observed its own boost then expiry"), bLocalBoost && bLocalRestored); }
				else { Test->TestTrue(TEXT("History window deficit was detected"), bRecoverySeen); }
				Test->TestTrue(TEXT("Owning vehicle performed real network resimulation"), Movement->GetResimulationRunCount() > InitialResims);
				Test->TestTrue(TEXT("Current authoritative identity and parameters recovered"), Status.bReady && Status.Value.Epoch.IsValid());
				Complete = FPlatformTime::Seconds();
				UE_LOG(LogTemp, Display, TEXT("HAL Movement smoke: Role=Client Complete=1 OwnBoostSeen=%d RecoverySeen=%d Revision=%u ResimRuns=%d RewindRequests=%d Accel=%.2f Speed=%.2f"),
					bLocalBoost,bRecoverySeen,Status.Value.Revision,Movement->GetResimulationRunCount(),Status.RewindRequests,Status.Value.AccelerationScale,Status.Value.SpeedScale);
				Movement->ResetInputCommand(); PC->HALGASStatus();
			}
			return Complete > 0 && FPlatformTime::Seconds() - Complete > 1;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMovementLiveTest, "HAL.NetworkSmoke.Phase10F2",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALMovementLiveTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FMovementLiveProbe>(this)); return true;
}
#endif
