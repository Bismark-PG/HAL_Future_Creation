#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HALVehicleStabilityTestRules.h"
#include "../HALBombActor.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALImpactAttributeSet.h"
#include "../HALCombatAttributeSet.h"
#include "../PassiveTestVehicle.h"
#include "../VehicleDefinition.h"
#include "../VehicleHealthComponent.h"
#include "../VehicleKnockbackSettings.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
	struct FBombWorld
	{
		FScopedHALStabilityTestRules Stability{false};
		UWorld* World;
		FBombWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
		~FBombWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		APassiveTestVehicle* Target(FVector Location = FVector(100,0,100))
		{
			auto* Actor = World->SpawnActor<APassiveTestVehicle>();
			auto* Def = NewObject<UVehicleDefinition>();
			Def->bReadyForUse = true; Def->CollisionShape = EVehicleCollisionShape::Box;
			Def->Visual.Mesh = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
			Def->Combat.Version = EHALCombatConfigVersion::AttackSnapshotV1;
			Def->Impact.Version = EHALImpactVersion::TieredV1;
			auto* Source = FindFProperty<FEnumProperty>(Actor->GetClass(),TEXT("ConfigurationSource"));
			Source->GetUnderlyingProperty()->SetIntPropertyValue(Source->ContainerPtrToValuePtr<void>(Actor),uint64(EVehicleConfigurationSource::Definition));
			FindFProperty<FObjectProperty>(Actor->GetClass(),TEXT("Definition"))->SetObjectPropertyValue_InContainer(Actor,Def);
			Actor->PreInitializeComponents(); Actor->DispatchBeginPlay(); Actor->SetActorLocation(Location);
			return Actor;
		}
		AHALBombExplosion* Explosion(FHALBombConfig Config = FHALBombConfig(), FHALAttackSourceSnapshot Attribution = FHALAttackSourceSnapshot())
		{
			const FTransform Transform(FVector(0,0,100));
			auto* Actor = World->SpawnActorDeferred<AHALBombExplosion>(AHALBombExplosion::StaticClass(),Transform);
			Actor->Initialize(Config,Attribution); Actor->FinishSpawning(Transform);
			return Actor;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBombGroundTest,"HAL.FutureCreation.Bomb.GroundArmingAndDestruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBombGroundTest::RunTest(const FString&)
{
	FBombWorld F;
	auto* Bomb = F.World->SpawnActor<AHALBombActor>(FVector(0,0,1000),FRotator::ZeroRotator);
	Bomb->DispatchBeginPlay();
	auto* Ground = F.World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Ground); Ground->SetRootComponent(Box); Box->RegisterComponent();
	FHitResult Hit(Ground,Box,FVector::ZeroVector,FVector::RightVector); Hit.bBlockingHit = true;
	TestFalse(TEXT("Airborne starts unarmed"),Bomb->IsArmed());
	TestFalse(TEXT("Wall contact never arms"),Bomb->TryArmFromGround(Hit));
	Hit.ImpactNormal = FVector::UpVector;
	TestTrue(TEXT("First valid ground arms"),Bomb->TryArmFromGround(Hit));
	const float End = Bomb->GetFuseEndServerTime();
	TestFalse(TEXT("Repeated bounce cannot rearm"),Bomb->TryArmFromGround(Hit));
	TestEqual(TEXT("Bounce preserves end time"),Bomb->GetFuseEndServerTime(),End);
	Bomb->Detonate();
	TestTrue(TEXT("Explosion destroys physical prefab"),Bomb->IsActorBeingDestroyed());
	int32 Count = 0; for (TActorIterator<AHALBombExplosion> It(F.World);It;++It) { ++Count; }
	TestEqual(TEXT("Exactly one explosion range"),Count,1);
	Bomb->Detonate();
	Count=0; for (TActorIterator<AHALBombExplosion> It(F.World);It;++It) { ++Count; }
	TestEqual(TEXT("Repeated detonation cannot spawn another pulse"),Count,1);
	FHALBombConfig Bad; Bad.Damage=std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite damage invalid"),Bad.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBombCombatTest,"HAL.FutureCreation.Bomb.DamageImpactImmunityAndDedup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBombCombatTest::RunTest(const FString&)
{
	FBombWorld F;
	auto* Target = F.Target();
	auto* ASC = CastChecked<UHALAbilitySystemComponent>(Target->GetAbilitySystemComponent());
	auto* Health = Target->FindComponentByClass<UVehicleHealthComponent>();
	auto* Explosion = F.Explosion();
	const auto First = Explosion->ResolveTarget(Target);
	TestTrue(TEXT("Configured explosion accepted"),First.bHitAccepted);
	TestEqual(TEXT("Damage uses shared GAS HP"),Health->GetCurrentHP(),75.f);
	TestEqual(TEXT("Authored heavy tier reaches receiver"),First.ImpactSummary.SourceTier,EHALImpactTier::Heavy);
	TestEqual(TEXT("Existing bomb remains Standard"),First.ImpactSummary.PushPreset,EHALImpactPushPreset::Standard);
	TestEqual(TEXT("Duplicate target rejected"),Explosion->ResolveTarget(Target).RejectReason,EHALHitRejectReason::Duplicate);
	ASC->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	const auto Immune = F.Explosion()->ResolveTarget(Target);
	TestEqual(TEXT("Impact immunity does not block damage"),Health->GetCurrentHP(),50.f);
	TestEqual(TEXT("Impact immunity suppresses rule impact"),Immune.ImpactSummary.EffectiveTier,EHALImpactTier::None);
	ASC->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	ASC->AddLooseGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
	const auto DamageImmune = F.Explosion()->ResolveTarget(Target);
	TestEqual(TEXT("Damage immunity preserves HP"),Health->GetCurrentHP(),50.f);
	TestEqual(TEXT("Damage immunity preserves heavy impact"),DamageImmune.ImpactSummary.EffectiveTier,EHALImpactTier::Heavy);
	ASC->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
	ASC->SetNumericAttributeBase(UHALImpactAttributeSet::GetTierReductionAttribute(),1);
	TestEqual(TEXT("GAS reduction lowers bomb impact"),F.Explosion()->ResolveTarget(Target).ImpactSummary.EffectiveTier,EHALImpactTier::Medium);
	auto* Far = F.Target(FVector(2000,0,100));
	TestFalse(TEXT("Outside radius rejected"),Explosion->ResolveTarget(Far).bHitAccepted);
	FHALBombConfig Config; Config.bCanHitInstigator=false;
	FHALAttackSourceSnapshot Attribution; Attribution.InstigatorActor=Far;
	Far->SetActorLocation(FVector(100,0,100));
	TestEqual(TEXT("Authored self exclusion"),F.Explosion(Config,Attribution)->ResolveTarget(Far).RejectReason,EHALHitRejectReason::ExcludedInstigator);
	Config.bCanHitInstigator=true; Config.ImpactPushPreset=EHALImpactPushPreset::Long;
	const auto Long = F.Explosion(Config)->ResolveTarget(Far);
	FVehicleKnockbackConfig Table; GetDefault<UVehicleKnockbackSettings>()->GetRuntimeRules(Table);
	FHALImpactPushRules Push; GetDefault<UVehicleKnockbackSettings>()->GetImpactPushRules(Push);
	TestEqual(TEXT("Explosion captures optional Long preset"),Long.ImpactSummary.PushPreset,EHALImpactPushPreset::Long);
	TestEqual(TEXT("Explosion uses shared Long multiplier"),Long.ImpactSummary.DesiredHorizontalDeltaSpeed,Table.HeavyTier.HorizontalDeltaSpeed*Push.LongMultiplier);
	return true;
}

namespace
{
	class FBombTimerAdvance : public IAutomationLatentCommand
	{
		TSharedRef<FBombWorld> Fixture;
		FAutomationTestBase* Test;
		TWeakObjectPtr<AHALBombActor> Bomb;
		float EndTime;
		uint64 LastFrame=MAX_uint64;
		double Deadline=FPlatformTime::Seconds()+15;
	public:
		FBombTimerAdvance(TSharedRef<FBombWorld> F, FAutomationTestBase* T, AHALBombActor* B)
			: Fixture(F), Test(T), Bomb(B), EndTime(F->World->GetTimeSeconds()+B->GetBombConfig().FuseSeconds+0.1f) {}
		bool Update() override
		{
			if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Bomb fuse timer test timed out")); return true; }
			if (Fixture->World->GetTimeSeconds()>=EndTime)
			{
				Test->TestFalse(TEXT("Real timer destroys bomb without a manual detonation"),Bomb.IsValid());
				int32 Count=0; for (TActorIterator<AHALBombExplosion> It(Fixture->World);It;++It) { ++Count; }
				Test->TestEqual(TEXT("Timer creates exactly one independent explosion"),Count,1);
				return true;
			}
			if (LastFrame!=GFrameCounter) { LastFrame=GFrameCounter; Fixture->World->Tick(LEVELTICK_All,0.05f); }
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBombTimerTest,"HAL.FutureCreation.Bomb.ExplicitCountdownAndRealTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBombTimerTest::RunTest(const FString&)
{
	auto Fixture=MakeShared<FBombWorld>();
	auto* Bomb=Fixture->World->SpawnActor<AHALBombActor>(FVector(0,0,1000),FRotator::ZeroRotator);
	Bomb->DispatchBeginPlay();
	TestFalse(TEXT("Placed airborne bomb waits for ground"),Bomb->IsArmed());
	TestTrue(TEXT("Explicit development trigger starts airborne fuse immediately"),Bomb->StartCountdown());
	const float End=Bomb->GetFuseEndServerTime();
	TestTrue(TEXT("Configured fuse duration"),FMath::IsNearlyEqual(End-Fixture->World->GetTimeSeconds(),3.f));
	TestFalse(TEXT("Repeated trigger cannot reset countdown"),Bomb->StartCountdown());
	TestEqual(TEXT("Fuse end remains stable"),Bomb->GetFuseEndServerTime(),End);
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FBombTimerAdvance>(Fixture,this,Bomb));
	return true;
}
#endif
