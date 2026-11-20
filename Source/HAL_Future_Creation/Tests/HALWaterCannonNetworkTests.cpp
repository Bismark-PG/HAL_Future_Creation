#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HALVehicleAbilityTestSupport.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../BasicBallActor.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALMatchGameMode.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALWaterCannon.h"
#include "../HALWaterCannonAbility.h"
#include "../TestVehiclePawn.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/Material.h"

namespace
{
	struct FWaterNetworkFixture
	{
		FWaterNetworkFixture()
		{
			FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* World,const UWorld::InitializationValues&)
			{
				if (!World->IsGameWorld() || !FParse::Param(FCommandLine::Get(),TEXT("HALWaterJetFixture"))) { return; }
				World->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* Actor)
				{
					if (auto* Mode = Cast<AHALMatchGameMode>(Actor))
					{
						Mode->DefaultPawnClass = LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_09.BP_VehiclePawn_Car_09_C"));
					}
				}));
			});
		}
	} WaterNetworkFixture;

	class FWaterLiveRun : public IAutomationLatentCommand
	{
		FAutomationTestBase& Test;
		double Deadline = FPlatformTime::Seconds()+90, NextRequest = 0, FinishedAt = 0;
		bool bSawJet = false, bSawBall = false, bSpawnedBall = false, bAppliedStun = false, bSawRecoilInStun = false, bSawView = false;
		float MinimumForwardSpeed = 0;
		int32 StunnedPumpFrames = 0;
	public:
		explicit FWaterLiveRun(FAutomationTestBase& InTest) : Test(InTest) {}
		bool Update() override
		{
			const double Clock = FPlatformTime::Seconds();
			if (FinishedAt) { return Clock > FinishedAt + 3; }
			if (Clock > Deadline)
			{
				Test.AddError(FString::Printf(TEXT("Timed out: jet=%d ball=%d spawned=%d stun=%d recoil=%d mesh=%d"),bSawJet,bSawBall,bSpawnedBall,bAppliedStun,bSawRecoilInStun,bSawView));
				return true;
			}
			UWorld* W = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* Candidate = Context.World();
				if (Candidate && Candidate->IsGameWorld() && (Candidate->GetNetMode() == NM_Client || Candidate->GetNetMode() == NM_ListenServer)) { W = Candidate; break; }
			}
			if (!W) { return false; }
			const bool bHost = W->GetNetMode() == NM_ListenServer;
			AHALPlayerController* LocalPC = nullptr;
			for (auto It = W->GetPlayerControllerIterator(); It; ++It)
			{
				auto* PC = Cast<AHALPlayerController>(It->Get());
				if (PC && PC->IsLocalController()) { LocalPC = PC; break; }
			}
			auto* LocalCar = LocalPC ? Cast<ATestVehiclePawn>(LocalPC->GetPawn()) : nullptr;
			auto* Match = W->GetGameState<AHALMatchGameState>();
			if (!LocalCar || !Match || !LocalCar->GetAbilitySystemComponent()) { return false; }
			if (bHost && Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers && W->GetNumPlayerControllers() >= 2) { LocalPC->HALStartMatch(); }
			if (Match->GetMatchPhase() != EHALMatchPhase::Playing) { return false; }
			AHALWaterCannon* Jet = nullptr;
			for (TActorIterator<AHALWaterCannon> It(W); It; ++It) { if (It->GetOwner()) { Jet = *It; break; } }
			if (!bHost && !bSawJet && !Jet && Clock >= NextRequest) { LocalPC->HALUseSkill(); NextRequest = Clock + .7; }
			if (Jet)
			{
				auto* Car = Cast<ATestVehiclePawn>(Jet->GetOwner());
				auto* ASC = Car ? Cast<UHALAbilitySystemComponent>(Car->GetAbilitySystemComponent()) : nullptr;
				if (!ASC) { return false; }
				if (!bSawJet) { Test.TestTrue(TEXT("Only client car owns the accepted primary"),bHost ? Car != LocalCar : Car == LocalCar); }
				bSawJet = true;
				auto* Mesh = Jet->FindComponentByClass<UDynamicMeshComponent>();
				bSawView |= Mesh && Mesh->IsVisible() && Mesh->GetMaterial(0) && Mesh->GetNumMaterials() > 0 && Mesh->Bounds.BoxExtent.Size() > 80;
				if (bHost && !bAppliedStun) { bAppliedStun = ASC->ApplyStun(1.8f,LocalCar); }
				if (bHost && ASC->IsStunned())
				{
					const auto* Body = Cast<UPrimitiveComponent>(Car->GetRootComponent());
					const auto Motion = Car->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetMovementEffectStatus();
					if (Body && Motion.Value.bControlsBlocked && Motion.Value.WaterJet.IsActive(Motion.ServerFrame))
					{
						++StunnedPumpFrames;
						MinimumForwardSpeed = FMath::Min(MinimumForwardSpeed,float(FVector::DotProduct(Body->GetPhysicsLinearVelocity(),Car->GetActorForwardVector())));
						bSawRecoilInStun |= MinimumForwardSpeed < -20;
					}
					if (!bSpawnedBall && StunnedPumpFrames >= 3)
					{
						auto* BallClass = LoadClass<ABasicBallActor>(nullptr,TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
						const FTransform Spawn(FRotator::ZeroRotator, Jet->GetActorLocation()+Car->GetActorForwardVector().GetSafeNormal2D()*350-FVector(0,0,18));
						auto* Ball = W->SpawnActorDeferred<ABasicBallActor>(BallClass,Spawn,LocalCar,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
						if (!Ball) { Test.AddError(TEXT("Ball fixture spawn failed")); return true; }
						Ball->GetVisualMesh()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
						Ball->GetVisualMesh()->SetMaterial(0,UMaterial::GetDefaultMaterial(MD_Surface));
						Ball->SetReplicates(true); Ball->FinishSpawning(Spawn); bSpawnedBall = true;
					}
				}
			}
			for (TActorIterator<ABasicBallActor> It(W); It; ++It)
			{
				auto* Launcher = Cast<ATestVehiclePawn>(It->GetLaunchedBy());
				if (Launcher && It->GetBallState() == EBasicBallState::Launched && (bHost ? Launcher != LocalCar : Launcher == LocalCar)) { bSawBall = true; }
			}
			if (bSawJet && !Jet)
			{
				ATestVehiclePawn* Caster = LocalCar;
				if (bHost)
				{
					for (TActorIterator<ATestVehiclePawn> It(W); It; ++It) { if (*It != LocalCar && It->GetController()) { Caster = *It; break; } }
				}
				const auto After = Caster->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetMovementEffectStatus();
				if (!After.bReady || After.Definition.WaterJet.Activation.IsValid()) { return false; }
				Test.TestTrue(TEXT("Server-confirmed launched ball attributed to client on both roles"),bSawBall);
				Test.TestTrue(TEXT("Owner and observer construct clipped water geometry"),bSawView);
				if (bHost)
				{
					Test.AddInfo(FString::Printf(TEXT("Stunned pump samples=%d minimum signed forward speed=%.1f cm/s ball spawned=%d"),StunnedPumpFrames,MinimumForwardSpeed,bSpawnedBall));
					Test.TestTrue(TEXT("Chaos pushes stationary caster backwards while stunned"),bSawRecoilInStun);
				}
				FinishedAt = Clock;
			}
			return false;
		}
	};

	/** Same car, same flat starting position and elapsed game time in each real Chaos run. */
	class FWaterPedalRun : public IAutomationLatentCommand
	{
		FAutomationTestBase& Test;
		int32 Trial = 0;
		double Deadline = FPlatformTime::Seconds()+65, SettleUntil = -1, DrivingAt = -1, FinishedAt = -1;
		FTransform StartPose;
		float Speeds[4] = {};
		TSet<TWeakObjectPtr<AHALWaterCannon>> HostJets;
	public:
		explicit FWaterPedalRun(FAutomationTestBase& InTest) : Test(InTest) {}
		bool Update() override
		{
			if (FinishedAt >= 0) { return FPlatformTime::Seconds() >= FinishedAt+2; }
			if (FPlatformTime::Seconds() > Deadline) { Test.AddError(FString::Printf(TEXT("Pedal trials timed out at %d / observed %d"),Trial,HostJets.Num())); return true; }
			UWorld* W = nullptr;
			for (const auto& C : GEngine->GetWorldContexts()) if (C.World() && C.World()->IsGameWorld()
				&& (C.World()->GetNetMode()==NM_ListenServer || C.World()->GetNetMode()==NM_Client)) { W=C.World(); break; }
			auto* PC = W ? Cast<AHALPlayerController>(W->GetFirstPlayerController()) : nullptr;
			auto* Car = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			auto* Match = W ? W->GetGameState<AHALMatchGameState>() : nullptr;
			if (!Car || !Match) { return false; }
			const bool Host = W->GetNetMode()==NM_ListenServer;
			if (Host && Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers && W->GetNumPlayerControllers()>=2) { PC->HALStartMatch(); }
			if (Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
			bool bJet = false;
			for (TActorIterator<AHALWaterCannon> It(W); It; ++It)
			{
				if (!It->GetOwner() || (Host ? It->GetOwner()!=Car : It->GetOwner()==Car)) { continue; }
				bJet = true;
				HostJets.Add(*It);
			}
			if (!Host)
			{
				if (!bJet && HostJets.Num()==2) { FinishedAt=FPlatformTime::Seconds(); }
				return false;
			}
			if (Trial==4)
			{
				if (bJet) { return false; }
				Test.AddInfo(FString::Printf(TEXT("Forward ordinary/jet %.1f/%.1f; reverse ordinary/jet %.1f/%.1f cm/s"),
					Speeds[0],Speeds[1],Speeds[2],Speeds[3]));
				Test.TestTrue(TEXT("Full throttle can advance against the pump at a reduced speed"),Speeds[1]>15 && Speeds[1]<Speeds[0]-25);
				Test.TestTrue(TEXT("Reverse pedal and pump accelerate faster than reverse alone"),Speeds[2]<-15 && Speeds[3]<Speeds[2]-25);
				FinishedAt=FPlatformTime::Seconds(); return false;
			}
			const double Now = W->GetTimeSeconds();
			auto* Body = CastChecked<UPrimitiveComponent>(Car->GetRootComponent());
			if (DrivingAt<0)
			{
				if (bJet) { return false; }
				if (SettleUntil<0)
				{
					if (!Trial) { StartPose=Car->GetActorTransform(); }
					Car->SetActorTransform(StartPose,false,nullptr,ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					SettleUntil=Now+.5; return false;
				}
				if (Now<SettleUntil || !Car->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetStabilityObservation().bDrivingAllowed) { return false; }
				if (Trial%2)
				{
					CastChecked<UHALAbilitySystemComponent>(Car->GetAbilitySystemComponent())->GrantUltimateEnergy(100);
					if (!Car->RequestPrimarySkill()) { Test.AddError(TEXT("Host fire engine could not start pedal trial")); return true; }
				}
				FHALVehicleAbilityTestAccess::Pedals(*Car,Trial<2 ? 1.f : 0.f,Trial<2 ? 0.f : 1.f);
				DrivingAt=Now; return false;
			}
			if (Now-DrivingAt>=.85)
			{
				Speeds[Trial]=FVector::DotProduct(Body->GetPhysicsLinearVelocity(),StartPose.GetUnitAxis(EAxis::X).GetSafeNormal2D());
				FHALVehicleAbilityTestAccess::Pedals(*Car,0,0);
				++Trial; DrivingAt=SettleUntil=-1;
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALWaterNetworkTest,"HAL.NetworkSmoke.WaterCannon.ClientCastStunRecoilAndBall",
	EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALWaterNetworkTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("HALWaterJetFixture"))) { AddWarning(TEXT("Run with -HALWaterJetFixture on listen server and one client.")); return true; }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWaterLiveRun>(*this)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALWaterPedalTest,"HAL.NetworkSmoke.WaterCannon.DriveDirections",
	EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALWaterPedalTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("HALWaterJetFixture"))) { AddWarning(TEXT("Requires the Car09 listen/client fixture")); return true; }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWaterPedalRun>(*this)); return true;
}
#endif
