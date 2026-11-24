#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALArenaEffectGenerator.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace
{
	int32 GeneratorSeed(EHALArenaEffect Effect)
	{
		for(int32 Seed=1;Seed<1000;++Seed) { FRandomStream R(Seed); if(FHALArenaGeneratorConfig::Draw(R)==Effect) { return Seed; } }
		return 1;
	}
	class FGeneratorNetworkProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase& Test; double Deadline=FPlatformTime::Seconds()+100,CompleteAt=-1;
		bool bNamed=false,bSpawned=false,bReadySent=false,bDoneSent=false;
		uint32 Phases[3]={0,0,0}; bool bSawActor[3]={false,false,false},bSawWarning[3]={false,false,false};
		bool bSawSink[3]={false,false,false},bSawRise[3]={false,false,false};
		bool bSawCollisionOff[3]={false,false,false},bSawCollisionBack[3]={false,false,false};
		TWeakObjectPtr<AHALArenaEffectGenerator> Generators[3];
	public:
		explicit FGeneratorNetworkProbe(FAutomationTestBase& T):Test(T) {}
		bool Update() override
		{
			if(CompleteAt>0) { return FPlatformTime::Seconds()>=CompleteAt; }
			if(FPlatformTime::Seconds()>Deadline)
			{ Test.AddError(FString::Printf(TEXT("Generator network timeout. Spawned=%d Ready=%d Phases=%x/%x/%x Actors=%d/%d/%d Sink=%d/%d/%d Rise=%d/%d/%d"),bSpawned,bReadySent,Phases[0],Phases[1],Phases[2],bSawActor[0],bSawActor[1],bSawActor[2],bSawSink[0],bSawSink[1],bSawSink[2],bSawRise[0],bSawRise[1],bSawRise[2])); return true; }
			UWorld* World=nullptr;
			for(const auto& Context:GEngine->GetWorldContexts())
			{ auto* W=Context.World(); if(W && W->IsGameWorld() && (W->IsNetMode(NM_Client)||W->IsNetMode(NM_ListenServer))) { World=W; break; } }
			if(!World) { return false; }
			const bool bHost=World->IsNetMode(NM_ListenServer);
			auto* PC=Cast<AHALPlayerController>(World->GetFirstPlayerController());
			auto* Match=World->GetGameState<AHALMatchGameState>();
			auto* Own=PC?Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr;
			ATestVehiclePawn* Other=nullptr;
			for(TActorIterator<ATestVehiclePawn> It(World);It;++It) { if(*It!=Own && It->GetPlayerState()) { Other=*It; break; } }
			if(!PC||!Match||!Own||!Other||!Other->GetPlayerState()) { return false; }
			if(!bNamed) { PC->SetName(bHost?TEXT("GenHost"):TEXT("GenClient")); bNamed=true; }
			if(bHost && !bSpawned && Other->GetPlayerState()->GetPlayerName()==TEXT("GenClient"))
			{
				auto* Class=LoadClass<AHALArenaEffectGenerator>(nullptr,TEXT("/Game/Blueprints/MapMechanic/BP_HALArenaEffectGenerator.BP_HALArenaEffectGenerator_C"));
				if(!Test.TestNotNull(TEXT("Saved two-mesh generator BP loads without editing"),Class)) { return true; }
				FPacketSimulationSettings Lag; Lag.PktLag=80; World->GetNetDriver()->SetPacketSimulationSettings(Lag);
				for(int32 I=0;I<3;++I)
				{
					FHALArenaGeneratorConfig C; C.RandomSeed=GeneratorSeed(static_cast<EHALArenaEffect>(I+1));
					C.RollSeconds=C.RevealSeconds=C.HintSeconds=.5f; C.SpawnDelaySeconds=1.2f; C.FinishSeconds=.5f;
					C.bSolidPillar=true;
					C.EffectOffset=FVector(10,20,80); C.SlipperyAreaOffset=FVector(100,0,-20);
					C.BlackHoleOffset=FVector(0,200,30); C.ElectricFieldOffset=FVector(-150,-90,120);
					const FTransform Pose(FVector(0,(I-1)*3000,10000));
					auto* G=World->SpawnActorDeferred<AHALArenaEffectGenerator>(Class,Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
					Test.TestTrue(TEXT("Authority configures generator before spawning"),G->ConfigureGenerator(C));
					G->FinishSpawning(Pose); Generators[I]=G;
				}
				bSpawned=true;
			}
			int32 Waiting=0;
			for(TActorIterator<AHALArenaEffectGenerator> It(World);It;++It)
			{
				if(FMath::Abs(It->GetActorLocation().Z-10000)>.1) { continue; }
				const int32 I=FMath::RoundToInt(It->GetActorLocation().Y/3000)+1;
				if(I<0||I>=3) { continue; } Generators[I]=*It;
				TInlineComponentArray<UStaticMeshComponent*> Authored; It->GetComponents(Authored);
				if(Authored.IsEmpty())
				{
					// The saved Match's two meshes are instance components, not defaults of this BP class.
					// Recreate the same presentation assembly locally on both peers without editing assets.
					const TCHAR* Paths[]={TEXT("/Game/Imported/Platformer14SciFi/Prefabs/Decorations/SM_0198_PF_engine_001.SM_0198_PF_engine_001"),TEXT("/Game/Imported/Platformer14SciFi/Prefabs/Decorations/SM_0201_PF_flask_003.SM_0201_PF_flask_003")};
					for(int32 M=0;M<2;++M)
					{
						auto* Mesh=LoadObject<UStaticMesh>(nullptr,Paths[M]);
						if(!Test.TestNotNull(TEXT("Saved pillar geometry loads read-only"),Mesh)) { return true; }
						auto* C=NewObject<UStaticMeshComponent>(*It); It->AddInstanceComponent(C); C->SetupAttachment(It->GetRootComponent());
						C->SetStaticMesh(Mesh); C->SetMobility(EComponentMobility::Movable); C->SetRelativeLocation(FVector(0,0,40+40*M));
						C->SetRelativeScale3D(FVector(M==0?.03:.2)); C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); C->RegisterComponent();
					}
				}
				const auto& S=It->GetGeneratorState();
				TInlineComponentArray<USceneComponent*> Components; It->GetComponents(Components);
				USceneComponent* Motion=nullptr;
				for(auto* Component:Components) { if(Component->GetFName()==TEXT("MeshMotionRoot")) { Motion=Component; break; } }
				if(Motion)
				{
					const float Depth=It->GetActorLocation().Z-Motion->GetComponentLocation().Z;
					const double Age=FMath::Max(0.,World->GetGameState()->GetServerWorldTimeSeconds()-S.StartedAt);
					const bool bSinking=S.Stage==EHALArenaGeneratorStage::Revealing || S.Stage==EHALArenaGeneratorStage::Hint || S.Stage==EHALArenaGeneratorStage::Delay;
					const double Previous=S.Stage==EHALArenaGeneratorStage::Delay?1.:S.Stage==EHALArenaGeneratorStage::Hint?.5:0.;
					const double Progress=S.Stage==EHALArenaGeneratorStage::Finished?1.-FMath::Clamp(Age/.5,0.,1.):FMath::Clamp((Previous+Age)/2.2,0.,1.);
					if(bSinking || S.Stage==EHALArenaGeneratorStage::Finished)
					{
						const double Expected=600*Progress*Progress*(3-2*Progress);
						Test.TestTrue(TEXT("Each peer derives smooth mesh travel from the authority stage clock"),FMath::Abs(Depth-Expected)<65);
						if(Depth>60 && Depth<540) { bSawSink[I]|=bSinking; bSawRise[I]|=S.Stage==EHALArenaGeneratorStage::Finished; }
					}
					int32 Copies=0,Sources=0;
					const bool bStableBelow=S.Stage==EHALArenaGeneratorStage::Active || (S.Stage==EHALArenaGeneratorStage::Finished && Age<.4);
					const bool bStableAbove=S.Stage!=EHALArenaGeneratorStage::Active && S.Stage!=EHALArenaGeneratorStage::Finished;
					if(bStableBelow) { bSawCollisionOff[I]=true; }
					if(bStableAbove && S.Cycle>=2 && bSawCollisionOff[I]) { bSawCollisionBack[I]=true; }
					for(auto* Component:Components)
					{
						if(auto* Primitive=Cast<UPrimitiveComponent>(Component); Primitive && (bStableBelow || bStableAbove)
							&& Component->GetFName()==TEXT("PillarBody"))
						{ Test.TestTrue(TEXT("Both peers disable the underground box and restore it for the next cycle"),Primitive->GetCollisionEnabled()==(bStableBelow?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics)); }
						if(auto* Mesh=Cast<UStaticMeshComponent>(Component))
						{
							Test.TestTrue(TEXT("All original and animated meshes stay visual only on both peers"),Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision && !Mesh->GetGenerateOverlapEvents() && !Mesh->IsSimulatingPhysics());
							if(Mesh->GetAttachParent()==Motion) { ++Copies; }
							else { ++Sources; Test.TestFalse(TEXT("Authored mesh rendering stays hidden"),Mesh->IsVisible()); }
						}
					}
					if(!Test.TestTrue(FString::Printf(TEXT("Exactly two authored meshes and two visual copies on both peers (sources=%d copies=%d)"),Sources,Copies),Copies==2 && Sources==2)) { return true; }
				}
				if(S.Stage==EHALArenaGeneratorStage::Waiting) { ++Waiting; }
				if(S.Cycle<=1) { Phases[I]|=1u<<uint8(S.Stage); }
				if(S.Cycle==1 && S.Effect!=EHALArenaEffect::None)
				{
					Test.TestTrue(TEXT("Both peers see the authority-selected result"),S.Effect==static_cast<EHALArenaEffect>(I+1));
					const FVector ExpectedOffset=I==0?FVector(110,20,60):I==1?FVector(10,220,110):FVector(-140,-70,200);
					Test.TestTrue(TEXT("Per-effect XYZ warning center is authoritative on both peers"),S.EffectCenter.Equals(It->GetActorLocation()+ExpectedOffset,.1));
				}
				if(S.Cycle==1 && (S.Stage==EHALArenaGeneratorStage::Hint || S.Stage==EHALArenaGeneratorStage::Delay))
				{
					TInlineComponentArray<UDynamicMeshComponent*> Meshes; It->GetComponents(Meshes);
					for(auto* Mesh:Meshes) { bSawWarning[I]|=Mesh->GetFName()==TEXT("HintRing") && Mesh->IsVisible() && Mesh->GetDynamicMesh()->GetMeshRef().TriangleCount()==128; }
					Test.TestTrue(TEXT("Warning phase has no active effect"),!S.ActiveEffect);
					Test.TestTrue(TEXT("Synchronized warning countdown"),It->GetStageRemainingSeconds()>=0 && It->GetStageRemainingSeconds()<=1.2f);
				}
				if(S.Cycle==1 && S.Stage==EHALArenaGeneratorStage::Active && S.ActiveEffect)
				{
					FHALArenaGeneratorConfig Defaults;
					bSawActor[I]|=S.ActiveEffect->IsA(Defaults.EffectClass(static_cast<EHALArenaEffect>(I+1)))
						&& S.ActiveEffect->GetActorLocation().Equals(S.EffectCenter,.1) && S.ActiveEffect->GetOwner()==*It;
				}
				if(bHost)
				{
					int32 Count=0; for(TActorIterator<AActor> Actor(World);Actor;++Actor) { if(Actor->GetOwner()==*It && !Actor->IsActorBeingDestroyed()) { ++Count; } }
					Test.TestTrue(TEXT("Authority never overlaps its own generated effects"),Count<=1);
				}
			}
			if(!bHost && Waiting==3 && !bReadySent) { PC->SetName(TEXT("GenClientReady")); bReadySent=true; }
			if(bHost && bSpawned && Match->GetMatchPhase()!=EHALMatchPhase::Playing && Other->GetPlayerState()->GetPlayerName()==TEXT("GenClientReady")) { PC->HALStartMatch(); }
			bool bComplete=true;
			for(int32 I=0;I<3;++I) { bComplete&=Generators[I].IsValid() && Generators[I]->GetGeneratorState().Cycle>=2 && bSawActor[I] && bSawWarning[I] && bSawSink[I] && bSawRise[I] && bSawCollisionOff[I] && bSawCollisionBack[I] && (Phases[I]&0x7f)==0x7f; }
			if(!bComplete) { return false; }
			if(!bHost && !bDoneSent) { PC->SetName(TEXT("GenClientDone")); bDoneSent=true; }
			if(bHost && Other->GetPlayerState()->GetPlayerName()!=TEXT("GenClientDone")) { return false; }
			for(int32 I=0;I<3;++I)
			{
				Test.TestTrue(TEXT("Waiting, rolling, reveal, hint, delay, active and finished all replicated"),(Phases[I]&0x7f)==0x7f);
				Test.TestTrue(TEXT("Visible warning ring and actual selected effect replicated"),bSawWarning[I]&&bSawActor[I]);
				Test.TestTrue(TEXT("Both peers observed intermediate sinking and rising poses"),bSawSink[I]&&bSawRise[I]);
				Test.TestTrue(TEXT("Both peers observed collision disable and restoration"),bSawCollisionOff[I]&&bSawCollisionBack[I]);
				if(bHost) { Generators[I]->Destroy(); }
			}
			Test.AddInfo(FString::Printf(TEXT("Generator three-effect network Host=%d Phases=%x/%x/%x"),bHost,Phases[0],Phases[1],Phases[2]));
			CompleteAt=FPlatformTime::Seconds()+(bHost?6:3); return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALArenaGeneratorNetworkTest,"HAL.NetworkSmoke.ArenaGenerator.ThreeEffectLoop",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALArenaGeneratorNetworkTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FGeneratorNetworkProbe>(*this)); return true;
}
#endif
