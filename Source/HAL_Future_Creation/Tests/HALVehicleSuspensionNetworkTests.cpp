#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehiclePresentationTestSupport.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace
{
	/** Real replicated vehicles; identical local query-only platforms isolate the visual source. */
	class FSuspensionLiveTest : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds() + 90, Complete = -1;
		bool bPrepared = false, bAligned = false, bFlat = false, bSteps = false;
		TArray<TWeakObjectPtr<AActor>> FixtureActors;
		void Platform(UWorld* World, const FVector& Center, const FVector& Extent)
		{
			auto* Actor = World->SpawnActor<AActor>();
			auto* Box = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Box);
			Box->SetMobility(EComponentMobility::Movable);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent(); Actor->SetActorLocation(Center);
			FixtureActors.Add(Actor);
		}
		static bool AllNeutral(const FHALVehiclePresentationFrame& Frame)
		{
			return Frame.bValid && Frame.SuspensionTraceCount == 4 && Frame.FL.bSuspensionSupported && Frame.FR.bSuspensionSupported
				&& Frame.BL.bSuspensionSupported && Frame.BR.bSuspensionSupported && FMath::Abs(Frame.FL.SuspensionOffsetCm) < .25f
				&& FMath::Abs(Frame.FR.SuspensionOffsetCm) < .25f && FMath::Abs(Frame.BL.SuspensionOffsetCm) < .25f && FMath::Abs(Frame.BR.SuspensionOffsetCm) < .25f;
		}
		static bool SingleStep(const FHALVehiclePresentationFrame& Frame)
		{
			return Frame.bValid && Frame.SuspensionTraceCount == 4 && Frame.FL.bSuspensionSupported && FMath::Abs(Frame.FL.SuspensionOffsetCm - 7) < .25f
				&& FMath::Abs(Frame.FR.SuspensionOffsetCm) < .25f && FMath::Abs(Frame.BL.SuspensionOffsetCm) < .25f && FMath::Abs(Frame.BR.SuspensionOffsetCm) < .25f;
		}
	public:
		explicit FSuspensionLiveTest(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual ~FSuspensionLiveTest() override { for (const auto& Actor : FixtureActors) { if (Actor.IsValid()) { Actor->Destroy(); } } }
		virtual bool Update() override
		{
			if (Complete > 0) { return FPlatformTime::Seconds() - Complete > 4; }
			if (FPlatformTime::Seconds() > Deadline)
			{ Test->AddError(TEXT("VP3 suspension requires matching Listen Server/client processes; timed out.")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* Candidate = Context.World();
				if (Candidate && Candidate->IsGameWorld() && (Candidate->GetNetMode() == NM_Client || Candidate->GetNetMode() == NM_ListenServer))
				{ World = Candidate; break; }
			}
			if (!World) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			AHALPlayerController* PC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			{ if (auto* Candidate = Cast<AHALPlayerController>(It->Get()); Candidate && Candidate->IsLocalController()) { PC = Candidate; break; } }
			auto* Own = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { if (*It != Own && It->GetPlayerState()) { Other = *It; break; } }
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!Own || !Other || !Own->GetPlayerState() || !Other->GetPlayerState() || !Match) { return false; }
			if (!bPrepared)
			{
				FHALPresentationTestAccess::EnableSuspensionProbe(Own);
				FHALPresentationTestAccess::EnableSuspensionProbe(Other);
				FHALPresentationTestAccess::SetInput(Own, 0);
				Platform(World, FVector(0,0,9990), FVector(5000,5000,10));
				PC->SetName(bHost ? TEXT("HAL_VP3ReadyHost") : TEXT("HAL_VP3ReadyClient"));
				bPrepared = true;
			}
			// Fixture only: these cars float above query-only terrain, not a physical suspension test.
			for (auto* Pawn : { Own, Other }) { FHALPresentationTestAccess::Body(Pawn)->SetEnableGravity(false); }
			if (Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers)
			{ if (bHost) { PC->HALStartMatch(); } return false; }
			const FString OtherName = Other->GetPlayerState()->GetPlayerName();
			if (!OtherName.StartsWith(TEXT("HAL_VP3"))) { return false; }
			if (bHost && !bAligned)
			{
				int32 Index = 0;
				for (auto* Pawn : { Own, Other })
				{
					auto* Component = Pawn->FindComponentByClass<UHALVehiclePresentationComponent>();
					auto* Body = FHALPresentationTestAccess::Body(Pawn);
					Body->SetWorldLocationAndRotation(FVector(Index++ ? 1000 : -1000,0,10000), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
					const FVector Center = FHALPresentationTestAccess::WheelCenter(Component, EHALVehicleWheel::FL);
					Body->SetWorldLocation(Body->GetComponentLocation() + FVector(0,0,10000 + FHALPresentationTestAccess::WheelRadius(Component, EHALVehicleWheel::FL) - Center.Z), false, nullptr, ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					Pawn->ForceNetUpdate();
				}
				bAligned = true; PC->SetName(TEXT("HAL_VP3AlignedHost"));
			}
			auto* Local = Own->FindComponentByClass<UHALVehiclePresentationComponent>();
			auto* Remote = Other->FindComponentByClass<UHALVehiclePresentationComponent>();
			const auto LocalFrame = Local->GetPresentationFrame(), RemoteFrame = Remote->GetPresentationFrame();
			if (!bFlat && Own->GetActorLocation().Z > 9900 && Other->GetActorLocation().Z > 9900 && AllNeutral(LocalFrame) && AllNeutral(RemoteFrame))
			{ bFlat = true; PC->SetName(bHost ? TEXT("HAL_VP3FlatHost") : TEXT("HAL_VP3FlatClient")); }
			if (bFlat && !bSteps && (bHost ? OtherName.Contains(TEXT("Flat")) : (OtherName.Contains(TEXT("Steps")) || OtherName.Contains(TEXT("Done")))))
			{
				for (auto* Component : { Local, Remote })
				{
					const FVector Center = FHALPresentationTestAccess::WheelCenter(Component, EHALVehicleWheel::FL);
					Platform(World, FVector(Center.X,Center.Y,10003.5), FVector(25,25,3.5));
				}
				bSteps = true; PC->SetName(bHost ? TEXT("HAL_VP3StepsHost") : TEXT("HAL_VP3StepsClient"));
			}
			if (bSteps && SingleStep(LocalFrame) && SingleStep(RemoteFrame) && (OtherName.Contains(TEXT("Steps")) || OtherName.Contains(TEXT("Done"))))
			{
				Test->TestTrue(TEXT("Local vehicle and replicated observer both passed neutral reference"), bFlat);
				Test->TestTrue(TEXT("A raised FL pad changes only that wheel in both network roles"), true);
				Test->TestEqual(TEXT("Two observed cars make eight local queries, without per-wheel replication"), LocalFrame.SuspensionTraceCount + RemoteFrame.SuspensionTraceCount, 8);
				UE_LOG(LogTemp, Display, TEXT("HAL VP3 smoke: Host=%d Complete=1 LocalFL=%.3f RemoteFL=%.3f TraceCount=%d LocalQueryUs=%.3f RemoteQueryUs=%.3f Resimulations=%d ActualRig=1 LocalQueryOnlyPlatforms=1 GravityDisabledFixture=1"),
					bHost ? 1 : 0, LocalFrame.FL.SuspensionOffsetCm, RemoteFrame.FL.SuspensionOffsetCm, LocalFrame.SuspensionTraceCount + RemoteFrame.SuspensionTraceCount,
					LocalFrame.SuspensionQueryMicroseconds, RemoteFrame.SuspensionQueryMicroseconds, Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetResimulationRunCount());
				PC->SetName(bHost ? TEXT("HAL_VP3DoneHost") : TEXT("HAL_VP3DoneClient")); Complete = FPlatformTime::Seconds();
			}
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSuspensionNetworkTest, "HAL.NetworkSmoke.VehicleVisualSuspension",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALSuspensionNetworkTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSuspensionLiveTest>(this)); return true;
}
#endif
