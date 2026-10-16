#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"

namespace
{
	/** Opt-in live two-process test. Uses the actual map, ownership, match-start and GAS activation paths. */
	class FHALAbilityLiveSmoke : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds() + 75;
		double RequestedAt = 0;
		double RequestedWorldTime = 0;
		double CompleteAt = 0;
		bool bRequested = false;
		bool bBothActive = false;
		bool bBothExpired = false;
	public:
		explicit FHALAbilityLiveSmoke(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() > Deadline)
			{
				Test->AddError(TEXT("Live ability smoke timed out: launch a host and one client on NetTest with this exact test filter."));
				return true;
			}
			UWorld* World = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* Candidate = Context.World();
				if (Candidate && Candidate->IsGameWorld()
					&& (Candidate->GetNetMode() == NM_ListenServer || Candidate->GetNetMode() == NM_Client)) { World = Candidate; break; }
			}
			if (!World) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			AHALPlayerController* Controller = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (auto* PC = Cast<AHALPlayerController>(It->Get()); PC && PC->IsLocalController()) { Controller = PC; break; }
			}
			if (!Controller) { return false; }
			auto* Vehicle = Cast<ATestVehiclePawn>(Controller->GetPawn());
			auto* Local = Vehicle ? Cast<UHALAbilitySystemComponent>(Vehicle->GetAbilitySystemComponent()) : nullptr;
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!Local || !Local->IsHealthInitialized() || !Local->IsImpactInitialized()
				|| !Local->HasAbilityForInput(HALGameplayTags::Input_Ability_Primary) || !Match) { return false; }
			UHALAbilitySystemComponent* Remote = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It)
			{
				if (*It != Vehicle) { Remote = Cast<UHALAbilitySystemComponent>(It->GetAbilitySystemComponent()); if (Remote) { break; } }
			}
			if (!Remote && !bBothExpired) { return false; }
			if (bHost && Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers && Remote)
			{
				Controller->HALStartMatch();
			}
			if (Match->GetMatchPhase() != EHALMatchPhase::Playing) { return false; }
			if (!bRequested)
			{
				// Client goes first: the host must observe a real client-originated GAS RPC before using its own skill.
				if (bHost && !Remote->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune)) { return false; }
				Controller->HALUseSkill();
				if (!bHost)
				{
					// Same-frame client requests arrive before its cooldown replication; authority must reject duplicates.
					Controller->HALUseSkill();
					Controller->HALUseSkill();
				}
				bRequested = true;
				RequestedAt = FPlatformTime::Seconds();
				RequestedWorldTime = World->GetTimeSeconds();
				return false;
			}
			const bool bLocalImmune = Local->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
			const bool bRemoteImmune = Remote && Remote->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
			if (!bBothActive)
			{
				if (!bLocalImmune || !bRemoteImmune) { return false; }
				bBothActive = true;
				Test->TestEqual(TEXT("Owner sees one immunity GE and one cooldown GE"), Local->GetActiveEffects(FGameplayEffectQuery()).Num(), 2);
				const auto Durations = Local->GetActiveEffectsTimeRemainingAndDuration(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
					FGameplayTagContainer(HALGameplayTags::Cooldown_ImpactImmunity)));
				Test->TestTrue(TEXT("Owner receives twenty-game-second cooldown"), Durations.Num() == 1 && FMath::IsNearlyEqual(Durations[0].Value, 20.0f, 0.1f));
				Controller->HALUseSkill(); // A repeated input must be denied locally/server-side by the same GAS cooldown.
				UE_LOG(LogTemp, Display, TEXT("HAL Ability smoke: Role=%s BothActive=1 LocalImmune=1 RemoteImmune=1 Cooldown=%.2f"),
					bHost ? TEXT("Host") : TEXT("Client"), Local->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity));
				Controller->HALGASStatus();
			}
			if (!bBothExpired)
			{
				if (bLocalImmune || bRemoteImmune) { return false; }
				bBothExpired = true;
				Test->TestTrue(TEXT("Immunity expires before cooldown"), Local->HasMatchingGameplayTag(HALGameplayTags::Cooldown_ImpactImmunity));
				UE_LOG(LogTemp, Display, TEXT("HAL Ability smoke: Role=%s BothExpired=1 Cooldown=%.2f GameElapsed=%.2f TimeDilation=%.2f"),
					bHost ? TEXT("Host") : TEXT("Client"), Local->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity),
					World->GetTimeSeconds() - RequestedWorldTime, World->GetWorldSettings()->GetEffectiveTimeDilation());
			}
			if (Local->HasMatchingGameplayTag(HALGameplayTags::Cooldown_ImpactImmunity)) { return false; }
			if (CompleteAt == 0)
			{
				CompleteAt = FPlatformTime::Seconds();
				// GAS uses World game time, not FPlatformTime; only authority owns the expiry clock.
				if (bHost) { Test->TestTrue(TEXT("Authority cooldown lasts twenty game seconds"), World->GetTimeSeconds() - RequestedWorldTime >= 19.5); }
				UE_LOG(LogTemp, Display, TEXT("HAL Ability smoke: Role=%s CooldownExpired=1 GameElapsed=%.2f WallElapsed=%.2f TimeDilation=%.2f"),
					bHost ? TEXT("Host") : TEXT("Client"), World->GetTimeSeconds() - RequestedWorldTime,
					CompleteAt - RequestedAt, World->GetWorldSettings()->GetEffectiveTimeDilation());
				Controller->HALGASStatus();
			}
			// Keep the server alive long enough for the client's final replication/report.
			return !bHost || FPlatformTime::Seconds() - CompleteAt > 3;
		}
	};
}

// Kept outside HAL.FutureCreation: never starts a live-network scenario in the normal Editor suite.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAbilityNetworkSmokeTest, "HAL.NetworkSmoke.Phase10E",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALAbilityNetworkSmokeTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHALAbilityLiveSmoke>(this));
	return true;
}

#endif
