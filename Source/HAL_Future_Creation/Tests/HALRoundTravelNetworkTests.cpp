#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALPracticeRoomGameMode.h"
#include "../HALRoomComponent.h"
#include "../HALRoomConfig.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerState.h"
#include "../HALPlayerController.h"
#include "../HALNetworkGameInstance.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALMovementAttributeSet.h"
#include "../HALMovementBoostEffect.h"
#include "../HALImpactImmunityEffects.h"
#include "../HALGameplayTags.h"
#include "../VehicleDefinition.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HALWidgetExtensionTestAccess.h"
#include "../HALMatchBallLifecycle.h"
#include "../BasicBallActor.h"
#include "Components/SphereComponent.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "HALSpecialBallNetworkCheck.h"

namespace
{
	/** Enabled only by the match-ball fixture. All clients must see the original before the host removes it. */
	struct FMatchBallNetworkCheck
	{
		const bool Enabled = FParse::Param(FCommandLine::Get(), TEXT("HALMatchBallFixture"));
		uint32 Round = 0;
		bool Armed = false, Triggered = false, SawNotice = false, Finished = false;
		TWeakObjectPtr<ABasicBallActor> Original;
		FVector AnnouncedGround = FVector::ZeroVector;

		bool Update(FAutomationTestBase& Test, AHALMatchGameState& State, AHALPlayerController& PC, bool Host, uint32 Id)
		{
			if (!Enabled) { return true; }
			UWorld* World = State.GetWorld();
			if (Round != Id)
			{
				Round = Id;
				Armed = Triggered = SawNotice = Finished = false;
				Original.Reset();
				if (Host)
				{
					FPacketSimulationSettings Lag;
					Lag.PktLag = 80;
					World->GetNetDriver()->SetPacketSimulationSettings(Lag);
				}
			}
			if (Finished) { return true; }
			const auto* Life = State.FindComponentByClass<UHALMatchBallLifecycle>();
			if (!Life) { return false; }
			ABasicBallActor* Primary = nullptr;
			int32 Count = 0;
			for (TActorIterator<ABasicBallActor> It(World); It; ++It)
			{
				if (It->IsPrimaryMatchBall() && !It->IsActorBeingDestroyed()) { Primary = *It; ++Count; }
			}
			const FString ArmedName = FString::Printf(TEXT("BALL_READY_%u"), Id);
			if (!Armed && Count == 1)
			{
				Original = Primary;
				Armed = true;
				if (!Host) { PC.SetName(ArmedName); }
			}
			if (!Armed) { return false; }
			if (Host && !Triggered)
			{
				for (const auto& Player : State.GetRoom()->GetData().Members)
				{
					if (!Player || (Player != PC.GetPlayerState<AHALPlayerState>() && Player->GetPlayerName() != ArmedName)) { return false; }
				}
				auto* Ball = Original.Get();
				if (!Ball) { Test.AddError(TEXT("Authored primary disappeared before fixture input")); return true; }
				if (Id == 1)
				{
					Ball->SetActorLocation(Ball->GetActorLocation() + FVector(30000, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
					Ball->GetPhysicsRoot()->SetPhysicsLinearVelocity(FVector::ZeroVector);
					Ball->ForceNetUpdate();
				}
				else { Ball->Destroy(); }
				Triggered = true;
			}
			if (Life->IsReturnAnnounced())
			{
				SawNotice = true;
				AnnouncedGround = Life->GetReturnGround();
				return false;
			}
			if (!SawNotice || Count != 1 || Primary == Original.Get()) { return false; }
			Test.TestTrue(TEXT("Replacement at the announced XY"), FVector::Dist2D(Primary->GetActorLocation(), AnnouncedGround) < 100.);
			Test.TestTrue(TEXT("Replacement Free, unowned and replicated"), Primary->GetBallState() == EBasicBallState::Free
				&& !Primary->GetControlledBy() && !Primary->GetBallRepState().AttackSnapshot.AttackId.IsValid() && Primary->GetIsReplicated());
			UE_LOG(LogTemp, Display, TEXT("Match ball verified: Host=%d Round=%u Unique=%d Cue=1 Free=1"), Host, Id, Count);
			Finished = true;
			return true;
		}
	};

	struct FGroundRollNetworkCheck
	{
		const bool Enabled = FParse::Param(FCommandLine::Get(), TEXT("HALGroundRollFixture"));
		bool Ready = false, SawLaunch = false, SawFree = false, Done = false;
		double ShotAt = -1., LandedAt = -1., FreeAt = -1., PickupAt = -1.;
		FVector Start = FVector::ZeroVector;
		float TravelAtFree = 0.f, SpeedAtFree = 0.f;

		bool Update(FAutomationTestBase& Test, AHALMatchGameState& State, AHALPlayerController& PC, bool Host, const FVector& Ground)
		{
			if (!Enabled || Done) { return true; }
			UWorld* World = State.GetWorld();
			ABasicBallActor* Ball = nullptr;
			for (TActorIterator<ABasicBallActor> It(World); It; ++It) { if (It->IsPrimaryMatchBall()) { Ball = *It; break; } }
			if (!Ball) { return false; }
			const double Now = World->GetTimeSeconds();
			const float Radius = Ball->GetPhysicsRoot()->GetScaledSphereRadius();
			if (!Ready)
			{
				Ready = true;
				if (!Host) { PC.SetName(TEXT("HAL_ROLL_READY")); }
			}
			if (Host && ShotAt < 0.)
			{
				for (const auto& Player : State.GetRoom()->GetData().Members)
				{ if (!Player || (Player != PC.GetPlayerState<AHALPlayerState>() && Player->GetPlayerName() != TEXT("HAL_ROLL_READY"))) { return false; } }
				Start = Ground + FVector(-4800, 3200, Radius + 40.f);
				Ball->SetActorLocation(Start, false, nullptr, ETeleportType::TeleportPhysics);
				Ball->GetPhysicsRoot()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
				if (!Ball->BeginControl(PC.GetPawn()) || !Ball->LaunchFromControl(PC.GetPawn(), PC.GetPawn(), FVector(2200,0,0)))
				{ Test.AddError(TEXT("Could not fire the ground-roll shot")); Done = true; return true; }
				ShotAt = Now;
			}
			if (Host && LandedAt < 0. && Ball->GetActorLocation().Z <= Ground.Z + Radius + 3.f) { LandedAt = Now; }
			SawLaunch |= Ball->GetBallState() == EBasicBallState::Launched && Ball->GetBallRepState().AttackSnapshot.AttackId.IsValid();
			if (!SawLaunch || Ball->GetBallState() != EBasicBallState::Free) { return false; }
			const float Speed = Ball->GetPhysicsRoot()->GetPhysicsLinearVelocity().Size();
			if (!SawFree)
			{
				SawFree = true;
				Test.TestTrue(TEXT("Free shot retires its attack on this peer"), !Ball->GetLaunchedBy() && !Ball->GetBallRepState().AttackSnapshot.AttackId.IsValid());
				FreeAt = Now; TravelAtFree = FVector::Dist2D(Ball->GetActorLocation(), Start); SpeedAtFree = Speed;
			}
			if (Host)
			{
				if (PickupAt < 0. && Ball->CanBeControlledBy(PC.GetPawn())) { PickupAt = Now; }
				if (PickupAt < 0. || Speed > 100.f) { return false; }
				Test.TestTrue(TEXT("Ball had landed before disarming"), LandedAt >= ShotAt && FreeAt >= LandedAt);
				UE_LOG(LogTemp, Display, TEXT("HAL main ball roll: launch=2200cm/s touchdown=%.2fs free=%.2fs groundToFree=%.2fs distanceToFree=%.0fcm speedAtFree=%.0fcm/s pickupAfterFree=%.2fs below1mps=%.2fs"),
					LandedAt-ShotAt, FreeAt-ShotAt, FreeAt-LandedAt, TravelAtFree, SpeedAtFree, PickupAt-FreeAt, Now-ShotAt);
			}
			UE_LOG(LogTemp, Display, TEXT("Main ball ground release verified Host=%d Launched=1 Free=1 AttackCleared=1"), Host);
			Done = true;
			return true;
		}
	};

	class FRoundProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		FMatchBallNetworkCheck MatchBallCheck;
		FGroundRollNetworkCheck GroundRollCheck;
		FHALSpecialBallNetworkCheck SpecialBallCheck;
		bool Solo=false,Timeout=false,RoleKnown=false,Host=false,Verified=false,SawLoading=false,SawCountdown=false,Disconnect=false,BadMap=false;
		uint32 LoadedRound=0, CountdownRound=0;
		uint32 SkillRequestedRound=0, SkillVerifiedRound=0;
		double SkillRequestedAt=0;
		bool ExitLoading=false,ScreenshotRequested=false,ContractConfigured=false,Exhibition=false;
		uint32 PracticeWorldId=0, DirtyRound=MAX_uint32, ReturnedRound=0, CountedPlaying=0;
		FGuid Key,FirstEpoch,FirstFlow,RoundOneEpoch; int32 Seat=INDEX_NONE;
		FLinearColor Color; FString PracticeMap;
		double Deadline=FPlatformTime::Seconds()+180, NextAction=0, Finished=-1, ExitAt=-1;
	public:
		FRoundProbe(FAutomationTestBase* T,bool S,bool O,bool D=false,bool B=false,bool H=false,bool E=false):Test(T),Solo(S),Timeout(O),Disconnect(D),BadMap(B),ExitLoading(H),Exhibition(E) {}
		virtual bool Update() override
		{
			const double Now=FPlatformTime::Seconds();
			if (Finished>0) { return Now-Finished>2; }
			if (Now>Deadline) { Test->AddError(FString::Printf(TEXT("GF4 timeout: Host=%d Return=%u Playing=%u Loading=%d Countdown=%d"),Host,ReturnedRound,CountedPlaying,SawLoading,SawCountdown)); return true; }
			UWorld* W=nullptr;
			for (const auto& C:GEngine->GetWorldContexts()) { if (auto* V=C.World(); V && V->IsGameWorld()) { W=V; break; } }
			if (!W) { return false; }
			if (Verified && W->GetMapName().Contains(TEXT("MVPB_NetEntry")))
			{
				if (ExitLoading) { Test->TestTrue(TEXT("Host exit while loading retains the reason"),W->GetGameInstance<UHALNetworkGameInstance>()->GetConnectionMessage().ToString().Contains(TEXT("Host closed"))); }
				UE_LOG(LogTemp,Display,TEXT("HAL GF4 probe: Host=%d Solo=%d TimeoutRecovery=%d PlayingRounds=%u Identity=1 Fresh=1 Entry=1"),Host,Solo,Timeout,CountedPlaying);
				Finished=Now; return false;
			}
			auto* S=W->GetGameState<AHALMatchGameState>(); if (!S) { return false; }
			auto* R=S->GetRoom(); if (!R->GetData().bPracticeRoom) { return false; }
			AHALPlayerController* PC=nullptr;
			for (auto It=W->GetPlayerControllerIterator();It;++It) { if (auto* P=Cast<AHALPlayerController>(It->Get()); P && P->IsLocalController()) { PC=P; break; } }
			auto* PS=PC?PC->GetPlayerState<AHALPlayerState>():nullptr;
			if (!PS || PS->GetRoomData().Phase==EHALRoomPlayerPhase::Unassigned) { return false; }
			if (!RoleKnown) { Host=Solo || W->GetNetMode()==NM_ListenServer; RoleKnown=true; }
			if (!ContractConfigured && FParse::Param(FCommandLine::Get(),TEXT("HALContractUI")))
			{
				if (auto* GI=W->GetGameInstance<UHALNetworkGameInstance>())
				{ FHALWidgetExtensionTestAccess::Configure(GI,true); GI->ShowPracticeRoom(PC); ContractConfigured=true;
					UE_LOG(LogTemp,Display,TEXT("Widget contract UI configured for local role Host=%d"),Host); }
			}
			const auto D=R->GetData(); const auto P=PS->GetRoomData();
			AHALPlayerState* Other=nullptr;
			for (const auto& Member:D.Members) { if (Member && Member!=PS) { Other=Member; break; } }
			const auto AllOthersNamed=[&](const FString& Name)
			{
				if (D.Members.Num()!=(Exhibition?4:2)) { return false; }
				for (const auto& Member:D.Members) { if (!Member || (Member!=PS && Member->GetPlayerName()!=Name)) { return false; } }
				return true;
			};
			auto* Pawn=Cast<ATestVehiclePawn>(PC->GetPawn()); auto* ASC=PS->GetHALAbilitySystemComponent();
			auto* M=Pawn?Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>():nullptr;
			const auto Physics=M?M->GetMovementEffectStatus():FHALMovementStatus();
			const bool Ready=Pawn && P.ReadyAvatar==Pawn && Pawn->GetPlayerState()==PS && ASC->GetAvatarActor()==Pawn && Physics.bReady;
			const bool SkillFixture=FParse::Param(FCommandLine::Get(),TEXT("HALVehicleAbilitiesFixture"));
			if (SkillFixture && Ready)
			{
				if (!PS->AreVehicleAbilitiesReady(Pawn)) { return false; }
				Test->TestTrue(TEXT("Fixture uses explicit Definition V1"),Pawn->GetVehicleAbilityConfig().Version==EHALVehicleAbilityVersion::VehicleDefinitionV1);
				int32 Primary=0;
				for (const auto& Spec:ASC->GetActivatableAbilities())
				{
					if (!Spec.PendingRemove && Spec.GetDynamicSpecSourceTags().HasTagExact(HALGameplayTags::Input_Ability_Primary))
					{
						++Primary; Test->TestTrue(TEXT("Primary source is the current replicated Pawn"),Spec.SourceObject==Pawn);
						Test->TestEqual(TEXT("Primary matches selected model"),Spec.Ability->GetClass()->GetName(),Host?FString(TEXT("GA_MovementBoost_C")):FString(TEXT("GA_ImpactImmunity_C")));
					}
				}
				Test->TestEqual(TEXT("Exactly one owner Primary across travel"),Primary,1);
			}
			if (!Key.IsValid())
			{ Key=P.PlayerKey; Seat=P.Seat; Color=P.Color; FirstFlow=D.FlowId; PracticeMap=W->GetOutermost()->GetName(); PracticeWorldId=W->GetUniqueID(); }
			if (P.PlayerKey!=Key || Seat!=P.Seat || Color!=P.Color || D.FlowId!=FirstFlow)
			{ Test->AddError(TEXT("GF4 identity changed across worlds")); return true; }
			if (Verified)
			{
				if (!Host) { PC->SetName(TEXT("GF4_DONE")); }
				else if ((Solo || Disconnect || AllOthersNamed(TEXT("GF4_DONE"))) && ExitAt<0) { ExitAt=Now+1; }
				if (Host && ExitAt>0 && Now>=ExitAt) { ExitAt=MAX_dbl; PC->LeaveRoom(); }
				return false;
			}
			if (D.Phase==EHALRoomPhase::Loading || D.Phase==EHALRoomPhase::Countdown)
			{
				if (!Ready) { return false; }
				if (Exhibition)
				{ Test->TestEqual(TEXT("Actual Exhibition requires four members"),D.Rules.MinimumPlayers(),4); Test->TestEqual(TEXT("All four real processes travel together"),D.ExpectedPlayers,4); }
				Test->TestTrue(TEXT("Actual new World and distinct match package"),W->GetUniqueID()!=PracticeWorldId && W->GetOutermost()->GetName()!=PracticeMap);
				Test->TestTrue(TEXT("Fresh physics epoch after map travel"),FirstEpoch.IsValid() && Physics.Value.Epoch!=FirstEpoch);
				if (D.RoundId==1) { RoundOneEpoch=Physics.Value.Epoch; }
				else { Test->TestTrue(TEXT("Second round cannot reuse first round epoch"),RoundOneEpoch.IsValid() && RoundOneEpoch!=Physics.Value.Epoch); }
				Test->TestTrue(TEXT("Loading/countdown physical controls blocked"),Physics.Value.bControlsBlocked);
				const auto* H=ASC->GetSet<UHALHealthAttributeSet>();
				Test->TestTrue(TEXT("Full configured HP in the new round"),H && FMath::IsNearlyEqual(H->GetCurrentHP(),Pawn->GetVehicleDefinition()->Health.Health.MaxHP));
				Test->TestTrue(TEXT("Practice effects and cooldown do not cross maps"),!ASC->IsStunned() && !ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost)
					&& ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity)==0 && ASC->GetSet<UHALMovementAttributeSet>()->GetAccelerationMultiplierDelta()==0);
				const auto Energy = ASC->GetUltimateEnergyView();
				if (const auto* Config = R->GetServerConfig())
				{ Test->TestEqual(TEXT("Formal round keeps configured starting energy until play"), Energy.Energy, Config->StartingUltimateEnergy); }
				Test->TestFalse(TEXT("Loading and countdown do not recharge"), Energy.bRecharging);
				if (D.Phase==EHALRoomPhase::Loading)
				{
					SawLoading=true; LoadedRound=D.RoundId;
					if (Disconnect && !Host) { Verified=true; PC->LeaveRoom(); }
					if (ExitLoading)
					{
						if (!Host) { PC->SetName(TEXT("GF4_DONE")); Verified=true; }
						else if (Other && Other->GetPlayerName()==TEXT("GF4_DONE")) { Verified=true; PC->LeaveRoom(); }
					}
				}
				else
				{
					SawCountdown=true;
					if (CountdownRound!=D.RoundId) { UE_LOG(LogTemp,Display,TEXT("GF4 countdown sample: Host=%d Round=%u End=%.3f"),Host,D.RoundId,D.CountdownEndServerTime); }
					CountdownRound=D.RoundId; Test->TestTrue(TEXT("Shared future server deadline"),D.CountdownEndServerTime>0);
					if (Host && !ScreenshotRequested && FParse::Param(FCommandLine::Get(),TEXT("GF4Screenshot")))
					{ ScreenshotRequested=true; FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("GameFlow/GF4_20261103/GF4_Countdown.png"),true,false); }
				}
				return false;
			}
			if (D.Phase==EHALRoomPhase::Playing)
			{
				if (!Ready || Physics.Value.bControlsBlocked) { return false; }
				if (Timeout) { Test->AddError(TEXT("Timeout fixture must never enter Playing")); return true; }
				if (!MatchBallCheck.Update(*Test, *S, *PC, Host, D.RoundId)) { return false; }
				if (D.RoundId == 1 && !GroundRollCheck.Update(*Test, *S, *PC, Host, MatchBallCheck.AnnouncedGround)) { return false; }
				if (!SpecialBallCheck.Update(*Test, *S, *PC, Host, D.RoundId)) { return false; }
				CountedPlaying=FMath::Max(CountedPlaying,D.RoundId);
				Test->TestTrue(TEXT("Barrier and countdown observed for this round"),LoadedRound==D.RoundId && CountdownRound==D.RoundId);
				Test->TestEqual(TEXT("Explicit new round identity"),PS->GetVehicleLifecycle().RoundId,D.RoundId);
				if (SkillFixture && SkillVerifiedRound!=D.RoundId)
				{
					if (SkillRequestedRound!=D.RoundId && !ASC->GetUltimateEnergyView().bHasEnergy) { return false; }
					if (SkillRequestedRound!=D.RoundId) { SkillRequestedRound=D.RoundId; SkillRequestedAt=Now; Pawn->RequestPrimarySkill(); return false; }
					const FGameplayTag Expected=Host?HALGameplayTags::State_Movement_Boost:HALGameplayTags::State_Combat_ImpactImmune;
					if (!ASC->HasMatchingGameplayTag(Expected))
					{ if (Now-SkillRequestedAt>8) { Test->AddError(TEXT("Selected model skill did not activate through primary input RPC")); return true; } return false; }
					Test->TestFalse(TEXT("Other model effect never installed on this owner"),ASC->HasMatchingGameplayTag(Host?HALGameplayTags::State_Combat_ImpactImmune:HALGameplayTags::State_Movement_Boost));
					Test->TestTrue(TEXT("Owner receives spent energy"), ASC->GetUltimateEnergyView().Energy < 25);
					SkillVerifiedRound=D.RoundId;
					UE_LOG(LogTemp,Display,TEXT("HAL vehicle skill verified Host=%d Round=%u Source=%s Class=%s"),Host,D.RoundId,*Pawn->GetName(),*Pawn->GetVehicleAbilityConfig().Grants[0].Ability->GetName());
				}
				if (D.RoundId==1)
				{
					if (!Host) { PC->SetName(TEXT("GF4_PLAY_1")); }
					if (Host && (Solo || AllOthersNamed(TEXT("GF4_PLAY_1"))) && Now>=NextAction)
					{
						NextAction=Now+.25;
						if (Exhibition) { Test->TestTrue(TEXT("Server launch opt-in permits Exhibition host return"),W->GetAuthGameMode<AHALPracticeRoomGameMode>()->CanRequestRoundReturn()); }
						PC->HALReturnToRoom();
					}
				}
				else { Verified=true; }
				return false;
			}
			if (D.Phase!=EHALRoomPhase::Open || Now<NextAction || PC->IsRoomRequestPending()) { return false; }
			NextAction=Now+.15;
			if (D.RoundId && ReturnedRound!=D.RoundId)
			{
				if (P.Phase!=EHALRoomPlayerPhase::Selecting) { return false; }
				ReturnedRound=D.RoundId;
				Test->TestTrue(TEXT("Returned practice package in another World"),W->GetOutermost()->GetName()==PracticeMap && W->GetUniqueID()!=PracticeWorldId);
				Test->TestTrue(TEXT("Return retains model and clears confirmation/Pawn"),P.Phase==EHALRoomPlayerPhase::Selecting && !Pawn && !P.VehicleId.IsNone());
				if (Timeout || Disconnect)
				{ Test->TestTrue(TEXT("Failure has a recoverable reason"),D.FlowMessage.Contains(Timeout?TEXT("timed out"):TEXT("participant left"))); Verified=true; return false; }
			}
			if (P.Phase==EHALRoomPlayerPhase::Selecting)
			{
				if (P.VehicleId.IsNone()) { PC->SelectRoomVehicle(Host?TEXT("Car_01"):TEXT("Car_02")); }
				else { PC->ConfirmRoomVehicle(); }
			}
			else if (Ready && !Physics.Value.bControlsBlocked)
			{
				if (BadMap)
				{
					if (PC->GetLastRoomResult()==EHALRoomResult::InvalidConfiguration)
					{ Test->TestTrue(TEXT("Missing target rejects start without retiring practice Avatar"),D.Phase==EHALRoomPhase::Open && Pawn->IsConfigurationValid()); Verified=true; }
					else { PC->RequestStartMatch(); }
					return false;
				}
				if (!FirstEpoch.IsValid()) { FirstEpoch=Physics.Value.Epoch; }
				if (!Host) { PC->SetName(FString::Printf(TEXT("GF4_READY_%u"),D.RoundId)); }
				if (Host && R->CheckStart(PC)==EHALRoomResult::Accepted && (Solo || AllOthersNamed(FString::Printf(TEXT("GF4_READY_%u"),D.RoundId))))
				{
					if (DirtyRound!=D.RoundId)
					{
						for (auto It=W->GetPlayerControllerIterator();It;++It)
						{
							auto* C=Cast<AHALPlayerController>(It->Get()); auto* State=C?C->GetPlayerState<AHALPlayerState>():nullptr; if (!State) { continue; }
							auto* A=State->GetHALAbilitySystemComponent(); A->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),11);
							A->ApplyGameplayEffectToSelf(GetDefault<UHALMovementBoostEffect>(),1,A->MakeEffectContext());
							A->ApplyGameplayEffectToSelf(GetDefault<UHALImpactImmunityCooldownEffect>(),1,A->MakeEffectContext()); A->ApplyStun(10,C);
						}
						DirtyRound=D.RoundId;
					}
					PC->RequestStartMatch();
				}
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundNetworkTest,"HAL.NetworkSmoke.GameFlow.GF4.TwoRounds",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundNetworkTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,false,false)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALExhibitionRoundNetworkTest,"HAL.NetworkSmoke.GameFlow.GF6.ExhibitionFourProcessesTwoRounds",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALExhibitionRoundNetworkTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,false,false,false,false,false,true)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundTimeoutNetworkTest,"HAL.NetworkSmoke.GameFlow.GF4.TimeoutRecovery",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundTimeoutNetworkTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,false,true)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundSingleTest,"HAL.NetworkSmoke.GameFlow.GF4.SingleHost",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundSingleTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,true,false)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundDisconnectTest,"HAL.NetworkSmoke.GameFlow.GF4.ClientLeavesLoading",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundDisconnectTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,false,false,true)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundMissingTest,"HAL.NetworkSmoke.GameFlow.GF4.MissingMap",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundMissingTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,true,false,false,true)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundHostExitTest,"HAL.NetworkSmoke.GameFlow.GF4.HostLeavesLoading",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundHostExitTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,false,false,false,false,true)); return true; }
#endif
