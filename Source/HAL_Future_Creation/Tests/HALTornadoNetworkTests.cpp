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
	struct FTornadoFixture
	{
		FTornadoFixture()
		{
			FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* W,const UWorld::InitializationValues&)
			{
				if(!W->IsGameWorld() || !FParse::Param(FCommandLine::Get(),TEXT("HALTornadoFixture"))) { return; }
				auto* Ability=GetMutableDefault<UHALTornadoAbility>(); auto* Config=NewObject<UHALTornadoConfig>(Ability);
				if(const auto* Saved=LoadObject<UHALTornadoConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car05_Tornado.DA_Ability_Car05_Tornado")))
				{ Config->Rules=Saved->Rules; Config->FX=Saved->FX; }
				Config->FX.MidColor=FLinearColor(.05f,.55f,.65f); Config->FX.GrayMix=.42f;
				Config->FX.DifferentialRotation=.72f; Config->FX.GlowStrength=.8f;
				Config->FX.FormationExponent=1.1f; Ability->SkillConfig=Config;
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* Actor)
				{
					if(auto* Mode=Cast<AHALMatchGameMode>(Actor))
					{ Mode->DefaultPawnClass=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_05.BP_VehiclePawn_Car_05_C")); }
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
		bool HitPlaced=false,SawUp=false,Shot=false,Disarmed=false;
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
			if(!V||!ASC||!Match||!Other||!ASC->IsHealthInitialized()) { return false; }
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
					const FTransform Transform(FRotator::ZeroRotator,S.Ground+Side*(S.Rules.Radius+100)+FVector(0,0,60));
					auto* B=W->SpawnActorDeferred<ABasicBallActor>(Class,Transform,V,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
					B->GetVisualMesh()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
					B->SetReplicates(true); B->FinishSpawning(Transform);
					// The saved probe definition may leave its first shell material empty. Give this
					// transient ball a real material before the normal control beam creates its MID.
					B->GetVisualMesh()->SetMaterial(0,UMaterial::GetDefaultMaterial(MD_Surface));
					if(Test->TestTrue(TEXT("Real defense probe launches"),B->BeginControl(V)&&B->LaunchFromControl(V,V,-Side*3500))) { ProbeBall=B; }
					Shot=true; ASC->ApplyStun(.4f,Other);
				}
			}
			for(TActorIterator<ABasicBallActor> It(W);It;++It) if(It->GetOwner()==V || It->GetOwner()==Other)
			{
				const auto B=It->GetBallRepState(); Disarmed|=B.State==EBasicBallState::Free && B.StateSequence>=3 && B.LastLauncherPlayerId==INDEX_NONE && !B.AttackSnapshot.AttackId.IsValid();
			}
			if(SawOwn&&SawOther&&Count==0 && !ASC->HasMatchingGameplayTag(HALGameplayTags::Cooldown_Tornado))
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
#endif
