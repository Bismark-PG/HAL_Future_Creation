#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALHealthAttributeSet.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace HALStunNetworkTests
{
	class FRun : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds()+100, Start = -1, Complete = -1;
		bool Signalled = false, ReadySent = false, Prepared = false, SawActive = false, SawOther = false, Short = false, Extended = false, PastOriginal = false;
		uint32 ActiveRevision = 0;
		float HP = 0;
	public:
		explicit FRun(FAutomationTestBase* T) : Test(T) {}
		virtual bool Update() override
		{
			if (Complete > 0) { return FPlatformTime::Seconds()-Complete > 4; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Stun network timeout; requires paired host/client test processes")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{ auto* W = Context.World(); if (W && W->IsGameWorld() && (W->GetNetMode()==NM_Client || W->GetNetMode()==NM_ListenServer)) { World = W; break; } }
			if (!World) { return false; }
			const bool Host = World->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			{ if (auto* P = Cast<AHALPlayerController>(It->Get()); P && P->IsLocalController()) { PC = P; break; } }
			auto* Own = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { if (*It != Own && It->GetPlayerState()) { Other = *It; break; } }
			auto* ASC = Own ? Cast<UHALAbilitySystemComponent>(Own->GetAbilitySystemComponent()) : nullptr;
			auto* Remote = Other ? Cast<UHALAbilitySystemComponent>(Other->GetAbilitySystemComponent()) : nullptr;
			auto* Movement = Own ? Own->FindComponentByClass<UArcadeVehicleMovementComponent>() : nullptr;
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!ASC || !Remote || !Movement || !Match || !ASC->IsHealthInitialized() || !Remote->IsHealthInitialized()) { return false; }
			if (!Signalled)
			{
				// Gravity is not a replicated property. Both observers must configure the same aerial fixture.
				for (auto* V : {Own, Other}) { CastChecked<UPrimitiveComponent>(V->GetRootComponent())->SetEnableGravity(false); }
				FVehicleInputCmd Warmup; Warmup.Throttle = .1f; Movement->SetInputCommand(Warmup);
				if (!Host)
				{
					Test->TestFalse(TEXT("Client cannot author own stun"), ASC->ApplyStun(30, Own));
					Test->TestFalse(TEXT("Client cannot author another player's stun"), Remote->ApplyStun(30, Own));
					PC->ConsoleCommand(FString::Printf(TEXT("HALStun %d 30"),Own->GetPlayerState()->GetPlayerId()));
					Test->TestFalse(TEXT("Client console cannot apply stun"),ASC->IsStunned());
					PC->SetName(TEXT("HAL_Stun_Warmup"));
				}
				Signalled = true;
			}
			const auto Physics = Movement->GetMovementEffectStatus();
			const double Now = World->GetTimeSeconds();
			if (!Host && !ReadySent && Physics.bReady)
			{ PC->SetName(TEXT("HAL_Stun_Ready")); ReadySent = true; }
			if (Host && !Prepared && Match->GetMatchPhase() != EHALMatchPhase::Playing)
			{ PC->HALStartMatch(); return false; }
			if (Host && !Prepared)
			{
				if (Other->GetPlayerState()->GetPlayerName() != TEXT("HAL_Stun_Ready") || !Physics.bReady) { return false; }
				if (Match->GetMatchPhase() != EHALMatchPhase::Playing) { PC->HALStartMatch(); return false; }
				// Runtime-only aerial isolation prevents unrelated map hazards and contact from changing this test.
				int32 Index = 0;
				for (auto* V : {Own, Other})
				{
					auto* Body = Cast<UPrimitiveComponent>(V->GetRootComponent());
					Body->SetEnableGravity(false); Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
					Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					V->SetActorLocation(FVector(Index++ * 3000, 0, 5000), false, nullptr, ETeleportType::TeleportPhysics);
				}
				FPacketSimulationSettings Settings; Settings.PktLag = 80; World->GetNetDriver()->SetPacketSimulationSettings(Settings);
				PC->ConsoleCommand(FString::Printf(TEXT("HALStun %d 3"),Own->GetPlayerState()->GetPlayerId()));
				PC->ConsoleCommand(FString::Printf(TEXT("HALStun %d 3"),Other->GetPlayerState()->GetPlayerId()));
				Test->TestTrue(TEXT("Host console stuns host"),ASC->IsStunned());
				Test->TestTrue(TEXT("Host console stuns remote player"),Remote->IsStunned());
				Start = Now; Prepared = true;
				UE_LOG(LogTemp,Display,TEXT("HAL Stun smoke: Applied=1 HostOutgoingLag=80ms GameTime=%.3f"),Now);
				return false;
			}
			SawOther |= Remote->IsStunned();
			if (!SawActive && ASC->IsStunned() && Physics.bReady && Physics.Value.bControlsBlocked)
			{
				SawActive = true; ActiveRevision = Physics.Value.Revision;
				HP = ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute());
				Test->TestFalse(TEXT("Skill input refused while stunned"), Own->RequestPrimarySkill());
				FVehicleInputCmd Cmd; Cmd.Throttle = .2f; Cmd.Steering = .5f; Cmd.bHandbrake = true;
				Movement->SetInputCommand(Cmd);
				UE_LOG(LogTemp,Display,TEXT("HAL Stun smoke: Host=%d Active=1 Revision=%u Frame=%d"),Host,ActiveRevision,Physics.ServerFrame);
			}
			if (Host && Prepared && !Short && Now-Start > .5)
			{
				Short = true;
				Test->TestTrue(TEXT("Shorter cross-source repeat accepted"), ASC->ApplyStun(.2f,Other));
				Remote->ApplyStun(.2f,Own);
				Test->TestTrue(TEXT("Shorter repeat retains longer deadline"),ASC->GetEffectTimeRemaining(HALGameplayTags::State_Stun)>2);
			}
			if (Host && Prepared && !Extended && Now-Start > 1)
			{
				Extended = true;
				Test->TestEqual(TEXT("Refresh does not publish a false physics boundary"), Physics.Value.Revision,ActiveRevision);
				ASC->ApplyStun(4,Other); Remote->ApplyStun(4,Own);
			}
			if (Host && Extended && !PastOriginal && Now-Start > 3.4)
			{
				PastOriginal = true;
				Test->TestTrue(TEXT("Both survive original expiry"),ASC->IsStunned() && Remote->IsStunned());
				Test->TestEqual(TEXT("One stun contribution after extension"),ASC->GetTagCount(HALGameplayTags::State_Stun),1);
			}
			if (SawActive && SawOther && !ASC->IsStunned() && !Remote->IsStunned() && Physics.bReady && !Physics.Value.bControlsBlocked)
			{
				if (Host && Other->GetPlayerState()->GetPlayerName() != TEXT("HAL_Stun_Complete")) { return false; }
				Test->TestEqual(TEXT("Only real expiry changes physics version"),Physics.Value.Revision,ActiveRevision+1);
				Test->TestEqual(TEXT("Stun does not damage"),ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute()),HP);
				if (Host) { Test->TestTrue(TEXT("Extension checks executed"),Short && Extended && PastOriginal); }
				Movement->ResetInputCommand(); Complete = FPlatformTime::Seconds();
				if (!Host) { PC->SetName(TEXT("HAL_Stun_Complete")); }
				UE_LOG(LogTemp,Display,TEXT("HAL Stun smoke: Host=%d Complete=1 Revision=%u Rewinds=%d Resimulations=%d"),
					Host,Physics.Value.Revision,Physics.RewindRequests,Movement->GetResimulationRunCount());
			}
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStunNetworkTest,"HAL.NetworkSmoke.Stun",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALStunNetworkTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALStunNetworkTests::FRun>(this)); return true;
}
#endif
