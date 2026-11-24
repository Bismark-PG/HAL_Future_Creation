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
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
	static USceneComponent* MotionRoot(const AHALArenaEffectGenerator* G) { return G->MeshMotionRoot; }
	static void MotionStage(AHALArenaEffectGenerator* G,EHALArenaGeneratorStage Stage,double Age,float Duration=1)
	{ G->State.Stage=Stage; G->State.StartedAt=G->ServerNow()-Age; G->State.Duration=Duration; G->RefreshPresentation(); }
	static void Motion(AHALArenaEffectGenerator* G) { G->UpdateMeshMotion(); }
	static void ClearMotion(AHALArenaEffectGenerator* G) { G->ClearMeshMotion(); }
	static UPrimitiveComponent* Body(const AHALArenaEffectGenerator* G) { return G->PillarBody; }
	static void CollisionStage(AHALArenaEffectGenerator* G,EHALArenaGeneratorStage Stage)
	{ G->State.Stage=Stage; G->UpdatePillarCollision(); }
	static USceneComponent* Text(const AHALArenaEffectGenerator* G) { return G->StageText; }
	static USceneComponent* Ring(const AHALArenaEffectGenerator* G) { return G->HintRing; }
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
	C={}; C.MeshSinkDepth=-1; TestFalse(TEXT("Reject negative mesh sink depth"),C.Validate(Error));
	C={}; C.MeshRotationDegrees=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Reject nonfinite mesh rotation"),C.Validate(Error));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALArenaGeneratorMeshMotionTest,"HAL.FutureCreation.ArenaGenerator.TwoMeshVisualMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALArenaGeneratorMeshMotionTest::RunTest(const FString& Parameters)
{
	FGeneratorWorld F; FHALArenaGeneratorConfig C;
	C.RevealSeconds=1; C.HintSeconds=1; C.SpawnDelaySeconds=2; C.MeshRotationDegrees=180; C.bSolidPillar=true;
	auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
	if(!TestNotNull(TEXT("Fixture geometry"),Cube)) { return false; }
	const FTransform Pose(FRotator(0,30,0),FVector(100,200,10000),FVector(3.5,2,1.5));
	TArray<UStaticMeshComponent*> Originals;
	const auto Spawn=[&](bool Enabled,int32 Count)
	{
		auto* G=F.World->SpawnActorDeferred<AHALArenaEffectGenerator>(AHALArenaEffectGenerator::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		auto Config=C; Config.bAnimateMeshes=Enabled; G->ConfigureGenerator(Config);
		for(int32 I=0;I<Count;++I)
		{
			auto* M=NewObject<UStaticMeshComponent>(G); G->AddInstanceComponent(M); M->SetupAttachment(G->GetRootComponent());
			M->SetStaticMesh(Cube); M->SetMobility(EComponentMobility::Movable);
			M->SetRelativeTransform(FTransform(FRotator(0,20*I,0),FVector(25*I,10,40+80*I),FVector(.2,.3,.4)));
			M->SetCollisionEnabled(I==0?ECollisionEnabled::QueryOnly:ECollisionEnabled::QueryAndPhysics); M->SetGenerateOverlapEvents(true);
			M->RegisterComponent(); Originals.Add(M);
		}
		G->FinishSpawning(Pose); return G;
	};
	auto* G=Spawn(true,2); auto* Root=FHALArenaGeneratorTestAccess::MotionRoot(G);
	if(!TestNotNull(TEXT("Two authored meshes create a presentation pivot"),Root)) { return false; }
	TArray<USceneComponent*> Copies; Root->GetChildrenComponents(false,Copies);
	TestEqual(TEXT("Only the two authored meshes have animated copies"),Copies.Num(),2);
	TInlineComponentArray<UStaticMeshComponent*> Owned; G->GetComponents(Owned);
	TestEqual(TEXT("Instance-template copies belong to the actor and survive ownership-based collection"),Owned.Num(),4);
	TArray<FTransform> OriginalPoses;
	for(int32 I=0;I<2;++I)
	{
		auto* Source=Originals[I]; OriginalPoses.Add(Source->GetComponentTransform());
		TestFalse(TEXT("Source render is hidden while the animated copy is shown"),Source->IsVisible());
		TestTrue(TEXT("Authored meshes are visual only even when authored with collision"),Source->GetCollisionEnabled()==ECollisionEnabled::NoCollision && !Source->GetGenerateOverlapEvents() && !Source->IsSimulatingPhysics());
		if(Copies.IsValidIndex(I))
		{
			auto* Copy=Cast<UStaticMeshComponent>(Copies[I]);
			if(!TestNotNull(TEXT("Copy remains a static mesh"),Copy)) { return false; }
			TestTrue(TEXT("Copy geometry and material match the source"),Copy->GetStaticMesh()==Source->GetStaticMesh() && Copy->GetMaterial(0)==Source->GetMaterial(0));
			TestTrue(TEXT("Copies cannot collide, simulate or replicate movement"),Copy->GetCollisionEnabled()==ECollisionEnabled::NoCollision && !Copy->IsSimulatingPhysics() && !Copy->GetIsReplicated());
			TestTrue(TEXT("Copy starts at the exact authored pose"),Copy->GetComponentTransform().Equals(OriginalPoses[I],.001));
		}
	}
	FHALArenaGeneratorTestAccess::MotionStage(G,EHALArenaGeneratorStage::Revealing,1);
	const auto RevealEnd=Root->GetComponentTransform();
	FHALArenaGeneratorTestAccess::MotionStage(G,EHALArenaGeneratorStage::Hint,0);
	TestTrue(TEXT("No pose jump from reveal to hint"),Root->GetComponentTransform().Equals(RevealEnd,.001));
	FHALArenaGeneratorTestAccess::MotionStage(G,EHALArenaGeneratorStage::Delay,0);
	TestTrue(TEXT("Halfway sinks 300 world cm despite nonuniform pillar scale"),Root->GetComponentLocation().Equals(Pose.GetLocation()-FVector(0,0,300),.001));
	TestTrue(TEXT("Both meshes turn about the common pillar axis"),Root->GetComponentQuat().Equals(FRotator(0,120,0).Quaternion(),.001));
	const auto Body=FHALArenaGeneratorTestAccess::Body(G)->GetComponentTransform();
	const auto Text=FHALArenaGeneratorTestAccess::Text(G)->GetComponentTransform();
	const auto Ring=FHALArenaGeneratorTestAccess::Ring(G)->GetComponentTransform();
	const auto Center=G->GetGeneratorState().EffectCenter;
	const auto BlocksPath=[&]()
	{
		FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		FHitResult Hit; const FVector Middle=Pose.GetLocation()+FVector(0,0,150);
		return F.World->LineTraceSingleByObjectType(Hit,Middle-FVector(1000,0,0),Middle+FVector(1000,0,0),Objects) && Hit.GetActor()==G;
	};
	TestTrue(TEXT("Raised and sinking pillar blocks the physical path"),BlocksPath());
	FHALArenaGeneratorTestAccess::MotionStage(G,EHALArenaGeneratorStage::Active,100);
	TestTrue(TEXT("Active keeps meshes fully lowered"),Root->GetComponentLocation().Equals(Pose.GetLocation()-FVector(0,0,600),.001));
	TestTrue(TEXT("Motion never relocates the actor or gameplay field"),G->GetActorTransform().Equals(Pose,.001) && G->GetGeneratorState().EffectCenter==Center);
	TestTrue(TEXT("Pillar collider and stage text remain fixed"),FHALArenaGeneratorTestAccess::Body(G)->GetComponentTransform().Equals(Body,.001) && FHALArenaGeneratorTestAccess::Text(G)->GetComponentTransform().Equals(Text,.001));
	TestTrue(TEXT("Warning position is not dragged underground"),FHALArenaGeneratorTestAccess::Ring(G)->GetComponentTransform().Equals(Ring,.001));
	for(int32 I=0;I<2;++I) { TestTrue(TEXT("Original mesh transforms do not rotate or sink"),Originals[I]->GetComponentTransform().Equals(OriginalPoses[I],.001)); }
	TestTrue(TEXT("Underground disables the box while visual meshes stay noncolliding"),FHALArenaGeneratorTestAccess::Body(G)->GetCollisionEnabled()==ECollisionEnabled::NoCollision
		&& Originals[0]->GetCollisionEnabled()==ECollisionEnabled::NoCollision && Originals[1]->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Vehicles and balls have an unobstructed path while the pillar is underground"),BlocksPath());
	FHALArenaGeneratorTestAccess::MotionStage(G,EHALArenaGeneratorStage::Finished,.5,1);
	TestTrue(TEXT("Finished smoothly raises the meshes"),Root->GetComponentLocation().Equals(Pose.GetLocation()-FVector(0,0,300),.001));
	TestFalse(TEXT("Collision stays off throughout rising"),BlocksPath());
	FHALArenaGeneratorTestAccess::MotionStage(G,EHALArenaGeneratorStage::Finished,1,1);
	TestTrue(TEXT("Collision returns only after the meshes are fully raised"),BlocksPath());
	TestTrue(TEXT("Only the box returns to blocking after rising"),FHALArenaGeneratorTestAccess::Body(G)->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics
		&& Originals[0]->GetCollisionEnabled()==ECollisionEnabled::NoCollision && Originals[1]->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
	FHALArenaGeneratorTestAccess::MotionStage(G,EHALArenaGeneratorStage::Active,0);
	FHALArenaGeneratorTestAccess::MotionStage(G,EHALArenaGeneratorStage::Finished,0,0);
	TestTrue(TEXT("Zero finish time restores immediately without dividing by zero"),Root->GetComponentTransform().Equals(Pose,.001));
	TestTrue(TEXT("Zero finish time also restores collision immediately"),BlocksPath());
	for(const auto Stage : {EHALArenaGeneratorStage::Rolling,EHALArenaGeneratorStage::Waiting,EHALArenaGeneratorStage::Disabled})
	{
		FHALArenaGeneratorTestAccess::MotionStage(G,EHALArenaGeneratorStage::Active,0);
		FHALArenaGeneratorTestAccess::MotionStage(G,Stage,0);
		TestTrue(TEXT("Next cycle or cancellation restores authored pose"),Root->GetComponentTransform().Equals(Pose,.001));
		TestTrue(TEXT("Next cycle or cancellation restores collision"),BlocksPath());
	}
	FHALArenaGeneratorTestAccess::ClearMotion(G);
	TestNull(TEXT("Presentation copies are cleaned up"),FHALArenaGeneratorTestAccess::MotionRoot(G));
	Owned.Reset(); G->GetComponents(Owned); TestEqual(TEXT("Cleanup removes only the two owned copies"),Owned.Num(),2);
	for(int32 I=0;I<2;++I) { TestTrue(TEXT("Cleanup restores source rendering"),Originals[I]->IsVisible() && !Originals[I]->bHiddenInGame && Originals[I]->CastShadow); }
	FHALArenaGeneratorTestAccess::CollisionStage(G,EHALArenaGeneratorStage::Active);
	TestFalse(TEXT("Authority collision switching does not depend on a visual pivot"),BlocksPath());
	FHALArenaGeneratorTestAccess::CollisionStage(G,EHALArenaGeneratorStage::Rolling);
	TestTrue(TEXT("Rootless authority also restores collision"),BlocksPath());
	G->Destroy(); Originals.Reset();
	auto* Disabled=Spawn(false,2);
	TestNull(TEXT("Animation can be disabled"),FHALArenaGeneratorTestAccess::MotionRoot(Disabled));
	FHALArenaGeneratorTestAccess::MotionStage(Disabled,EHALArenaGeneratorStage::Active,0);
	TestTrue(TEXT("Disabling animation keeps only the box collision"),Originals[0]->GetCollisionEnabled()==ECollisionEnabled::NoCollision && FHALArenaGeneratorTestAccess::Body(Disabled)->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics);
	TestTrue(TEXT("Disabled animation does not hide the authored meshes"),Originals[0]->IsVisible()); Disabled->Destroy(); Originals.Reset();
	auto* Extra=Spawn(true,3);
	TestNull(TEXT("Do not animate an ambiguous set of extra mesh decorations"),FHALArenaGeneratorTestAccess::MotionRoot(Extra)); Extra->Destroy(); Originals.Reset();
	C.MeshSinkDepth=0; auto* NoSink=Spawn(true,2);
	FHALArenaGeneratorTestAccess::MotionStage(NoSink,EHALArenaGeneratorStage::Active,0);
	TestTrue(TEXT("Zero-depth rotation keeps only the box collision"),Originals[0]->GetCollisionEnabled()==ECollisionEnabled::NoCollision && FHALArenaGeneratorTestAccess::Body(NoSink)->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics);
	NoSink->Destroy(); Originals.Reset(); C.MeshSinkDepth=600;
	C.bSolidPillar=false; auto* Nonblocking=Spawn(true,2);
	for(const auto Stage : {EHALArenaGeneratorStage::Rolling,EHALArenaGeneratorStage::Active,EHALArenaGeneratorStage::Finished})
	{
		FHALArenaGeneratorTestAccess::MotionStage(Nonblocking,Stage,1,1);
		TestTrue(TEXT("Solid Pillar opt-out keeps both box and meshes noncolliding"),FHALArenaGeneratorTestAccess::Body(Nonblocking)->GetCollisionEnabled()==ECollisionEnabled::NoCollision
			&& Originals[0]->GetCollisionEnabled()==ECollisionEnabled::NoCollision && Originals[1]->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
	}
	Nonblocking->Destroy(); Originals.Reset(); C.bSolidPillar=true;
	auto* Late=Spawn(true,0);
	for(int32 I=0;I<2;++I)
	{
		auto* Mesh=NewObject<UStaticMeshComponent>(Late); Late->AddInstanceComponent(Mesh); Mesh->SetupAttachment(Late->GetRootComponent());
		Mesh->SetStaticMesh(Cube); Mesh->RegisterComponent();
	}
	FHALArenaGeneratorTestAccess::MotionStage(Late,EHALArenaGeneratorStage::Delay,1);
	auto* LateRoot=FHALArenaGeneratorTestAccess::MotionRoot(Late);
	if(TestNotNull(TEXT("Meshes assembled after BeginPlay also initialize presentation"),LateRoot))
	{ TestTrue(TEXT("Late assembly immediately uses the current stage pose"),LateRoot->GetComponentLocation().Equals(Pose.GetLocation()-FVector(0,0,506.25),.001)); }
	Late->Destroy();
	return true;
}
#endif
