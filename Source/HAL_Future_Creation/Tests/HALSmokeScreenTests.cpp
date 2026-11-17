#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "HALRoomTestSupport.h"
#include "../HALSmokeScreen.h"
#include "../HALSmokeScreenAbility.h"
#include "../HALSmokeScreenView.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../VehicleHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "NiagaraSystem.h"
#include "Misc/ScopeExit.h"
#include <limits>

struct FHALSmokeTestAccess
{
	static void Pose(ATestVehiclePawn& V,bool Grounded=true)
	{
		auto* M=V.FindComponentByClass<UArcadeVehicleMovementComponent>(); M->bShouldRunPhysics.Store(true);
		M->PublishedStability.bValid=M->PublishedStability.bDrivingAllowed=true; M->PublishedStability.bGrounded=Grounded;
		M->PublishedStability.Up=M->PublishedStability.GroundNormal=FVector::UpVector;
	}
	static void Age(AHALSmokeScreen& S,double A) { S.StartTime=HALSmoke::ServerTime(S.GetWorld())-A; }
	static void Receivers(AHALSmokeScreen& S) { S.UpdateReceivers(); }
	static void Step(AHALSmokeScreen& S) { S.Update(); }
	static void Clip(AHALSmokeScreen& S) { for(auto& P:S.Puffs) { S.ClipPuff(P); } }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSmokeClockTest,"HAL.FutureCreation.SmokeScreen.IndependentPuffClockAndConfiguration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSmokeClockTest::RunTest(const FString&)
{
	auto* C=NewObject<UHALSmokeScreenConfig>(); FString Error; auto& R=C->Rules;
	TestTrue(TEXT("Confirmed defaults are valid"),C->Validate(Error));
	TestEqual(TEXT("Four seconds emission"),R.EmissionSeconds,4.f); TestEqual(TEXT("Six seconds per puff"),R.PuffSeconds,6.f);
	TestEqual(TEXT("Cooldown starts with eighteen seconds"),C->CooldownSeconds,18.f);
	TestEqual(TEXT("Initial radius"),R.Radius(0),100.f); TestEqual(TEXT("Grown radius"),R.Radius(2),350.f);
	TestEqual(TEXT("Future sample never obscures"),R.Density(-.1f),0.f);
	TestEqual(TEXT("Mature smoke is full strength"),R.Density(1),1.f);
	TestTrue(TEXT("Final second is shared gameplay/visual fade"),FMath::IsNearlyEqual(R.Density(5.5f),.5f));
	TestEqual(TEXT("Exact lifetime is clear"),R.Density(6),0.f);
	TestTrue(TEXT("Later puff remains alive when the first expires"),R.Density(6-3.9f)>0&&R.Density(6)==0);
	FHALSmokePuff P; P.WallDistances.Init(350,24);
	TestTrue(TEXT("Vehicle center inside smoke cylinder"),P.Strength(R,1,FVector(20,0,100))>.9f);
	TestEqual(TEXT("Upper floor stays clear"),P.Strength(R,1,FVector(0,0,400)),0.f);
	TestEqual(TEXT("Footprint boundary is feathered to zero"),P.Strength(R,1,FVector(350,0,100)),0.f);
	P.WallDistances[0]=40; TestEqual(TEXT("Solid boundary clips radius"),P.Strength(R,1,FVector(100,0,100)),0.f);
	R.MaximumPuffs=10; TestFalse(TEXT("Budget cannot silently truncate a valid four-second cast"),C->Validate(Error)); R.MaximumPuffs=96;
	R.MaximumRadius=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Nonfinite geometry rejected"),C->Validate(Error)); R.MaximumRadius=350;
	R.OutletOffset.X=std::numeric_limits<double>::infinity(); TestFalse(TEXT("Invalid outlet rejected"),C->Validate(Error));
	TestTrue(TEXT("Immediate local ramp is bounded"),FMath::IsNearlyEqual(HALSmoke::AdvanceObscuration(0,.8f,.1f,.2f,.5f),.4f));
	TestTrue(TEXT("Exit recovers without snapping"),FMath::IsNearlyEqual(HALSmoke::AdvanceObscuration(.8f,0,.25f,.2f,.5f),.4f));
	TestTrue(TEXT("A retuned maximum keeps the configured ramp duration"),FMath::IsNearlyEqual(HALSmoke::AdvanceObscuration(0,.4f,.1f,.2f,.5f,.4f),.2f));
	TestEqual(TEXT("Overlapping source minimum never clears the remaining veil"),HALSmoke::AdvanceObscuration(.4f,.5f,1,.2f,.5f),.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSmokeAuthorityTest,"HAL.FutureCreation.SmokeScreen.AuthorityGASWallsMultipleSourcesPauseAndCleanup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSmokeAuthorityTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
		.CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	auto* Ability=GetMutableDefault<UHALSmokeScreenAbility>(); auto* Original=Ability->SkillConfig.Get();
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); Ability->SkillConfig=Original; };
	auto* Config=NewObject<UHALSmokeScreenConfig>(Ability); Ability->SkillConfig=Config;
	auto* Match=W->SpawnActor<AHALMatchGameState>(); W->SetGameState(Match); Match->SetMatchPhase(EHALMatchPhase::Playing);
	const auto Box=[&](FVector P,FVector Extent)
	{
		auto* A=W->SpawnActor<AActor>(); auto* C=NewObject<UBoxComponent>(A); A->SetRootComponent(C); C->SetBoxExtent(Extent);
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); C->SetCollisionObjectType(ECC_WorldStatic); C->SetCollisionResponseToAllChannels(ECR_Block);
		C->SetWorldLocation(P); C->RegisterComponent(); return C;
	};
	Box(FVector(0,0,-50),FVector(10000,10000,50));
	// World Tick clamps a large delta. Advance real positive game time before backdating a puff.
	for(int32 I=0;I<12;++I) { W->Tick(LEVELTICK_All,.2f); } int32 Id=0;
	auto* CarClass=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_08.BP_VehiclePawn_Car_08_C"));
	if(!TestNotNull(TEXT("Saved Car08 Blueprint exists"),CarClass)) { return false; }
	const auto Car=[&]()
	{
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* V=W->SpawnActor<ATestVehiclePawn>(CarClass,FVector(0,++Id*1200,60),FRotator::ZeroRotator,P);
		auto* D=FHALVehicleAbilityTestAccess::Configure(V,UHALSmokeScreenAbility::StaticClass());
		FHALVehicleAbilityTestAccess::RuntimeConfig(V,D->Abilities); V->PreInitializeComponents(); V->InitializeComponents(); V->PostInitializeComponents();
		auto* PC=W->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController(); auto* PS=W->SpawnActor<AHALPlayerState>();
		PS->SetOwner(PC); PC->SetPlayerState(PS); PS->SetPlayerId(Id); PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay();
		PS->ChangeVehicleAvatar(V,1); V->DispatchBeginPlay(); FHALSmokeTestAccess::Pose(*V); return V;
	};
	auto* A=Car(); auto* B=Car(); auto* Other=Car();
	auto* ASC=CastChecked<UHALAbilitySystemComponent>(A->GetAbilitySystemComponent()); auto* BSC=CastChecked<UHALAbilitySystemComponent>(B->GetAbilitySystemComponent());
	TestTrue(TEXT("Skill uses normal GAS Primary input"),ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	AHALSmokeScreen* Smoke=nullptr; for(TActorIterator<AHALSmokeScreen> It(W);It;++It) { Smoke=*It; }
	if(!TestNotNull(TEXT("Authority created smoke field"),Smoke)) { return false; }
	TestFalse(TEXT("Duplicate cast rejected"),ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	TestTrue(TEXT("Cooldown Tag exists from activation"),ASC->HasMatchingGameplayTag(HALGameplayTags::Cooldown_SmokeScreen));
	FHALSmokeTestAccess::Age(*Smoke,1); const FVector Ground=Smoke->GetPuffs()[0].Ground;
	B->SetActorLocation(Ground+FVector(90,0,60)); const float HP=B->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP();
	AddInfo(FString::Printf(TEXT("Smoke fixture: now=%.2f start=%.2f ground=%s target=%s actor=%s influence=%.3f"),HALSmoke::ServerTime(W),Smoke->GetStartTime(),*Ground.ToString(),*HALSmoke::TargetPosition(*B).ToString(),*B->GetActorLocation().ToString(),Smoke->Influence(*B,true,HALSmoke::ServerTime(W))));
	FHALSmokeTestAccess::Receivers(*Smoke);
	TestTrue(TEXT("Other vehicle receives authority vision GE"),BSC->HasMatchingGameplayTag(HALGameplayTags::State_Vision_SmokeObscured));
	TestFalse(TEXT("Caster has no own vision GE"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Vision_SmokeObscured));
	TestEqual(TEXT("Caster does not receive own screen veil"),Smoke->Influence(*A,true,HALSmoke::ServerTime(W)),0.f);
	TestTrue(TEXT("Target world label policy hides in smoke"),UHALSmokeScreenView::ShouldHideWorldTarget(B));
	TestEqual(TEXT("Smoke never damages HP"),B->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(),HP);
	TestFalse(TEXT("Smoke is not Stun"),BSC->IsStunned());
	auto* Second=W->SpawnActor<AHALSmokeScreen>(); Second->SetOwner(Other); TestTrue(TEXT("Independent second source"),Second->Start(*Config,Ground));
	FHALSmokeTestAccess::Age(*Second,1); FHALSmokeTestAccess::Receivers(*Second);
	Smoke->Destroy(); TestTrue(TEXT("Leaving one source preserves another source's Tag"),BSC->HasMatchingGameplayTag(HALGameplayTags::State_Vision_SmokeObscured));
	auto* Wall=Box(Ground+FVector(45,0,150),FVector(8,500,150)); FHALSmokeTestAccess::Clip(*Second); FHALSmokeTestAccess::Receivers(*Second);
	TestFalse(TEXT("Wall blocks smoke GE"),BSC->HasMatchingGameplayTag(HALGameplayTags::State_Vision_SmokeObscured));
	TestFalse(TEXT("Wall also blocks concealment, not only damage-side state"),HALSmoke::IsConcealed(B));
	Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision); FHALSmokeTestAccess::Clip(*Second); FHALSmokeTestAccess::Receivers(*Second);
	TestTrue(TEXT("Removed wall restores correct overlap"),BSC->HasMatchingGameplayTag(HALGameplayTags::State_Vision_SmokeObscured));
	auto* Ceiling=Box(Ground+FVector(0,0,145),FVector(300,300,10)); B->SetActorLocation(Ground+FVector(90,0,200));
	FHALSmokeTestAccess::Receivers(*Second); TestFalse(TEXT("An intervening floor blocks a target within the nominal smoke height"),BSC->HasMatchingGameplayTag(HALGameplayTags::State_Vision_SmokeObscured));
	Ceiling->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	B->SetActorLocation(Ground+FVector(0,0,700)); FHALSmokeTestAccess::Receivers(*Second);
	TestFalse(TEXT("Smoke does not cross unrelated height levels"),BSC->HasMatchingGameplayTag(HALGameplayTags::State_Vision_SmokeObscured));
	B->SetActorLocation(Ground+FVector(-600,0,60)); const FVector Before=HALSmoke::TargetPosition(*B); B->SetActorLocation(Ground+FVector(600,0,60));
	TestTrue(TEXT("High-speed crossing is sampled along the shared footprint"),Second->Influence(*B,true,HALSmoke::ServerTime(W),&Before)>.5f);
	FHALSmokeTestAccess::Pose(*Other,false); FHALSmokeTestAccess::Step(*Second); TestFalse(TEXT("Airborne caster pauses jet"),Second->IsEmitting());
	TestTrue(TEXT("Old smoke survives a pause"),!Second->GetPuffs().IsEmpty());
	FHALSmokeTestAccess::Pose(*Other); auto* OSC=CastChecked<UHALAbilitySystemComponent>(Other->GetAbilitySystemComponent());
	OSC->AddLooseGameplayTag(HALGameplayTags::State_Stun); FHALSmokeTestAccess::Step(*Second); TestFalse(TEXT("Stun pauses jet"),Second->IsEmitting());
	OSC->RemoveLooseGameplayTag(HALGameplayTags::State_Stun); FHALSmokeTestAccess::Step(*Second); TestTrue(TEXT("Recovery resumes within original emission window"),Second->IsEmitting());
	B->SetActorLocation(Ground+FVector(90,0,60)); FHALSmokeTestAccess::Receivers(*Second);
	OSC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),0); FHALSmokeTestAccess::Step(*Second);
	TestTrue(TEXT("Eliminated caster clears smoke"),Second->IsActorBeingDestroyed());
	TestFalse(TEXT("Retiring final source removes vision GE"),BSC->HasMatchingGameplayTag(HALGameplayTags::State_Vision_SmokeObscured));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSmokeAssetTest,"HAL.FutureCreation.SmokeScreen.SavedRenderingContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSmokeAssetTest::RunTest(const FString&)
{
	const auto* C=LoadObject<UHALSmokeScreenConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car08_SmokeScreen"));
	if(!TestNotNull(TEXT("Authored Car08 smoke config"),C)) { return false; } FString Error;
	TestTrue(TEXT("Saved configuration passes runtime validation"),C->Validate(Error));
	if(!TestTrue(TEXT("All three resources referenced for Cook"),C->FX.JetSystem&&C->FX.CloudMaterial&&C->FX.ScreenMaterial)) { return false; }
	TestEqual(TEXT("World coverage uses explicit scene-above/HUD-below composition"),C->FX.CloudMaterial->GetMaterial()->MaterialDomain,MD_UI);
	TestEqual(TEXT("Local veil shares the UI-safe layer"),C->FX.ScreenMaterial->GetMaterial()->MaterialDomain,MD_UI);
	const auto& Params=C->FX.JetSystem->GetExposedParameters();
	TestTrue(TEXT("Jet has controllable emission"),Params.FindParameterOffset(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SpawnRate")))!=nullptr);
	TestTrue(TEXT("Jet supports world-space upward velocity"),Params.FindParameterOffset(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.VelocityMin")))!=nullptr);
	TestTrue(TEXT("No camera-distance field or physical forces in smoke config"),!FindFProperty<FProperty>(C->GetClass(),TEXT("CameraDistance"))&&!FindFProperty<FProperty>(C->GetClass(),TEXT("Impulse")));
	return true;
}
#endif
