#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehiclePresentationTestSupport.h"
#include "HALLegacyCombatTestRules.h"
#include "../HALPlayerState.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

namespace
{
	UNiagaraSystem* TireSystem() { return LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/FX/NS_TireSkid.NS_TireSkid")); }
	struct FTireSkidWorld
	{
		FScopedHALLegacyCombatTestRules Rules;
		UWorld* World;
		ATestVehiclePawn* Pawn = nullptr;
		UHALVehiclePresentationComponent* Presentation = nullptr;
		UPrimitiveComponent* Body = nullptr;
		explicit FTireSkidWorld(bool bFX = false)
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(bFX)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				bFX ? GEngine->GetDefaultWorldFeatureLevel() : ERHIFeatureLevel::Num, &Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* Class = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Pawn = Class ? World->SpawnActor<ATestVehiclePawn>(Class, FVector(0,0,80), FRotator::ZeroRotator, Spawn) : nullptr;
			if (!Pawn) { return; }
			Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents();
			auto* Controller = World->SpawnActor<APlayerController>();
			Controller->SetAsLocalPlayerController();
			auto* State = World->SpawnActor<AHALPlayerState>(); State->SetOwner(Controller); Controller->SetPlayerState(State);
			State->PreInitializeComponents(); State->InitializeComponents(); State->PostInitializeComponents(); State->DispatchBeginPlay();
			Controller->Possess(Pawn);
			Presentation = FHALPresentationTestAccess::EnableTireSkidProbe(Pawn, TireSystem());
			Body = FHALPresentationTestAccess::Body(Pawn);
			FHALPresentationTestAccess::PublishSupport(Pawn, true);
			FHALPresentationTestAccess::PublishTireSkidInput(Pawn, 2);
			Body->SetPhysicsLinearVelocity(FVector(600,0,0));
		}
		UBoxComponent* Ground(const FVector& Center, ECollisionChannel Channel = ECC_WorldStatic)
		{
			auto* Actor = World->SpawnActor<AActor>(); auto* Box = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Box); Box->SetMobility(EComponentMobility::Movable);
			Box->SetBoxExtent(FVector(45,45,50)); Box->SetCollisionObjectType(Channel);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent(); Actor->SetActorLocation(Center); return Box;
		}
		void Tick() { Presentation->TickComponent(.02f, LEVELTICK_All, nullptr); }
		~FTireSkidWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTireSkidConfigTest, "HAL.FutureCreation.Presentation.TireSkid.AssetAndConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALTireSkidConfigTest::RunTest(const FString&)
{
	FHALTireSkidConfig Config; FString Error;
	TestTrue(TEXT("Disabled old assets require no FX"), Config.Validate(Error));
	Config.bEnabled = true;
	TestFalse(TEXT("Enabled requires asset"), Config.Validate(Error));
	Config.System = TireSystem();
	if (!TestNotNull(TEXT("Actual saved Niagara system"), Config.System.Get())) { return false; }
	TestTrue(TEXT("Saved system has typed User parameters and world-space emitters"), Config.Validate(Error));
	Config.StopSpeed = Config.StartSpeed + 1; TestFalse(TEXT("Invalid hysteresis"), Config.Validate(Error)); Config.StopSpeed = 300;
	Config.Wheels.Add(EHALVehicleWheel::BL); TestFalse(TEXT("Duplicate wheel"), Config.Validate(Error)); Config.Wheels.Pop();
	Config.SlipRatioParameter = TEXT("User.RibbonID"); TestFalse(TEXT("Wrong User parameter type"), Config.Validate(Error));
	Config.SlipRatioParameter = TEXT("User.SlipRatio");
	Config.SurfaceNormalParameter = TEXT("User.SurfaceNormal"); TestFalse(TEXT("Unbound normal is not silently accepted"), Config.Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTireSkidTriggerTest, "HAL.FutureCreation.Presentation.TireSkid.TriggerAndHysteresis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALTireSkidTriggerTest::RunTest(const FString&)
{
	FHALTireSkidConfig C; C.bEnabled = true;
	auto Evaluate = [&C](uint8 Input, float Forward, float Lateral, uint8 Previous = 0, bool bAllowed = true)
	{ return HALVehiclePresentation::TireSkidReasons(C,Input,Forward,Lateral,Previous,bAllowed); };
	TestEqual(TEXT("Fast handbrake sideways drift"), int32(Evaluate(1,600,200)), 1);
	TestEqual(TEXT("Straight handbrake has no drift trail"), int32(Evaluate(1,600,0)), 0);
	TestEqual(TEXT("Sideways impact without demand"), int32(Evaluate(0,600,300)), 0);
	TestEqual(TEXT("Start thresholds prevent noise"), int32(Evaluate(1,320,120)), 0);
	TestEqual(TEXT("Existing drift survives hysteresis band"), int32(Evaluate(1,320,120,1)), 1);
	TestEqual(TEXT("Slip ends below stop threshold"), int32(Evaluate(1,600,90,1)), 0);
	TestEqual(TEXT("Release ends immediately"), int32(Evaluate(0,600,200,1)), 0);
	TestEqual(TEXT("Forward braking"), int32(Evaluate(2,600,0)), 2);
	TestEqual(TEXT("Reverse acceleration is not braking"), int32(Evaluate(2,-600,0)), 0);
	TestEqual(TEXT("Throttle counter-brakes reverse motion"), int32(Evaluate(4,-600,0)), 2);
	TestEqual(TEXT("Stop/reverse low-speed input"), int32(Evaluate(2,0,0)), 0);
	TestEqual(TEXT("Airborne/recovery/stun permission"), int32(Evaluate(3,600,200,3,false)), 0);
	TestEqual(TEXT("Brake wins over throttle"), int32(HALVehiclePresentation::TireSkidInputs(1,1,true)), 3);
	C.StopSpeed = C.DriftStopLateralSpeed = 0;
	TestEqual(TEXT("Zero release thresholds still stop a stationary held handbrake"), int32(Evaluate(1,0,0,1)), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTireSkidGroundTest, "HAL.FutureCreation.Presentation.TireSkid.GroundAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALTireSkidGroundTest::RunTest(const FString&)
{
	FTireSkidWorld F; if (!TestNotNull(TEXT("Saved vehicle"), F.Pawn)) { return false; }
	F.Ground(FVector(-100,-100,0)); F.Ground(FVector(-100,100,0), ECC_WorldDynamic);
	F.Tick(); F.Tick();
	TestEqual(TEXT("Only grounded BL emits; dynamic gameplay object is excluded"), int32(F.Presentation->GetPresentationFrame().TireSkidWheelMask), 4);
	TestFalse(TEXT("Skid support traces do not enable bone suspension"), F.Presentation->GetPresentationFrame().bSuspensionActive);
	FHALPresentationTestAccess::PublishTireSkidInput(F.Pawn,0); F.Tick();
	TestEqual(TEXT("Coasting after braking has no trails"), int32(F.Presentation->GetPresentationFrame().TireSkidWheelMask), 0);
	FHALPresentationTestAccess::PublishTireSkidInput(F.Pawn,2);
	F.Body->SetWorldLocation(FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics); F.Tick();
	TestEqual(TEXT("Wheel-ground gap stops emission"), int32(F.Presentation->GetPresentationFrame().TireSkidWheelMask), 0);
	F.Body->SetWorldLocation(FVector(0,0,80),false,nullptr,ETeleportType::TeleportPhysics);
	FHALPresentationTestAccess::SetRecoveryExecuting(F.Pawn,true); F.Tick();
	TestEqual(TEXT("Recovery stops emission"), int32(F.Presentation->GetPresentationFrame().TireSkidWheelMask), 0);
	FHALPresentationTestAccess::SetRecoveryExecuting(F.Pawn,false); F.Tick();
	TestEqual(TEXT("Recovery release starts a fresh segment"), int32(F.Presentation->GetPresentationFrame().TireSkidWheelMask), 4);
	FHALPresentationTestAccess::Reset(F.Presentation);
	TestEqual(TEXT("Reset clears emission decisions"), int32(F.Presentation->GetPresentationFrame().TireSkidWheelMask), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTireSkidRHITest, "HAL.RenderSmoke.TireSkid.ComponentLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALTireSkidRHITest::RunTest(const FString&)
{
	FTireSkidWorld F(true); if (!TestNotNull(TEXT("Saved vehicle"), F.Pawn)) { return false; }
	F.Ground(FVector(-100,-100,0)); F.Tick(); F.Tick();
	auto* FX = FHALPresentationTestAccess::TireSkid(F.Presentation,2);
	if (!TestNotNull(TEXT("Requires real RHI: Niagara component created"), FX)) { return false; }
	TestEqual(TEXT("Saved system assigned"), FX->GetAsset(), TireSystem());
	TestTrue(TEXT("Ground offset applied"), FX->GetComponentLocation().Equals(FVector(-100,-100,51),.1));
	const FNiagaraVariable Parameter(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SlipRatio"));
	TestEqual(TEXT("Actual User parameter override"), FX->GetOverrideParameters().GetParameterValue<float>(Parameter),1.f);
	FHALPresentationTestAccess::PublishTireSkidInput(F.Pawn,0); F.Tick();
	TestNull(TEXT("No active segment after release"), FHALPresentationTestAccess::TireSkid(F.Presentation,2));
	TestEqual(TEXT("Old segment retained to fade"), FHALPresentationTestAccess::FadingTireSkids(F.Presentation),1);
	FHALPresentationTestAccess::PublishTireSkidInput(F.Pawn,2); F.Tick();
	TestTrue(TEXT("Restart uses a new component and cannot bridge old ribbon"), FHALPresentationTestAccess::TireSkid(F.Presentation,2) != FX);
	FHALPresentationTestAccess::Reset(F.Presentation);
	TestFalse(TEXT("Reset destroys fading segment"), IsValid(FX));
	TestEqual(TEXT("Reset clears fading ownership"),FHALPresentationTestAccess::FadingTireSkids(F.Presentation),0);
	return true;
}
#endif
