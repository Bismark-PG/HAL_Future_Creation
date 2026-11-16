#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALBullChargeAbility.h"
#include "../HALBullChargeComponent.h"
#include "../HALVehicleFXTargets.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include <limits>

struct FHALBullChargeFXTestAccess
{
	static void State(UHALBullChargeComponent& C, UHALBullChargeConfig* Skill, FGuid Activation)
	{
		C.VisualState.FX = Skill ? Skill->FX : FHALBullChargeFXConfig(); C.VisualState.Activation = Activation;
		C.VisualState.DurationSeconds = Skill ? Skill->Motion.DurationSeconds : 0;
		C.VisualState.RampSeconds = Skill ? Skill->Motion.RampSeconds : 0;
		C.VisualState.StartServerTime = C.GetWorld()->GetTimeSeconds(); C.OnRep_VisualState();
	}
	static void Receive(UHALBullChargeComponent& C, FHALBullChargeImpactFXEvent Event) { C.ConsumeImpactFX(Event); }
	static uint32 Count(const UHALBullChargeComponent& C) { return C.ReceivedImpactFXCount; }
	static int32 Pending(const UHALBullChargeComponent& C) { return C.PendingImpactFX.Num(); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBullChargeFXMathTest, "HAL.FutureCreation.BullCharge.FX.SpeedPhaseAndVelocity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBullChargeFXMathTest::RunTest(const FString&)
{
	const FHALBullChargeFXConfig Defaults;
	TestTrue(TEXT("Default art recipe is valid without resource references"), Defaults.IsValid());
	TestEqual(TEXT("Pre-activation time has no ramp"), HALBullChargeFX::Ramp(-1, 2), 0.f);
	TestEqual(TEXT("An observer arriving after ramp uses its existing phase"), HALBullChargeFX::Ramp(4, 2), 1.f);
	TestEqual(TEXT("Stopped vehicle has no headwind even while boosting"), HALBullChargeFX::WindStrength(0, 3000, 1, true), 0.f);
	TestEqual(TEXT("Stun/air/roll suppress headwind independently of speed"), HALBullChargeFX::WindStrength(3000, 3000, 1, false), 0.f);
	TestEqual(TEXT("Backward movement cannot produce frontal headwind"), HALBullChargeFX::WindStrength(-300, 3000, 1, true), 0.f);
	TestEqual(TEXT("Activating at full road speed starts invisibly rather than at 65 percent"), HALBullChargeFX::WindStrength(3000, 3000, 0, true), 0.f);
	TestEqual(TEXT("Halfway through acceleration gives half the full-speed envelope"), HALBullChargeFX::WindStrength(3000, 3000, HALBullChargeFX::Ramp(1, 2), true), .5f);
	TestEqual(TEXT("No acceleration interval intentionally gives immediate full phase"), HALBullChargeFX::Ramp(0, 0), 1.f);
	TestEqual(TEXT("High speed is bounded"), HALBullChargeFX::WindStrength(9000, 3000, 1, true), 1.f);
	TestTrue(TEXT("Mid speed retains a strong readable airflow"), HALBullChargeFX::WindStrength(750, 3000, 1, true) >= .5f);
	float At30 = 0, At120 = 0;
	for (int32 I = 0; I < 30; ++I) { At30 = HALBullChargeFX::SmoothWind(At30, 1, 1.f / 30, .1f, true); }
	for (int32 I = 0; I < 120; ++I) { At120 = HALBullChargeFX::SmoothWind(At120, 1, 1.f / 120, .1f, true); }
	TestTrue(TEXT("Visual response agrees at 30 and 120 fps"), FMath::IsNearlyEqual(At30, At120, .0001f));
	TestEqual(TEXT("Drive suspension hides wind immediately rather than leaving a smoothing ghost"), HALBullChargeFX::SmoothWind(1, 1, .016f, .1f, false), 0.f);
	for (int32 Angle = 0; Angle < 360; Angle += 15)
	{
		const FVector D = FRotator(19, Angle, 13).RotateVector(-FVector::ForwardVector);
		FVector Min, Max; HALBullChargeFX::VelocityBox(D, 1600, 70, Min, Max);
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector V(Corner & 1 ? Min.X : Max.X, Corner & 2 ? Min.Y : Max.Y, Corner & 4 ? Min.Z : Max.Z);
			TestTrue(TEXT("Every world-space velocity box corner remains behind/outward"), FVector::DotProduct(V, D) > 0);
		}
	}
	auto Invalid = Defaults; Invalid.RearMounts.SetNum(5);
	TestFalse(TEXT("Rear emitter count is bounded"), Invalid.IsValid());
	Invalid = Defaults; Invalid.FlashSize = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite art settings are rejected"), Invalid.IsValid());
	Invalid = Defaults; Invalid.WindLengthScale = 0;
	TestFalse(TEXT("Collapsed wind length is rejected"), Invalid.IsValid());
	Invalid.WindLengthScale = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite wind length is rejected"), Invalid.IsValid());
	Invalid.WindLengthScale = 4;
	TestTrue(TEXT("Maximum independent length remains valid"), Invalid.IsValid());
	Invalid.WindLengthScale = 4.1f;
	TestFalse(TEXT("Independent length is bounded"), Invalid.IsValid());
	Invalid = Defaults; Invalid.PressureShadowThreshold = .8f;
	TestFalse(TEXT("Inverted shell tone thresholds are rejected"), Invalid.IsValid());
	Invalid = Defaults; Invalid.PressureToneSoftness = 0;
	TestFalse(TEXT("Shell transitions retain a nonzero soft width"), Invalid.IsValid());
	Invalid = Defaults; Invalid.PressureMidColor.R = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite shell colors are rejected"), Invalid.IsValid());
	auto* Skill = NewObject<UHALBullChargeConfig>(); Skill->FX = Invalid;
	FString Error;
	TestTrue(TEXT("Invalid cosmetic tuning cannot disable the gameplay ability"), Skill->Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleFXMountTest, "HAL.FutureCreation.BullCharge.FX.SocketMountAndReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALVehicleFXMountTest::RunTest(const FString&)
{
	auto* Source = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Engine test mesh"), Source)) { return false; }
	auto* Asset = DuplicateObject<UStaticMesh>(Source, GetTransientPackage());
	auto* Socket = NewObject<UStaticMeshSocket>(Asset); Socket->SocketName = TEXT("Exhaust");
	Socket->RelativeLocation = FVector(-35, 20, 8); Socket->RelativeRotation = FRotator(0, 40, 0); Asset->Sockets.Add(Socket);
	auto* Mesh = NewObject<UStaticMeshComponent>(); Mesh->SetStaticMesh(Asset);
	Mesh->SetWorldTransform(FTransform(FRotator(0, 30, 0), FVector(300, 100, 20), FVector(1.2, 1.2, 1.2)));
	FHALVehicleFXTargetSnapshot Targets; auto& T = Targets.Targets.AddDefaulted_GetRef(); T.Id = TEXT("Body"); T.Mesh = Mesh;
	FHALVehicleFXMount Mount; Mount.SocketOrBone = TEXT("Exhaust"); Mount.Offset.SetLocation(FVector(3, 4, 5));
	FHALResolvedFXMount Out;
	TestTrue(TEXT("A named static socket resolves on the selected presentation part"), HALVehicleFX::ResolveMount(Targets, Mount, Out));
	TestTrue(TEXT("Socket and local transform are both applied"), Out.World.Equals(Mount.Offset * Mesh->GetSocketTransform(TEXT("Exhaust")), .001));
	Mount.SocketOrBone = TEXT("Missing");
	TestFalse(TEXT("Missing named socket is not silently attached to root"), HALVehicleFX::ResolveMount(Targets, Mount, Out));
	Mount.bFallbackToMeshOrigin = true;
	TestTrue(TEXT("Explicit fallback is usable and reported"), HALVehicleFX::ResolveMount(Targets, Mount, Out) && Out.bUsedFallback && Out.Socket.IsNone());
	auto* Replacement = NewObject<UStaticMeshComponent>(); Replacement->SetStaticMesh(Asset); Replacement->SetWorldLocation(FVector(700, 0, 0));
	T.Mesh = Replacement;
	TestTrue(TEXT("A fresh query binds the replacement mesh"), HALVehicleFX::ResolveMount(Targets, Mount, Out) && Out.Mesh == Replacement);
	Mount.PartId = TEXT("UnregisteredPart");
	TestFalse(TEXT("Missing part cannot fall back to a collision root"), HALVehicleFX::ResolveMount(Targets, Mount, Out));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBullChargeFXEventsTest, "HAL.FutureCreation.BullCharge.FX.EventOrderingAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBullChargeFXEventsTest::RunTest(const FString&)
{
	const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).SetTransactional(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* Owner = W->SpawnActor<AActor>(); auto* C = NewObject<UHALBullChargeComponent>(Owner); C->RegisterComponent();
	auto* Skill = NewObject<UHALBullChargeConfig>(); Skill->FX.bEnabled = true;
	FHALBullChargeImpactFXEvent Event; Event.Activation = FGuid::NewGuid(); Event.Sequence = 1; Event.ServerTime = W->GetTimeSeconds();
	const FGuid Activation = Event.Activation;
	FHALBullChargeFXTestAccess::Receive(*C, Event);
	TestEqual(TEXT("An RPC preceding persistent activation is held in a bounded queue"), FHALBullChargeFXTestAccess::Pending(*C), 1);
	FHALBullChargeFXTestAccess::State(*C, Skill, Event.Activation);
	TestEqual(TEXT("Activation consumes the early event exactly once"), FHALBullChargeFXTestAccess::Count(*C), uint32(1));
	FHALBullChargeFXTestAccess::Receive(*C, Event);
	TestEqual(TEXT("Duplicate network event cannot flash twice"), FHALBullChargeFXTestAccess::Count(*C), uint32(1));
	Event.Sequence = 2; Event.ServerTime -= 1;
	FHALBullChargeFXTestAccess::Receive(*C, Event);
	TestEqual(TEXT("Stale burst is rejected"), FHALBullChargeFXTestAccess::Count(*C), uint32(1));
	Event.ServerTime = W->GetTimeSeconds(); Event.Activation = FGuid::NewGuid();
	FHALBullChargeFXTestAccess::Receive(*C, Event);
	TestEqual(TEXT("A different activation cannot inject a burst"), FHALBullChargeFXTestAccess::Count(*C), uint32(1));
	FHALBullChargeFXTestAccess::State(*C, nullptr, FGuid());
	TestFalse(TEXT("Retiring the activation disables cosmetic tick"), C->IsComponentTickEnabled());
	TestEqual(TEXT("Retirement clears pending events"), FHALBullChargeFXTestAccess::Pending(*C), 0);
	Event.Activation = Activation; Event.Sequence = 3;
	FHALBullChargeFXTestAccess::Receive(*C, Event);
	TestEqual(TEXT("A late event from the retired activation cannot repopulate the queue"), FHALBullChargeFXTestAccess::Pending(*C), 0);
	return true;
}
#endif
