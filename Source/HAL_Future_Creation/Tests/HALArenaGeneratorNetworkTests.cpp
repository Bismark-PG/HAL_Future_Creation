#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALArenaEffectGenerator.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
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
		TWeakObjectPtr<AHALArenaEffectGenerator> Generators[3];
	public:
		explicit FGeneratorNetworkProbe(FAutomationTestBase& T):Test(T) {}
		bool Update() override
		{
			if(CompleteAt>0) { return FPlatformTime::Seconds()>=CompleteAt; }
			if(FPlatformTime::Seconds()>Deadline)
			{ Test.AddError(FString::Printf(TEXT("Generator network timeout. Spawned=%d Ready=%d Phases=%x/%x/%x Actors=%d/%d/%d"),bSpawned,bReadySent,Phases[0],Phases[1],Phases[2],bSawActor[0],bSawActor[1],bSawActor[2])); return true; }
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
				FPacketSimulationSettings Lag; Lag.PktLag=80; World->GetNetDriver()->SetPacketSimulationSettings(Lag);
				for(int32 I=0;I<3;++I)
				{
					FHALArenaGeneratorConfig C; C.RandomSeed=GeneratorSeed(static_cast<EHALArenaEffect>(I+1));
					C.RollSeconds=C.RevealSeconds=C.HintSeconds=.5f; C.SpawnDelaySeconds=1.2f; C.FinishSeconds=.5f;
					C.EffectOffset=FVector(10,20,80); C.SlipperyAreaOffset=FVector(100,0,-20);
					C.BlackHoleOffset=FVector(0,200,30); C.ElectricFieldOffset=FVector(-150,-90,120);
					const FTransform Pose(FVector(0,(I-1)*3000,10000));
					auto* G=World->SpawnActorDeferred<AHALArenaEffectGenerator>(AHALArenaEffectGenerator::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
					Test.TestTrue(TEXT("Authority configures generator before spawning"),G->ConfigureGenerator(C));
					G->FinishSpawning(Pose); Generators[I]=G;
				}
				bSpawned=true;
			}
			int32 Waiting=0;
			for(TActorIterator<AHALArenaEffectGenerator> It(World);It;++It)
			{
				if(It->GetClass()!=AHALArenaEffectGenerator::StaticClass() || FMath::Abs(It->GetActorLocation().Z-10000)>.1) { continue; }
				const int32 I=FMath::RoundToInt(It->GetActorLocation().Y/3000)+1;
				if(I<0||I>=3) { continue; } Generators[I]=*It;
				const auto& S=It->GetGeneratorState();
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
			for(int32 I=0;I<3;++I) { bComplete&=Generators[I].IsValid() && Generators[I]->GetGeneratorState().Cycle>=2 && bSawActor[I] && bSawWarning[I] && (Phases[I]&0x7f)==0x7f; }
			if(!bComplete) { return false; }
			if(!bHost && !bDoneSent) { PC->SetName(TEXT("GenClientDone")); bDoneSent=true; }
			if(bHost && Other->GetPlayerState()->GetPlayerName()!=TEXT("GenClientDone")) { return false; }
			for(int32 I=0;I<3;++I)
			{
				Test.TestTrue(TEXT("Waiting, rolling, reveal, hint, delay, active and finished all replicated"),(Phases[I]&0x7f)==0x7f);
				Test.TestTrue(TEXT("Visible warning ring and actual selected effect replicated"),bSawWarning[I]&&bSawActor[I]);
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
