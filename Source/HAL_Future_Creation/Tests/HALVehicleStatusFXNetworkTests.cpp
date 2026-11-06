#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALVehicleStatusFXComponent.h"
#include "../HALVehicleStatusFXSubsystem.h"
#include "../HALVehicleStatusFXDefinition.h"
#include "../HALShockedPresentationComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALGameplayTags.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALRoomComponent.h"
#include "../HALRoomConfig.h"
#include "../HALPracticeRoomGameMode.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "NiagaraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "UObject/UnrealType.h"

namespace HALStatusFXNetworkTests
{
	class FRun : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds() + 120, Complete = -1;
		int32 Phase = 0;
		float InitialHP = 0;
		bool bCleaned = false;
		bool bRoomConfigured = false;
		double NextRoomAction = 0;
		FHALStatusFXRenderRequest OwnRequest;
	public:
		explicit FRun(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (Complete >= 0) { return FPlatformTime::Seconds() > Complete + 1.5; }
			if (FPlatformTime::Seconds() > Deadline)
			{ Test->AddError(FString::Printf(TEXT("StatusFX paired observation timeout, Phase=%d"), Phase)); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* W = Context.World();
				if (W && W->IsGameWorld() && (W->GetNetMode() == NM_Client || W->GetNetMode() == NM_ListenServer)) { World = W; break; }
			}
			if (!World) { return false; }
			const bool Host = World->GetNetMode() == NM_ListenServer;
			AHALPlayerController* PC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			{ if (auto* C = Cast<AHALPlayerController>(It->Get()); C && C->IsLocalController()) { PC = C; break; } }
			auto* Match = World->GetGameState<AHALMatchGameState>();
			auto* Room = Match ? Match->GetRoom() : nullptr;
			auto* Player = PC ? PC->GetPlayerState<AHALPlayerState>() : nullptr;
			if (Room && Room->GetData().bPracticeRoom && Room->GetData().Phase == EHALRoomPhase::Open)
			{
				if (!Player || !Player->GetRoomData().PlayerKey.IsValid()) { return false; }
				if (Host && !bRoomConfigured)
				{
					auto* Mode = World->GetAuthGameMode<AHALPracticeRoomGameMode>();
					if (!Mode || !Room->GetServerConfig()) { return false; }
					auto* Config = DuplicateObject<UHALRoomConfig>(Room->GetServerConfig(), Mode);
					Config->SetFlags(RF_Transient); Config->ClearFlags(RF_Public | RF_Standalone);
					Config->MatchRules.DurationSeconds = 90; Config->MatchRules.ResultsSeconds = 3;
					FindFProperty<FObjectProperty>(Mode->GetClass(), TEXT("RoomConfig"))->SetObjectPropertyValue_InContainer(Mode, Config);
					bRoomConfigured = true; // Spawned instance only; never a saved DA or CDO.
				}
				const double Now = FPlatformTime::Seconds();
				if (Now < NextRoomAction || PC->IsRoomRequestPending()) { return false; }
				NextRoomAction = Now + 0.15;
				if (Player->GetRoomData().Phase == EHALRoomPlayerPhase::Selecting)
				{
					if (Player->GetRoomData().VehicleId.IsNone()) { PC->SelectRoomVehicle(Host ? TEXT("Car_01") : TEXT("Car_02")); }
					else { PC->ConfirmRoomVehicle(); }
				}
				else if (Host && Room->GetData().Members.Num() == 2 && Room->CheckStart(PC) == EHALRoomResult::Accepted)
				{ PC->RequestStartMatch(); }
				return false;
			}
			auto* Own = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { if (*It != Own && It->GetPlayerState()) { Other = *It; break; } }
			auto* ASC = Own ? Cast<UHALAbilitySystemComponent>(Own->GetAbilitySystemComponent()) : nullptr;
			auto* Remote = Other ? Cast<UHALAbilitySystemComponent>(Other->GetAbilitySystemComponent()) : nullptr;
			auto* FX = Own ? Own->FindComponentByClass<UHALVehicleStatusFXComponent>() : nullptr;
			auto* OtherFX = Other ? Other->FindComponentByClass<UHALVehicleStatusFXComponent>() : nullptr;
			auto* Movement = Own ? Own->FindComponentByClass<UArcadeVehicleMovementComponent>() : nullptr;
			if (!ASC || !Remote || !FX || !OtherFX || !Movement || !Match || !ASC->IsHealthInitialized() || !Remote->IsHealthInitialized()) { return false; }
			const FString OtherName = Other->GetPlayerState()->GetPlayerName();
			const FGameplayTag Tag = HALGameplayTags::State_Visual_Shocked;
			if (Phase == 0)
			{
				if (Host && Match->GetMatchPhase() != EHALMatchPhase::Playing) { PC->HALStartMatch(); return false; }
				if (Match->GetMatchPhase() != EHALMatchPhase::Playing || !Movement->GetMovementEffectStatus().bReady) { return false; }
				auto* Cache = World->GetGameInstance()->GetSubsystem<UHALVehicleStatusFXSubsystem>();
				if (!Cache || !Cache->FindEffect(Tag, Own->GetVehicleDefinition()) || !Cache->FindEffect(Tag, Other->GetVehicleDefinition())) { return false; }
				if (!FX->CaptureDiagnostics().bObservingASC || !OtherFX->CaptureDiagnostics().bObservingASC) { return false; }
				int32 Index = 0;
				for (auto* Car : {Own, Other})
				{
					auto* Body = CastChecked<UPrimitiveComponent>(Car->GetRootComponent()); Body->SetEnableGravity(false);
					if (Host)
					{
						Car->SetActorLocation(FVector(Index++ * 3000, 0, 5000), false, nullptr, ETeleportType::TeleportPhysics);
						Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					}
				}
				Movement->ResetInputCommand(); InitialHP = ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute());
				Phase = 1;
			}
			if (Phase == 1)
			{
				if (!Host)
				{
					const auto Revision = Movement->GetMovementEffectStatus().Value.Revision;
					PC->ConsoleCommand(TEXT("hal.StatusFXDebug local State.Visual.Shocked A 1"));
					PC->ConsoleCommand(TEXT("hal.StatusFXDebug local State.Visual.Shocked B 1"));
					const auto D = FX->CaptureDiagnostics();
					Test->TestEqual(TEXT("Client command creates two local sources"), D.DebugSources, 2);
					Test->TestEqual(TEXT("Local debug does not invent an ASC source"), D.ASCSources, 0);
					Test->TestFalse(TEXT("Client command cannot write gameplay shock"), ASC->HasMatchingGameplayTag(Tag));
					Test->TestEqual(TEXT("Debug leaves HP unchanged"), ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute()), InitialHP);
					Test->TestEqual(TEXT("Debug leaves physics rule revision unchanged"), Movement->GetMovementEffectStatus().Value.Revision, Revision);
					TArray<FHALStatusFXRenderRequest> Requests; FX->CaptureRenderRequests(Requests);
					if (Requests.IsEmpty()) { Test->AddError(TEXT("Client debug did not prepare resources")); return true; }
					OwnRequest = Requests[0]; PC->SetName(TEXT("HAL_FX3C_DebugOn")); Phase = 2; return false;
				}
				if (OtherName != TEXT("HAL_FX3C_DebugOn")) { return false; }
				Test->TestEqual(TEXT("Client debug did not replicate to server's remote car"), OtherFX->CaptureDiagnostics().DebugSources, 0);
				Test->TestEqual(TEXT("Client debug did not create server instances"), OtherFX->CaptureDiagnostics().Instances.Num(), 0);
				Test->TestFalse(TEXT("Client debug never authored a server tag"), Remote->HasMatchingGameplayTag(Tag));
				for (auto* System : {ASC, Remote})
				{
					Test->TestTrue(TEXT("Authority starts real stun GE"), System->ApplyStun(6, Own));
					Test->TestTrue(TEXT("Authority marks its real stun as shocked"), System->MarkShockedForCurrentStun(Own));
				}
				Phase = 3;
			}
			if (Phase == 2 || Phase == 3)
			{
				if (!ASC->HasMatchingGameplayTag(Tag) || !Remote->HasMatchingGameplayTag(Tag)) { return false; }
				const auto D = FX->CaptureDiagnostics(), OtherD = OtherFX->CaptureDiagnostics();
				if (D.ASCSources != 1 || OtherD.ASCSources != 1) { return false; }
				TArray<FHALStatusFXRenderRequest> Requests, OtherRequests; FX->CaptureRenderRequests(Requests); OtherFX->CaptureRenderRequests(OtherRequests);
				if (Requests.IsEmpty() || OtherRequests.IsEmpty()) { return false; }
				Test->TestTrue(TEXT("Original electric component still owns its active presentation"),
					Own->FindComponentByClass<UHALShockedPresentationComponent>()->IsShocked()
					&& Other->FindComponentByClass<UHALShockedPresentationComponent>()->IsShocked());
				Test->TestTrue(TEXT("Both new paths remain dormant"), D.RegisteredParticles == 0 && D.ActiveParticles == 0
					&& OtherD.RegisteredParticles == 0 && OtherD.ActiveParticles == 0);
				if (Requests[0].Surface.IsValid()) { Test->TestTrue(TEXT("Actual two-car MIDs are independent"), Requests[0].Surface != OtherRequests[0].Surface); }
				if (!Requests[0].Particles.IsEmpty())
				{ Test->TestTrue(TEXT("Actual two-car particles are independent"), Requests[0].Particles[0].Component != OtherRequests[0].Particles[0].Component); }
				if (!Host)
				{
					Test->TestEqual(TEXT("ASC merges with two debug inputs"), Requests[0].State.SourceCount, 3);
					Test->TestTrue(TEXT("Replication preserves the existing debug instance clock"), Requests[0].State.InstanceId == OwnRequest.State.InstanceId
						&& Requests[0].State.ClockSeconds >= OwnRequest.State.ClockSeconds && Requests[0].Surface == OwnRequest.Surface);
					PC->ConsoleCommand(TEXT("hal.StatusFXDebug local State.Visual.Shocked A 1"));
					PC->ConsoleCommand(TEXT("hal.StatusFXDebug local State.Visual.Shocked A 0"));
					Test->TestEqual(TEXT("Removing A preserves B and ASC"), FX->CaptureDiagnostics().ActiveSources, 2);
					PC->ConsoleCommand(TEXT("hal.StatusFXDebugClear local"));
					Test->TestEqual(TEXT("Clear preserves the replicated ASC source"), FX->CaptureDiagnostics().ActiveSources, 1);
					const auto Previous = Requests[0]; FX->Unbind(); Own->RefreshAbilitySystemBinding();
					FX->CaptureRenderRequests(Requests);
					if (Requests.IsEmpty()) { Test->AddError(TEXT("Rebind failed to read the already-active replicated tag")); return true; }
					Test->TestTrue(TEXT("New binding observes existing replicated state"), FX->CaptureDiagnostics().ASCSources == 1 && Requests[0].State.Binding != Previous.State.Binding);
					Test->TestFalse(TEXT("Old binding request is rejected"), FX->IsRequestCurrent(Previous));
					PC->ConsoleCommand(TEXT("hal.StatusFXStatus local")); PC->SetName(TEXT("HAL_FX3C_TagSeen"));
				}
				else
				{
					if (OtherName != TEXT("HAL_FX3C_TagSeen")) { return false; }
					FX->ObserveAbilitySystem(ASC);
					TArray<FHALStatusFXRenderRequest> Again; FX->CaptureRenderRequests(Again);
					Test->TestTrue(TEXT("Same server observer keeps resources"), !Again.IsEmpty() && Again[0].Surface == Requests[0].Surface
						&& Again[0].State.Binding == Requests[0].State.Binding);
				}
				OwnRequest = Requests[0]; Phase = 4;
			}
			if (Phase == 4)
			{
				if (ASC->HasMatchingGameplayTag(Tag) || Remote->HasMatchingGameplayTag(Tag)) { return false; }
				const auto D = FX->CaptureDiagnostics(), OtherD = OtherFX->CaptureDiagnostics();
				if (!D.Instances.IsEmpty() || !OtherD.Instances.IsEmpty()) { return false; }
				if (!bCleaned)
				{
					Test->TestTrue(TEXT("Expiry removes ASC sources and all prepared resources"), D.ASCSources == 0 && D.DebugSources == 0
						&& D.Materials == 0 && D.Particles == 0 && OtherD.Materials == 0 && OtherD.Particles == 0);
					Test->TestFalse(TEXT("Idle component stops ticking"), FX->IsComponentTickEnabled());
					for (const auto& P : OwnRequest.Particles)
					{ Test->TestTrue(TEXT("Real particle is destroyed on expiry"), !P.Component.IsValid() || P.Component->IsBeingDestroyed()); }
					Test->TestFalse(TEXT("Expired request is rejected"), FX->IsRequestCurrent(OwnRequest));
					Test->TestEqual(TEXT("Observation and debug did not damage"), ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute()), InitialHP);
					FX->LogDiagnostics(); OtherFX->LogDiagnostics(); bCleaned = true;
					if (!Host) { PC->SetName(TEXT("HAL_FX3C_Done")); }
				}
				if (Host && OtherName != TEXT("HAL_FX3C_Done")) { return false; }
				UE_LOG(LogTemp, Display, TEXT("HAL StatusFX observation smoke Host=%d Complete=1 LocalDebugOnly=1 SourcesCleared=1 Dormant=1"), Host);
				Complete = FPlatformTime::Seconds();
			}
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXNetworkTest, "HAL.NetworkSmoke.StatusFX.Observation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXNetworkTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALStatusFXNetworkTests::FRun>(this)); return true;
}
#endif
