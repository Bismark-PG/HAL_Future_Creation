#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "HALRoomTestSupport.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../BallControlComponent.h"
#include "../BasicBallActor.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALBattleTopAbility.h"
#include "../HALBattleTopComponent.h"
#include "../HALImpactReceiverComponent.h"
#include "../VehicleRecoveryComponent.h"
#include "../HALPlayerState.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALMovementAttributeSet.h"
#include "../HALHealthAttributeSet.h"
#include "../HALImpactImmunityEffects.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/SphereComponent.h"
#include "Misc/ScopeExit.h"

struct FHALBattleTopTestAccess
{
	static void SetPose(ATestVehiclePawn* Pawn, bool bGrounded, FVector Up = FVector::UpVector)
	{
		auto* Movement = Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>();
		Movement->bShouldRunPhysics.Store(true);
		Movement->PublishedStability.bValid = true;
		Movement->PublishedStability.bGrounded = bGrounded;
		Movement->PublishedStability.bDrivingAllowed = true;
		Movement->PublishedStability.Up = Up;
	}
	static bool Acquire(ATestVehiclePawn* Pawn, ABasicBallActor* Ball)
	{ return Pawn->FindComponentByClass<UBallControlComponent>()->AcquireBall(Ball); }
	static void AcknowledgePhysicsFrame(ATestVehiclePawn* Pawn, FHALMovementSnapshot Snapshot)
	{
		// This world has no simulation step. The live network test exercises the actual acknowledgement.
		Snapshot.Stamp(nullptr, 1, 1.f / 60.f);
		Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>()->PublishedMovementStatus.Definition = Snapshot;
	}
	static void SetRecovering(ATestVehiclePawn* Pawn, bool bRecovering)
	{
		auto* Recovery = Pawn->FindComponentByClass<UVehicleRecoveryComponent>();
		auto* Status = FindFProperty<FStructProperty>(Recovery->GetClass(), TEXT("Status"))
			->ContainerPtrToValuePtr<FVehicleRecoveryStatus>(Recovery);
		Status->Phase = bRecovering ? EVehicleRecoveryPhase::Executing : EVehicleRecoveryPhase::Idle;
	}
};

namespace
{
	struct FSkillWorld
	{
		UWorld* W;
		FSkillWorld()
		{
			const auto V=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&V);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
			auto* S=W->SpawnActor<AHALMatchGameState>(); W->SetGameState(S); S->SetMatchPhase(EHALMatchPhase::Playing);
		}
		AHALPlayerState* Player()
		{
			auto* PC=W->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
			auto* PS=W->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS);
			PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay(); return PS;
		}
		ATestVehiclePawn* Car(bool Boost,UVehicleDefinition* Shared=nullptr)
		{
			auto* C=LoadClass<ATestVehiclePawn>(nullptr,Boost?TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C")
				:TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_02.BP_VehiclePawn_Car_02_C"));
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* A=W->SpawnActor<ATestVehiclePawn>(C,FVector(0,0,2000),FRotator::ZeroRotator,P);
			if (Shared) { FHALVehicleAbilityTestAccess::UseDefinition(A,Shared); }
			else { FHALVehicleAbilityTestAccess::Configure(A,Boost?UHALMovementBoostAbility::StaticClass():UHALImpactImmunityAbility::StaticClass()); }
			A->PreInitializeComponents(); A->InitializeComponents(); A->PostInitializeComponents(); return A;
		}
		~FSkillWorld() { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); }
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleGrantValidationTest,"HAL.FutureCreation.VehicleAbilities.AtomicBatchAndExplicitVersion",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALVehicleGrantValidationTest::RunTest(const FString&)
{
	FString Error; FHALVehicleAbilityConfig C; TestTrue(TEXT("Explicit legacy transition remains usable"),C.Validate(Error));
	C.Version=EHALVehicleAbilityVersion::VehicleDefinitionV1; TestFalse(TEXT("Missing V1 never falls back"),C.Validate(Error));
	C=FHALVehicleAbilityTestAccess::Config(UHALMovementBoostAbility::StaticClass()); TestTrue(TEXT("Valid unique Primary"),C.Validate(Error));
	const auto First=C.Grants[0]; C.Grants.Add(First); TestFalse(TEXT("Duplicate Primary rejected"),C.Validate(Error)); C.Grants.Pop();
	C.Grants[0].Level=0; TestFalse(TEXT("Invalid level rejected"),C.Validate(Error)); C.Grants[0]=First;
	C.Grants[0].InputTag=HALGameplayTags::State_Movement_Boost; TestFalse(TEXT("State tag cannot substitute Primary"),C.Validate(Error));
	FSkillWorld F; auto* PS=F.Player(); auto* ASC=PS->GetHALAbilitySystemComponent();
	auto* SourceA=F.W->SpawnActor<AActor>(); auto* SourceB=F.W->SpawnActor<AActor>();
	TArray<FHALAbilityGrant> Bad={First}; Bad.AddDefaulted();
	AddExpectedMessage(TEXT("Ability grant rejected"),ELogVerbosity::Error,EAutomationExpectedMessageFlags::Contains,1);
	TestFalse(TEXT("Invalid batch rejected before partial give"),ASC->GrantAbilities(Bad,SourceA));
	TestEqual(TEXT("No partial spec"),ASC->GetActivatableAbilities().Num(),0);
	TestTrue(TEXT("Valid batch given"),ASC->GrantAbilities({First},SourceA));
	TestTrue(TEXT("Same source/class/slot/level idempotent"),ASC->GrantAbilities({First},SourceA));
	TestFalse(TEXT("Cross-source duplicate slot detected before give"),ASC->CanGrantAbilities({First},SourceB,Error));
	TestTrue(TEXT("Preflight may ignore only outgoing source"),ASC->CanGrantAbilities({First},SourceB,Error,SourceA));
	TestEqual(TEXT("Only one spec remains"),ASC->GetActivatableAbilities().Num(),1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleModelSkillTest,"HAL.FutureCreation.VehicleAbilities.ModelSwitchAndOwnerIsolation",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALVehicleModelSkillTest::RunTest(const FString&)
{
	FSkillWorld F; auto* PS=F.Player(); auto* ASC=PS->GetHALAbilitySystemComponent(); auto* A=F.Car(true);
	TestTrue(TEXT("Car01 explicit lifecycle binds"),PS->ChangeVehicleAvatar(A,1)); A->DispatchBeginPlay();
	TestTrue(TEXT("Car01 Primary installed"),PS->AreVehicleAbilitiesReady(A));
	TestTrue(TEXT("Car01 activates boost despite legacy PS immunity default"),ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	TestTrue(TEXT("Boost effect present"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost));
	ASC->ApplyGameplayEffectToSelf(GetDefault<UHALImpactImmunityCooldownEffect>(),1,ASC->MakeEffectContext());
	auto* B=F.Car(false); const auto Config=B->GetVehicleAbilityConfig(); auto Invalid=Config; Invalid.Grants.Reset();
	FHALVehicleAbilityTestAccess::RuntimeConfig(B,Invalid);
	AddExpectedMessage(TEXT("Vehicle transition abilities rejected"),ELogVerbosity::Error,EAutomationExpectedMessageFlags::Contains,1);
	TestFalse(TEXT("Invalid replacement refused before retiring old car"),PS->ChangeVehicleAvatar(B,2));
	TestTrue(TEXT("Old control and effect survive failed preflight"),ASC->GetAvatarActor()==A && A->IsConfigurationValid() && ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost));
	FHALVehicleAbilityTestAccess::RuntimeConfig(B,Config);
	TestTrue(TEXT("Car02 binds"),PS->ChangeVehicleAvatar(B,2)); B->DispatchBeginPlay();
	TestFalse(TEXT("Old boost removed by GF2 lifecycle"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost));
	TestEqual(TEXT("Old cooldown reset by explicit change"),ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity),0.f);
	TestTrue(TEXT("Car02 Primary activates immunity"),ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	TestTrue(TEXT("Immunity active"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune));
	const float Remaining=ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity);
	B->RefreshAbilitySystemBinding(); TestTrue(TEXT("Repeated grant succeeds idempotently"),PS->GrantVehicleAbilities());
	TestEqual(TEXT("Ordinary binding preserves cooldown"),ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity),Remaining);
	TestEqual(TEXT("Exactly one current vehicle spec"),ASC->GetActivatableAbilities().Num(),1);
	auto* A2=F.Car(true); TestTrue(TEXT("Car01 again with fresh source"),PS->ChangeVehicleAvatar(A2,3)); A2->DispatchBeginPlay();
	TestFalse(TEXT("Car02 immunity does not cross model change"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune));
	ASC->ClearAvatarIfCurrent(B); TestTrue(TEXT("Delayed old cleanup cannot revoke new skill"),PS->AreVehicleAbilitiesReady(A2));
	auto* PS2=F.Player(); auto* Other=F.Car(true,const_cast<UVehicleDefinition*>(A2->GetVehicleDefinition()));
	TestTrue(TEXT("Second player same shared model definition"),PS2->ChangeVehicleAvatar(Other,1)); Other->DispatchBeginPlay();
	TestTrue(TEXT("First player's boost activates"),ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	TestFalse(TEXT("Same Definition never shares active effect state"),PS2->GetHALAbilitySystemComponent()->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost));
	auto* PS3=F.Player(); auto* ImmunePeer=F.Car(false,const_cast<UVehicleDefinition*>(B->GetVehicleDefinition()));
	TestTrue(TEXT("Same immunity model has its own source"),PS3->ChangeVehicleAvatar(ImmunePeer,1)); ImmunePeer->DispatchBeginPlay();
	TestTrue(TEXT("Peer immunity activates independently"),PS3->GetHALAbilitySystemComponent()->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	TestEqual(TEXT("Peer cooldown does not affect another owner"),ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity),0.f);
	for (const auto& S:ASC->GetActivatableAbilities()) { TestEqual(TEXT("Spec owns live Pawn, not shared DA/PlayerState"),S.SourceObject.Get(),static_cast<UObject*>(A2)); }
	A2->Destroy(); TestEqual(TEXT("Destroyed Pawn source grants removed even during weak invalidation"),ASC->GetActivatableAbilities().Num(),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBattleTopWallTest, "HAL.FutureCreation.BattleTop.WallResponseAndConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBattleTopWallTest::RunTest(const FString&)
{
	const auto* Config = GetDefault<UHALBattleTopConfig>();
	FString Error;
	TestTrue(TEXT("Default skill DA tuning is valid"), Config->Validate(Error));
	TestTrue(TEXT("Wall supplements a head-on hit in XY and leaves Z and tangent alone"),
		UHALBattleTopComponent::CalculateWallDelta(*Config, FVector(1, 0, .2), FVector(100, 240, -50), FVector(600, 0, 0))
			.Equals(FVector(475, 0, 0), .01));
	TestTrue(TEXT("Grazing hit adds no velocity"),
		UHALBattleTopComponent::CalculateWallDelta(*Config, FVector::ForwardVector, FVector(20, 1000, 0), FVector(90, 0, 0)).IsZero());
	TestTrue(TEXT("Strong bounce has an authored cap"),
		UHALBattleTopComponent::CalculateWallDelta(*Config, FVector::ForwardVector, FVector(100, 0, 0), FVector(9000, 0, 0))
			.Equals(FVector(2300, 0, 0), .01));
	TestTrue(TEXT("Natural outward response already at target is retained"),
		UHALBattleTopComponent::CalculateWallDelta(*Config, FVector::ForwardVector, FVector(600, 100, 0), FVector(900, 0, 0)).IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBattleTopLifecycleTest, "HAL.FutureCreation.BattleTop.AuthoritySkillContactsAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBattleTopLifecycleTest::RunTest(const FString&)
{
	FSkillWorld F;
	auto* Native = GetMutableDefault<UHALBattleTopAbility>();
	auto* Property = FindFProperty<FObjectProperty>(Native->GetClass(), TEXT("SkillConfig"));
	auto* Previous = Property->GetObjectPropertyValue_InContainer(Native);
	auto* Config = NewObject<UHALBattleTopConfig>();
	Config->DurationSeconds = 7.f; Config->CooldownSeconds = 13.f;
	Config->SpinRate = 720.f;
	Property->SetObjectPropertyValue_InContainer(Native, Config);
	ON_SCOPE_EXIT { Property->SetObjectPropertyValue_InContainer(Native, Previous); };
	int32 PlayerId = 100;
	const auto MakeCar = [&]()
	{
		auto* Pawn = F.Car(false);
		auto* Definition = FHALVehicleAbilityTestAccess::Configure(Pawn, UHALBattleTopAbility::StaticClass());
		FHALVehicleAbilityTestAccess::RuntimeConfig(Pawn, Definition->Abilities);
		auto* Player = F.Player();
		Player->SetPlayerId(++PlayerId);
		if (!TestTrue(TEXT("Vehicle is granted Battle Top through the normal Primary slot"), Player->ChangeVehicleAvatar(Pawn, 1))) { return static_cast<ATestVehiclePawn*>(nullptr); }
		Pawn->DispatchBeginPlay();
		FHALBattleTopTestAccess::SetPose(Pawn, true);
		return Pawn;
	};
	auto* A = MakeCar(); auto* B = MakeCar();
	if (!A || !B) { return false; }
	B->SetActorLocation(A->GetActorLocation() + FVector(400, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
	auto* AA = CastChecked<UHALAbilitySystemComponent>(A->GetAbilitySystemComponent());
	auto* BA = CastChecked<UHALAbilitySystemComponent>(B->GetAbilitySystemComponent());
	auto* TopA = A->FindComponentByClass<UHALBattleTopComponent>();
	auto* TopB = B->FindComponentByClass<UHALBattleTopComponent>();
	auto* BodyA = CastChecked<UPrimitiveComponent>(A->GetRootComponent());
	auto* BodyB = CastChecked<UPrimitiveComponent>(B->GetRootComponent());
	const FGameplayTag Input = HALGameplayTags::Input_Ability_Primary;
	FHALBattleTopTestAccess::SetPose(A, false);
	TestFalse(TEXT("Airborne cannot activate"), AA->TryActivateAbilityInput(Input));
	FHALBattleTopTestAccess::SetPose(A, true, -FVector::UpVector);
	TestFalse(TEXT("Upside down cannot activate"), AA->TryActivateAbilityInput(Input));
	FHALBattleTopTestAccess::SetPose(A, true);
	FHALBattleTopTestAccess::SetRecovering(A, true);
	TestFalse(TEXT("Recovery cannot activate"), AA->TryActivateAbilityInput(Input));
	FHALBattleTopTestAccess::SetRecovering(A, false);
	const auto MakeBall = [&]()
	{
		auto* Class = LoadClass<ABasicBallActor>(nullptr, TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Ball = F.W->SpawnActor<ABasicBallActor>(Class, A->GetActorLocation() + FVector(210, 0, 0), FRotator::ZeroRotator, Params);
		Ball->PreInitializeComponents(); Ball->DispatchBeginPlay();
		return Ball;
	};
	auto* Held = MakeBall();
	if (!TestTrue(TEXT("Initial ball is held"), FHALBattleTopTestAccess::Acquire(A, Held))) { return false; }
	TestTrue(TEXT("Grounded component permits activation before GAS commit"), TopA->CanStart());
	TestTrue(TEXT("The normal slot is granted the native Battle Top class"), AA->GetActivatableAbilities().Num() == 1
		&& AA->GetActivatableAbilities()[0].Ability->IsA<UHALBattleTopAbility>());
	const FVector BeforeShot = BodyA->GetPhysicsLinearVelocity();
	TestTrue(TEXT("Grounded Primary activates"), AA->TryActivateAbilityInput(Input));
	if (!TestTrue(TEXT("Both GAS and component report the skill"), TopA->IsActive() && AA->HasMatchingGameplayTag(HALGameplayTags::State_BattleTop))) { return false; }
	TestEqual(TEXT("Visual speed comes from the skill DA"), TopA->GetVisualSpinRate(), 720.f);
	TestTrue(TEXT("Held ball gets one launch from this player at authored speed"),
		Held->GetBallState() == EBasicBallState::Launched && Held->GetLaunchedBy() == A
		&& FMath::IsNearlyEqual(Held->GetPhysicsRoot()->GetPhysicsLinearVelocity().Size(), double(Config->HeldBallSpeed), 1.0));
	TestTrue(TEXT("Skill launches without vehicle recoil"), BodyA->GetPhysicsLinearVelocity().Equals(BeforeShot, .1));
	const float Remaining = AA->GetEffectTimeRemaining(HALGameplayTags::State_BattleTop);
	TestTrue(TEXT("GE duration and cooldown both use the skill DA"), Remaining > 6.9f && Remaining <= 7.01f
		&& AA->GetEffectTimeRemaining(HALGameplayTags::Cooldown_BattleTop) > 12.9f);
	TestFalse(TEXT("Duplicate Primary does not refresh"), AA->TryActivateAbilityInput(Input));
	auto* Bridge = A->FindComponentByClass<UHALMovementEffectBridge>();
	const auto Restriction = FHALRoomTestAccess::Flush(Bridge);
	TestTrue(TEXT("GAS publishes the handbrake bit to physics"), Restriction.bHandbrakeBlocked);
	FHALBattleTopTestAccess::AcknowledgePhysicsFrame(A, Restriction);

	TestTrue(TEXT("Stun coexists"), AA->ApplyStun(2.f, B));
	auto* Free = MakeBall();
	FHALBattleTopTestAccess::SetRecovering(A, true);
	TestFalse(TEXT("Self-right suspends free-ball contact"), TopA->HandleFreeBall(*Free));
	FHALBattleTopTestAccess::SetRecovering(A, false);
	TestTrue(TEXT("Stunned top can still launch a free ball"), TopA->HandleFreeBall(*Free));
	TestTrue(TEXT("Free ball belongs to the top"), Free->GetLaunchedBy() == A && Free->GetBallState() == EBasicBallState::Launched);
	TestFalse(TEXT("Already-launched ball is not claimed twice"), TopA->HandleFreeBall(*Free));
	TestTrue(TEXT("Second top starts independently"), BA->TryActivateAbilityInput(Input) && TopB->IsActive());
	const auto HP = UHALHealthAttributeSet::GetCurrentHPAttribute();
	const float BeforeA = AA->GetNumericAttribute(HP), BeforeB = BA->GetNumericAttribute(HP);
	FHitResult Hit; Hit.ImpactNormal = FVector(-1, 0, 0);
	TestTrue(TEXT("Vehicle pair is owned by the skill"), UHALBattleTopComponent::HandleVehiclePair(*BodyA, *BodyB, FVector(-2000, 0, 0), Hit));
	const float AfterA = AA->GetNumericAttribute(HP), AfterB = BA->GetNumericAttribute(HP);
	TestTrue(TEXT("Both players take damage, including the stunned top's target"), AfterA < BeforeA && AfterB < BeforeB);
	TestTrue(TEXT("Both effective tiers remain available for the horizontal push"),
		A->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact().EffectiveTier != EHALImpactTier::None
		&& B->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact().EffectiveTier != EHALImpactTier::None);
	TestTrue(TEXT("Reciprocal callbacks do not hit again"), UHALBattleTopComponent::HandleVehiclePair(*BodyB, *BodyA, FVector(2000, 0, 0), Hit));
	TestTrue(TEXT("Both hit records are retained"), AA->GetNumericAttribute(HP) == AfterA && BA->GetNumericAttribute(HP) == AfterB);

	FHALBattleTopTestAccess::SetRecovering(A, true);
	AA->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(HALGameplayTags::State_BattleTop)));
	TestFalse(TEXT("GE removal ends the component while stun persists"), TopA->IsActive());
	TestEqual(TEXT("Visual speed clears with activation"), TopA->GetVisualSpinRate(), 0.f);
	TestTrue(TEXT("Stun stays independently active"), AA->IsStunned());
	TestTrue(TEXT("Top expiry leaves an ongoing self-right untouched"), A->FindComponentByClass<UVehicleRecoveryComponent>()->IsExecuting());
	TestFalse(TEXT("GE removal restores the physical handbrake bit"), FHALRoomTestAccess::Flush(Bridge).bHandbrakeBlocked);
	B->RetireForVehicleTransition();
	TestTrue(TEXT("Retirement removes the active state from both Pawn and ASC"), !TopB->IsActive() && !BA->HasMatchingGameplayTag(HALGameplayTags::State_BattleTop));
	return true;
}
#endif
