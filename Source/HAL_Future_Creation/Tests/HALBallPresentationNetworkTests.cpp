#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehiclePresentationTestSupport.h"
#include "../HALBallPresentationComponent.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALDebugSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

struct FHALBallRelativeNetworkAccess
{
	static bool Acquire(UBallControlComponent* Control, ABasicBallActor* Ball)
	{
		return Control && Ball && Control->AcquireBall(Ball);
	}
};

namespace
{
	class FBallRelativeLiveTest : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline=FPlatformTime::Seconds()+90,Complete=-1;
		bool bPrepared=false,bSetup=false,bDriving=false,bShot=false,bObservedOwn=false,bObservedRemote=false;
		float DriveStart=0; FVector InitialOwn=FVector::ZeroVector;
		TWeakObjectPtr<ABasicBallActor> OwnBall,RemoteBall;
	public:
		explicit FBallRelativeLiveTest(FAutomationTestBase* T):Test(T){}
		virtual bool Update() override
		{
			if (Complete>0) { return FPlatformTime::Seconds()-Complete>4; }
			if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Ball relative smoke timed out; paired host/client required.")); return true; }
			UWorld* World=nullptr;
			for (const auto& C:GEngine->GetWorldContexts())
			{ auto* W=C.World(); if (W && W->IsGameWorld() && (W->GetNetMode()==NM_Client || W->GetNetMode()==NM_ListenServer)) { World=W; break; } }
			if (!World) { return false; }
			const bool Host=World->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC=nullptr;
			for (auto It=World->GetPlayerControllerIterator();It;++It) { if (auto* C=Cast<AHALPlayerController>(It->Get());C && C->IsLocalController()) { PC=C;break; } }
			auto* Own=PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other=nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World);It;++It) { if (*It!=Own && It->GetPlayerState()) { Other=*It;break; } }
			auto* Match=World->GetGameState<AHALMatchGameState>();
			if (!Own || !Other || !Own->GetPlayerState() || !Other->GetPlayerState() || !Match) { return false; }
			if (!bPrepared)
			{
				auto* A=World->SpawnActor<AActor>(); auto* Floor=NewObject<UStaticMeshComponent>(A); A->AddInstanceComponent(Floor); A->SetRootComponent(Floor);
				Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
				Floor->SetCollisionProfileName(TEXT("BlockAll")); Floor->SetWorldLocation(FVector(0,0,9950)); Floor->SetWorldScale3D(FVector(80,80,1)); Floor->RegisterComponent();
				PC->SetName(Host ? TEXT("HAL_BallRReadyH") : TEXT("HAL_BallRReadyC")); bPrepared=true;
			}
			const FString OtherName=Other->GetPlayerState()->GetPlayerName();
			if (!OtherName.StartsWith(TEXT("HAL_BallR"))) { return false; }
			if (Match->GetMatchPhase()!=EHALMatchPhase::Playing) { if (Host) { PC->HALStartMatch(); } return false; }
			if (Host && !bSetup)
			{
				FPacketSimulationSettings Lag; Lag.PktLag=80; World->GetNetDriver()->SetPacketSimulationSettings(Lag);
				auto* BallClass=GetDefault<UHALDebugSettings>()->DefaultBallClass.LoadSynchronous();
				if (!BallClass) { Test->AddError(TEXT("Missing configured ball class")); return true; }
				int Index=0;
				for (auto* P:{Own,Other})
				{
					auto* Body=FHALPresentationTestAccess::Body(P);
					const auto Bounds=Body->CalcBounds(FTransform(Body->GetComponentScale()));
					Body->SetWorldLocationAndRotation(FVector(Index++ ? 1000:-1000,0,10000+Bounds.BoxExtent.Z-Bounds.Origin.Z+.5),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); P->ForceNetUpdate();
					FTransform Target; P->FindComponentByClass<UBallControlComponent>()->GetControlTargetWorldTransform(Target);
					FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					auto* B=World->SpawnActor<ABasicBallActor>(BallClass,Target.GetLocation()+FVector(120,30,0),FRotator::ZeroRotator,Spawn);
					if (!FHALBallRelativeNetworkAccess::Acquire(P->FindComponentByClass<UBallControlComponent>(),B)) { Test->AddError(TEXT("Native constrained capture failed")); return true; }
					if (P==Own) { OwnBall=B; } else { RemoteBall=B; }
				}
				bSetup=true;
			}
			if (!Host)
			{
				for (TActorIterator<ABasicBallActor> It(World);It;++It)
				{
					if (It->GetBallState()!=EBasicBallState::Controlled) { continue; }
					if (It->GetControlledBy()==Own) { OwnBall=*It; } else if (It->GetControlledBy()==Other) { RemoteBall=*It; }
				}
			}
			if (!OwnBall.IsValid() || !RemoteBall.IsValid()) { return false; }
			auto* Local=OwnBall->FindComponentByClass<UHALBallPresentationComponent>();
			auto* Remote=RemoteBall->FindComponentByClass<UHALBallPresentationComponent>();
			if (!Local || !Remote) { Test->AddError(TEXT("Replicated ball presentation components missing")); return true; }
			if (!Host && Local->GetVisualMode()==EHALBallVisualMode::Relative && Remote->GetVisualMode()==EHALBallVisualMode::Relative)
			{
				bObservedOwn=true; bObservedRemote=true;
				if (!bDriving) { PC->SetName(TEXT("HAL_BallRBoundC")); }
			}
			if (!bDriving && ((Host && OtherName.Contains(TEXT("Bound"))) || (!Host && OtherName.Contains(TEXT("Drive")))))
			{
				bDriving=true; DriveStart=World->GetTimeSeconds(); InitialOwn=Own->GetActorLocation();
				FHALPresentationTestAccess::SetInput(Own,Host ? -.7f:.7f,1);
				if (Host) { PC->SetName(TEXT("HAL_BallRDriveH")); }
			}
			if (!bDriving) { return false; }
			const float Elapsed=World->GetTimeSeconds()-DriveStart;
			if (!bShot && Elapsed>2)
			{
				Test->TestTrue(TEXT("Actual driving input moves owning car"),FVector::Dist2D(InitialOwn,Own->GetActorLocation())>100);
				if (Host)
				{ Test->TestTrue(TEXT("Authority sampled both constrained balls"),Local->GetPublishedSampleCount()>10 && Remote->GetPublishedSampleCount()>10); }
				else
				{
					Test->TestTrue(TEXT("Both local and remote holder paths rendered"),bObservedOwn && bObservedRemote);
					Test->TestTrue(TEXT("Repeated physics samples reached client"),Local->GetReceivedSampleCount()>10 && Remote->GetReceivedSampleCount()>10);
					Test->TestTrue(TEXT("Held presentation persists while steering"),Local->GetRelativeDisplayFrames()>30 && Remote->GetRelativeDisplayFrames()>30);
				}
				FHALPresentationTestAccess::SetInput(Own,0); FHALPresentationTestAccess::StartLaunch(Own); bShot=true;
			}
			if (bShot && Elapsed>4 && OwnBall->GetBallState()!=EBasicBallState::Controlled && RemoteBall->GetBallState()!=EBasicBallState::Controlled
				&& (Host || (Local->GetVisualMode()==EHALBallVisualMode::Physics && Remote->GetVisualMode()==EHALBallVisualMode::Physics)))
			{
				Test->TestTrue(TEXT("Both ball physics roots remain alive after launch"),OwnBall->GetPhysicsRoot()->IsSimulatingPhysics() && RemoteBall->GetPhysicsRoot()->IsSimulatingPhysics());
				if (!Host) { Test->TestFalse(TEXT("Local mesh attachment restored after handoff"),OwnBall->GetVisualMesh()->IsUsingAbsoluteLocation()); }
				UE_LOG(LogTemp,Display,TEXT("HAL BallRelative smoke: Host=%d Complete=1 OwnSamples=%u RemoteSamples=%u OwnRelativeFrames=%u RemoteRelativeFrames=%u Resimulations=%d HostOutgoingLag=80ms"),
					Host,Host ? Local->GetPublishedSampleCount():Local->GetReceivedSampleCount(),Host ? Remote->GetPublishedSampleCount():Remote->GetReceivedSampleCount(),
					Local->GetRelativeDisplayFrames(),Remote->GetRelativeDisplayFrames(),Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetResimulationRunCount());
				Complete=FPlatformTime::Seconds();
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBallRelativeNetworkTest,"HAL.NetworkSmoke.BallRelativePresentation",
	EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALBallRelativeNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FBallRelativeLiveTest>(this)); return true; }
#endif
