#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../VehicleRecoveryComponent.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"

namespace
{
	class FRecoveryLiveTest : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline=FPlatformTime::Seconds()+90, Complete=-1;
		bool bPrepared=false, bFloorPrepared=false, bSignalled=false, bSawOwnHold=false, bSawOwnExecution=false, bSawOtherExecution=false;
	public:
		explicit FRecoveryLiveTest(FAutomationTestBase* InTest):Test(InTest){}
		virtual bool Update() override
		{
			if (Complete>0) { return FPlatformTime::Seconds()-Complete>4; }
			if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Recovery network timeout; paired fixture host/client required")); return true; }
			UWorld* World=nullptr;
			for (const auto& Context:GEngine->GetWorldContexts())
			{ auto* W=Context.World(); if (W && W->IsGameWorld() && (W->GetNetMode()==NM_Client || W->GetNetMode()==NM_ListenServer)) { World=W; break; } }
			if (!World) { return false; }
			AHALPlayerController* PC=nullptr;
			for (auto It=World->GetPlayerControllerIterator();It;++It)
			{ if (auto* P=Cast<AHALPlayerController>(It->Get());P && P->IsLocalController()) { PC=P; break; } }
			auto* Own=PC ? Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr;
			ATestVehiclePawn* Other=nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World);It;++It) { if (*It!=Own && It->GetPlayerState()) { Other=*It; break; } }
			auto* Match=World->GetGameState<AHALMatchGameState>();
			if (!Own || !Other || !Match) { return false; }
			const bool Host=World->GetNetMode()==NM_ListenServer;
			if (!Own->FindComponentByClass<UHALImpactReceiverComponent>()->HasMatchingStabilityRules()
				|| !Other->FindComponentByClass<UHALImpactReceiverComponent>()->HasMatchingStabilityRules()) { return false; }
			if (Match->GetMatchPhase()!=EHALMatchPhase::Playing) { if (Host) { PC->HALStartMatch(); } return false; }
			if (!bFloorPrepared)
			{
				// Identical local fixture geometry on both peers, before readiness is signalled.
				// Isolate the physical test from team edits to the saved map's floor/obstacles.
				auto* FloorActor=World->SpawnActor<AActor>();
				auto* Floor=NewObject<UStaticMeshComponent>(FloorActor);
				FloorActor->AddInstanceComponent(Floor); FloorActor->SetRootComponent(Floor);
				Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
				Floor->SetCollisionProfileName(TEXT("BlockAll"));
				Floor->SetWorldLocation(FVector(0,0,9950)); Floor->SetWorldScale3D(FVector(80,80,1));
				Floor->RegisterComponent(); bFloorPrepared=true;
			}
			if (!bSignalled) { if (!Host) { Own->SetSelfRightHeld(true); } bSignalled=true; }
			if (!bPrepared)
			{
				if (Host)
				{
					// Wait for both automation observers before flipping either car. Otherwise a slow
					// client startup can miss the host's entire (correctly replicated) transient phase.
					if (!Other->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetStabilityObservation().bRecoveryHeld) { return false; }
					Test->TestTrue(TEXT("Neither recovery starts before fixture setup"),
						Own->FindComponentByClass<UVehicleRecoveryComponent>()->GetStatus().Sequence==0
						&& Other->FindComponentByClass<UVehicleRecoveryComponent>()->GetStatus().Sequence==0);
					UE_LOG(LogTemp,Display,TEXT("HAL Recovery fixture ready: HostUp=%.3f ClientUp=%.3f"),Own->GetActorUpVector().Z,Other->GetActorUpVector().Z);
					FPacketSimulationSettings Lag; Lag.PktLag=80; World->GetNetDriver()->SetPacketSimulationSettings(Lag);
					int32 Index=0;
					for (auto* Pawn:{Own,Other})
					{
						auto* Body=CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent());
						// Opt-in test setup only. Production recovery never teleports or clears velocities.
						const bool bClientCar=Index++!=0;
						Body->SetWorldLocationAndRotation(FVector(bClientCar ? 1000:-1000,0,10200),FRotator(0,0,bClientCar ? -90:90),false,nullptr,ETeleportType::TeleportPhysics);
						Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
						Pawn->ForceNetUpdate();
					}
					Own->SetSelfRightHeld(true);
				}
				bPrepared=true;
			}
			const auto OwnStatus=Own->FindComponentByClass<UVehicleRecoveryComponent>()->GetStatus();
			const auto OtherStatus=Other->FindComponentByClass<UVehicleRecoveryComponent>()->GetStatus();
			bSawOwnHold |= OwnStatus.Phase==EVehicleRecoveryPhase::Holding;
			bSawOwnExecution |= OwnStatus.Phase==EVehicleRecoveryPhase::Executing;
			bSawOtherExecution |= OtherStatus.Phase==EVehicleRecoveryPhase::Executing;
			if (OwnStatus.LastOutcome!=TEXT("Succeeded") || OtherStatus.LastOutcome!=TEXT("Succeeded")) { return false; }
			Test->TestTrue(TEXT("Server-owned hold progress and both execution phases observed"),bSawOwnHold && bSawOwnExecution && bSawOtherExecution);
			Test->TestTrue(TEXT("Both accepted starts and stops have sequence 2"),OwnStatus.Sequence==2 && OtherStatus.Sequence==2);
			Test->TestTrue(TEXT("Both simulated roots finish upright"),Own->GetActorUpVector().Z>.9 && Other->GetActorUpVector().Z>.9);
			const int32 Resim=Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetResimulationRunCount();
			if (!Host) { Test->TestTrue(TEXT("Real autonomous resimulation observed"),Resim>0); }
			UE_LOG(LogTemp,Display,TEXT("HAL Recovery smoke: Host=%d Complete=1 OwnSeq=%lld OtherSeq=%lld OwnOutcome=%s OtherOutcome=%s Resimulations=%d HostOutgoingLag=80ms"),
				Host,OwnStatus.Sequence,OtherStatus.Sequence,*OwnStatus.LastOutcome.ToString(),*OtherStatus.LastOutcome.ToString(),Resim);
			Own->SetSelfRightHeld(false); Complete=FPlatformTime::Seconds(); return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRecoveryNetworkTest,"HAL.NetworkSmoke.VehicleRecovery",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALRecoveryNetworkTest::RunTest(const FString& Parameters)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("HALStabilityNetworkFixture"))) { AddError(TEXT("Requires -HALStabilityNetworkFixture")); return false; }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRecoveryLiveTest>(this)); return true;
}
#endif
