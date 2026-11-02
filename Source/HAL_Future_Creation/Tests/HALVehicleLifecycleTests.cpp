#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehicleLifecycleTestSupport.h"
#include "HALRoomTestSupport.h"
#include "../HALPlayerState.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALCombatAttributeSet.h"
#include "../HALImpactAttributeSet.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALMovementAttributeSet.h"
#include "../HALMovementBoostEffect.h"
#include "../HALMovementAreaReceiver.h"
#include "../HALImpactImmunityEffects.h"
#include "../HALImpactImmunityAbility.h"
#include "../HALGameplayTags.h"
#include "../CombatResolver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"

namespace
{
	struct FVehicleLifeWorld
	{
		UWorld* World;
		AHALMatchGameState* Match;
		AHALPlayerController* PC;
		AHALPlayerState* PS;
		UHALAbilitySystemComponent* ASC;
		ATestVehiclePawn* Initial;
		FVehicleLifeWorld()
		{
			const auto V = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&V);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Match = World->SpawnActor<AHALMatchGameState>(); World->SetGameState(Match); Match->SetMatchPhase(EHALMatchPhase::Playing);
			PC = World->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
			PS = World->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS);
			PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay();
			ASC = PS->GetHALAbilitySystemComponent(); Initial = Spawn(false); PC->Possess(Initial); Initial->DispatchBeginPlay();
		}
		ATestVehiclePawn* Spawn(bool B)
		{
			auto* Class = LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Pawn = World->SpawnActor<ATestVehiclePawn>(Class,FVector(0,0,1000),FRotator::ZeroRotator,P);
			FHALVehicleLifecycleTestAccess::Configure(Pawn,B);
			Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents(); return Pawn;
		}
		ABasicBallActor* Ball()
		{
			auto* Class = LoadClass<ABasicBallActor>(nullptr,TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<ABasicBallActor>(Class,FVector(200,0,1000),FRotator::ZeroRotator,P);
		}
		float HP() const { return ASC->GetSet<UHALHealthAttributeSet>()->GetCurrentHP(); }
		void SetHP(float Value) { ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),Value); }
		void Effect(const UGameplayEffect* E) { ASC->ApplyGameplayEffectToSelf(E,1,ASC->MakeEffectContext()); }
		void Practice()
		{
			auto* Room = Match->GetRoom(); Room->Initialize(FHALRoomTestAccess::Config(Room,EHALRoomProfile::Development)); Room->AddMember(PC);
			Room->Request(PC,EHALRoomAction::SelectVehicle,TEXT("Car_01"),PS->GetRoomData().SelectionRevision);
			Room->Request(PC,EHALRoomAction::ConfirmVehicle,NAME_None,PS->GetRoomData().SelectionRevision);
			Room->CompleteAvatar(PC,PS->GetRoomData().SelectionRevision);
		}
		~FVehicleLifeWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleChangeTest,"HAL.FutureCreation.GameFlow.GF2.VehicleSeedsSourcesAndRebinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALVehicleChangeTest::RunTest(const FString&)
{
	FVehicleLifeWorld F;
	F.SetHP(25); F.Effect(GetDefault<UHALMovementBoostEffect>()); F.Effect(GetDefault<UHALImpactImmunityCooldownEffect>());
	const int32 Shared = F.ASC->GetActivatableAbilities().Num();
	FHALAbilityGrant Grant; Grant.Ability = UHALImpactImmunityAbility::StaticClass(); Grant.InputTag = HALGameplayTags::Input_Ability_Primary;
	F.ASC->GrantAbilities({Grant},F.Initial);
	auto* AreaSource = F.World->SpawnActor<AActor>(); FHALMovementAreaConfig Area;
	F.Initial->FindComponentByClass<UHALMovementAreaReceiver>()->RegisterArea(AreaSource,Area);
	auto* Ball = F.Ball(); Ball->DispatchBeginPlay();
	TestTrue(TEXT("Acquire actual controlled ball/constraint"),FHALVehicleLifecycleTestAccess::Acquire(F.Initial,Ball));
	auto* B = F.Spawn(true);
	TestTrue(TEXT("Explicit A to B transition"),F.PS->ChangeVehicleAvatar(B,1)); B->DispatchBeginPlay();
	TestEqual(TEXT("B correct max/full HP"),F.HP(),220.f);
	TestEqual(TEXT("B attack source"),F.ASC->GetSet<UHALCombatAttributeSet>()->GetAttackPower(),75.f);
	TestEqual(TEXT("B impact resistance independent"),F.ASC->GetSet<UHALImpactAttributeSet>()->GetTierReduction(),1.f);
	TestEqual(TEXT("B output delta source"),F.ASC->GetSet<UHALCombatAttributeSet>()->GetOutputMultiplierDelta(),.25f);
	TestEqual(TEXT("B damage resistance source"),F.ASC->GetSet<UHALCombatAttributeSet>()->GetDamageTakenMultiplier(),.7f);
	TestEqual(TEXT("B movement seed belongs to fresh component"),FHALVehicleLifecycleTestAccess::Acceleration(B),3600.f);
	TestEqual(TEXT("Old boost removed"),F.ASC->GetSet<UHALMovementAttributeSet>()->GetAccelerationMultiplierDelta(),0.f);
	TestEqual(TEXT("Old area membership removed"),F.Initial->FindComponentByClass<UHALMovementAreaReceiver>()->GetAreaCount(),0);
	TestEqual(TEXT("Old controlled ball released"),Ball->GetBallState(),EBasicBallState::Free);
	TestEqual(TEXT("Old source grants removed, shared retained"),F.ASC->GetActivatableAbilities().Num(),Shared);
	TestEqual(TEXT("Explicit vehicle change clears cooldown"),F.ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity),0.f);
	F.SetHP(51); F.Effect(GetDefault<UHALImpactImmunityCooldownEffect>());
	TestTrue(TEXT("Same operation idempotent"),F.PS->ChangeVehicleAvatar(B,1)); B->RefreshAbilitySystemBinding(); F.PS->GrantVehicleAbilities();
	TestEqual(TEXT("Repeated binding does not heal"),F.HP(),51.f);
	TestTrue(TEXT("Repeated binding preserves new cooldown"),F.ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity)>0);
	auto* A2 = F.Spawn(false); TestTrue(TEXT("B back to A with a fresh avatar"),F.PS->ChangeVehicleAvatar(A2,2)); A2->DispatchBeginPlay();
	TestEqual(TEXT("A seed restored"),F.HP(),100.f);
	TestEqual(TEXT("A attack restored"),F.ASC->GetSet<UHALCombatAttributeSet>()->GetAttackPower(),20.f);
	TestEqual(TEXT("A resistance restored"),F.ASC->GetSet<UHALImpactAttributeSet>()->GetTierReduction(),0.f);
	TestEqual(TEXT("A movement restored"),FHALVehicleLifecycleTestAccess::Acceleration(A2),1800.f);
	F.Effect(GetDefault<UHALImpactImmunityEffect>()); B->RefreshAbilitySystemBinding(); B->Destroy(); F.Initial->Destroy();
	TestEqual(TEXT("Late old avatar binding/cleanup cannot steal current ASC"),F.ASC->GetAvatarActor(),static_cast<AActor*>(A2));
	TestTrue(TEXT("New effect survives delayed old cleanup"),F.ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleRoundTest,"HAL.FutureCreation.GameFlow.GF2.RoundIdentityAndFreshAvatar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALVehicleRoundTest::RunTest(const FString&)
{
	FVehicleLifeWorld F; auto* B = F.Spawn(true);
	TestFalse(TEXT("Round zero rejected"),F.PS->BeginNewRoundAvatar(B,0,1));
	TestTrue(TEXT("Explicit new round"),F.PS->BeginNewRoundAvatar(B,1,1)); B->DispatchBeginPlay();
	TestEqual(TEXT("Round identity committed"),F.PS->GetVehicleLifecycle().RoundId,1u);
	F.SetHP(12); F.Effect(GetDefault<UHALDebugImpactImmunityEffect>()); F.Effect(GetDefault<UHALImpactImmunityCooldownEffect>());
	TestTrue(TEXT("Duplicate round operation succeeds without reset"),F.PS->BeginNewRoundAvatar(B,1,1));
	TestEqual(TEXT("No duplicate round heal"),F.HP(),12.f);
	TestTrue(TEXT("No duplicate round cleanse"),F.ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune));
	auto* A2 = F.Spawn(false);
	TestFalse(TEXT("Old round cannot initialize another pawn"),F.PS->BeginNewRoundAvatar(A2,1,2));
	TestFalse(TEXT("Serial reuse cannot target another pawn"),F.PS->ChangeVehicleAvatar(A2,1));
	TestTrue(TEXT("Next round uses fresh physics/input components"),F.PS->BeginNewRoundAvatar(A2,2,2)); A2->DispatchBeginPlay();
	TestEqual(TEXT("Round two reset to A"),F.HP(),100.f);
	TestFalse(TEXT("New round clears even unscoped practice effects"),F.ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune));
	TestEqual(TEXT("New round cooldown clear"),F.ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity),0.f);
	TestFalse(TEXT("Stale serial rejected"),F.PS->ChangeVehicleAvatar(F.Spawn(true),1));
	TestFalse(TEXT("Same live pawn cannot be reused to erase physics history"),F.PS->BeginNewRoundAvatar(A2,3,3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPracticeDamageTest,"HAL.FutureCreation.GameFlow.GF2.PracticeDamageImpactAndConsumption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALPracticeDamageTest::RunTest(const FString&)
{
	FVehicleLifeWorld F; F.Practice();
	auto* Ball = F.Ball(); Ball->DispatchBeginPlay();
	TestTrue(TEXT("Practice victim can hold a ball before hit"),FHALVehicleLifecycleTestAccess::Acquire(F.Initial,Ball));
	auto* Source = F.World->SpawnActor<AActor>();
	auto Hit = [&F,Source](FHALCombatAction& Action)
	{
		FHALAttackSourceSnapshot Snapshot; Snapshot.SourceActor = Source; Snapshot.FixedDamage = 25; Action.Begin(*Source,Snapshot);
		FHALCombatHitRequest R; R.AttackId = Action.GetSnapshot().AttackId; R.Hit.TargetActor = F.PC->GetPawn();
		R.bApplyImpact = true;
		R.Hit.TargetPhysicsBody = Cast<UPrimitiveComponent>(F.PC->GetPawn()->GetRootComponent()); R.Hit.ImpactNormal = FVector(-1,0,0);
		R.Impact.Mode = EHALImpactMode::ExplicitTier; R.Impact.Tier = EHALImpactTier::Heavy; return R;
	};
	FHALCombatAction Practice; auto R = Hit(Practice); const auto P = FCombatResolver::ResolveCombatHit(Practice,R);
	TestTrue(TEXT("Practice still accepts standard hit"),P.bHitAccepted);
	TestTrue(TEXT("Suppression is phase policy, independent of immunity"),P.bDamageSuppressedByFlow && !P.bDamageImmune);
	TestEqual(TEXT("Practice HP unchanged"),F.HP(),100.f);
	TestTrue(TEXT("Impact tier result still resolves"),P.ImpactSummary.SourceTier == EHALImpactTier::Heavy && P.bImpactApplied);
	TestTrue(TEXT("Practice hit still drops controlled ball"),P.ImpactSummary.bBallDropped && Ball->GetBallState() == EBasicBallState::Free);
	TestFalse(TEXT("Same hit already consumed"),FCombatResolver::ResolveCombatHit(Practice,R).bHitAccepted);
	auto* Room = F.Match->GetRoom(); Room->Request(F.PC,EHALRoomAction::Start,NAME_None,F.PS->GetRoomData().SelectionRevision);
	Room->AdvanceRound(1,EHALRoomPhase::Loading);
	auto* Fresh = F.Spawn(false); TestTrue(TEXT("Formal entry uses explicit new round avatar"),F.PS->BeginNewRoundAvatar(Fresh,1,1)); Fresh->DispatchBeginPlay();
	TestEqual(TEXT("Stale loading readiness rejected"),Room->CompleteAvatar(F.PC,F.PS->GetRoomData().SelectionRevision,2),EHALRoomResult::StaleSelection);
	TestEqual(TEXT("Correct server round avatar readiness"),Room->CompleteAvatar(F.PC,F.PS->GetRoomData().SelectionRevision,1),EHALRoomResult::Accepted);
	Room->AdvanceRound(1,EHALRoomPhase::Countdown); Room->AdvanceRound(1,EHALRoomPhase::Playing);
	FHALCombatAction Formal; const auto FResult = FCombatResolver::ResolveCombatHit(Formal,Hit(Formal));
	TestTrue(TEXT("Formal damage restored"),FResult.bHitAccepted && !FResult.bDamageSuppressedByFlow && FResult.ActualHPRemoved > 0);
	TestEqual(TEXT("Formal HP reduced normally"),F.HP(),75.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALConfiguredRoomAssetsTest,"HAL.FutureCreation.GameFlow.GF2.ConfiguredCatalogAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALConfiguredRoomAssetsTest::RunTest(const FString&)
{
	auto* Catalog = LoadObject<UHALVehicleCatalog>(nullptr,TEXT("/Game/Data/Gameflow/DA_VehicleCatalog_Default.DA_VehicleCatalog_Default"));
	if (!TestNotNull(TEXT("Team's saved catalog"),Catalog)) { return false; }
	FString Error; TestTrue(TEXT("Both saved classes have valid migrated definitions"),Catalog->Validate(Error));
	TestNotNull(TEXT("Car_01 ID resolves"),Catalog->Resolve(TEXT("Car_01"),Error)); TestNotNull(TEXT("Car_02 ID resolves"),Catalog->Resolve(TEXT("Car_02"),Error));
	// Asset names are not rules; profile intent is checked separately in the handoff record.
	return true;
}
#endif
