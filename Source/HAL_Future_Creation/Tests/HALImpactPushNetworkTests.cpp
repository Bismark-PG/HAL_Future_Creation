#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehiclePresentationTestSupport.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALImpactReceiverComponent.h"
#include "../VehicleKnockbackSettings.h"
#include "../VehicleHealthComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../CombatResolver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace
{
	class FImpactPushLiveTest : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds()+90, Complete = -1;
		double NextDiagnosticAt = 0;
		FString Diagnostics;
		bool bPrepared = false, bAligned = false, bFired = false;
		float AlignedAt = 0, ShortPeak = 0, LongPeak = 0;
	public:
		explicit FImpactPushLiveTest(FAutomationTestBase* InTest) : Test(InTest) {}
		bool Update() override
		{
			if (Complete > 0) { return FPlatformTime::Seconds()-Complete > 4; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Impact push smoke timed out: ")+Diagnostics); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{ auto* W = Context.World(); if (W && W->IsGameWorld() && (W->GetNetMode() == NM_Client || W->GetNetMode() == NM_ListenServer)) { World = W; break; } }
			if (!World) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			AHALPlayerController* PC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			{ if (auto* C = Cast<AHALPlayerController>(It->Get()); C && C->IsLocalController()) { PC = C; break; } }
			auto* Own = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { if (*It != Own && It->GetPlayerState()) { Other = *It; break; } }
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!Own || !Other || !Match || !Own->GetPlayerState() || !Other->GetPlayerState()) { return false; }
			if (!bPrepared)
			{
				// Set fixture properties once. Repeated GT writes can overwrite PT object-state transitions.
				for (auto* Pawn : {Own,Other})
				{ auto* Body = CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent()); Body->SetEnableGravity(false); Body->SetLinearDamping(0); }
				FHALPresentationTestAccess::SetInput(Own,0);
				PC->SetName(bHost ? TEXT("HAL_PushReadyH") : TEXT("HAL_PushReadyC")); bPrepared = true;
			}
			const FString OtherName = Other->GetPlayerState()->GetPlayerName();
			if (Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers)
			{ if (bHost && OtherName.StartsWith(TEXT("HAL_Push"))) { PC->HALStartMatch(); } return false; }
			if (!OtherName.StartsWith(TEXT("HAL_Push"))) { return false; }
			if (bHost && !bAligned)
			{
				int32 Index = 0;
				for (auto* Pawn : {Own,Other})
				{
					auto* Body = CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent());
					Body->SetWorldLocationAndRotation(FVector(0,Index++ ? 2000 : -2000,10000),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); Pawn->ForceNetUpdate();
					// The saved arena can hit a spawn before automation starts. Reset only this isolated fixture.
					Pawn->GetAbilitySystemComponent()->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),100);
				}
				bAligned = true; AlignedAt = World->GetTimeSeconds(); PC->SetName(TEXT("HAL_PushAlignedH"));
			}
			if (!bHost && !bAligned && Own->GetActorLocation().Z > 9000 && Other->GetActorLocation().Z > 9000)
			{ bAligned = true; PC->SetName(TEXT("HAL_PushAlignedC")); }
			FVehicleKnockbackConfig Table; FHALImpactPushRules Push;
			if (!GetDefault<UVehicleKnockbackSettings>()->GetRuntimeRules(Table) || !GetDefault<UVehicleKnockbackSettings>()->GetImpactPushRules(Push)) { return false; }
			if (bHost && bAligned && !bFired && OtherName.Contains(TEXT("Aligned")) && World->GetTimeSeconds()-AlignedAt > .5f)
			{
				auto* Source = World->SpawnActor<AActor>();
				for (auto* Pawn : {Own,Other})
				{
					FHALAttackSourceSnapshot Snapshot; Snapshot.SourceActor = Source; Snapshot.Impact.Version = EHALImpactVersion::TieredV1;
					Snapshot.Impact.Tier = EHALImpactTier::Heavy;
					Snapshot.Impact.PushPreset = Pawn == Own ? EHALImpactPushPreset::Short : EHALImpactPushPreset::Long;
					FHALCombatAction Action; Action.Begin(*Source,Snapshot);
					FHALCombatHitRequest Hit; Hit.AttackId = Action.GetSnapshot().AttackId; Hit.bApplyDamage = false; Hit.bApplyImpact = true;
					Hit.Impact.Mode = EHALImpactMode::ExplicitTier; Hit.Impact.Tier = Action.GetSnapshot().Impact.Tier; Hit.Impact.PushPreset = Action.GetSnapshot().Impact.PushPreset;
					Hit.Hit.TargetActor = Pawn; Hit.Hit.TargetPhysicsBody = CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent()); Hit.Hit.ImpactNormal = -FVector::ForwardVector;
					const auto Result = FCombatResolver::ResolveCombatHit(Action,Hit);
					Test->TestTrue(TEXT("Authority Heavy preset queued through production impact action"), Result.bHitAccepted && Result.bImpactApplied);
					Test->TestEqual(TEXT("Only horizontal response changes"), Result.RuleMinimumUpwardSpeed,Table.HeavyTier.TargetVerticalSpeed);
					Test->TestTrue(TEXT("Expected scaled delta queued"), Result.RuleDeltaVelocity.Equals(FVector(Table.HeavyTier.HorizontalDeltaSpeed*Push.GetMultiplier(Hit.Impact.PushPreset),0,0),.01));
				}
				Source->Destroy(); bFired = true; PC->SetName(TEXT("HAL_PushFiredH"));
			}
			auto* Near = bHost ? Own : Other;
			auto* Far = bHost ? Other : Own;
			const auto& NearImpact = Near->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact();
			const auto& FarImpact = Far->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact();
			Diagnostics = FString::Printf(TEXT("Host=%d Aligned=%d Fired=%d Other=%s Own=%s Near=%s Tier/Preset=%d/%d Target=%.1f V=%s Far=%s Tier/Preset=%d/%d Target=%.1f V=%s Peaks=%.1f/%.1f HP=%.1f/%.1f"),
				bHost?1:0,bAligned?1:0,bFired?1:0,*OtherName,*Own->GetName(),*Near->GetName(),int32(NearImpact.EffectiveTier),int32(NearImpact.PushPreset),NearImpact.DesiredHorizontalDeltaSpeed,
				*CastChecked<UPrimitiveComponent>(Near->GetRootComponent())->GetPhysicsLinearVelocity().ToCompactString(),*Far->GetName(),int32(FarImpact.EffectiveTier),int32(FarImpact.PushPreset),FarImpact.DesiredHorizontalDeltaSpeed,
				*CastChecked<UPrimitiveComponent>(Far->GetRootComponent())->GetPhysicsLinearVelocity().ToCompactString(),ShortPeak,LongPeak,Near->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(),Far->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP());
			Diagnostics += FString::Printf(TEXT(" PTSpeed=%.1f/%.1f Awake=%d/%d Z=%.1f/%.1f"),Near->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetStabilityObservation().Speed,
				Far->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetStabilityObservation().Speed,CastChecked<UPrimitiveComponent>(Near->GetRootComponent())->IsAnyRigidBodyAwake()?1:0,
				CastChecked<UPrimitiveComponent>(Far->GetRootComponent())->IsAnyRigidBodyAwake()?1:0,Near->GetActorLocation().Z,Far->GetActorLocation().Z);
			if (bAligned && FPlatformTime::Seconds() > NextDiagnosticAt)
			{ UE_LOG(LogTemp,Display,TEXT("HAL ImpactPush probe: %s"),*Diagnostics); NextDiagnosticAt=FPlatformTime::Seconds()+2; }
			if (NearImpact.PushPreset != EHALImpactPushPreset::Short || FarImpact.PushPreset != EHALImpactPushPreset::Long
				|| NearImpact.EffectiveTier != EHALImpactTier::Heavy || FarImpact.EffectiveTier != EHALImpactTier::Heavy) { return false; }
			ShortPeak = FMath::Max(ShortPeak,float(CastChecked<UPrimitiveComponent>(Near->GetRootComponent())->GetPhysicsLinearVelocity().X));
			LongPeak = FMath::Max(LongPeak,float(CastChecked<UPrimitiveComponent>(Far->GetRootComponent())->GetPhysicsLinearVelocity().X));
			const float ShortTarget = Table.HeavyTier.HorizontalDeltaSpeed*Push.ShortMultiplier, LongTarget = Table.HeavyTier.HorizontalDeltaSpeed*Push.LongMultiplier;
			if (ShortPeak < ShortTarget*.7f || LongPeak < LongTarget*.7f || (bHost && !OtherName.Contains(TEXT("Done")))) { return false; }
			Test->TestEqual(TEXT("Replicated Short target matches shared rules"), NearImpact.DesiredHorizontalDeltaSpeed,ShortTarget);
			Test->TestEqual(TEXT("Replicated Long target matches shared rules"), FarImpact.DesiredHorizontalDeltaSpeed,LongTarget);
			Test->TestTrue(TEXT("Both roles observe larger actual Long horizontal velocity"), LongPeak > ShortPeak*1.4f);
			Test->TestTrue(TEXT("No damage introduced"), Near->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP() == 100 && Far->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP() == 100);
			UE_LOG(LogTemp,Display,TEXT("HAL ImpactPush smoke: Host=%d Complete=1 ShortTarget=%.3f LongTarget=%.3f ShortPeak=%.3f LongPeak=%.3f BothHeavy=1 HP=100/100 Resimulations=%d"),bHost?1:0,ShortTarget,LongTarget,ShortPeak,LongPeak,Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetResimulationRunCount());
			PC->SetName(bHost ? TEXT("HAL_PushDoneH") : TEXT("HAL_PushDoneC")); Complete = FPlatformTime::Seconds();
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactPushNetworkTest,"HAL.NetworkSmoke.ImpactPushPresets",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactPushNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FImpactPushLiveTest>(this)); return true; }
#endif
