#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "../HALAbilitySystemComponent.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../HALBattleTopComponent.h"
#include "../HALVehiclePresentationComponent.h"
#include "../HALGameplayTags.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Misc/Paths.h"
#include "HighResScreenshot.h"

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
		bool bFXCaptured = false;
		bool bReportedFXReadiness = false;
		bool bSawLocalSpray = false;
		bool bSawRemoteSpray = false;
		double LastSprayRequest = -1;
		bool bBattleTop;
		TOptional<FTransform> OriginalVisual;
	public:
		explicit FHALAbilityLiveSmoke(FAutomationTestBase* InTest, bool bInBattleTop = false) : Test(InTest), bBattleTop(bInBattleTop) {}
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
			const FGameplayTag ActiveTag = bBattleTop ? HALGameplayTags::State_BattleTop.GetTag() : HALGameplayTags::State_Combat_ImpactImmune.GetTag();
			const FGameplayTag CooldownTag = bBattleTop ? HALGameplayTags::Cooldown_BattleTop.GetTag() : HALGameplayTags::Cooldown_ImpactImmunity.GetTag();
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
			ATestVehiclePawn* RemoteVehicle = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It)
			{
				if (*It != Vehicle)
				{ Remote = Cast<UHALAbilitySystemComponent>(It->GetAbilitySystemComponent()); if (Remote) { RemoteVehicle = *It; break; } }
			}
			if (!Remote && !bBothExpired) { return false; }
			if (bHost && Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers && Remote)
			{
				Controller->HALStartMatch();
			}
			if (Match->GetMatchPhase() != EHALMatchPhase::Playing) { return false; }
			const auto* Presentation = bBattleTop ? Vehicle->FindComponentByClass<UHALVehiclePresentationComponent>() : nullptr;
			const auto* Mesh = Presentation ? Presentation->GetPresentationMesh() : nullptr;
			if (!bRequested)
			{
				// Client goes first: the host must observe a real client-originated GAS RPC before using its own skill.
				if (bHost && !Remote->HasMatchingGameplayTag(ActiveTag)) { return false; }
				if (bBattleTop && !Vehicle->FindComponentByClass<UHALBattleTopComponent>()->CanStart()) { return false; }
				if (bBattleTop && FApp::CanEverRender())
				{
					if (!Mesh) { return false; }
					OriginalVisual = Mesh->GetRelativeTransform();
				}
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
			const bool bLocalActive = Local->HasMatchingGameplayTag(ActiveTag);
			const bool bRemoteActive = Remote && Remote->HasMatchingGameplayTag(ActiveTag);
			if(bBattleTop&&!bReportedFXReadiness&&bLocalActive&&bRemoteActive&&World->GetTimeSeconds()>RequestedWorldTime+2
				&&FParse::Param(FCommandLine::Get(),TEXT("HALBattleTopCartoonFixture")))
			{
				bReportedFXReadiness=true;
				auto* S=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Assets/FX/BattleTop/NS_BattleTop_ToonWind.NS_BattleTop_ToonWind"));
				TArray<UNiagaraComponent*> Cs;Vehicle->GetComponents(Cs);
				Test->AddInfo(FString::Printf(TEXT("Wind readiness Role=%s Ready=%d Components=%d VisualChanged=%d"),
					bHost?TEXT("Host"):TEXT("Client"),S&&S->IsReadyToRun(),Cs.FilterByPredicate([S](const UNiagaraComponent* C){return C&&C->IsRegistered()&&C->GetAsset()==S;}).Num(),
					OriginalVisual.IsSet()&&Mesh&&!Mesh->GetRelativeTransform().Equals(OriginalVisual.GetValue())));
			}
			const auto* Movement = Vehicle->FindComponentByClass<UArcadeVehicleMovementComponent>();
			const bool bRestriction = Movement && Movement->GetMovementEffectStatus().Value.bHandbrakeBlocked;
			if (!bBothActive)
			{
				if (!bLocalActive || !bRemoteActive) { return false; }
				if (bBattleTop && (!bRestriction || !UHALBattleTopComponent::IsActiveOn(Vehicle)
					|| !UHALBattleTopComponent::IsActiveOn(RemoteVehicle))) { return false; }
				if (OriginalVisual.IsSet() && (!Mesh || Mesh->GetRelativeTransform().Equals(OriginalVisual.GetValue()))) { return false; }
				if (OriginalVisual.IsSet() && FParse::Param(FCommandLine::Get(), TEXT("HALBattleTopFXFixture")))
				{
					const bool bCartoon=FParse::Param(FCommandLine::Get(),TEXT("HALBattleTopCartoonFixture"));
					const auto WindCount = [bCartoon](ATestVehiclePawn* P)
					{
						TArray<UNiagaraComponent*> Components; P->GetComponents(Components);
						return Components.FilterByPredicate([bCartoon](const UNiagaraComponent* C){ return IsValid(C) && C->IsRegistered() && GetNameSafe(C->GetAsset()) == (bCartoon?TEXT("NS_BattleTop_ToonWind"):TEXT("NS_BattleTop_Wind")); }).Num();
					};
					if (WindCount(Vehicle) != 1 || WindCount(RemoteVehicle) != 1) { return false; }
					Test->AddInfo(TEXT("Owner and observer each render exactly one saved BattleTop wind system."));
					if(bCartoon)for(auto* P:{Vehicle,RemoteVehicle})
					{
						TArray<UNiagaraComponent*> Cs;P->GetComponents(Cs);
						for(auto* C:Cs)if(IsValid(C)&&GetNameSafe(C->GetAsset())==TEXT("NS_BattleTop_ToonWind"))
						{
							const FNiagaraVariable Yaw(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.YawRate"));
							Test->TestEqual(TEXT("Replicated airflow rate is independent of authored body spin"),C->GetOverrideParameters().GetParameterValue<float>(Yaw),1440.f);
							const FNiagaraVariable Tilt(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.TiltDegrees"));
							Test->TestEqual(TEXT("Replicated nominal tilt reaches Niagara directly"),C->GetOverrideParameters().GetParameterValue<float>(Tilt),45.f);
							Test->TestTrue(TEXT("Airflow stays on world vertical independently of chassis rotation"),C->IsUsingAbsoluteRotation()&&C->GetComponentQuat().Equals(FQuat::Identity));
						}
					}
				}
				if (bBattleTop)
				{
					Test->TestEqual(TEXT("Owner receives the authored skill spin rate"),
						Vehicle->FindComponentByClass<UHALBattleTopComponent>()->GetVisualSpinRate(),
						FHALVehicleAbilityTestAccess::BattleTopNetworkSpinRate);
					Test->TestEqual(TEXT("Observer receives the authored skill spin rate"),
						RemoteVehicle->FindComponentByClass<UHALBattleTopComponent>()->GetVisualSpinRate(),
						FHALVehicleAbilityTestAccess::BattleTopNetworkSpinRate);
				}
				bBothActive = true;
				Test->TestEqual(TEXT("Owner sees one active GE and one cooldown GE"), Local->GetActiveEffects(FGameplayEffectQuery()).Num(), 2);
				const auto Durations = Local->GetActiveEffectsTimeRemainingAndDuration(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
					FGameplayTagContainer(CooldownTag)));
				Test->TestTrue(TEXT("Owner receives twenty-game-second cooldown"), Durations.Num() == 1 && FMath::IsNearlyEqual(Durations[0].Value, 20.0f, 0.1f));
				Controller->HALUseSkill(); // A repeated input must be denied locally/server-side by the same GAS cooldown.
				UE_LOG(LogTemp, Display, TEXT("HAL Ability smoke: Role=%s BothActive=1 BattleTop=%d HandbrakeBlocked=%d Cooldown=%.2f"),
					bHost ? TEXT("Host") : TEXT("Client"), bBattleTop, bRestriction, Local->GetEffectTimeRemaining(CooldownTag));
				Controller->HALGASStatus();
			}
			const bool bCartoonFixture=FParse::Param(FCommandLine::Get(),TEXT("HALBattleTopCartoonFixture"));
			const bool bSprayFixture=FParse::Param(FCommandLine::Get(),TEXT("HALBattleTopContactSprayFixture"))||bCartoonFixture;
			if (bSprayFixture && bBothActive && bLocalActive && bRemoteActive)
			{
				const double Now=World->GetTimeSeconds();
				if (bHost && Now>RequestedWorldTime+.4 && Now>LastSprayRequest+.12)
				{
					LastSprayRequest=Now;
					// Synthetic authority contact observations isolate RPC/payload delivery from collision classification.
					// Native geometry tests and team physical-contact testing cover their separate contracts.
					const auto Emit=[](ATestVehiclePawn* Source,ATestVehiclePawn* Other)
					{
						auto* Body=Cast<UPrimitiveComponent>(Source->GetRootComponent());auto* Target=Cast<UPrimitiveComponent>(Other->GetRootComponent());
						FHitResult H;H.ImpactNormal=(Body->GetComponentLocation()-Target->GetComponentLocation()).GetSafeNormal2D();
						if(H.ImpactNormal.IsNearlyZero())H.ImpactNormal=FVector::ForwardVector;
						H.ImpactPoint=Body->GetComponentLocation()-H.ImpactNormal*Body->Bounds.BoxExtent.X;
						Source->FindComponentByClass<UHALBattleTopComponent>()->ObserveContact(*Body,*Target,FVector::ZeroVector,H);
					};
					Emit(Vehicle,RemoteVehicle);Emit(RemoteVehicle,Vehicle);
				}
				const auto Observe=[&](ATestVehiclePawn* Source,bool& Seen)
				{
					if(Seen){return;}TArray<UNiagaraComponent*> Components;Source->GetComponents(Components);
					for(const auto* C:Components)
					{
						if(IsValid(C)&&C->IsRegistered()&&GetNameSafe(C->GetAsset())==(bCartoonFixture?TEXT("NS_BattleTop_ToonContact"):TEXT("NS_BattleTop_ContactSpray")))
						{
							const FNiagaraVariable Count(FNiagaraTypeDefinition::GetIntDef(),TEXT("User.BurstCount"));
							Test->TestEqual(TEXT("Replicated contact spray retains authored burst count"),C->GetOverrideParameters().GetParameterValue<int32>(Count),bCartoonFixture?7:32);
							Seen=true;Test->AddInfo(FString::Printf(TEXT("Contact spray observed Role=%s Pawn=%s"),bHost?TEXT("Host"):TEXT("Client"),*Source->GetName()));break;
						}
					}
				};
				Observe(Vehicle,bSawLocalSpray);Observe(RemoteVehicle,bSawRemoteSpray);
			}
			if (!bFXCaptured && bBothActive && bLocalActive && FApp::CanEverRender()
				&& FParse::Param(FCommandLine::Get(), TEXT("HALBattleTopFXFixture"))
				&& World->GetTimeSeconds() > RequestedWorldTime + 1.5)
			{
				bFXCaptured = true;
				FString RenderOutput=FPaths::ProjectSavedDir() / (bCartoonFixture?TEXT("BattleTop/Cartoon_20261113"):TEXT("BattleTop/Polish_20261113"));
				FParse::Value(FCommandLine::Get(),TEXT("HALBattleTopRenderOutput="),RenderOutput);
				FScreenshotRequest::RequestScreenshot(RenderOutput / (bHost ? TEXT("Live_Host.png") : TEXT("Live_Client.png")), false, false);
			}
			if (!bBothExpired)
			{
				if (bLocalActive || bRemoteActive) { return false; }
				if (bBattleTop && (bRestriction || UHALBattleTopComponent::IsActiveOn(Vehicle)
					|| UHALBattleTopComponent::IsActiveOn(RemoteVehicle))) { return false; }
				if (OriginalVisual.IsSet())
				{
					Test->TestTrue(TEXT("Rendered car restores the authored relative visual transform"),
						Mesh && Mesh->GetRelativeTransform().Equals(OriginalVisual.GetValue()));
					TArray<UNiagaraComponent*> Components; Vehicle->GetComponents(Components);
					Test->TestFalse(TEXT("Expiry removes skill particle components"), Components.ContainsByPredicate([](const UNiagaraComponent* C)
					{ return IsValid(C) && C->IsRegistered() && GetNameSafe(C->GetAsset()).StartsWith(TEXT("NS_BattleTop_")); }));
				}
				bBothExpired = true;
				if(bSprayFixture){Test->TestTrue(TEXT("Owner and observer received real cosmetic multicast bursts"),bSawLocalSpray&&bSawRemoteSpray);}
				Test->TestTrue(TEXT("Active effect expires before cooldown"), Local->HasMatchingGameplayTag(CooldownTag));
				UE_LOG(LogTemp, Display, TEXT("HAL Ability smoke: Role=%s BothExpired=1 Cooldown=%.2f GameElapsed=%.2f TimeDilation=%.2f"),
					bHost ? TEXT("Host") : TEXT("Client"), Local->GetEffectTimeRemaining(CooldownTag),
					World->GetTimeSeconds() - RequestedWorldTime, World->GetWorldSettings()->GetEffectiveTimeDilation());
			}
			if (Local->HasMatchingGameplayTag(CooldownTag)) { return false; }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBattleTopNetworkSmokeTest, "HAL.NetworkSmoke.BattleTop.ActivationAndDrift",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALBattleTopNetworkSmokeTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHALAbilityLiveSmoke>(this, true));
	return true;
}

#endif
