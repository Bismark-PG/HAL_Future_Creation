#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALExpandingHealingArea.h"
#include "../HALExpandingSlipperyArea.h"
#include "../HALHealingAreaReceiver.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALPlayerState.h"
#include "../HALPlayerController.h"
#include "../HALDebugSettings.h"
#include "../HALGameplayTags.h"
#include "../TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/HUD.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
	struct FHealingWorld
	{
		UWorld* World;
		AHALPlayerState* State;
		UHALAbilitySystemComponent* ASC;
		AActor* Avatar;
		UHALHealingAreaReceiver* Receiver;
		uint64 LastFrame = MAX_uint64;
		FHealingWorld()
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL(), false); World->SetBegunPlay(true);
			State = World->SpawnActor<AHALPlayerState>(); ASC = State->GetHALAbilitySystemComponent();
			Avatar = World->SpawnActor<AActor>();
			Receiver = AddReceiver(Avatar); ASC->InitAbilityActorInfo(State, Avatar); ASC->InitializeHealth(100.f); Receiver->Bind(ASC);
		}
		~FHealingWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		UHALHealingAreaReceiver* AddReceiver(AActor* Owner)
		{
			auto* R = NewObject<UHALHealingAreaReceiver>(Owner); Owner->AddInstanceComponent(R); R->RegisterComponent(); return R;
		}
		float HP() const { return ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute()); }
		void SetHP(float Value) { ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(), Value); }
		int32 EffectCount() const { return ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FGameplayTagContainer(HALGameplayTags::Effect_Healing_Area))).Num(); }
	};
	class FAdvanceHealingWorld : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test; TSharedRef<FHealingWorld> F; float Seconds;
		TOptional<double> Until; double Deadline = 0; TFunction<void()> Check;
	public:
		FAdvanceHealingWorld(FAutomationTestBase* InTest, TSharedRef<FHealingWorld> InF, float InSeconds, TFunction<void()> InCheck)
			: Test(InTest), F(InF), Seconds(InSeconds), Check(MoveTemp(InCheck)) {}
		bool Update() override
		{
			if (!Until.IsSet()) { Until = F->World->GetTimeSeconds() + Seconds; Deadline = FPlatformTime::Seconds() + 30; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Healing World/GAS timer timeout")); return true; }
			if (F->World->GetTimeSeconds() + .001 >= Until.GetValue()) { Check(); return true; }
			if (F->LastFrame != GFrameCounter) { F->LastFrame = GFrameCounter; F->World->Tick(LEVELTICK_All, .05f); }
			return false;
		}
	};
	void After(FAutomationTestBase* Test, TSharedRef<FHealingWorld> F, float Seconds, TFunction<void()> Check)
	{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceHealingWorld>(Test, F, Seconds, MoveTemp(Check))); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHealingAreaConfigTest, "HAL.FutureCreation.HealingAreas.ConfigurationAndSharedGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALHealingAreaConfigTest::RunTest(const FString& Parameters)
{
	FHALExpandingHealingAreaConfig C; FHALExpandingSlipperyAreaConfig Slippery;
	TestTrue(TEXT("Native configuration is valid"), C.IsValid());
	TestEqual(TEXT("Default heal per completed second"), C.Healing.HealPerTick, 5.f);
	TestEqual(TEXT("Default period"), C.Healing.PeriodSeconds, 1.f);
	for (double Age : {0., 1., 2., 6.}) { TestEqual(TEXT("Same expansion geometry as slippery"), C.RadiusAt(Age), Slippery.RadiusAt(Age)); }
	TestFalse(TEXT("Hold begins after expansion"), C.IsExpired(5)); TestTrue(TEXT("Total lifetime"), C.IsExpired(7));
	C.ExpansionSeconds = 0; TestEqual(TEXT("Zero expansion is immediate"), C.RadiusAt(0), 500.f);
	TestTrue(TEXT("XY and height boundary included"), HALExpandingArea::Contains(FVector::ZeroVector, FVector(300,400,200),500,200));
	TestFalse(TEXT("Height above volume excluded"), HALExpandingArea::Contains(FVector::ZeroVector,FVector(0,0,201),500,200));
	TestFalse(TEXT("XY outside excluded"), HALExpandingArea::Contains(FVector::ZeroVector,FVector(501,0,0),500,200));
	C.Healing.PeriodSeconds=0; TestFalse(TEXT("Zero period rejected"),C.IsValid());
	C={}; C.Healing.HealPerTick=-1; TestFalse(TEXT("Negative healing rejected"),C.IsValid());
	C={}; C.Healing.HealPerTick=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Nonfinite healing rejected"),C.IsValid());
	C={}; C.HalfHeight=0; TestFalse(TEXT("Invalid height rejected"),C.IsValid());
	C={}; C.ExpansionSeconds=61; TestFalse(TEXT("Out-of-range expansion rejected"),C.IsValid());
	TestEqual(TEXT("HoT is infinite"),GetDefault<UHALHealingAreaEffect>()->DurationPolicy,EGameplayEffectDurationType::Infinite);
	TestFalse(TEXT("No entry heal"),GetDefault<UHALHealingAreaEffect>()->bExecutePeriodicEffectOnApplication);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHealingAreaPeriodicTest,"HAL.FutureCreation.HealingAreas.PeriodOverlapExitReentryAndCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALHealingAreaPeriodicTest::RunTest(const FString& Parameters)
{
	auto F=MakeShared<FHealingWorld>(); F->SetHP(50);
	auto* A=F->World->SpawnActor<AActor>(); auto* B=F->World->SpawnActor<AActor>();
	TestTrue(TEXT("First source accepted"),F->Receiver->RegisterArea(A,{}));
	TestEqual(TEXT("Entry never heals instantly"),F->HP(),50.f);
	After(this,F,.6f,[this,F,A,B]
	{
		TestEqual(TEXT("No pulse before period"),F->HP(),50.f);
		F->Receiver->RegisterArea(B,{}); F->Receiver->UnregisterArea(A);
		for(int32 I=0;I<10;++I) { F->Receiver->RegisterArea(B,{}); }
		TestEqual(TEXT("Overlap/source refresh maintains only one GE"),F->EffectCount(),1);
	});
	After(this,F,.55f,[this,F,B]
	{
		TestEqual(TEXT("Same-recipe handoff preserves original pulse time"),F->HP(),55.f);
		F->Receiver->UnregisterArea(B); TestEqual(TEXT("Final exit removes actual GE"),F->EffectCount(),0);
	});
	After(this,F,1.2f,[this,F,A]
	{
		TestEqual(TEXT("Leaving retains earned HP but ends periodic pulses"),F->HP(),55.f);
		F->Receiver->RegisterArea(A,{}); F->Receiver->UnregisterArea(A); F->Receiver->RegisterArea(A,{});
		TestEqual(TEXT("Boundary reentry grants no immediate healing"),F->HP(),55.f);
		F->SetHP(98.f);
	});
	After(this,F,1.2f,[this,F,A]
	{
		TestEqual(TEXT("Real periodic GE respects existing MaxHP clamp"),F->HP(),100.f);
		F->SetHP(90.f);
		A->Destroy(); TestEqual(TEXT("Source destruction removes GE synchronously"),F->EffectCount(),0);
	});
	After(this,F,1.2f,[this,F] { TestEqual(TEXT("Destroyed source has no later pulses"),F->HP(),90.f); });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHealingAreaSourcesTest,"HAL.FutureCreation.HealingAreas.StrongestRecipeSelectiveCleanupAndAvatar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALHealingAreaSourcesTest::RunTest(const FString& Parameters)
{
	FHealingWorld F; F.SetHP(50);
	auto* A=F.World->SpawnActor<AActor>(); auto* B=F.World->SpawnActor<AActor>();
	FHALHealingAreaRecipe Strong; Strong.HealPerTick=8;
	F.Receiver->RegisterArea(A,{}); F.Receiver->RegisterArea(B,Strong);
	TestEqual(TEXT("Different-strength overlap still one GE"),F.EffectCount(),1);
	const auto Handles=F.ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FGameplayTagContainer(HALGameplayTags::Effect_Healing_Area)));
	if (!TestEqual(TEXT("One query handle"),Handles.Num(),1)) { return false; }
	TestEqual(TEXT("Highest HP/second selected"),F.ASC->GetActiveGameplayEffect(Handles[0])->Spec.GetSetByCallerMagnitude(HALGameplayTags::Data_Healing_AreaAmount),8.f);
	FHALHealingAreaRecipe Tie; Tie.HealPerTick=4; Tie.PeriodSeconds=.5f;
	F.Receiver->RegisterArea(A,Tie);
	const auto TieHandles=F.ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FGameplayTagContainer(HALGameplayTags::Effect_Healing_Area)));
	TestEqual(TEXT("Equal rate selects shorter period deterministically"),F.ASC->GetActiveGameplayEffect(TieHandles[0])->Spec.Period,.5f);
	auto Independent=F.ASC->MakeOutgoingSpec(UHALHealingAreaEffect::StaticClass(),1,F.ASC->MakeEffectContext());
	Independent.Data->SetSetByCallerMagnitude(HALGameplayTags::Data_Healing_AreaAmount,2.f);
	const auto IndependentHandle=F.ASC->ApplyGameplayEffectSpecToSelf(*Independent.Data);
	F.Receiver->UnregisterArea(A); F.Receiver->UnregisterArea(B);
	TestNotNull(TEXT("Exit never removes independent skill GE even of same class"),F.ASC->GetActiveGameplayEffect(IndependentHandle));
	F.ASC->RemoveActiveGameplayEffect(IndependentHandle);
	F.Receiver->RegisterArea(A,{}); F.Receiver->Unbind(); TestEqual(TEXT("Unbind removes old effect"),F.EffectCount(),0);
	F.Receiver->Bind(F.ASC); TestEqual(TEXT("Same Avatar rebind restores live sources"),F.EffectCount(),1);
	auto* NewAvatar=F.World->SpawnActor<AActor>(); auto* NewReceiver=F.AddReceiver(NewAvatar);
	F.ASC->InitAbilityActorInfo(F.State,NewAvatar);
	TestEqual(TEXT("Avatar-scope cleanup removes former Avatar effect"),F.EffectCount(),0);
	NewReceiver->Bind(F.ASC); NewReceiver->RegisterArea(B,{});
	F.Receiver->ClearForOwnerExit(); TestEqual(TEXT("Delayed old cleanup preserves new Avatar effect"),F.EffectCount(),1);
	NewAvatar->Destroy(); TestEqual(TEXT("Owner destruction clears effect"),F.EffectCount(),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHealingAreaDeathTest,"HAL.FutureCreation.HealingAreas.DeathPreventsRevival",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALHealingAreaDeathTest::RunTest(const FString& Parameters)
{
	auto F=MakeShared<FHealingWorld>(); auto* A=F->World->SpawnActor<AActor>();
	F->SetHP(10); F->Receiver->RegisterArea(A,{});
	After(this,F,.9f,[this,F,A]
	{
		F->SetHP(0); TestEqual(TEXT("Death synchronously clears sources"),F->Receiver->GetAreaCount(),0);
		TestEqual(TEXT("Death removes real GE before its first period"),F->EffectCount(),0);
		TestFalse(TEXT("Dead target cannot be re-registered by area sample"),F->Receiver->RegisterArea(A,{}));
	});
	After(this,F,1.2f,[this,F] { TestEqual(TEXT("Queued period cannot revive"),F->HP(),0.f); });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHealingAreaLifecycleTest,"HAL.FutureCreation.HealingAreas.ExpandingVehicleLifecycleAndDebugCommand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALHealingAreaLifecycleTest::RunTest(const FString& Parameters)
{
	FHealingWorld F;
	auto* Class=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
	if(!TestNotNull(TEXT("Saved vehicle loads read-only"),Class)) { return false; }
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* V=F.World->SpawnActor<ATestVehiclePawn>(Class,FVector(0,0,10000),FRotator::ZeroRotator,Params);
	CastChecked<UPrimitiveComponent>(V->GetRootComponent())->SetSimulatePhysics(false);
	auto* PC=F.World->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
	PC->SetPlayerState(F.State); F.State->SetOwner(PC); PC->Possess(V);
	auto* Receiver=V->FindComponentByClass<UHALHealingAreaReceiver>();
	if(!TestNotNull(TEXT("Native receiver inherited by saved vehicle BP"),Receiver)) { return false; }
	Receiver->Bind(F.ASC); F.SetHP(50);
	const FTransform Pose(V->GetActorLocation());
	auto* Area=F.World->SpawnActorDeferred<AHALExpandingHealingArea>(AHALExpandingHealingArea::StaticClass(),Pose,V,V,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	TestTrue(TEXT("Configure before FinishSpawning"),Area->ConfigureArea({})); Area->FinishSpawning(Pose);
	TestFalse(TEXT("Configure cannot overwrite running authority rules"),Area->ConfigureArea({}));
	TestTrue(TEXT("Area replicates"),Area->GetIsReplicated());
	TestEqual(TEXT("Creator qualifies immediately in seed"),Receiver->GetAreaCount(),1);
	TestEqual(TEXT("Initial registration does not heal"),F.HP(),50.f);
	V->SetActorLocation(FVector(250,0,10000)); Area->UpdateArea();
	TestEqual(TEXT("Outside current seed exits"),Receiver->GetAreaCount(),0);
	Area->StartedAt-=1.1; Area->UpdateArea();
	TestEqual(TEXT("Expansion reaches stationary vehicle before hold"),Receiver->GetAreaCount(),1);
	V->SetActorLocation(FVector(250,0,10201)); Area->UpdateArea();
	TestEqual(TEXT("Height gate excludes airborne vehicle"),Receiver->GetAreaCount(),0);
	V->SetActorLocation(FVector(250,0,10000)); Area->UpdateArea();
	Area->StartedAt-=10; Area->UpdateArea();
	TestTrue(TEXT("Expiry destroys the area"),Area->IsActorBeingDestroyed());
	TestEqual(TEXT("Expiry removes sources"),Receiver->GetAreaCount(),0);
#if !UE_BUILD_SHIPPING
	auto* Debug=GetMutableDefault<UHALDebugSettings>();
	const auto Previous=Debug->DefaultHealingAreaClass; const float PreviousDistance=Debug->HealingAreaForwardDistance;
	Debug->DefaultHealingAreaClass=AHALExpandingHealingArea::StaticClass(); Debug->HealingAreaForwardDistance=350;
	PC->HALSpawnHealingArea(); AHALExpandingHealingArea* Spawned=nullptr; int32 Count=0;
	for(TActorIterator<AHALExpandingHealingArea> It(F.World);It;++It) { if(!It->IsActorBeingDestroyed()) { Spawned=*It; ++Count; } }
	TestEqual(TEXT("One debug request creates one live area"),Count,1);
	if(Spawned) { TestTrue(TEXT("Authority chooses position using vehicle root/heading"),Spawned->GetActorLocation().Equals(V->GetActorLocation()+V->GetActorForwardVector().GetSafeNormal2D()*350,.01)); }
	Debug->DefaultHealingAreaClass.Reset(); PC->HALSpawnHealingArea();
	Count=0; for(TActorIterator<AHALExpandingHealingArea> It(F.World);It;++It) { if(!It->IsActorBeingDestroyed()) { ++Count; } }
	TestEqual(TEXT("Explicitly empty debug class has no fallback spawn"),Count,1);
	Debug->DefaultHealingAreaClass=Previous; Debug->HealingAreaForwardDistance=PreviousDistance;
#endif
	return true;
}

#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHealingAreaDebugTextTest,"HAL.FutureCreation.HealingAreas.DebugTextUsesActualAreaHealOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALHealingAreaDebugTextTest::RunTest(const FString& Parameters)
{
	auto F=MakeShared<FHealingWorld>(); F->SetHP(50);
	auto* PC=F->World->SpawnActor<APlayerController>(); PC->SetAsLocalPlayerController();
	PC->Player=NewObject<ULocalPlayer>(GEngine);
	auto* HUD=F->World->SpawnActor<AHUD>(); HUD->SetOwner(PC); PC->MyHUD=HUD;
	const auto* TextProperty=FindFProperty<FArrayProperty>(AHUD::StaticClass(),TEXT("DebugTextList"));
	if(!TestNotNull(TEXT("Engine HUD exposes its real debug text list"),TextProperty)) { return false; }
	auto* Texts=TextProperty->ContainerPtrToValuePtr<TArray<FDebugTextInfo>>(HUD);
	const auto Spawn=[&]()
	{
		FHALExpandingHealingAreaConfig C; C.ExpansionSeconds=0; C.DurationSeconds=120;
		const FTransform Pose(FVector::ZeroVector);
		auto* Area=F->World->SpawnActorDeferred<AHALExpandingHealingArea>(AHALExpandingHealingArea::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Area->ConfigureArea(C); Area->FinishSpawning(Pose); F->Receiver->RegisterArea(Area,C.Healing); return Area;
	};
	auto* A=Spawn(); auto* B=Spawn();
	After(this,F,1.2f,[this,F,Texts,A,B]
	{
		TestEqual(TEXT("Healing still occurs with debug disabled"),F->HP(),55.f);
		TestEqual(TEXT("Debug off creates no HUD text"),Texts->Num(),0);
		A->bDebugDraw=true; B->bDebugDraw=true;
	});
	After(this,F,1.2f,[this,F,Texts]
	{
		TestEqual(TEXT("Normal pulse adds five actual HP"),F->HP(),60.f);
		if(TestEqual(TEXT("Two overlapping debug areas emit only one text"),Texts->Num(),1))
		{
			TestTrue(TEXT("Text records the real HP result"),(*Texts)[0].DebugText==TEXT("+5.00 HP (60.00 / 100.00)"));
			TestTrue(TEXT("Text is relative to the healed Avatar above its center"),(*Texts)[0].SrcActor==F->Avatar && (*Texts)[0].SrcActorOffset.Z>0);
			TestEqual(TEXT("Text has a finite short lifetime"),(*Texts)[0].Duration,.8f);
		}
		F->SetHP(98); TestEqual(TEXT("Base HP edits do not create fake heal pulses"),Texts->Num(),1);
	});
	After(this,F,1.2f,[this,F,Texts]
	{
		TestEqual(TEXT("Near-cap pulse restores only two"),F->HP(),100.f);
		if(TestEqual(TEXT("Partial heal creates one new text"),Texts->Num(),2))
		{ TestTrue(TEXT("Text uses actual two, not configured five"),(*Texts)[1].DebugText==TEXT("+2.00 HP (100.00 / 100.00)")); }
	});
	After(this,F,1.2f,[this,F,Texts,A,B]
	{
		TestEqual(TEXT("Full HP does not emit zero-healing text"),Texts->Num(),2);
		F->SetHP(90);
		auto* SkillHeal=NewObject<UGameplayEffect>(); SkillHeal->DurationPolicy=EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Modifier; Modifier.Attribute=UHALHealthAttributeSet::GetCurrentHPAttribute();
		Modifier.ModifierOp=EGameplayModOp::Additive; Modifier.ModifierMagnitude=FScalableFloat(3.f); SkillHeal->Modifiers.Add(Modifier);
		F->ASC->ApplyGameplayEffectToSelf(SkillHeal,1,F->ASC->MakeEffectContext());
		TestEqual(TEXT("Independent skill really heals"),F->HP(),93.f);
		TestEqual(TEXT("Independent healing cannot masquerade as this area's pulse"),Texts->Num(),2);
		A->bDebugDraw=false; B->bDebugDraw=false;
	});
	After(this,F,1.2f,[this,F,Texts]
	{
		TestTrue(TEXT("Healing continues after turning debug off"),F->HP()>93.f);
		TestEqual(TEXT("Turning debug off suppresses later text"),Texts->Num(),2);
	});
	return true;
}
#endif
#endif
