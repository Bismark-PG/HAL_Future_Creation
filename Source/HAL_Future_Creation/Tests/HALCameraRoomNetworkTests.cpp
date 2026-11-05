#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALCameraTestSupport.h"
#include "HALWidgetExtensionTestAccess.h"
#include "../HALPlayerController.h"
#include "../HALNetworkGameInstance.h"
#include "../HALMatchGameState.h"
#include "../HALRoomComponent.h"
#include "../HALPracticeRoomWidget.h"
#include "../HALElectricWallGimmick.h"
#include "../BasicBallActor.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	/** Actual Entry->practice->Playing->returned room; only spawned runtime fixtures are modified. */
	class FCameraRoomProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		const bool Host=FParse::Param(FCommandLine::Get(),TEXT("CameraRoomHost"));
		bool Connected=false,Final=false,Tracking=false,Outside=false,Prepared=false,Custom=false,SawRecovery=false;
		double Deadline=FPlatformTime::Seconds()+180,NextName=0,NextAction=0,WarmAt=0,HitAt=0,MenuAt=0,FinishAt=0;
		int32 Cycles=0,MenuStep=0,CompletedScenes=0;
		uint32 ResetSerial=0;
		TWeakObjectPtr<ATestVehiclePawn> Configured;
		FString LastName;
		void Name(AHALPlayerController* PC,const FString& Value)
		{ if (LastName!=Value || FPlatformTime::Seconds()>NextName) { LastName=Value; NextName=FPlatformTime::Seconds()+.5; PC->SetName(Value); } }
		TArray<ATestVehiclePawn*> Vehicles(UWorld* W)
		{ TArray<ATestVehiclePawn*> V; for (TActorIterator<ATestVehiclePawn> I(W);I;++I) { if (I->GetPlayerState()) { V.Add(*I); } } return V; }
		void MoveOutside(UWorld* W)
		{
			for (auto* V:Vehicles(W))
			{
				auto* B=CastChecked<UPrimitiveComponent>(V->GetRootComponent());
				const auto* P=V->GetPlayerState<AHALPlayerState>();
				B->SetWorldLocationAndRotation(FVector(-500,P->GetRoomData().Number==1?-500:500,10300),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
				B->SetPhysicsLinearVelocity(FVector::ZeroVector); B->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); V->ForceNetUpdate();
			}
		}
	public:
		explicit FCameraRoomProbe(FAutomationTestBase* T):Test(T) {}
		bool Update() override
		{
			const double Now=FPlatformTime::Seconds(); if (FinishAt>0) { return Now>=FinishAt; }
			if (Now>Deadline) { Test->AddError(FString::Printf(TEXT("Camera room timeout Host=%d Scenes=%d Cycles=%d Prepared=%d Tracking=%d"),Host,CompletedScenes,Cycles,Prepared,Tracking)); return true; }
			UWorld* W=nullptr; for (const auto& C:GEngine->GetWorldContexts()) { if (C.World() && C.World()->IsGameWorld()) { W=C.World(); break; } }
			if (!W) { return false; } auto* GI=W->GetGameInstance<UHALNetworkGameInstance>(); if (!GI) { return false; }
			if (!Custom && FParse::Param(FCommandLine::Get(),TEXT("HALContractUI"))) { FHALWidgetExtensionTestAccess::Configure(GI,true); Custom=true; }
			if (W->GetMapName().Contains(TEXT("MVPB_NetEntry")))
			{
				if (Final) { Test->TestEqual(TEXT("Both practice and Playing were tested"),CompletedScenes,2); FinishAt=Now+1; return false; }
				if (!GI->GetFirstLocalPlayerController()) { return false; }
				if (!Connected) { Connected=true; Test->TestTrue(TEXT("Normal Entry Host/Join request accepted"),Host?GI->HostMatch():GI->JoinMatch(TEXT("127.0.0.1"))); }
				return false;
			}
			auto* S=W->GetGameState<AHALMatchGameState>(); auto* R=S?S->GetRoom():nullptr;
			AHALPlayerController* PC=nullptr; for (auto I=W->GetPlayerControllerIterator();I;++I) { if (auto* P=Cast<AHALPlayerController>(I->Get()); P && P->IsLocalController()) { PC=P; break; } }
			auto* PS=PC?PC->GetPlayerState<AHALPlayerState>():nullptr;
			if (!R || !PS || !PC->GetLocalPlayer() || (W->GetNetMode()==NM_Client && !PC->GetNetConnection())) { return false; }
			const auto D=R->GetData(); const auto Identity=PS->GetRoomData();
			if (D.Phase==EHALRoomPhase::Open && D.RoundId==1)
			{
				Final=true; if (!Host) { Name(PC,TEXT("CAM_END")); }
				else if (D.Members.Num()==2) { for (const auto& M:D.Members) { if (M!=PS && M->GetPlayerName()==TEXT("CAM_END")) { PC->LeaveRoom(); break; } } }
				return false;
			}
			if (D.Phase!=EHALRoomPhase::Open && D.Phase!=EHALRoomPhase::Playing) { return false; }
			if (Identity.Phase==EHALRoomPlayerPhase::Selecting)
			{
				if (Now<NextAction || PC->IsRoomRequestPending()) { return false; } NextAction=Now+.2;
				if (Identity.VehicleId.IsNone()) { PC->SelectRoomVehicle(Host?TEXT("Car_01"):TEXT("Car_02")); } else { PC->ConfirmRoomVehicle(); }
				return false;
			}
			auto* V=Cast<ATestVehiclePawn>(PC->GetPawn()); if (!V || !V->IsLocalInputInitialized()) { return false; }
			const int32 Stage=D.Phase==EHALRoomPhase::Playing?1:0;
			auto* Camera=V->FindComponentByClass<UHALDynamicCameraComponent>();
			if (Configured.Get()!=V)
			{
				Configured=V; Tracking=Prepared=Outside=false; Cycles=MenuStep=0;
				auto* C=FHALCameraTestAccess::Enable(V,60,1600); C->OtherPlayers.Weight=0;
				C->MainBall.Weight=10; C->MainBall.FullWeightDistance=10000; C->MainBall.ZeroWeightDistance=20000;
				if (Host && W->GetNetDriver())
				{
					int32 Lag=0; FParse::Value(FCommandLine::Get(),TEXT("CameraRoomLag="),Lag);
					FPacketSimulationSettings Settings; Settings.PktLag=FMath::Clamp(Lag,0,500); W->GetNetDriver()->SetPacketSimulationSettings(Settings);
					UE_LOG(LogTemp,Display,TEXT("CameraRoom network HostOutboundLag=%dms Stage=%d"),Settings.PktLag,Stage);
				}
			}
			AHALElectricWallGimmick* Wall=nullptr;
			for (TActorIterator<AHALElectricWallGimmick> I(W);I;++I)
			{ if (I->GetClass()==AHALElectricWallGimmick::StaticClass() && I->GetActorLocation().Equals(FVector(0,0,10000),1)) { Wall=*I; break; } }
			const auto OthersNamed=[&](const FString& Expected)
			{ if (D.Members.Num()!=2) { return false; } for (const auto& M:D.Members) { if (M!=PS && (!M || M->GetPlayerName()!=Expected)) { return false; } } return true; };
			if (!Prepared)
			{
				if (!Host) { Name(PC,FString::Printf(TEXT("CAM_PREP_%d"),Stage)); }
				if (Host && OthersNamed(FString::Printf(TEXT("CAM_PREP_%d"),Stage)))
				{
					for (auto* Vehicle:Vehicles(W)) { CastChecked<UPrimitiveComponent>(Vehicle->GetRootComponent())->SetEnableGravity(false); }
					MoveOutside(W);
					FHALElectricWallConfig C; C.PillarSpacing=2400; C.Height=600;
					const FTransform Pose(FRotator(0,90,0),FVector(0,0,10000));
					Wall=W->SpawnActorDeferred<AHALElectricWallGimmick>(AHALElectricWallGimmick::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
					if (!Wall || !Wall->Configure(C)) { Test->AddError(TEXT("Cannot prepare camera electric-wall fixture")); return true; } Wall->FinishSpawning(Pose);
					for (TActorIterator<ABasicBallActor> I(W);I;++I) { I->SetPrimaryCameraTarget(false); }
					auto* BallClass=LoadClass<ABasicBallActor>(nullptr,TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
					if (!BallClass) { Test->AddError(TEXT("Saved migrated ball class is missing")); return true; }
					auto* Ball=W->SpawnActor<ABasicBallActor>(BallClass,FVector(0,4000,10300),FRotator::ZeroRotator);
					if (!Ball) { Test->AddError(TEXT("Cannot prepare camera main-ball fixture")); return true; }
					CastChecked<UPrimitiveComponent>(Ball->GetRootComponent())->SetSimulatePhysics(false); Ball->SetPrimaryCameraTarget(true); Ball->ForceNetUpdate();
					Prepared=true; WarmAt=Now+2;
				}
				else if (!Host && Wall && V->GetActorLocation().Z>10200)
				{ for (auto* Vehicle:Vehicles(W)) { CastChecked<UPrimitiveComponent>(Vehicle->GetRootComponent())->SetEnableGravity(false); } Prepared=true; WarmAt=Now+2; }
				return false;
			}
			if (!Wall) { return false; }
			TArray<UUserWidget*> Screens; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Screens,UHALPracticeRoomWidget::StaticClass(),true);
			auto* Screen=Screens.IsEmpty()?nullptr:Cast<UHALPracticeRoomWidget>(Screens[0]); if (!Screen) { return false; }
			if (!Tracking)
			{
				if (Now<WarmAt || !Camera->GetDebugFrame().bValid || Camera->GetDebugFrame().Distance<1700) { return false; }
				if (PC->WantsRoomMenu()) { PC->ToggleRoomMenu(); return false; }
				if (Screen->GetRoomView().bMenuVisible) { return false; }
				ResetSerial=Camera->GetResetSerial(); Tracking=true; HitAt=Now+.5;
			}
			const auto F=Camera->GetDebugFrame(); const auto Physics=V->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetMovementEffectStatus();
			SawRecovery|=Physics.bRecovery || !Physics.bReady;
			if (!F.bValid || F.Distance<1700 || Camera->GetResetSerial()!=ResetSerial)
			{ Test->AddError(FString::Printf(TEXT("Camera continuity lost Host=%d Stage=%d D=%.2f Goal=%.2f Reset=%u/%u"),Host,Stage,F.Distance,F.DesiredDistance,Camera->GetResetSerial(),ResetSerial)); return true; }
			if (!MenuStep && (Screen->GetRoomView().bMenuVisible || PC->bShowMouseCursor)) { Test->AddError(TEXT("Runtime recovery incorrectly switched to menu input")); return true; }
			if (!Host && !MenuStep) { Name(PC,FString::Printf(TEXT("CAM_ARMED_%d"),Stage)); }
			if (Host && !MenuStep && Cycles<3 && OthersNamed(FString::Printf(TEXT("CAM_ARMED_%d"),Stage)) && Now>=HitAt)
			{
				if (!Outside) { MoveOutside(W); Outside=true; HitAt=Now+.5; }
				else { for (auto* Vehicle:Vehicles(W)) { CastChecked<UPrimitiveComponent>(Vehicle->GetRootComponent())->SetPhysicsLinearVelocity(FVector(2000,0,0)); } Outside=false; ++Cycles; HitAt=Now+1.6; }
			}
			if (!MenuStep && Wall->GetAcceptedContactCount()>=6 && (!Host || Now>=HitAt))
			{ PC->ToggleRoomMenu(); MenuStep=1; MenuAt=Now+.3; }
			if (MenuStep==1 && Now>=MenuAt)
			{ Test->TestTrue(TEXT("Real menu owns UI without resetting zoom"),Screen->GetRoomView().bMenuVisible); PC->ToggleRoomMenu(); MenuStep=2; MenuAt=Now+.3; }
			if (MenuStep==2 && Now>=MenuAt)
			{
				Test->TestFalse(TEXT("Closing menu restores driving input ownership"),Screen->GetRoomView().bMenuVisible);
				Test->TestTrue(TEXT("Custom layout selection is preserved"),Screen->UsesDesignerLayout()==Custom);
				MenuStep=3; ++CompletedScenes;
				UE_LOG(LogTemp,Display,TEXT("CameraRoom scene passed Host=%d Stage=%d Contacts=%u D=%.2f RecoveryObserved=%d Custom=%d"),Host,Stage,Wall->GetAcceptedContactCount(),F.Distance,SawRecovery,Custom);
			}
			if (MenuStep==3)
			{
				if (!Host) { Name(PC,FString::Printf(TEXT("CAM_DONE_%d"),Stage)); }
				else if (OthersNamed(FString::Printf(TEXT("CAM_DONE_%d"),Stage)) && Now>=NextAction)
				{ NextAction=Now+.5; if (!Stage) { if (R->CheckStart(PC)==EHALRoomResult::Accepted) { PC->RequestStartMatch(); } } else { PC->HALReturnToRoom(); } }
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCameraRoomNetworkTest,"HAL.NetworkSmoke.Camera.RoomElectricContinuity",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALCameraRoomNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCameraRoomProbe>(this)); return true; }
#endif
