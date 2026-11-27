#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "../BallControlComponent.h"
#include "../HALHomingBall.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALIncomingBallWidget.h"
#include "../HALPlayerController.h"
#include "../HALSpecialBallCue.h"
#include "../HALSpecialBallPad.h"
#include "../TestVehiclePawn.h"
#include "../VehicleHealthComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	class FHomingMatchShot : public IAutomationLatentCommand
	{
	public:
		explicit FHomingMatchShot(FAutomationTestBase& InTest) : Test(InTest) {}
		~FHomingMatchShot() { RestorePool(); }
		bool Update() override;
	private:
		FAutomationTestBase& Test;
		const double Deadline = FPlatformTime::Seconds() + 95.;
		double Finished = -1., SetUp = -1.;
		float InitialHP = -1.f, PeakSpeed = 0.f;
		bool bNamed = false, bShot = false, bArrival = false, bHeld = false, bLocked = false;
		bool bWarned = false, bInFrame = false, bImpact = false, bShooterWarned = false;
		bool bHitVerified = false, bFreeArrival = false, bFreeBody = false, bFreeExpiry = false;
		TOptional<FVector> FreeSite;
		TWeakObjectPtr<UHALSpecialBallSupplyDefinition> FixtureSupply;
		TArray<EHALPadBall> OriginalPool;
		TWeakObjectPtr<AHALHomingBall> Launched;
		void RestorePool()
		{
			if (FixtureSupply.IsValid()) { FixtureSupply->BallPool = MoveTemp(OriginalPool); FixtureSupply.Reset(); }
		}
		void Photo(const TCHAR* Name) const
		{
			if (FParse::Param(FCommandLine::Get(), TEXT("HALHomingShots")))
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("HomingBall")/Name),true,false);
			}
		}
	};

	bool FHomingMatchShot::Update()
	{
		const double Now = FPlatformTime::Seconds();
		if (Finished >= 0.) { return Now >= Finished; }
		if (Now > Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Shot timed out: setup=%.2f launched=%d arrival=%d lock=%d warning=%d inFrame=%d impact=%d peak=%.0f free arrival/body/expiry=%d/%d/%d"),
				SetUp,bShot,bArrival,bLocked,bWarned,bInFrame,bImpact,PeakSpeed,bFreeArrival,bFreeBody,bFreeExpiry));
			return true;
		}
		UWorld* World = nullptr;
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			auto* Candidate = Context.World();
			if (Candidate && Candidate->IsGameWorld() && (Candidate->IsNetMode(NM_Client) || Candidate->IsNetMode(NM_ListenServer))) { World=Candidate; break; }
		}
		if (!World) { return false; }
		auto* PC = Cast<AHALPlayerController>(World->GetFirstPlayerController());
		auto* Match = World->GetGameState<AHALMatchGameState>();
		auto* Local = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
		if (!Local || !Match || !PC->IsLocalController() || !PC->PlayerState) { return false; }
		const bool bServer = World->IsNetMode(NM_ListenServer);
		if (!bServer && !bNamed) { PC->SetName(TEXT("HomingObserver")); bNamed = true; }
		ATestVehiclePawn* Target = nullptr;
		for (const auto& State : Match->PlayerArray)
		{
			if (State && State->GetPlayerName()==TEXT("HomingObserver")) { Target = Cast<ATestVehiclePawn>(State->GetPawn()); break; }
		}
		if (!Target) { return false; }
		if (bServer && Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers) { PC->HALStartMatch(); }
		if (Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
		auto* Health = Target->FindComponentByClass<UVehicleHealthComponent>();
		if (!Health || !Health->IsHealthReady()) { return false; }
		const auto* Type = GetDefault<UHALMatchBallSettings>()->HomingBallType.LoadSynchronous();
		if (!Test.TestNotNull(TEXT("Saved homing ball DA"),Type)) { return true; }
		const double Clock = World->GetTimeSeconds();
		if (SetUp < 0.)
		{
			SetUp = Clock;
			InitialHP = Health->GetCurrentHP();
			if (bServer)
			{
				// Exercise the real round timer and both pad/ball cues; restrict only this host process's first bag.
				auto* Supply = GetDefault<UHALMatchBallSettings>()->SpecialBallSupply.LoadSynchronous();
				if (!Test.TestTrue(TEXT("Production pool offers the new ball"),Supply && Supply->BallPool.Contains(EHALPadBall::Homing))) { return true; }
				FixtureSupply=Supply; OriginalPool=Supply->BallPool; Supply->BallPool={EHALPadBall::Homing};
				FPacketSimulationSettings Lag;
				Lag.PktLag = 80;
				World->GetNetDriver()->SetPacketSimulationSettings(Lag);
				auto Place = [](ATestVehiclePawn& Car, double X)
				{
					Car.SetActorLocationAndRotation(FVector(X,3600.,-100.),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
					auto* Body = CastChecked<UPrimitiveComponent>(Car.GetRootComponent());
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
					Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
					Car.ForceNetUpdate();
				};
				Place(*Local,-3400.);
				Place(*Target,2500.);
			}
		}
		if (bServer && !Launched.IsValid() && !bShot && Clock-SetUp>1.5)
		{
			const FVector Pickup = Local->GetActorLocation() + Local->GetActorForwardVector().GetSafeNormal2D()*310. + FVector::UpVector*60.;
			Launched = World->SpawnActor<AHALHomingBall>(Pickup,FRotator::ZeroRotator);
			if (!Test.TestNotNull(TEXT("Production missile spawned"),Launched.Get())) { return true; }
		}
		if (bServer && Launched.IsValid() && !bShot && Clock-SetUp>2.6)
		{
			auto* Control = Local->FindComponentByClass<UBallControlComponent>();
			FVector Recoil;
			if (!Test.TestTrue(TEXT("Shooter automatically acquired the missile"),Control && Control->GetHeldBall()==Launched.Get())
				|| !Test.TestTrue(TEXT("Normal vehicle launch releases the missile"),Control->LaunchHeldBall(Recoil) && !Recoil.IsNearlyZero())) { return true; }
			bShot = true;
		}
		const UHALIncomingBallWidget* HUD = nullptr;
		for (TObjectIterator<UHALIncomingBallWidget> It; It; ++It)
		{
			if (It->GetWorld()==World && It->GetOwningPlayer()==PC) { HUD=*It; break; }
		}
		for (TActorIterator<AHALHomingBall> It(World); It; ++It)
		{
			auto* Ball=*It;
			if (!bHeld && Ball->GetBallState()==EBasicBallState::Controlled && Ball->GetControlledBy())
			{
				bHeld=true;
				Test.TestTrue(TEXT("The held pod renders with its collision sphere hidden"),Ball->GetPhysicsRoot()->bHiddenInGame && !Ball->GetVisualMesh()->bHiddenInGame);
			}
			if (Ball->GetBallState()!=EBasicBallState::Launched || Ball->GetLockedVehicle()!=Target) { continue; }
			bLocked = true;
			PeakSpeed = FMath::Max(PeakSpeed,float(Ball->GetPhysicsRoot()->GetPhysicsLinearVelocity().Size2D()));
			const auto* Arrow = HUD ? HUD->GetArrows().FindByPredicate([Ball](const FHALIncomingBallArrow& Item)
				{ return Item.Ball.Get()==Ball && Item.bMissileLock && Item.bShown; }) : nullptr;
			if (bServer) { bShooterWarned |= !!Arrow; continue; }
			if (Arrow && !bWarned)
			{
				if (FMath::Abs(Arrow->Position.X-.5)<.10 && Arrow->Position.Y>.5)
				{
					Test.TestTrue(TEXT("Bottom-centre missile clears the energy HUD"),
						Arrow->Position.Y+Arrow->MissileSize*.64 <= 1.-Type->WarningBottomInset);
				}
				FVector2D Screen, OwnScreen;
				if (PC->ProjectWorldLocationToScreen(Ball->GetActorLocation(),Screen) && PC->ProjectWorldLocationToScreen(Local->GetActorLocation(),OwnScreen))
				{
					if (!Test.TestTrue(TEXT("HUD points from local car toward the incoming missile"),FVector2D::DotProduct(Arrow->Direction,(Screen-OwnScreen).GetSafeNormal())>.90)) { return true; }
				}
				bWarned=true; Photo(TEXT("ClientLock.png"));
			}
			FVector2D Screen;
			int32 Width=0,Height=0;
			PC->GetViewportSize(Width,Height);
			if (bWarned && !bInFrame && !Arrow && PC->ProjectWorldLocationToScreen(Ball->GetActorLocation(),Screen)
				&& Screen.X>Width*.1 && Screen.X<Width*.9 && Screen.Y>Height*.1 && Screen.Y<Height*.9)
			{
				TInlineComponentArray<UStaticMeshComponent*> Parts(Ball);
				const bool bJet = Parts.ContainsByPredicate([Type](const UStaticMeshComponent* Part)
					{ return Part->GetStaticMesh()==Type->ExhaustMesh && Part->IsVisible() && Part->GetRelativeScale3D().X>1.; });
				if (!Test.TestTrue(TEXT("Visible missile carries its accelerated exhaust"),bJet)) { return true; }
				bInFrame = true; Photo(TEXT("ClientCruise.png"));
			}
		}
		if (!FreeSite.IsSet())
		{
			for (TActorIterator<AHALSpecialBallPad> It(World); It; ++It)
			{
				if (It->GetWarning().Ball!=EHALPadBall::Homing) { continue; }
				FreeSite=It->GetActorLocation();
				RestorePool();
				if (!bServer && FParse::Param(FCommandLine::Get(),TEXT("HALHomingShots")))
				{
					// The shot above uses the player's unmodified match camera. Frame the separate pad sequence for art review.
					const FVector LookAt=FreeSite.GetValue()+FVector::UpVector*80.;
					const FVector Eye=LookAt+FVector(470,-580,530);
					auto* Camera=World->SpawnActor<ACameraActor>(Eye,(LookAt-Eye).Rotation());
					Camera->GetCameraComponent()->SetFieldOfView(49.f);
					PC->SetViewTarget(Camera);
					Photo(TEXT("ClientPad.png"));
				}
				break;
			}
		}
		for (TActorIterator<AHALSpecialBallCue> It(World); It; ++It)
		{
			const auto& Cue = It->GetCueState();
			if (Cue.Type!=Type) { continue; }
			bArrival |= Cue.Kind==EHALBallCue::Arrival;
			const double CueAge = Match->GetServerWorldTimeSeconds()-Cue.Started;
			if (Cue.Kind==EHALBallCue::Impact && !bImpact && CueAge>Type->ImpactSeconds*.27)
			{
				Test.TestEqual(TEXT("Visual wave has DA game radius"),Cue.Radius,Type->ExplosionRadius);
				bImpact = true;
				if (!bServer) { Photo(TEXT("ClientImpact.png")); }
			}
			if (bHitVerified && FreeSite.IsSet() && FVector::Dist2D(It->GetActorLocation(),FreeSite.GetValue())<220. && CueAge>.07)
			{
				if (Cue.Kind==EHALBallCue::Arrival && !bFreeArrival)
				{
					bFreeArrival=true;
					if (!bServer) { Photo(TEXT("ClientArrival.png")); }
				}
				if (Cue.Kind==EHALBallCue::Expiration && !bFreeExpiry)
				{
					bFreeExpiry=true;
					if (!bServer) { Photo(TEXT("ClientExpiry.png")); }
				}
			}
		}
		if (!bHitVerified)
		{
			if (!bImpact || Health->GetCurrentHP() >= InitialHP) { return false; }
			const auto Impact=Target->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact();
			if (!Impact.EventId.IsValid()) { return false; }
			Test.TestTrue(TEXT("Arrival, possession, moving lock and terminal cue on this peer"),bArrival && bHeld && bLocked);
			Test.TestTrue(TEXT("Missile accelerates in the match"),PeakSpeed > Type->InitialSpeed*1.3f);
			Test.TestEqual(TEXT("Victim receives DA impact tier"),Impact.SourceTier,Type->Gameplay.Impact.Tier);
			if (bServer) { Test.TestFalse(TEXT("Shooter has no lock warning"),bShooterWarned); }
			else { Test.TestTrue(TEXT("Recipient sees screen-edge warning then normal onscreen missile"),bWarned && bInFrame); }
			Test.AddInfo(FString::Printf(TEXT("Host=%d HP %.0f to %.0f; peak %.0f cm/s; radius %.0f cm; warning %d, in frame %d."),
				bServer,InitialHP,Health->GetCurrentHP(),PeakSpeed,Type->ExplosionRadius,bWarned,bInFrame));
			bHitVerified=true;
		}
		if (!bFreeBody && FreeSite.IsSet())
		{
			for (TActorIterator<AHALHomingBall> It(World); It; ++It)
			{
				if (It->GetBallState()==EBasicBallState::Free && FVector::Dist2D(It->GetActorLocation(),FreeSite.GetValue())<220.
					&& It->RemainingFreeSeconds()>0 && It->RemainingFreeSeconds()<Type->FreeLifeSeconds-1.5)
				{
					bFreeBody=true;
					if (!bServer) { Photo(TEXT("ClientPod.png")); }
					break;
				}
			}
		}
		if (!bFreeExpiry) { return false; }
		Test.TestTrue(TEXT("Second ball appeared on the deck and dissipated naturally on this peer"),bFreeArrival && bFreeBody);
		Finished = Now + (bServer ? 5. : 1.);
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHomingMatchShotTest,"HAL.NetworkSmoke.HomingBall.FormalMatch",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALHomingMatchShotTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("HALHomingFixture"))) { AddWarning(TEXT("Needs -HALHomingFixture on a listen host and a rendering client.")); return true; }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHomingMatchShot>(*this));
	return true;
}
#endif
