#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehiclePresentationTestSupport.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALImpactAttributeSet.h"
#include "../HALGameplayTags.h"
#include "../CombatResolver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace
{
	FHALCombatHitResult Impact(AActor* Source, ATestVehiclePawn* Target, EHALImpactTier Tier, FVector Direction)
	{
		FHALAttackSourceSnapshot Snapshot; Snapshot.SourceActor = Source;
		FHALCombatAction Action; Action.Begin(*Source, Snapshot);
		FHALCombatHitRequest Request; Request.AttackId = Action.GetSnapshot().AttackId;
		Request.bApplyDamage = false; Request.bApplyImpact = true;
		Request.Impact.Mode = EHALImpactMode::PhysicalContact; Request.Impact.Tier = Tier;
		Request.Hit.TargetActor = Target; Request.Hit.TargetPhysicsBody = FHALPresentationTestAccess::Body(Target);
		Request.Hit.ImpactNormal = -Direction;
		// The natural response already meets the tier; zero supplement must still produce one visual event.
		Request.Hit.NormalImpulse = Direction * Request.Hit.TargetPhysicsBody->GetMass() * 1000;
		return FCombatResolver::ResolveCombatHit(Action, Request);
	}
	class FImpactVisualLiveTest : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds() + 90, Complete = -1;
		float LastStageAt = -1;
		bool bPrepared = false, bAligned = false, bFirstSeen = false;
		int32 Stage = 0;
		float LocalPeak = 0, RemotePeak = 0;
	public:
		explicit FImpactVisualLiveTest(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (Complete > 0) { return FPlatformTime::Seconds() - Complete > 4; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Impact visual smoke requires matching host/client; timed out.")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{ auto* W = Context.World(); if (W && W->IsGameWorld() && (W->GetNetMode() == NM_Client || W->GetNetMode() == NM_ListenServer)) { World = W; break; } }
			if (!World) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			AHALPlayerController* PC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It) { if (auto* C = Cast<AHALPlayerController>(It->Get()); C && C->IsLocalController()) { PC = C; break; } }
			auto* Own = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { if (*It != Own && It->GetPlayerState()) { Other = *It; break; } }
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!Own || !Other || !Own->GetPlayerState() || !Other->GetPlayerState() || !Match) { return false; }
			if (!bPrepared)
			{
				FHALPresentationTestAccess::EnableProbe(Own); FHALPresentationTestAccess::EnableProbe(Other);
				FHALPresentationTestAccess::SetInput(Own, 0);
				PC->SetName(bHost ? TEXT("HAL_VP4BReadyH") : TEXT("HAL_VP4BReadyC")); bPrepared = true;
			}
			for (auto* Pawn : {Own,Other}) { FHALPresentationTestAccess::Body(Pawn)->SetEnableGravity(false); }
			const FString OtherName = Other->GetPlayerState()->GetPlayerName();
			if (Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers)
			{ if (bHost && OtherName.StartsWith(TEXT("HAL_VP4B"))) { PC->HALStartMatch(); } return false; }
			if (!OtherName.StartsWith(TEXT("HAL_VP4B"))) { return false; }
			if (bHost && !bAligned)
			{
				int32 Index = 0;
				for (auto* Pawn : {Own,Other})
				{
					auto* Body = FHALPresentationTestAccess::Body(Pawn);
					Body->SetWorldLocationAndRotation(FVector(Index++ ? 1200 : -1200,0,10000), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); Pawn->ForceNetUpdate();
				}
				FHALPresentationTestAccess::EnableImpactProbe(Own); FHALPresentationTestAccess::EnableImpactProbe(Other);
				bAligned = true; LastStageAt = World->GetTimeSeconds(); PC->SetName(TEXT("HAL_VP4BAlignedH"));
			}
			auto* Local = Own->FindComponentByClass<UHALVehiclePresentationComponent>();
			auto* Remote = Other->FindComponentByClass<UHALVehiclePresentationComponent>();
			if (!bHost && !bAligned && Own->GetActorLocation().Z > 9000 && Other->GetActorLocation().Z > 9000
				&& Own->GetActorUpVector().Z > .99 && Other->GetActorUpVector().Z > .99 && Local->GetPresentationFrame().bValid && Remote->GetPresentationFrame().bValid)
			{
				FHALPresentationTestAccess::EnableImpactProbe(Own); FHALPresentationTestAccess::EnableImpactProbe(Other);
				bAligned = true; PC->SetName(TEXT("HAL_VP4BAlignedC"));
			}
			const auto LocalFrame = Local->GetPresentationFrame(), RemoteFrame = Remote->GetPresentationFrame();
			LocalPeak = FMath::Max(LocalPeak, FMath::Max(FMath::Abs(LocalFrame.ImpactPitchDegrees), FMath::Abs(LocalFrame.ImpactRollDegrees)));
			RemotePeak = FMath::Max(RemotePeak, FMath::Max(FMath::Abs(RemoteFrame.ImpactPitchDegrees), FMath::Abs(RemoteFrame.ImpactRollDegrees)));
			const auto& LocalStats = FHALPresentationTestAccess::ImpactState(Local);
			const auto& RemoteStats = FHALPresentationTestAccess::ImpactState(Remote);
			if (bHost && bAligned && Stage == 0 && OtherName.Contains(TEXT("Aligned")) && World->GetTimeSeconds()-LastStageAt > .5f)
			{
				auto* Source = World->SpawnActor<AActor>();
				for (auto* Pawn : {Own,Other})
				{
					const auto Result = Impact(Source, Pawn, Pawn == Own ? EHALImpactTier::Light : EHALImpactTier::Heavy, Pawn == Own ? FVector::ForwardVector : FVector::RightVector);
					Test->TestTrue(TEXT("Authority accepted real impact with zero natural supplement"), Result.bHitAccepted && Result.bImpactApplied && Result.RuleDeltaVelocity.IsNearlyZero());
				}
				Source->Destroy(); Stage = 1; LastStageAt = World->GetTimeSeconds(); PC->SetName(TEXT("HAL_VP4BFiringH"));
			}
			if (!bFirstSeen && LocalStats.StartedCount == 1 && RemoteStats.StartedCount == 1 && LocalPeak > .1f && RemotePeak > .1f)
			{ bFirstSeen = true; PC->SetName(bHost ? TEXT("HAL_VP4BSeenH") : TEXT("HAL_VP4BSeenC")); }
			if (bHost && Stage == 1 && bFirstSeen && OtherName.Contains(TEXT("Seen")) && World->GetTimeSeconds()-LastStageAt > .7f)
			{
				auto* Source = World->SpawnActor<AActor>();
				auto* OtherASC = CastChecked<UHALAbilitySystemComponent>(Other->GetAbilitySystemComponent());
				OtherASC->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
				const auto Immune = Impact(Source, Other, EHALImpactTier::Heavy, FVector::RightVector);
				OtherASC->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
				Test->TestTrue(TEXT("Authority immunity suppresses visual event"), Immune.ImpactSummary.EffectiveTier == EHALImpactTier::None && FHALPresentationTestAccess::ImpactState(Remote).StartedCount == 1);
				auto* OwnASC = CastChecked<UHALAbilitySystemComponent>(Own->GetAbilitySystemComponent());
				OwnASC->SetNumericAttributeBase(UHALImpactAttributeSet::GetTierReductionAttribute(), 2);
				const auto Reduced = Impact(Source, Own, EHALImpactTier::Heavy, -FVector::ForwardVector);
				OwnASC->SetNumericAttributeBase(UHALImpactAttributeSet::GetTierReductionAttribute(), 0);
				Test->TestEqual(TEXT("Reduction selects Light on authority"), Reduced.ImpactSummary.EffectiveTier, EHALImpactTier::Light);
				Impact(Source, Other, EHALImpactTier::Medium, -FVector::RightVector);
				Source->Destroy(); Stage = 2; LastStageAt = World->GetTimeSeconds(); PC->SetName(TEXT("HAL_VP4BFinalH"));
			}
			const bool bFinishedInput = bHost ? Stage == 2 && OtherName.Contains(TEXT("Done")) : OtherName.Contains(TEXT("Final")) || OtherName.Contains(TEXT("Done"));
			if (bFinishedInput && LocalStats.StartedCount == 2 && RemoteStats.StartedCount == 2 && LocalStats.Pulses.IsEmpty() && RemoteStats.Pulses.IsEmpty())
			{
				Test->TestTrue(TEXT("Both roles saw local and observer impact poses"), bFirstSeen && LocalPeak > .1f && RemotePeak > .1f);
				Test->TestTrue(TEXT("Final chassis snapshots returned to neutral"), LocalFrame.ChassisRotation.IsNearlyZero(.01f) && RemoteFrame.ChassisRotation.IsNearlyZero(.01f));
				UE_LOG(LogTemp, Display, TEXT("HAL VP4B smoke: Host=%d Complete=1 LocalStarts=%u RemoteStarts=%u LocalPeak=%.3f RemotePeak=%.3f LocalSequence=%u RemoteSequence=%u Resimulations=%d TransientConfig=1 ZeroSupplement=1 NoClientPrediction=1"),
					bHost ? 1 : 0, LocalStats.StartedCount, RemoteStats.StartedCount, LocalPeak, RemotePeak, LocalStats.LastSeenSequence, RemoteStats.LastSeenSequence, Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetResimulationRunCount());
				PC->SetName(bHost ? TEXT("HAL_VP4BDoneH") : TEXT("HAL_VP4BDoneC")); Complete = FPlatformTime::Seconds();
			}
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactVisualNetworkTest, "HAL.NetworkSmoke.VehicleImpactPresentation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactVisualNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FImpactVisualLiveTest>(this)); return true; }
#endif
