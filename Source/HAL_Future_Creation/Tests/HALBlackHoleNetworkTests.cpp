#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALBlackHole.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALMovementAttributeSet.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace
{
	class FBlackHoleNetworkProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase& Test;
		double Deadline = FPlatformTime::Seconds() + 90;
		double CompleteAt = -1;
		float SpawnAfter = -1;
		bool bPrepared = false, bAligned = false, bSpawned = false, bSawField = false, bSawExpansion = false, bSawHold = false;
		float PeakInwardSpeed = 0;
		bool bSawEntrySlowdown = false;
		bool bSawSwirlMotion = false;
		bool bSawDrag = false;
		bool bSawGravityDrive = false;
		bool bSawDragTag = false, bSawReady = false, bSawUnwantedSlippery = false;
		float MinimumAttributeGrip = 1, MinimumPhysicsGrip = 1;
		TWeakObjectPtr<AHALBlackHole> Observed;
	public:
		explicit FBlackHoleNetworkProbe(FAutomationTestBase& InTest) : Test(InTest) {}
		bool Update() override
		{
			if (CompleteAt > 0) { return FPlatformTime::Seconds() >= CompleteAt; }
			if (FPlatformTime::Seconds() > Deadline)
			{
				Test.AddError(FString::Printf(TEXT("Black hole paired NetTest timeout. Prepared=%d Aligned=%d Spawned=%d Seen=%d Expand=%d Hold=%d Peak=%.1f"),
					bPrepared, bAligned, bSpawned, bSawField, bSawExpansion, bSawHold, PeakInwardSpeed));
				return true;
			}
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* W = Context.World();
				if (W && W->IsGameWorld() && (W->IsNetMode(NM_Client) || W->IsNetMode(NM_ListenServer))) { World = W; break; }
			}
			if (!World) { return false; }
			const bool bHost = World->IsNetMode(NM_ListenServer);
			auto* PC = Cast<AHALPlayerController>(World->GetFirstPlayerController());
			auto* Own = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { if (*It != Own && It->GetPlayerState()) { Other = *It; break; } }
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!Own || !Other || !Match || !Other->GetPlayerState()) { return false; }
			if (!bPrepared)
			{
				for (auto* Pawn : {Own, Other})
				{
					auto* Body = CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent());
					Body->SetEnableGravity(false); Body->SetLinearDamping(0);
				}
				PC->SetName(bHost ? TEXT("HAL_BlackHole_Host") : TEXT("HAL_BlackHole_Client")); bPrepared = true;
			}
			if (Match->GetMatchPhase() != EHALMatchPhase::Playing)
			{
				if (bHost && Other->GetPlayerState()->GetPlayerName() == TEXT("HAL_BlackHole_Client")) { PC->HALStartMatch(); }
				return false;
			}
			if (bHost && !bAligned)
			{
				FPacketSimulationSettings Lag; Lag.PktLag = 80; World->GetNetDriver()->SetPacketSimulationSettings(Lag);
				for (auto* Pawn : {Own, Other})
				{
					auto* Body = CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent());
					Body->SetWorldLocationAndRotation(FVector(0, Pawn == Own ? 1000 : -1000, 10000), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					Pawn->ForceNetUpdate();
				}
				SpawnAfter = World->GetTimeSeconds() + 2; bAligned = true;
			}
			if (bHost && !bSpawned && World->GetTimeSeconds() >= SpawnAfter)
			{
				for (auto* Pawn : {Own, Other})
				{
					auto* Body = CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent());
					Body->SetPhysicsLinearVelocity(-Pawn->GetActorLocation().GetSafeNormal2D() * 500.);
				}
				const FTransform Transform(FVector(0,0,10000));
				auto* Hole = World->SpawnActorDeferred<AHALBlackHole>(AHALBlackHole::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				FHALBlackHoleConfig Config; Config.ExpansionSeconds = 1; Config.DurationSeconds = 4;
				Config.MaximumRadius = 2500; Config.ContinuousAcceleration = 40; Config.PulseDeltaSpeed = 400; Config.PulseInterval = 1;
				Config.EntrySpeedMultiplier = .4f; // Preserve entry regression independently of the gentler native default.
				Test.TestTrue(TEXT("Server configures deferred field"), Hole->ConfigureBlackHole(Config));
				Hole->FinishSpawning(Transform); bSpawned = true;
			}
			for (TActorIterator<AHALBlackHole> It(World); It; ++It)
			{
				// Team maps may contain long-lived Blueprint fields. Observe only this native fixture's volume.
				if (It->GetClass() != AHALBlackHole::StaticClass() || !It->GetActorLocation().Equals(FVector(0,0,10000),.1)) { continue; }
				Observed = *It; bSawField = true;
				bSawExpansion |= It->GetStage() == EHALBlackHoleStage::Expanding;
				bSawHold |= It->GetStage() == EHALBlackHoleStage::PreparingPulse && FMath::IsNearlyEqual(It->GetCurrentRadius(),2500.f);
				if (It->GetStage() == EHALBlackHoleStage::Expanding && Own->GetActorLocation().Size2D() <= It->GetCurrentRadius())
				{
					const FVector Direction = -Own->GetActorLocation().GetSafeNormal2D();
					const double Inward = FVector::DotProduct(Direction, CastChecked<UPrimitiveComponent>(Own->GetRootComponent())->GetPhysicsLinearVelocity());
					bSawEntrySlowdown |= Inward > 50 && Inward < 350;
				}
			}
			if (bSawField)
			{
				if (auto* ASC = Cast<UHALAbilitySystemComponent>(Own->GetAbilitySystemComponent()))
				{
					const auto* Values = ASC->GetSet<UHALMovementAttributeSet>();
					const auto MovementStatus = Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetMovementEffectStatus();
					bSawDragTag |= ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Drag);
					bSawUnwantedSlippery |= ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_SlipperyArea);
					bSawReady |= MovementStatus.bReady;
					bSawGravityDrive |= ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Drag)
						&& MovementStatus.bReady && MovementStatus.Value.GravityDrive.Source.IsValid()
						&& MovementStatus.Value.GravityDrive.Center.Equals(FVector(0,0,10000),.1)
						&& FMath::IsNearlyEqual(MovementStatus.Value.GravityDrive.Strength,.85f)
						&& MovementStatus.Value.GravityDrive.StartFrame >= 0;
					if (Values) { MinimumAttributeGrip = FMath::Min(MinimumAttributeGrip,Values->GetAreaGripDelta()+1.f); }
					MinimumPhysicsGrip = FMath::Min(MinimumPhysicsGrip,MovementStatus.Value.GripScale);
					bSawDrag |= ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Drag)
						&& !ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_SlipperyArea)
						&& Values && FMath::IsNearlyEqual(Values->GetAreaGripDelta(),-.8f,1.e-4f)
						&& MovementStatus.bReady && FMath::IsNearlyEqual(MovementStatus.Value.GripScale,.2f,1.e-4f);
				}
				const FVector Direction = -Own->GetActorLocation().GetSafeNormal2D();
				const FVector Tangent = FVector::CrossProduct(FVector::UpVector,-Direction);
				bSawSwirlMotion |= FVector::DotProduct(Tangent,CastChecked<UPrimitiveComponent>(Own->GetRootComponent())->GetPhysicsLinearVelocity()) > 5;
				PeakInwardSpeed = FMath::Max(PeakInwardSpeed, float(FVector::DotProduct(Direction, CastChecked<UPrimitiveComponent>(Own->GetRootComponent())->GetPhysicsLinearVelocity())));
			}
			if (bSawField && !Observed.IsValid())
			{
				// Actor destruction and PlayerState tags travel on different channels; await both within the deadline.
				if (auto* ASC = Own->GetAbilitySystemComponent(); ASC && ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Drag)) { return false; }
				if (Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetMovementEffectStatus().Value.GravityDrive.Source.IsValid()) { return false; }
				Test.TestTrue(TEXT("Authority-stamped gravity drive reaches local owner history"),bSawGravityDrive);
				Test.TestTrue(TEXT("Synchronized expansion observed"), bSawExpansion);
				Test.TestTrue(TEXT("Authority entry slowdown observed on local owned moving vehicle"), bSawEntrySlowdown);
				Test.TestTrue(TEXT("Local owned vehicle receives counterclockwise swirl"), bSawSwirlMotion);
				Test.TestTrue(TEXT("Drag state and selected grip replicated to local owner"), bSawDrag);
				Test.AddInfo(FString::Printf(TEXT("Drag probe Host=%d Tag=%d UnwantedSlip=%d Ready=%d MinAttributeGrip=%.9f MinPhysicsGrip=%.9f"),
					bHost,bSawDragTag,bSawUnwantedSlippery,bSawReady,MinimumAttributeGrip,MinimumPhysicsGrip));
				if (auto* ASC = Own->GetAbilitySystemComponent())
				{ Test.TestFalse(TEXT("Field expiration clears Drag state"), ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Drag)); }
				Test.TestTrue(TEXT("Replicated maximum range and hold observed"), bSawHold);
				Test.TestTrue(TEXT("Local owned vehicle pulled by authority physics actions"), PeakInwardSpeed > 200);
				bool bFixtureRemains = false;
				for (TActorIterator<AHALBlackHole> It(World); It; ++It)
				{ bFixtureRemains |= It->GetClass() == AHALBlackHole::StaticClass() && It->GetActorLocation().Equals(FVector(0,0,10000),.1); }
				Test.TestFalse(TEXT("No local duplicate fixture black hole remains"), bFixtureRemains);
				UE_LOG(LogTemp, Display, TEXT("BlackHole network Host=%d PeakInwardSpeed=%.2f Resimulations=%d"), bHost,
					PeakInwardSpeed, Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetResimulationRunCount());
				CompleteAt = FPlatformTime::Seconds() + (bHost ? 6 : 2);
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBlackHoleNetworkTest, "HAL.NetworkSmoke.BlackHole.AttractionLifecycle",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALBlackHoleNetworkTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FBlackHoleNetworkProbe>(*this));
	return true;
}
#endif
