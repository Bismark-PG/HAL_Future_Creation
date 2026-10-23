#if WITH_DEV_AUTOMATION_TESTS

#include "VehicleAssetGenerator/HALVehicleAssetGenerator.h"
#include "Animation/AnimClassInterface.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Components/ActorComponent.h"
#include "HALVehicleAnimInstance.h"
#include "HALVehiclePresentationConfig.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Physics/NetworkPhysicsSettingsComponent.h"
#include "TestVehiclePawn.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "VehicleDefinition.h"

namespace
{
	struct FGeneratorFixture
	{
		TStrongObjectPtr<UVehicleLocalConfig> SharedLocal { LoadObject<UVehicleLocalConfig>(nullptr, HALVehicleAssets::SharedLocalConfigPath) };
		TStrongObjectPtr<UNetworkPhysicsSettingsDataAsset> SharedNetwork { LoadObject<UNetworkPhysicsSettingsDataAsset>(nullptr, TEXT("/Game/Data/Network/DA_NetworkPhysics_MVPB.DA_NetworkPhysics_MVPB")) };
		TStrongObjectPtr<USkeletalMesh> Source { LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Assets/Cars/SKM_Car_01.SKM_Car_01")) };
		TStrongObjectPtr<USkeletalMesh> Mesh { NewObject<USkeletalMesh>(GetTransientPackage(),
			FName(*(TEXT("SKM_HALGeneratorTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)))) };
		HALVehicleAssets::FGeneratedAssets Generated;
		FGeneratorFixture() { if (Source.IsValid()) { Mesh->SetSkeleton(Source->GetSkeleton()); } }
		~FGeneratorFixture() { HALVehicleAssets::DiscardUnsaved(Generated); }
	};
	UObject* ObjectProperty(const UObject* Object, FName Name)
	{
		const auto* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
		return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleAssetNamingTest, "HAL.Editor.VehicleAssetGenerator.Naming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALVehicleAssetNamingTest::RunTest(const FString&)
{
	HALVehicleAssets::FOutputPaths Paths;
	FString Error;
	TestTrue(TEXT("Model keeps all suffix underscores"), HALVehicleAssets::BuildOutputPaths(TEXT("SKM_Car_02"), Paths, Error));
	TestEqual(TEXT("Animation path"), Paths.AnimBlueprint, FString(TEXT("/Game/Blueprints/AnimBP/ABP_Car_02")));
	TestEqual(TEXT("Presentation path"), Paths.Presentation, FString(TEXT("/Game/Data/Car_Presentation/DA_Car_02_Presentation")));
	TestEqual(TEXT("Definition path"), Paths.VehicleDefinition, FString(TEXT("/Game/Data/MVP_B/DA_Vehicle_Car_02")));
	TestEqual(TEXT("Vehicle Blueprint path"), Paths.VehicleBlueprint, FString(TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_02")));
	for (const FString Name : { TEXT("SK_Car"), TEXT("skm_Car"), TEXT("SKM_"), TEXT("SKM_Bad/Path"), TEXT("SKM_Bad\\Path"), TEXT("SKM_Bad.Name"), TEXT("SKM_Bad Name") })
	{
		TestFalse(TEXT("Invalid model name rejected"), HALVehicleAssets::BuildOutputPaths(Name, Paths, Error));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleAssetGenerationTest, "HAL.Editor.VehicleAssetGenerator.ReferenceChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALVehicleAssetGenerationTest::RunTest(const FString&)
{
	FGeneratorFixture F;
	if (!TestTrue(TEXT("Mesh and shared Local/network DAs loaded without an existing vehicle Definition or Blueprint"), F.SharedLocal.IsValid() && F.SharedNetwork.IsValid() && F.Source.IsValid())) { return false; }
	const bool bLocalDirty = F.SharedLocal->GetOutermost()->IsDirty();
	const bool bNetworkDirty = F.SharedNetwork->GetOutermost()->IsDirty();
	const auto* NativeDefaults = GetDefault<ATestVehiclePawn>();
	UObject* NativeNetwork = ObjectProperty(NativeDefaults, TEXT("NetworkPhysicsSettings"));
	UObject* OriginalNativeNetworkAsset = ObjectProperty(NativeNetwork, TEXT("SettingsDataAsset"));
	FString Error;
	if (!TestTrue(TEXT("Generate complete unsaved chain without a source vehicle Definition or Blueprint"), HALVehicleAssets::Create(F.Mesh.Get(), F.SharedNetwork.Get(), F.Generated, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Exactly four assets"), F.Generated.Assets.Num(), 4);
	UAnimBlueprint* Anim = Cast<UAnimBlueprint>(F.Generated.Assets[0].Get());
	auto* Presentation = Cast<UHALVehiclePresentationConfig>(F.Generated.Assets[1].Get());
	auto* Definition = Cast<UVehicleDefinition>(F.Generated.Assets[2].Get());
	UBlueprint* Pawn = Cast<UBlueprint>(F.Generated.Assets[3].Get());
	if (!TestTrue(TEXT("Expected asset types"), Anim && Presentation && Definition && Pawn)) { return false; }
	TestTrue(TEXT("HAL animation parent"), Anim->ParentClass == UHALVehicleAnimInstance::StaticClass());
	TestTrue(TEXT("Exact target Skeleton"), Anim->TargetSkeleton == F.Mesh->GetSkeleton());
	TestTrue(TEXT("Mesh and generated animation class bound"), Presentation->Mesh == F.Mesh.Get() && Presentation->AnimClass == Anim->GeneratedClass);
	const IAnimClassInterface* Animation = IAnimClassInterface::GetFromClass(Anim->GeneratedClass);
	TestTrue(TEXT("Compiled class targets same Skeleton"), Animation && Animation->GetTargetSkeleton() == F.Mesh->GetSkeleton());
	TestTrue(TEXT("Presentation bound and not ready"), Definition->Presentation == Presentation && !Presentation->bReadyForUse && !Definition->bReadyForUse);
	TestTrue(TEXT("Exact shared Local configuration bound"), Definition->LocalConfig == F.SharedLocal.Get());
	TestEqual(TEXT("Project Local configuration path"), Definition->LocalConfig->GetPathName(), FString(TEXT("/Game/Data/MVP_B/DA_Local_TestBaseline_v1.DA_Local_TestBaseline_v1")));
	TestNull(TEXT("Collision mesh awaits model-specific configuration"), Definition->CollisionMesh.Get());
	TestNull(TEXT("Fallback visual awaits model-specific configuration"), Definition->Visual.Mesh.Get());
	const auto* NativeDefinition = GetDefault<UVehicleDefinition>();
	for (const FName Field : { TEXT("Physics"), TEXT("Movement"), TEXT("BallControl"), TEXT("BallControlPoint"),
		TEXT("Health"), TEXT("Combat"), TEXT("Impact"), TEXT("Visual") })
	{
		const auto* Property = FindFProperty<FProperty>(UVehicleDefinition::StaticClass(), Field);
		TestTrue(*FString::Printf(TEXT("%s uses native initial values"), *Field.ToString()),
			Property && Property->Identical_InContainer(Definition, NativeDefinition));
	}
	TestTrue(TEXT("Player Pawn directly inherits the native vehicle class"), Pawn->ParentClass == ATestVehiclePawn::StaticClass());
	TestTrue(TEXT("Vehicle links new Definition"), HALVehicleAssets::ReadDefinition(Pawn) == Definition);
	auto* Defaults = Cast<ATestVehiclePawn>(Pawn->GeneratedClass->GetDefaultObject());
	TestTrue(TEXT("Definition mode retained after compile"), Defaults && Defaults->GetConfigurationSource() == EVehicleConfigurationSource::Definition);
	UObject* ChildNetwork = ObjectProperty(Defaults, TEXT("NetworkPhysicsSettings"));
	TestTrue(TEXT("Selected shared network settings retained after compile"), ChildNetwork && ObjectProperty(ChildNetwork, TEXT("SettingsDataAsset")) == F.SharedNetwork.Get());
	TestTrue(TEXT("Independent network component template"), ChildNetwork != NativeNetwork);
	TestTrue(TEXT("Network replication and Resimulation configured"), Defaults && Defaults->GetIsReplicated() && Defaults->GetPhysicsReplicationMode() == EPhysicsReplicationMode::Resimulation);
	for (const FName Field : { TEXT("CollisionRoot"), TEXT("VisualMesh"), TEXT("SkeletalVisual"), TEXT("VehiclePresentation"),
		TEXT("ForwardArrow"), TEXT("ArcadeMovement"), TEXT("VehicleRecovery"), TEXT("NetworkPhysicsSettings"), TEXT("NetworkPhysicsHistory"),
		TEXT("MovementEffectBridge"), TEXT("BallControlPoint"), TEXT("BallConstraint"), TEXT("BallControl"), TEXT("Health"),
		TEXT("ImpactReceiver"), TEXT("CameraBoom"), TEXT("TopDownCamera") })
	{
		UObject* Component = ObjectProperty(Defaults, Field);
		UObject* NativeComponent = ObjectProperty(NativeDefaults, Field);
		TestTrue(*FString::Printf(TEXT("Complete native player component: %s"), *Field.ToString()),
			Component && NativeComponent && Component->IsA<UActorComponent>() && Component->GetClass() == NativeComponent->GetClass());
	}
	UEdGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : Anim->FunctionGraphs) { if (Graph->GetFName() == TEXT("AnimGraph")) { AnimGraph = Graph; } }
	if (TestNotNull(TEXT("Default AnimGraph exists"), AnimGraph))
	{
		TestEqual(TEXT("Only factory default output node"), AnimGraph->Nodes.Num(), 1);
		for (const UEdGraphNode* Node : AnimGraph->Nodes)
		{
			TestEqual(TEXT("Factory root node only"), Node->GetClass()->GetName(), FString(TEXT("AnimGraphNode_Root")));
			for (const UEdGraphPin* Pin : Node->Pins) { TestTrue(TEXT("No animation wiring"), Pin->LinkedTo.IsEmpty()); }
		}
	}
	FGeneratorFixture OtherModel;
	if (!TestTrue(TEXT("Second model generates independently"), HALVehicleAssets::Create(OtherModel.Mesh.Get(), F.SharedNetwork.Get(), OtherModel.Generated, Error)))
	{
		AddError(Error);
		return false;
	}
	auto* OtherDefinition = Cast<UVehicleDefinition>(OtherModel.Generated.Assets[2].Get());
	UBlueprint* OtherPawn = Cast<UBlueprint>(OtherModel.Generated.Assets[3].Get());
	if (!TestNotNull(TEXT("Second model owns a Vehicle Definition"), OtherDefinition)) { return false; }
	TestTrue(TEXT("Each Pawn references its own model Definition"), Definition != OtherDefinition && HALVehicleAssets::ReadDefinition(OtherPawn) == OtherDefinition);
	TestTrue(TEXT("Model presentations are independent and Local stays shared"), Definition->Presentation != OtherDefinition->Presentation && OtherDefinition->LocalConfig == F.SharedLocal.Get());
	const float OtherSpeed = OtherDefinition->Movement.Drive.MaxForwardSpeed;
	Definition->Movement.Drive.MaxForwardSpeed += 123;
	TestEqual(TEXT("Editing one model's driving values does not change the other model"), OtherDefinition->Movement.Drive.MaxForwardSpeed, OtherSpeed);
	TestEqual(TEXT("Shared Local DA dirty flag unchanged"), F.SharedLocal->GetOutermost()->IsDirty(), bLocalDirty);
	TestEqual(TEXT("Shared network DA dirty flag unchanged"), F.SharedNetwork->GetOutermost()->IsDirty(), bNetworkDirty);
	TestTrue(TEXT("Native CDO network settings unchanged"), ObjectProperty(NativeNetwork, TEXT("SettingsDataAsset")) == OriginalNativeNetworkAsset);
	for (const FString& Path : F.Generated.Paths.Packages()) { TestFalse(TEXT("No generated disk assets"), FPackageName::DoesPackageExist(Path)); }
	TestTrue(TEXT("Unsaved fixture cleanup"), HALVehicleAssets::DiscardUnsaved(F.Generated));
	HALVehicleAssets::FOutputPaths Paths;
	TestTrue(TEXT("Names reusable after unsaved cleanup"), HALVehicleAssets::Validate(F.Mesh.Get(), F.SharedNetwork.Get(), Paths, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleAssetConflictTest, "HAL.Editor.VehicleAssetGenerator.Preflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALVehicleAssetConflictTest::RunTest(const FString&)
{
	FGeneratorFixture F;
	if (!TestTrue(TEXT("Mesh and shared Local/network DAs loaded"), F.SharedLocal.IsValid() && F.SharedNetwork.IsValid() && F.Source.IsValid())) { return false; }
	HALVehicleAssets::FOutputPaths Paths;
	FString Error;
	TestFalse(TEXT("Missing Mesh rejected"), HALVehicleAssets::Validate(nullptr, F.SharedNetwork.Get(), Paths, Error));
	TestFalse(TEXT("Missing shared network DA rejected"), HALVehicleAssets::Validate(F.Mesh.Get(), nullptr, Paths, Error));
	TStrongObjectPtr<USkeletalMesh> NoSkeleton(NewObject<USkeletalMesh>(GetTransientPackage(), TEXT("SKM_NoSkeleton")));
	TestFalse(TEXT("Missing Skeleton rejected"), HALVehicleAssets::Validate(NoSkeleton.Get(), F.SharedNetwork.Get(), Paths, Error));
	if (!TestTrue(TEXT("Paths generated"), HALVehicleAssets::BuildOutputPaths(F.Mesh->GetName(), Paths, Error))) { return false; }
	// Occupy only the last output. Preflight must reject before creating the other three packages.
	UPackage* Conflict = CreatePackage(*Paths.VehicleBlueprint);
	TestFalse(TEXT("Any occupied output blocks entire generation"), HALVehicleAssets::Create(F.Mesh.Get(), F.SharedNetwork.Get(), F.Generated, Error));
	TestTrue(TEXT("No partial result"), F.Generated.Assets.IsEmpty());
	for (const FString& Path : { Paths.AnimBlueprint, Paths.Presentation, Paths.VehicleDefinition })
	{
		TestNull(TEXT("No partial package created"), FindPackage(nullptr, *Path));
	}
	Conflict->Rename(*(TEXT("/Temp/HALGeneratorConflict_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)), nullptr, REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
	TestFalse(TEXT("No empty generation saved"), HALVehicleAssets::Save(F.Generated, Error));
	return true;
}

#endif
