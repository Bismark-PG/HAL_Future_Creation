#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "../HALTornado.h"
#include "../HALTornadoAbility.h"
#include "../HALMatchGameMode.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALAbilitySystemComponent.h"
#include "../BasicBallActor.h"
#include "../VehicleHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

struct FHALTornadoNetworkAccess
{
	static bool Mounted(const AHALTornado& T)
	{
		const auto Controller=T.Particles?T.Particles->GetSystemInstanceController():nullptr;
		const auto* Instance=Controller?Controller->GetSystemInstance_Unsafe():nullptr;
		if(!Instance || Instance->GetEmitters().Num()!=3) { return false; }
		for(const auto& E:Instance->GetEmitters()) { if(E->GetNumParticles()!=1) { return false; } }
		return T.Particles->IsActive() && T.Particles->GetAttachParent()==T.GetRootComponent()
			&& T.Materials.Num()==3 && T.Materials[0] && T.Materials[1] && T.Materials[2]
			&& FMath::IsNearlyEqual(T.Materials[2]->K2_GetScalarParameterValue(TEXT("OuterOpacity")),T.State.FX.OuterOpacity)
			&& T.Materials[0]->K2_GetVectorParameterValue(TEXT("MidColor")).Equals(T.State.FX.MidColor)
			&& FMath::IsNearlyEqual(T.Materials[0]->K2_GetScalarParameterValue(TEXT("GrayMix")),.42f)
			&& FMath::IsNearlyEqual(T.Materials[0]->K2_GetScalarParameterValue(TEXT("GlowStrength")),.8f)
			&& FMath::IsNearlyEqual(T.State.FX.FormationExponent,1.1f)
			&& FMath::IsNearlyEqual(T.State.FX.DifferentialRotation,.72f);
	}
};
namespace
{
	void AddTornadoTravelBox(UWorld* World, const FVector& Center, const FVector& Extent)
	{
		auto* Actor = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->SetWorldLocation(Center);
		Box->RegisterComponent();
	}

	bool PrepareTornadoLane(UWorld* World, int32 Players, bool& bReady)
	{
		if (bReady) { return true; }
		TArray<ATestVehiclePawn*> Cars;
		if (World->GetNetMode() == NM_ListenServer)
		{
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It)
			{ if (It->GetPlayerState() && It->GetController()) { Cars.Add(*It); } }
			if (Cars.Num() != Players) { return false; }
		}
		// Keep map hazards out of the firing lane, including client vehicle ground prediction.
		AddTornadoTravelBox(World, FVector(24500,0,950), FVector(5500,2200,50));
		for (int32 Index = 0; Index < Cars.Num(); ++Index)
		{
			const double LaneY = (Index - (Players - 1) * .5) * 750.;
			Cars[Index]->SetActorLocationAndRotation(FVector(20000,LaneY,1140), FRotator::ZeroRotator,
				false, nullptr, ETeleportType::ResetPhysics);
		}
		bReady = true;
		return true;
	}

	struct FTornadoFixture
	{
		FTornadoFixture()
		{
			FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* W,const UWorld::InitializationValues&)
			{
				if(!W->IsGameWorld()) { return; }
				const bool bTravel = FParse::Param(FCommandLine::Get(), TEXT("HALTornadoTravelFixture"));
				if (!bTravel && !FParse::Param(FCommandLine::Get(),TEXT("HALTornadoFixture"))) { return; }
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* Actor)
				{
					if(auto* Mode=Cast<AHALMatchGameMode>(Actor))
					{ Mode->DefaultPawnClass=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_05.BP_VehiclePawn_Car_05_C")); }
				}));
				// The travel run uses the vehicle's saved Blueprint ability and unmodified skill DA.
				if (bTravel) { return; }
				auto* Ability=GetMutableDefault<UHALTornadoAbility>(); auto* Config=NewObject<UHALTornadoConfig>(Ability);
				if(const auto* Saved=LoadObject<UHALTornadoConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car05_Tornado.DA_Ability_Car05_Tornado")))
				{ Config->Rules=Saved->Rules; Config->FX=Saved->FX; }
				Config->FX.MidColor=FLinearColor(.05f,.55f,.65f); Config->FX.GrayMix=.42f;
				Config->FX.DifferentialRotation=.72f; Config->FX.GlowStrength=.8f;
				Config->FX.FormationExponent=1.1f; Ability->SkillConfig=Config;
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* Actor)
				{
					auto* Pawn=Cast<ATestVehiclePawn>(Actor); if(!Pawn || !Pawn->GetVehicleDefinition()) { return; }
					auto* D=FHALVehicleAbilityTestAccess::Configure(Pawn,UHALTornadoAbility::StaticClass());
					D->Combat.Version=EHALCombatConfigVersion::AttackSnapshotV1; D->Impact.Version=EHALImpactVersion::TieredV1;
				}));
			});
		}
	} TornadoFixture;
	class FTornadoLive : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test; double Deadline=FPlatformTime::Seconds()+85, Requested=-1, LastAttempt=-1, Done=-1;
		bool SawOwn=false,SawOther=false,SawGather=false,SawActive=false,SawOver=false,SawStop=false,SawFX=false;
		bool HitPlaced=false,SawUp=false,Shot=false,Disarmed=false,bLaneReady=false;
		TWeakObjectPtr<ABasicBallActor> ProbeBall; TSet<TWeakObjectPtr<AHALTornado>> Walled;
	public:
		explicit FTornadoLive(FAutomationTestBase* T):Test(T){}
		bool Update() override
		{
			if(Done>=0) { return FPlatformTime::Seconds()-Done>3; }
			if(FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Tornado network fixture timed out")); return true; }
			UWorld* W=nullptr;
			for(const auto& C:GEngine->GetWorldContexts()) if(auto* Candidate=C.World(); Candidate && Candidate->IsGameWorld()
				&& (Candidate->GetNetMode()==NM_Client || Candidate->GetNetMode()==NM_ListenServer)) { W=Candidate; break; }
			if(!W) { return false; } const bool Host=W->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC=nullptr;
			for(auto It=W->GetPlayerControllerIterator();It;++It) { auto* C=Cast<AHALPlayerController>(It->Get()); if(C&&C->IsLocalController()) { PC=C; break; } }
			auto* V=PC?Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr;
			auto* ASC=V?Cast<UHALAbilitySystemComponent>(V->GetAbilitySystemComponent()):nullptr;
			auto* Match=W->GetGameState<AHALMatchGameState>(); ATestVehiclePawn* Other=nullptr;
			for(TActorIterator<ATestVehiclePawn> It(W);It;++It) if(*It!=V && It->GetAbilitySystemComponent()) { Other=*It; break; }
			if(!V||!ASC||!Match||!Other||!ASC->IsHealthInitialized() || !PrepareTornadoLane(W,2,bLaneReady)) { return false; }
			if(Host && Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers) { PC->HALStartMatch(); }
			if(Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
			const double Now=Match->GetServerWorldTimeSeconds(); int32 Count=0; AHALTornado* Own=nullptr; AHALTornado* Remote=nullptr;
			for(TActorIterator<AHALTornado> It(W);It;++It)
			{
				auto* T=*It; ++Count; const auto& S=T->GetState(); if(!S.Activation.IsValid()) { continue; }
				if(T->GetOwner()==V) { Own=T; SawOwn=true; } else { Remote=T; SawOther=true; }
				const auto Phase=S.Rules.Phase(Now-S.StartServerTime);
				SawGather|=Phase==EHALTornadoPhase::Gathering; SawActive|=Phase==EHALTornadoPhase::Active; SawOver|=Phase==EHALTornadoPhase::Dissipating;
				SawStop|=S.bStopped && Phase==EHALTornadoPhase::Active;
				if(FApp::CanEverRender()) { SawFX|=FHALTornadoNetworkAccess::Mounted(*T) && S.FX.MidColor.Equals(FLinearColor(.05f,.55f,.65f)); }
				if(Host && !Walled.Contains(T))
				{
					auto* A=W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(A); A->SetRootComponent(Box);
					Box->SetBoxExtent(FVector(10,300,300)); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionResponseToAllChannels(ECR_Block);
					Box->SetWorldLocationAndRotation(S.Ground+S.Direction*(S.Rules.Radius+100)+FVector(0,0,300),S.Direction.Rotation()); Box->RegisterComponent(); Walled.Add(T);
				}
			}
			if(!Own && !SawOwn && (LastAttempt<0 || Now-LastAttempt>1))
			{ PC->HALUseSkill(); LastAttempt=Now; if(Requested<0) { Requested=Now; } }
			if(Host && Own && Remote && Own->GetState().bStopped && Remote->GetState().bStopped && !HitPlaced)
			{
				const auto& S=Own->GetState();
				Other->SetActorLocation(S.Ground-S.Direction*35+FVector(0,0,60),false,nullptr,ETeleportType::TeleportPhysics);
				CastChecked<UPrimitiveComponent>(Other->GetRootComponent())->SetPhysicsLinearVelocity(FVector::ZeroVector);
				HitPlaced=true;
			}
			if(Host && HitPlaced)
			{
				SawUp|=CastChecked<UPrimitiveComponent>(Other->GetRootComponent())->GetPhysicsLinearVelocity().Z>50;
				if(Own && Own->GetVehicleHitCount()>0 && !Shot)
				{
					const auto& S=Own->GetState();
					auto* Class=LoadClass<ABasicBallActor>(nullptr,TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
					const FVector Side=FVector::CrossProduct(FVector::UpVector,S.Direction);
					const float BallRadius = Class->GetDefaultObject<ABasicBallActor>()->GetConfiguredSphereRadius();
					const FTransform Transform(FRotator::ZeroRotator,S.Ground+Side*(S.Rules.Radius+BallRadius+40)+FVector(0,0,BallRadius+12));
					auto* B=W->SpawnActorDeferred<ABasicBallActor>(Class,Transform,V,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
					B->GetVisualMesh()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
					B->SetReplicates(true); B->FinishSpawning(Transform);
					// The saved probe definition may leave its first shell material empty. Give this
					// transient ball a real material before the normal control beam creates its MID.
					B->GetVisualMesh()->SetMaterial(0,UMaterial::GetDefaultMaterial(MD_Surface));
					if (Test->TestTrue(TEXT("Healthy caster takes control of the defense probe"), B->BeginControl(V))
						&& Test->TestTrue(TEXT("Real defense probe launches"), B->LaunchFromControl(V,V,-Side*3500))) { ProbeBall=B; }
					Shot=true; ASC->ApplyStun(.4f,Other);
				}
			}
			for(TActorIterator<ABasicBallActor> It(W);It;++It) if(It->GetOwner()==V || It->GetOwner()==Other)
			{
				const auto B=It->GetBallRepState(); Disarmed|=B.State==EBasicBallState::Free && B.StateSequence>=3 && B.LastLauncherPlayerId==INDEX_NONE && !B.AttackSnapshot.AttackId.IsValid();
			}
			if(SawOwn&&SawOther&&Count==0 && ASC->GetUltimateEnergyView().bHasEnergy)
			{
				Test->TestTrue(TEXT("Owner and observer see complete three-stage lifecycle"),SawGather&&SawActive&&SawOver);
				Test->TestTrue(TEXT("Wall stop replicates while gameplay remains active"),SawStop);
				Test->TestTrue(TEXT("Attack ball becomes an unattributed Free ball on both ends"),Disarmed);
				if(FApp::CanEverRender()) { Test->TestTrue(TEXT("Native Niagara mount and non-default cyan style bind on replicated actor"),SawFX); }
				if(Host) { Test->TestTrue(TEXT("Actual target Chaos velocity includes upward launch"),SawUp); }
				Test->AddInfo(FString::Printf(TEXT("Tornado role=%s Own=%d Observer=%d Gather=%d Active=%d Over=%d Stop=%d FX=%d Disarm=%d Up=%d"),Host?TEXT("Host"):TEXT("Client"),SawOwn,SawOther,SawGather,SawActive,SawOver,SawStop,SawFX,Disarmed,SawUp));
				Done=FPlatformTime::Seconds();
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTornadoLiveTest,"HAL.NetworkSmoke.Tornado.LifecycleWallUpdraftDisarmAndFX",
	EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALTornadoLiveTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FTornadoLive>(this)); return true; }

namespace
{
	class FTornadoTravelRun : public IAutomationLatentCommand
	{
	public:
		explicit FTornadoTravelRun(FAutomationTestBase& InTest) : Test(InTest) {}

		bool Update() override
		{
			const double Clock = FPlatformTime::Seconds();
			if (DoneAt > 0) { return Clock > DoneAt + 3.; }
			if (Clock > Deadline) { Test.AddError(TEXT("Timed out waiting for four players, a Car05 cast and retirement")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* Candidate = Context.World();
				if (Candidate && (Candidate->GetNetMode() == NM_ListenServer || Candidate->GetNetMode() == NM_Client))
				{ World = Candidate; break; }
			}
			auto* Match = World ? World->GetGameState<AHALMatchGameState>() : nullptr;
			auto* PC = World ? Cast<AHALPlayerController>(World->GetFirstPlayerController()) : nullptr;
			if (!Match || !PC || !PC->GetPawn() || Match->PlayerArray.Num() != 4 || !PrepareTornadoLane(World,4,bLaneReady)) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			if (Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers)
			{
				// A peer can join before its automation worker starts. Announce readiness on the
				// test player's replicated name so every peer is sampling before the first cast.
				constexpr TCHAR ReadyPrefix[] = TEXT("TornadoReady");
				if (!bAnnouncedReady)
				{
					const auto* Player = PC->GetPlayerState<APlayerState>();
					if (!Player) { return false; }
					PC->SetName(FString::Printf(TEXT("%s%d"), ReadyPrefix, Player->GetPlayerId()));
					bAnnouncedReady = true;
				}
				if (!bHost) { return false; }
				for (const APlayerState* Player : Match->PlayerArray)
				{
					if (!Player || !Player->GetPlayerName().StartsWith(ReadyPrefix)) { return false; }
				}
				PC->HALStartMatch();
			}
			if (Match->GetMatchPhase() != EHALMatchPhase::Playing) { return false; }
			TActorIterator<AHALTornado> It(World);
			if (!It)
			{
				if (bSawWind)
				{
					Test.TestTrue(TEXT("Sustained travel supplied enough frames"), Frames >= 10 && TravelSeconds > .5);
					Test.TestTrue(TEXT("At least 95% of travel frames are within 20% of the configured forward speed"),
						Frames > 0 && UnevenFrames <= Frames * .05);
					Test.TestTrue(TEXT("The wind settled at the authoritative wall stop"), bCheckedStop && StopError < 1.);
					if (FApp::CanEverRender()) { Test.TestTrue(TEXT("Sampled the mounted Niagara effect"), bSawFX); }
					Test.AddInfo(FString::Printf(TEXT("Travel peers=4 caster=%d frames=%d uneven=%d range=%.2f..%.2fx "
						"configured=%.0fcm/s maxSampleGap=%.0fms stopError=%.2fcm Niagara=%d"),
						bCaster, Frames, UnevenFrames, MinRatio, MaxRatio, Speed, MaxSampleGap*1000., StopError, bSawFX));
					DoneAt = Clock;
				}
				else if (bCaster && Clock > NextPress && PC->GetPawn()->GetActorLocation().X > 19000)
				{ PC->HALUseSkill(); NextPress = Clock + .65; }
				return false;
			}
			auto* Tornado = *It;
			const auto& State = Tornado->GetState();
			if (!State.Activation.IsValid()) { return false; }
			const double ClearSeconds = FMath::Min(2.6, double(State.Rules.ActiveSeconds)*.7);
			if (!bSawWind)
			{
				if (bHost)
				{
					// Optional sparse updates exercise packet gaps without changing any production class defaults.
					float TestHz;
					if (FParse::Value(FCommandLine::Get(), TEXT("HALTornadoHz="), TestHz))
					{ Tornado->SetNetUpdateFrequency(FMath::Clamp(TestHz, 2.f, 25.f)); }
					const double Distance = State.Rules.Radius + State.Rules.Speed * ClearSeconds + 10.;
					AddTornadoTravelBox(World, State.Ground + State.Direction * Distance + FVector(0,0,300), FVector(10,400,300));
				}
				bSawWind = true;
				Speed = State.Rules.Speed;
				Test.TestEqual(TEXT("Cast ownership matches this peer"), Tornado->GetOwner() == PC->GetPawn(), bCaster);
			}
			const auto* FX = Tornado->FindComponentByClass<UNiagaraComponent>();
			const FVector Visual = FX ? FX->GetComponentLocation() : Tornado->GetActorLocation();
			bSawFX |= FX && FX->IsActive() && FX->GetAttachParent() == Tornado->GetRootComponent();
			// Compare travel per game frame, independently of the server-clock estimate used by the effect.
			const double Now = World->GetTimeSeconds();
			const double ActiveAge = Match->GetServerWorldTimeSeconds() - State.StartServerTime - State.Rules.GatherSeconds;
			if (State.bStopped)
			{
				if (StopAt < 0) { StopAt = Now; }
				if (Now - StopAt > .4)
				{ StopError = FMath::Max(StopError, FVector::Dist(Visual,State.Ground)); bCheckedStop = true; }
			}
			else if (ActiveAge > .35 && ActiveAge < ClearSeconds - .25 && Now > LastTime && LastTime >= 0 && Speed > 0)
			{
				const double Dt = Now - LastTime;
				const double Ratio = FVector::DotProduct(Visual - LastPosition, State.Direction) / (Dt * Speed);
				MinRatio = FMath::Min(MinRatio, Ratio); MaxRatio = FMath::Max(MaxRatio, Ratio);
				++Frames;
				if (FMath::Abs(Ratio - 1.) > .2) { ++UnevenFrames; }
				TravelSeconds += Dt;
				MaxSampleGap = FMath::Max(MaxSampleGap, State.SampleServerTime - LastSampleTime);
			}
			LastPosition = Visual; LastTime = Now; LastSampleTime = State.SampleServerTime;
			return false;
		}

	private:
		FAutomationTestBase& Test;
		const bool bCaster = FParse::Param(FCommandLine::Get(), TEXT("HALTornadoCaster"));
		const double Deadline = FPlatformTime::Seconds() + 100.;
		bool bLaneReady = false, bAnnouncedReady = false, bSawWind = false, bCheckedStop = false, bSawFX = false;
		int32 Frames = 0, UnevenFrames = 0;
		double DoneAt = -1, NextPress = 0, LastTime = -1, LastSampleTime = -1, StopAt = -1;
		double MinRatio = TNumericLimits<double>::Max(), MaxRatio = 0, Speed = 0, MaxSampleGap = 0, StopError = 0, TravelSeconds = 0;
		FVector LastPosition = FVector::ZeroVector;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTornadoTravelTest,"HAL.NetworkSmoke.Tornado.FourPlayerContinuousTravel",
	EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALTornadoTravelTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FTornadoTravelRun>(*this)); return true; }
#endif
