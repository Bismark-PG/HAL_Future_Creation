#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../VehicleRecoveryComponent.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "HighResScreenshot.h"
#include "../HALRecoveryPromptWidget.h"
#include "../HALInputHintSubsystem.h"
#include "Misc/CommandLine.h"

namespace
{
	class FRecoveryLiveTest : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline=FPlatformTime::Seconds()+90, Complete=-1;
		bool bPrepared=false, bFloorPrepared=false, bSignalled=false, bSawOwnHold=false, bSawOwnExecution=false, bSawOtherExecution=false;
		int32 PressCount=0;
		double NextPress=0, ExecutionStart=-1, ExecutionSeconds=-1;
		bool bReleased=true, bPhotographed=false;
		FKey PressedKey;
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
					if (UHALAbilitySystemComponent::IsActorStunned(Own) || UHALAbilitySystemComponent::IsActorStunned(Other)) { return false; }
					Test->TestTrue(TEXT("Neither recovery starts before fixture setup"),
						Own->FindComponentByClass<UVehicleRecoveryComponent>()->GetStatus().Sequence==0
						&& Other->FindComponentByClass<UVehicleRecoveryComponent>()->GetStatus().Sequence==0);
					UE_LOG(LogTemp,Display,TEXT("HAL Recovery fixture ready: HostUp=%.3f ClientUp=%.3f"),Own->GetActorUpVector().Z,Other->GetActorUpVector().Z);
					FPacketSimulationSettings Lag; Lag.PktLag=80; World->GetNetDriver()->SetPacketSimulationSettings(Lag);
					int32 Index=0;
					for (auto* Pawn:{Own,Other})
					{
						auto* Body=CastChecked<UStaticMeshComponent>(Pawn->GetRootComponent());
						// Opt-in test setup only. Production recovery never teleports or clears velocities.
						const bool bClientCar=Index++!=0;
						const FQuat Pose=FRotator(0,0,bClientCar ? 180:90).Quaternion();
						double Lowest=0;
						for (const auto& Convex : Body->GetBodySetup()->AggGeom.ConvexElems)
						{
							for (const FVector& Vertex : Convex.VertexData)
							{ Lowest=FMath::Min(Lowest,Pose.RotateVector(Convex.GetTransform().TransformPosition(Vertex)*Body->GetComponentScale()).Z); }
						}
						Body->SetWorldLocationAndRotation(FVector(bClientCar ? 1000:-1000,0,10000.5-Lowest),Pose,false,nullptr,ETeleportType::TeleportPhysics);
						Body->SetPhysicsLinearVelocity(FVector(260,0,0)); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
						Pawn->ForceNetUpdate();
					}
				}
				bPrepared=true;
			}
			auto* Recovery=Own->FindComponentByClass<UVehicleRecoveryComponent>();
			const auto OwnStatus=Recovery->GetStatus();
			const auto OtherStatus=Other->FindComponentByClass<UVehicleRecoveryComponent>()->GetStatus();
			const double Now=World->GetTimeSeconds();
			const bool bCapture=FParse::Param(FCommandLine::Get(),TEXT("HALRecoveryCapture"));
			auto Button=[&](bool Down)
			{
				if (bCapture)
				{
					const auto User=PC->GetLocalPlayer()->GetSlateUser();
					if (Down) { PressedKey=Host || PressCount<2 ? EKeys::R : EKeys::Gamepad_Special_Left; }
					const FKeyEvent E(PressedKey,FModifierKeysState(),User->GetUserIndex(),false,0,0);
					if (Down) { FSlateApplication::Get().ProcessKeyDownEvent(E); }
					else { FSlateApplication::Get().ProcessKeyUpEvent(E); }
				}
				else { Own->SetSelfRightHeld(Down); }
			};
			if (!bReleased) { Button(false); bReleased=true; }
			const bool bPrompt=OwnStatus.Phase==EVehicleRecoveryPhase::Eligible || OwnStatus.Phase==EVehicleRecoveryPhase::Holding;
			if (bPrompt && !PressCount)
			{
				Own->SetSelfRightHeld(false);
				if (bCapture)
				{
					// Each offscreen game receives the same focus events as a player's active viewport.
					auto& Slate=FSlateApplication::Get();
					if (!Slate.IsActive()) { Slate.OnApplicationActivationChanged(true); }
					Slate.SetUserFocusToGameViewport(PC->GetLocalPlayer()->GetSlateUser()->GetUserIndex());
				}
			}
			if (bPrompt && PressCount<6 && Now>=NextPress)
			{
				if (!PressCount)
				{
					const auto Sample=Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetStabilityObservation();
					UE_LOG(LogTemp,Display,TEXT("HAL Recovery first tap: Host=%d Ground=%d UpZ=%.3f SpeedCmPerSec=%.1f Block=%s"),
						Host,Sample.bNearGround,Sample.Up.Z,Sample.Speed,*OwnStatus.BlockReason.ToString());
				}
				Button(true); bReleased=false; ++PressCount; NextPress=Now+.12;
				if (bCapture && !Host && PressCount==2)
				{
					const auto* Hints=PC->GetLocalPlayer()->GetSubsystem<UHALInputHintSubsystem>();
					Test->TestTrue(TEXT("Client uses keyboard for the first two taps before switching to View"),Hints && Hints->GetCurrentInputDevice()==EHALInputHintDevice::KeyboardMouse);
				}
				if (!bCapture)
				{
					Own->SetSelfRightHeld(true); // A duplicate down cannot earn a point.
					if (PressCount==4)
					{
						Own->SetSelfRightHeld(false); Own->SetSelfRightHeld(true); ++PressCount;
						Test->TestTrue(TEXT("Two taps in one frame appear immediately"),Recovery->GetDisplayProgress()>.81f);
					}
				}
			}
			if (bCapture && !bPhotographed && PressCount==3 && Recovery->GetDisplayProgress()>.48f)
			{
				TArray<UUserWidget*> Prompts;
				UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Prompts,UHALRecoveryPromptWidget::StaticClass(),true);
				Test->TestTrue(TEXT("Only the local owner has a tap ring"),Prompts.Num()==1 && Prompts[0]->GetOwningPlayer()==PC);
				const auto* Hints=PC->GetLocalPlayer()->GetSubsystem<UHALInputHintSubsystem>();
				Test->TestTrue(TEXT("Ring uses the device that sent the mapped action"),Hints && Hints->GetCurrentInputDevice()==
					(Host ? EHALInputHintDevice::KeyboardMouse : EHALInputHintDevice::Gamepad));
				FScreenshotRequest::RequestScreenshot(Host ? TEXT("RecoveryTap_Keyboard.png") : TEXT("RecoveryTap_View.png"),true,false);
				bPhotographed=true;
			}
			bSawOwnHold |= OwnStatus.Phase==EVehicleRecoveryPhase::Holding;
			bSawOwnExecution |= OwnStatus.Phase==EVehicleRecoveryPhase::Executing;
			bSawOtherExecution |= OtherStatus.Phase==EVehicleRecoveryPhase::Executing;
			if (ExecutionStart<0 && OwnStatus.Phase==EVehicleRecoveryPhase::Executing) { ExecutionStart=Now; }
			if (ExecutionStart>=0 && ExecutionSeconds<0 && OwnStatus.LastOutcome==TEXT("Succeeded")) { ExecutionSeconds=Now-ExecutionStart; }
			if (OwnStatus.LastOutcome!=TEXT("Succeeded") || OtherStatus.LastOutcome!=TEXT("Succeeded")) { return false; }
			Test->TestTrue(TEXT("Server-confirmed taps and both execution phases observed"),bSawOwnHold && bSawOwnExecution && bSawOtherExecution);
			Test->TestTrue(TEXT("Both accepted starts and stops have sequence 2"),OwnStatus.Sequence==2 && OtherStatus.Sequence==2);
			Test->TestEqual(TEXT("Exactly six distinct presses"),PressCount,6);
			if (bCapture) { Test->TestTrue(TEXT("Captured the actual mapped key and partial ring"),bPhotographed); }
			Test->TestTrue(TEXT("Both simulated roots finish upright"),Own->GetActorUpVector().Z>.9 && Other->GetActorUpVector().Z>.9);
			const int32 Resim=Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetResimulationRunCount();
			if (!Host) { Test->TestTrue(TEXT("Real autonomous resimulation observed"),Resim>0); }
			UE_LOG(LogTemp,Display,TEXT("HAL Recovery smoke: Host=%d Complete=1 OwnSeq=%lld OtherSeq=%lld OwnOutcome=%s OtherOutcome=%s SettledSeconds=%.3f Resimulations=%d HostOutgoingLag=80ms"),
				Host,OwnStatus.Sequence,OtherStatus.Sequence,*OwnStatus.LastOutcome.ToString(),*OtherStatus.LastOutcome.ToString(),ExecutionSeconds,Resim);
			Own->SetSelfRightHeld(false); Complete=FPlatformTime::Seconds(); return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRecoveryNetworkTest,"HAL.NetworkSmoke.VehicleRecovery",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALRecoveryNetworkTest::RunTest(const FString& Parameters)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("HALStabilityNetworkFixture")) && !FParse::Param(FCommandLine::Get(),TEXT("HALRecoveryTapFixture")))
	{ AddError(TEXT("Requires paired -HALRecoveryTapFixture (production rules) or -HALStabilityNetworkFixture")); return false; }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRecoveryLiveTest>(this)); return true;
}
#endif
