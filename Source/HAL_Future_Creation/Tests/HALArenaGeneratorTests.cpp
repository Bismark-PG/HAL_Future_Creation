#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALArenaEffectGenerator.h"
#include "../HALBlackHole.h"
#include "../HALExpandingSlipperyArea.h"
#include "../HALElectricSphereGimmick.h"
#include "../HALMatchGameState.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include <limits>

struct FHALArenaGeneratorTestAccess
{
	static void Due(AHALArenaEffectGenerator* G)
	{ G->State.StartedAt=G->ServerNow()-G->State.Duration-.001; G->UpdateGenerator(); }
	static void Pump(AHALArenaEffectGenerator* G) { G->UpdateGenerator(); }
	static void LoseClass(AHALArenaEffectGenerator* G) { G->PendingClass=nullptr; }
	static bool Warning(const AHALArenaEffectGenerator* G) { return G->HintRing->IsVisible(); }
	static int32 BoxTriangles(const AHALArenaEffectGenerator* G) { return G->PillarVisual->GetDynamicMesh()->GetMeshRef().TriangleCount(); }
	static int32 HintTriangles(const AHALArenaEffectGenerator* G) { return G->HintRing->GetDynamicMesh()->GetMeshRef().TriangleCount(); }
};
namespace
{
	struct FGeneratorWorld
	{
		UWorld* World;
		FGeneratorWorld()
		{
			const auto Options=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->TimeSeconds=100; World->InitializeActorsForPlay(FURL(),false); World->SetBegunPlay(true);
		}
		~FGeneratorWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		AHALArenaEffectGenerator* Spawn(const FHALArenaGeneratorConfig& Config)
		{
			const FTransform Pose(FVector(0,0,10000));
			auto* G=World->SpawnActorDeferred<AHALArenaEffectGenerator>(AHALArenaEffectGenerator::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!G->ConfigureGenerator(Config)) { G->Destroy(); return nullptr; }
			G->FinishSpawning(Pose); return G;
		}
	};
	int32 SeedFor(EHALArenaEffect Effect)
	{
		for (int32 Seed=1;Seed<1000;++Seed) { FRandomStream Random(Seed); if(FHALArenaGeneratorConfig::Draw(Random)==Effect) { return Seed; } }
		return 1;
	}
	int32 OwnedEffects(UWorld* World, AActor* Owner)
	{
		int32 Count=0; for(TActorIterator<AActor> It(World);It;++It) { if(It->GetOwner()==Owner && !It->IsActorBeingDestroyed()) { ++Count; } } return Count;
	}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALArenaGeneratorConfigTest,"HAL.FutureCreation.ArenaGenerator.ConfigurationAndSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALArenaGeneratorConfigTest::RunTest(const FString& Parameters)
{
	FHALArenaGeneratorConfig C; FString Error;
	TestTrue(TEXT("Native defaults valid"),C.Validate(Error));
	TestTrue(TEXT("Initial slippery height matches selected saved Match reference"),
		(FVector(890,-1210,-190)+C.PlacementOffset(EHALArenaEffect::SlipperyArea)).Equals(FVector(890,-1210,-80)));
	TestTrue(TEXT("Initial black hole height matches saved Match reference"),
		(FVector(890,-1210,-190)+C.PlacementOffset(EHALArenaEffect::BlackHole)).Equals(FVector(890,-1210,0)));
	TestTrue(TEXT("Initial electric height matches saved Match reference"),
		(FVector(890,-1210,-190)+C.PlacementOffset(EHALArenaEffect::ElectricField)).Equals(FVector(890,-1210,0)));
	auto Legacy=C; Legacy.EffectOffset=FVector(0,0,80);
	Legacy.SlipperyAreaOffset=Legacy.BlackHoleOffset=Legacy.ElectricFieldOffset=FVector::ZeroVector;
	for(const auto Effect : {EHALArenaEffect::SlipperyArea,EHALArenaEffect::BlackHole,EHALArenaEffect::ElectricField})
	{ TestTrue(TEXT("Explicit legacy zero offsets still preserve shared placement"),Legacy.PlacementOffset(Effect).Equals(FVector(0,0,80))); }
	TestEqual(TEXT("Slippery warning reads its own radius"),C.PreviewRadius(EHALArenaEffect::SlipperyArea),GetDefault<AHALExpandingSlipperyArea>()->GetAreaConfig().MaximumRadius);
	TestEqual(TEXT("Black hole warning reads its own radius"),C.PreviewRadius(EHALArenaEffect::BlackHole),GetDefault<AHALBlackHole>()->GetBlackHoleConfig().MaximumRadius);
	TestEqual(TEXT("Electric warning reads sphere radius"),C.PreviewRadius(EHALArenaEffect::ElectricField),GetDefault<AHALElectricSphereGimmick>()->GetSphereConfig().Radius);
	C.SpawnDelaySeconds=-1; TestFalse(TEXT("Reject negative delay"),C.Validate(Error));
	C={}; C.RevealSeconds=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Reject nonfinite phase"),C.Validate(Error));
	C={}; C.BlackHoleClass=nullptr; TestFalse(TEXT("Missing effect does not silently shrink the pool"),C.Validate(Error));
	C={}; C.PillarHalfExtent.Z=0; TestFalse(TEXT("Reject empty pillar"),C.Validate(Error));
	C={}; C.SlipperyAreaOffset.X=std::numeric_limits<double>::quiet_NaN(); TestFalse(TEXT("Reject nonfinite per-effect offset"),C.Validate(Error));
	C={}; C.EffectOffset.Z=9000; C.ElectricFieldOffset.Z=2000; TestFalse(TEXT("Validate combined offsets as well as individual fields"),C.Validate(Error));
	FRandomStream A(123),B(123); TSet<EHALArenaEffect> Seen; bool bRepeat=false; EHALArenaEffect Previous=EHALArenaEffect::None;
	for(int32 I=0;I<96;++I)
	{
		const auto Choice=FHALArenaGeneratorConfig::Draw(A); TestTrue(TEXT("Authority seed reproduces the sequence"),Choice==FHALArenaGeneratorConfig::Draw(B));
		Seen.Add(Choice); bRepeat|=Previous==Choice; Previous=Choice;
	}
	TestEqual(TEXT("All three effects can be chosen"),Seen.Num(),3); TestTrue(TEXT("Consecutive repeats are allowed"),bRepeat);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALArenaGeneratorCycleTest,"HAL.FutureCreation.ArenaGenerator.AuthorityPhasesAndOwnedLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALArenaGeneratorCycleTest::RunTest(const FString& Parameters)
{
	FGeneratorWorld F;
	for(const auto Choice : {EHALArenaEffect::SlipperyArea,EHALArenaEffect::BlackHole,EHALArenaEffect::ElectricField})
	{
		FHALArenaGeneratorConfig C; C.RandomSeed=SeedFor(Choice);
		C.EffectOffset=FVector(10,20,80); C.SlipperyAreaOffset=FVector(100,0,-20);
		C.BlackHoleOffset=FVector(0,200,30); C.ElectricFieldOffset=FVector(-150,-90,120);
		auto* G=F.Spawn(C); if(!TestNotNull(TEXT("Generator spawns"),G)) { return false; }
		TestFalse(TEXT("No Actor Tick"),G->PrimaryActorTick.bCanEverTick);
		TestEqual(TEXT("Visible box geometry has twelve triangles"),FHALArenaGeneratorTestAccess::BoxTriangles(G),12);
		TestTrue(TEXT("Starts with rolling and concealed result"),G->GetGeneratorState().Stage==EHALArenaGeneratorStage::Rolling && G->GetGeneratorState().Effect==EHALArenaEffect::None);
		TestEqual(TEXT("Rolling creates no gameplay effect"),OwnedEffects(F.World,G),0);
		TestFalse(TEXT("Runtime config cannot overwrite a warning"),G->ConfigureGenerator(C));
		FHALArenaGeneratorTestAccess::Due(G);
		TestTrue(TEXT("Reveals authority's exact choice"),G->GetGeneratorState().Stage==EHALArenaGeneratorStage::Revealing && G->GetGeneratorState().Effect==Choice);
		const FVector WarnedCenter=G->GetGeneratorState().EffectCenter;
		const FVector ExpectedOffset=Choice==EHALArenaEffect::SlipperyArea?FVector(110,20,60):Choice==EHALArenaEffect::BlackHole?FVector(10,220,110):FVector(-140,-70,200);
		TestTrue(TEXT("Reveal captures the selected effect's independent XYZ offset"),WarnedCenter.Equals(G->GetActorLocation()+ExpectedOffset));
		TestEqual(TEXT("No effect during reveal"),OwnedEffects(F.World,G),0);
		FHALArenaGeneratorTestAccess::Due(G);
		TestTrue(TEXT("Hint displays a real noncolliding ring"),G->GetGeneratorState().Stage==EHALArenaGeneratorStage::Hint && FHALArenaGeneratorTestAccess::Warning(G));
		TestEqual(TEXT("Warning ring has geometry"),FHALArenaGeneratorTestAccess::HintTriangles(G),128);
		TestEqual(TEXT("Hint radius matches selected class"),G->GetGeneratorState().WarningRadius,C.PreviewRadius(Choice));
		FHALArenaGeneratorTestAccess::Due(G);
		TestTrue(TEXT("Warning remains through reaction delay"),G->GetGeneratorState().Stage==EHALArenaGeneratorStage::Delay && FHALArenaGeneratorTestAccess::Warning(G));
		FHALArenaGeneratorTestAccess::Pump(G); TestEqual(TEXT("No premature effect during delay"),OwnedEffects(F.World,G),0);
		// Moving/scaling the pillar after the warning must not move/resize the already announced field.
		G->SetActorLocation(G->GetActorLocation()+FVector(100,0,0)); G->SetActorScale3D(FVector(2));
		FHALArenaGeneratorTestAccess::Due(G);
		auto* Effect=G->GetGeneratorState().ActiveEffect.Get();
		if(!TestNotNull(TEXT("Selected native effect actually spawned"),Effect)) { return false; }
		TestTrue(TEXT("Active owns exactly one effect"),G->GetGeneratorState().Stage==EHALArenaGeneratorStage::Active && OwnedEffects(F.World,G)==1);
		TestTrue(TEXT("Preserve selected class"),Effect->IsA(C.EffectClass(Choice)));
		TestTrue(TEXT("Actual field matches warned position and unit scale"),Effect->GetActorLocation().Equals(WarnedCenter) && Effect->GetActorScale3D().Equals(FVector::OneVector));
		if(const auto* Hole=Cast<AHALBlackHole>(Effect)) { TestEqual(TEXT("Keep black hole hold duration"),Hole->GetBlackHoleConfig().DurationSeconds,GetDefault<AHALBlackHole>()->GetBlackHoleConfig().DurationSeconds); }
		if(const auto* Area=Cast<AHALExpandingSlipperyArea>(Effect)) { TestEqual(TEXT("Keep slippery handling"),Area->GetAreaConfig().Handling.LateralGripMultiplier,GetDefault<AHALExpandingSlipperyArea>()->GetAreaConfig().Handling.LateralGripMultiplier); }
		if(const auto* Sphere=Cast<AHALElectricSphereGimmick>(Effect)) { TestEqual(TEXT("Keep electric startup stage"),Sphere->GetSphereConfig().BeginSeconds,GetDefault<AHALElectricSphereGimmick>()->GetSphereConfig().BeginSeconds); }
		FHALArenaGeneratorTestAccess::Due(G); TestEqual(TEXT("No generator timeout or duplicate while effect lives"),OwnedEffects(F.World,G),1);
		Effect->Destroy(); TestTrue(TEXT("Effect EndPlay drives finished stage"),G->GetGeneratorState().Stage==EHALArenaGeneratorStage::Finished && !G->GetGeneratorState().ActiveEffect);
		FHALArenaGeneratorTestAccess::Due(G); TestTrue(TEXT("Finished returns to next rolling cycle"),G->GetGeneratorState().Stage==EHALArenaGeneratorStage::Rolling && G->GetGeneratorState().Cycle==2);
		for(int32 I=0;I<4;++I) { FHALArenaGeneratorTestAccess::Due(G); }
		auto* Last=G->GetGeneratorState().ActiveEffect.Get();
		if(!TestNotNull(TEXT("Second cycle spawns"),Last)) { return false; }
		G->Destroy(); TestTrue(TEXT("Removing pillar removes its remaining effect"),Last->IsActorBeingDestroyed());
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALArenaGeneratorGateTest,"HAL.FutureCreation.ArenaGenerator.MatchGateAndFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALArenaGeneratorGateTest::RunTest(const FString& Parameters)
{
	FGeneratorWorld F; auto* Match=F.World->SpawnActor<AHALMatchGameState>(); F.World->SetGameState(Match);
	FHALArenaGeneratorConfig C; auto* G=F.Spawn(C);
	if(!TestNotNull(TEXT("Generator"),G)) { return false; }
	TestTrue(TEXT("Loading/waiting never consumes a cycle"),G->GetGeneratorState().Stage==EHALArenaGeneratorStage::Waiting && G->GetGeneratorState().Cycle==0);
	Match->SetMatchPhase(EHALMatchPhase::Playing); FHALArenaGeneratorTestAccess::Pump(G);
	for(int32 I=0;I<4;++I) { FHALArenaGeneratorTestAccess::Due(G); }
	auto* Effect=G->GetGeneratorState().ActiveEffect.Get();
	if(!TestNotNull(TEXT("Playing starts generation"),Effect)) { return false; }
	Match->SetMatchPhase(EHALMatchPhase::Finished); FHALArenaGeneratorTestAccess::Pump(G);
	TestTrue(TEXT("Round ending clears owned effect and waits"),Effect->IsActorBeingDestroyed() && G->GetGeneratorState().Stage==EHALArenaGeneratorStage::Waiting);
	Match->SetMatchPhase(EHALMatchPhase::Playing); FHALArenaGeneratorTestAccess::Pump(G);
	for(int32 I=0;I<3;++I) { FHALArenaGeneratorTestAccess::Due(G); }
	FHALArenaGeneratorTestAccess::LoseClass(G); AddExpectedError(TEXT("lost its selected effect class"),EAutomationExpectedErrorFlags::Contains,1);
	FHALArenaGeneratorTestAccess::Due(G);
	TestTrue(TEXT("Spawn failure stops instead of spinning or substituting"),G->GetGeneratorState().Stage==EHALArenaGeneratorStage::Disabled && !G->GetGeneratorState().ActiveEffect);
	G->Destroy(); Match->SetMatchPhase(EHALMatchPhase::WaitingForPlayers); C.bWaitForMatchStart=false;
	auto* Preview=F.Spawn(C); if(!TestNotNull(TEXT("Optional preview generator"),Preview)) { return false; }
	TestTrue(TEXT("Startup wait can be explicitly disabled"),Preview->GetGeneratorState().Stage==EHALArenaGeneratorStage::Rolling);
	for(int32 I=0;I<4;++I) { FHALArenaGeneratorTestAccess::Due(Preview); }
	auto* PreviewEffect=Preview->GetGeneratorState().ActiveEffect.Get();
	if(!TestNotNull(TEXT("Preview effect"),PreviewEffect)) { return false; }
	Match->SetMatchPhase(EHALMatchPhase::Finished); FHALArenaGeneratorTestAccess::Pump(Preview);
	TestTrue(TEXT("Disabling startup wait still respects round-end cleanup"),PreviewEffect->IsActorBeingDestroyed() && Preview->GetGeneratorState().Stage==EHALArenaGeneratorStage::Waiting);
	return true;
}
namespace
{
	class FGeneratorTimerProbe : public IAutomationLatentCommand
	{
		TSharedRef<FGeneratorWorld> F; FAutomationTestBase& Test; TWeakObjectPtr<AHALArenaEffectGenerator> Generator;
		double Deadline=FPlatformTime::Seconds()+30; uint64 Last=MAX_uint64; uint32 Stages=0; bool bShortened=false;
	public:
		FGeneratorTimerProbe(TSharedRef<FGeneratorWorld> In,FAutomationTestBase& T,AHALArenaEffectGenerator* G):F(In),Test(T),Generator(G) {}
		bool Update() override
		{
			auto* G=Generator.Get(); if(!G || FPlatformTime::Seconds()>Deadline) { Test.AddError(TEXT("Generator timer loop timeout")); return true; }
			if(Last!=GFrameCounter) { Last=GFrameCounter; F->World->Tick(LEVELTICK_All,.05f); }
			const auto& S=G->GetGeneratorState(); Stages|=1u<<uint8(S.Stage);
			if(OwnedEffects(F->World,G)>1) { Test.AddError(TEXT("Generator overlaps owned effects")); return true; }
			if(S.ActiveEffect && !bShortened) { S.ActiveEffect->SetLifeSpan(.2f); bShortened=true; }
			if(S.Cycle>=2)
			{
				for(const auto Stage : {EHALArenaGeneratorStage::Rolling,EHALArenaGeneratorStage::Revealing,EHALArenaGeneratorStage::Hint,EHALArenaGeneratorStage::Delay,EHALArenaGeneratorStage::Active,EHALArenaGeneratorStage::Finished})
				{ Test.TestTrue(TEXT("Real timers observed every cycle phase"),(Stages&(1u<<uint8(Stage)))!=0); }
				G->Destroy(); return true;
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALArenaGeneratorTimerTest,"HAL.FutureCreation.ArenaGenerator.RealTimerLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALArenaGeneratorTimerTest::RunTest(const FString& Parameters)
{
	auto F=MakeShared<FGeneratorWorld>(); FHALArenaGeneratorConfig C;
	C.RollSeconds=C.RevealSeconds=C.HintSeconds=C.SpawnDelaySeconds=C.FinishSeconds=.15f;
	auto* G=F->Spawn(C); if(!TestNotNull(TEXT("Timer generator"),G)) { return false; }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FGeneratorTimerProbe>(F,*this,G)); return true;
}
#endif
