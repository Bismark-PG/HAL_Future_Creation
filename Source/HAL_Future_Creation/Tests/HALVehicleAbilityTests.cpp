#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALPlayerState.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALMovementAttributeSet.h"
#include "../HALHealthAttributeSet.h"
#include "../HALImpactImmunityEffects.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

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
#endif
