#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehicleStabilityTestRules.h"
#include "HALVehiclePresentationTestSupport.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"

namespace
{
	// Opt-in command-line fixture, selected before any vehicle initializes. No saved asset/settings writes.
	struct FRegisterStabilityNetworkFixture
	{
		FRegisterStabilityNetworkFixture()
		{
			FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* World, const UWorld::InitializationValues)
			{
				if (!World->IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("HALStabilityNetworkFixture"))) { return; }
				auto* Settings = GetMutableDefault<UVehicleStabilitySettings>();
				if (auto* Rules = FindObject<UVehicleStabilityRules>(GetTransientPackage(), TEXT("HAL_StabilityNetworkRules")))
				{ Settings->SharedRules = Rules; }
				else
				{ auto* NewRules = MakeHALStabilityTestRules(TEXT("HAL_StabilityNetworkRules")); NewRules->AddToRoot(); Settings->SharedRules = NewRules; }
				Settings->bUseSharedRules = true;
			});
		}
	} RegisterStabilityNetworkFixture;

	class FStabilityLiveTest : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds()+90, Complete = -1;
		float StartedAt = -1;
		bool bStarted = false;
	public:
		explicit FStabilityLiveTest(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (Complete > 0) { return FPlatformTime::Seconds()-Complete > 4; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Stability network timeout; matching fixture host/client required")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* W = Context.World();
				if (W && W->IsGameWorld() && (W->GetNetMode() == NM_Client || W->GetNetMode() == NM_ListenServer)) { World = W; break; }
			}
			if (!World) { return false; }
			auto* Shared = World->GetSubsystem<UVehicleStabilitySubsystem>();
			AHALPlayerController* PC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			{ if (auto* C = Cast<AHALPlayerController>(It->Get()); C && C->IsLocalController()) { PC = C; break; } }
			auto* Own = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { if (*It != Own && It->GetPlayerState()) { Other = *It; break; } }
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!Own || !Other || !Match) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			if (!Own->FindComponentByClass<UHALImpactReceiverComponent>()->HasMatchingStabilityRules()
				|| !Other->FindComponentByClass<UHALImpactReceiverComponent>()->HasMatchingStabilityRules()) { return false; }
			if (Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers)
			{
				if (bHost) { PC->HALStartMatch(); }
				return false;
			}
			if (!bStarted)
			{
				Test->TestTrue(TEXT("Fixture selected shared zero/Min material"), Shared->GetVehicleMaterial() && Shared->GetVehicleMaterial()->Restitution == 0);
				if (bHost) { FPacketSimulationSettings Lag; Lag.PktLag = 80; World->GetNetDriver()->SetPacketSimulationSettings(Lag); }
				FHALPresentationTestAccess::SetInput(Own, bHost ? .6f : -.6f);
				StartedAt = World->GetTimeSeconds(); bStarted = true;
			}
			if (World->GetTimeSeconds()-StartedAt < 4) { return false; }
			const auto Input = Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetPresentationInput();
			if (!Input.bValid || FMath::Abs(Input.Steering) < .5f) { return false; }
			for (auto* Vehicle : {Own, Other})
			{
				auto* Body = FHALPresentationTestAccess::Body(Vehicle);
				Test->TestTrue(TEXT("Both replicated vehicles retain valid matching material and physics"),
					Body->IsSimulatingPhysics() && Body->BodyInstance.GetSimplePhysicalMaterial() == Shared->GetVehicleMaterial()
					&& Vehicle->FindComponentByClass<UHALImpactReceiverComponent>()->HasMatchingStabilityRules());
			}
			UE_LOG(LogTemp, Display, TEXT("HAL Stability smoke: Host=%d Complete=1 Signature=%u MaterialRestitution=%.3f Input=%.2f Resimulations=%d HostOutgoingLag=80ms"),
				bHost, Shared->GetSignature(), Shared->GetVehicleMaterial()->Restitution, Input.Steering,
				Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetResimulationRunCount());
			FHALPresentationTestAccess::SetInput(Own, 0);
			Complete = FPlatformTime::Seconds(); return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStabilityNetworkTest, "HAL.NetworkSmoke.VehicleStability",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALStabilityNetworkTest::RunTest(const FString& Parameters)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("HALStabilityNetworkFixture"))) { AddError(TEXT("Requires -HALStabilityNetworkFixture")); return false; }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStabilityLiveTest>(this)); return true;
}
#endif
