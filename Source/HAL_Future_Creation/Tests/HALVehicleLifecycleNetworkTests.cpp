#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehicleLifecycleTestSupport.h"
#include "HALRoomTestSupport.h"
#include "../HALPlayerState.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALCombatAttributeSet.h"
#include "../HALImpactAttributeSet.h"
#include "../HALMovementAttributeSet.h"
#include "../HALImpactImmunityEffects.h"
#include "../HALMovementBoostEffect.h"
#include "../HALGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	class FVehicleLifecycleNetworkProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds()+120, Complete = -1;
		uint32 Step = 0;
		FGuid FirstEpoch;
		bool Switch(UHALRoomComponent* Room, AHALPlayerController* PC, uint32 Serial, uint32 Round)
		{
			auto* PS = PC->GetPlayerState<AHALPlayerState>(); auto* Old = Cast<ATestVehiclePawn>(PC->GetPawn());
			if (!Old) { return false; }
			if (!Round)
			{
				if (PS->GetRoomData().Phase==EHALRoomPlayerPhase::Ready) { Room->Request(PC,EHALRoomAction::CancelConfirmation,NAME_None,PS->GetRoomData().SelectionRevision); }
				if (PS->GetRoomData().VehicleId.IsNone()) { Room->Request(PC,EHALRoomAction::SelectVehicle,TEXT("ProbeVehicle"),PS->GetRoomData().SelectionRevision); }
				Room->Request(PC,EHALRoomAction::ConfirmVehicle,NAME_None,PS->GetRoomData().SelectionRevision);
			}
			auto* ASC = PS->GetHALAbilitySystemComponent();
			if (Serial==2 || Round)
			{
				ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),11);
				ASC->ApplyGameplayEffectToSelf(GetDefault<UHALMovementBoostEffect>(),1,ASC->MakeEffectContext());
				ASC->ApplyGameplayEffectToSelf(GetDefault<UHALImpactImmunityCooldownEffect>(),1,ASC->MakeEffectContext());
				if (Round) { ASC->ApplyGameplayEffectToSelf(GetDefault<UHALDebugImpactImmunityEffect>(),1,ASC->MakeEffectContext()); }
				else { ASC->ApplyStun(3,PC); }
			}
			const FTransform At(Old->GetActorRotation(),Old->GetActorLocation()+FVector(0,0,40));
			auto* Next = PC->GetWorld()->SpawnActorDeferred<ATestVehiclePawn>(Old->GetClass(),At,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Next) { return false; }
			// GAS seed differences replicate through the ASC. Do not create unreplicated physics tuning differences in a network fixture.
			FHALVehicleLifecycleTestAccess::Configure(Next,Serial==2,false); Next->FinishSpawning(At);
			const bool Changed = Round ? PS->BeginNewRoundAvatar(Next,Round,Serial) : PS->ChangeVehicleAvatar(Next,Serial);
			if (!Changed || Room->CompleteAvatar(PC,PS->GetRoomData().SelectionRevision,Round)!=EHALRoomResult::Accepted) { return false; }
			Old->Destroy(); return true;
		}
	public:
		explicit FVehicleLifecycleNetworkProbe(FAutomationTestBase* T) : Test(T) {}
		virtual bool Update() override
		{
			if (Complete>0) { return FPlatformTime::Seconds()-Complete>4; }
			if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("GF-2 lifecycle network timeout; requires matching Listen Server/client processes.")); return true; }
			UWorld* World = nullptr;
			for (const auto& C : GEngine->GetWorldContexts())
			{ if (auto* W=C.World(); W && W->IsGameWorld() && (W->GetNetMode()==NM_Client || W->GetNetMode()==NM_ListenServer)) { World=W; break; } }
			if (!World) { return false; }
			const bool Host=World->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC=nullptr; AHALPlayerController* Remote=nullptr;
			for (auto It=World->GetPlayerControllerIterator(); It; ++It)
			{ if (auto* P=Cast<AHALPlayerController>(It->Get())) { if (P->IsLocalController()) { PC=P; } else { Remote=P; } } }
			auto* PS=PC ? PC->GetPlayerState<AHALPlayerState>() : nullptr; auto* Pawn=PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			auto* State=World->GetGameState<AHALMatchGameState>(); AHALPlayerState* Other=nullptr;
			if (State) { for (const auto& P : State->PlayerArray) { if (auto* S=Cast<AHALPlayerState>(P); S && S!=PS) { Other=S; break; } } }
			if (!PS || !Pawn || !State || !Other) { return false; }
			if (State->GetMatchPhase()!=EHALMatchPhase::Playing) { if (Host) { PC->HALStartMatch(); } return false; }
			auto* Room=State->GetRoom();
			if (Host && !Step)
			{
				if (!Remote || !Remote->GetPawn()) { return false; }
				auto* C=FHALRoomTestAccess::Config(Room,EHALRoomProfile::Development);
				C->Catalog->Vehicles[0].VehicleId=TEXT("ProbeVehicle"); C->Catalog->Vehicles[0].VehicleClass=Pawn->GetClass();
				if (Room->Initialize(C)!=EHALRoomResult::Accepted) { Test->AddError(TEXT("Transient GF-2 room config rejected")); return true; }
				Room->AddMember(PC); Room->AddMember(Remote);
				if (!Switch(Room,PC,1,0) || !Switch(Room,Remote,1,0)) { Test->AddError(TEXT("Initial fresh A avatar transition failed")); return true; }
				Step=1; return false;
			}
			const auto L=PS->GetVehicleLifecycle(); auto* ASC=PS->GetHALAbilitySystemComponent();
			auto* M=Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>(); const auto Physics=M ? M->GetMovementEffectStatus() : FHALMovementStatus();
			const auto* H=ASC->GetSet<UHALHealthAttributeSet>(); const auto* C=ASC->GetSet<UHALCombatAttributeSet>(); const auto* I=ASC->GetSet<UHALImpactAttributeSet>();
			const bool B=L.ChangeSerial==2;
			const bool Ready=L.ChangeSerial && L.Avatar==Pawn && ASC->GetAvatarActor()==Pawn && H && C && I && Physics.bReady
				&& FMath::IsNearlyEqual(H->GetCurrentHP(),B ? 220.f : 100.f) && FMath::IsNearlyEqual(H->GetMaxHP(),B ? 220.f : 100.f)
				&& FMath::IsNearlyEqual(C->GetAttackPower(),B ? 75.f : 20.f) && FMath::IsNearlyEqual(I->GetTierReduction(),B ? 1.f : 0.f)
				&& !ASC->IsStunned() && !ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune)
				&& ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity)==0
				&& ASC->GetSet<UHALMovementAttributeSet>()->GetAccelerationMultiplierDelta()==0;
			if (!Ready) { return false; }
			if (L.ChangeSerial==1) { FirstEpoch=Physics.Value.Epoch; }
			if (!Host && L.ChangeSerial<4) { PC->SetName(FString::Printf(TEXT("GF2_%u"),L.ChangeSerial)); }
			if (!Host && L.ChangeSerial==4 && L.RoundId==1) { PC->SetName(TEXT("GF2_4")); }
			if (Host && Step<4 && Other->GetPlayerName()==FString::Printf(TEXT("GF2_%u"),Step))
			{
				++Step;
				if (Step==4)
				{
					if (Room->Request(PC,EHALRoomAction::Start,NAME_None,PS->GetRoomData().SelectionRevision)!=EHALRoomResult::Accepted
						|| Room->AdvanceRound(1,EHALRoomPhase::Loading)!=EHALRoomResult::Accepted) { Test->AddError(TEXT("Round lock/loading contract rejected")); return true; }
				}
				if (!Switch(Room,PC,Step,Step==4 ? 1 : 0) || !Switch(Room,Remote,Step,Step==4 ? 1 : 0)) { Test->AddError(TEXT("A/B/A/new-round transition failed")); return true; }
				return false;
			}
			if (Host && Step==4 && Other->GetPlayerName()==TEXT("GF2_4") && Room->GetData().Phase==EHALRoomPhase::Loading)
			{ Room->AdvanceRound(1,EHALRoomPhase::Countdown); Room->AdvanceRound(1,EHALRoomPhase::Playing); return false; }
			if (L.ChangeSerial!=4 || L.RoundId!=1 || Room->GetData().Phase!=EHALRoomPhase::Playing || Physics.Value.bControlsBlocked) { return false; }
			Test->TestTrue(TEXT("Fresh physics epoch, not reused old history"),FirstEpoch.IsValid() && Physics.Value.Epoch!=FirstEpoch);
			Test->TestEqual(TEXT("Exactly four explicit lifecycle commits"),L.Revision,4u);
			Test->TestEqual(TEXT("Final clean round HP"),H->GetCurrentHP(),100.f);
			if (Host) { Test->TestTrue(TEXT("Duplicate round operation remains idempotent"),PS->BeginNewRoundAvatar(Pawn,1,4)); }
			else
			{
				Test->TestFalse(TEXT("Client cannot invoke authority transition"),PS->ChangeVehicleAvatar(Pawn,5));
				Test->TestFalse(TEXT("Client cannot invoke authority new round"),PS->BeginNewRoundAvatar(Pawn,2,5));
			}
			UE_LOG(LogTemp,Display,TEXT("HAL GF2 probe: Host=%d Serial=%u Revision=%u Round=%u HP=%.1f Attack=%.1f Reduction=%.1f Clean=1 Epoch=%s"),
				Host,L.ChangeSerial,L.Revision,L.RoundId,H->GetCurrentHP(),C->GetAttackPower(),I->GetTierReduction(),*Physics.Value.Epoch.ToString());
			Complete=FPlatformTime::Seconds(); return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleLifecycleNetworkTest,"HAL.NetworkSmoke.GameFlow.GF2",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALVehicleLifecycleNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FVehicleLifecycleNetworkProbe>(this)); return true; }
#endif
