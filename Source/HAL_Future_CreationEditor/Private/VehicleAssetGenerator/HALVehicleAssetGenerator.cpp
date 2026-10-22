#include "VehicleAssetGenerator/HALVehicleAssetGenerator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "HALVehicleAnimInstance.h"
#include "HALVehiclePresentationConfig.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "TestVehiclePawn.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "VehicleDefinition.h"

namespace
{
	bool Fail(FString& Error, const FString& Message) { Error = Message; return false; }
	FString ObjectPath(const FString& Package) { return Package + TEXT(".") + FPackageName::GetLongPackageAssetName(Package); }
	bool IsOccupied(const FString& Package)
	{
		return FindPackage(nullptr, *Package) || FPackageName::DoesPackageExist(Package)
			|| FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get()
				.GetAssetByObjectPath(FSoftObjectPath(ObjectPath(Package))).IsValid();
	}
	bool CanGenerate(FString& Error)
	{
		return (!GEditor || !GEditor->PlayWorld) && !IsRunningGame()
			? true : Fail(Error, TEXT("End PIE / simulation before creating or saving vehicle assets."));
	}
	bool Compile(UBlueprint* Blueprint, FString& Error)
	{
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
		if (Results.NumErrors || Blueprint->Status == BS_Error || !Blueprint->GeneratedClass)
		{
			return Fail(Error, FString::Printf(TEXT("Compilation failed for %s. See the Blueprint compiler log."), *Blueprint->GetName()));
		}
		return true;
	}
}

bool HALVehicleAssets::BuildOutputPaths(const FString& MeshName, FOutputPaths& Out, FString& Error)
{
	Out = {};
	Error.Reset();
	if (!MeshName.StartsWith(TEXT("SKM_"), ESearchCase::CaseSensitive) || MeshName.Len() == 4)
	{
		return Fail(Error, TEXT("Select a Skeletal Mesh named SKM_<model>."));
	}
	Out.Model = MeshName.Mid(4);
	Out.Presentation = TEXT("/Game/Data/Car_Presentation/DA_") + Out.Model + TEXT("_Presentation");
	Out.VehicleDefinition = TEXT("/Game/Data/MVP_B/DA_Vehicle_") + Out.Model;
	Out.AnimBlueprint = TEXT("/Game/Blueprints/AnimBP/ABP_") + Out.Model;
	Out.VehicleBlueprint = TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_") + Out.Model;
	for (const FString& Package : Out.Packages())
	{
		FText Reason;
		const FString Name = FPackageName::GetLongPackageAssetName(Package);
		// Keep a single model token: slashes must not introduce additional directories.
		if (Out.Model.Contains(TEXT("/")) || Out.Model.Contains(TEXT("\\"))
			|| !FPackageName::IsValidLongPackageName(Package, false, &Reason)
			|| !FName::IsValidXName(Name, INVALID_OBJECTNAME_CHARACTERS, &Reason))
		{
			Out = {};
			return Fail(Error, TEXT("The model name contains invalid asset-name characters."));
		}
	}
	return true;
}

UVehicleDefinition* HALVehicleAssets::ReadDefinition(const UBlueprint* Blueprint)
{
	const UClass* Class = Blueprint ? Blueprint->GeneratedClass : nullptr;
	if (!Class || !Class->IsChildOf(ATestVehiclePawn::StaticClass())) { return nullptr; }
	const auto* Property = FindFProperty<FObjectPropertyBase>(Class, TEXT("Definition"));
	return Property ? Cast<UVehicleDefinition>(Property->GetObjectPropertyValue_InContainer(Class->GetDefaultObject())) : nullptr;
}

bool HALVehicleAssets::Validate(USkeletalMesh* Mesh, UBlueprint* BasePawn, UVehicleDefinition* BaseDefinition, FOutputPaths& Out, FString& Error)
{
	Out = {};
	if (!CanGenerate(Error)) { return false; }
	if (!IsValid(Mesh) || !BuildOutputPaths(Mesh->GetName(), Out, Error))
	{
		if (!Mesh) { Error = TEXT("Select a Skeletal Mesh named SKM_<model>."); }
		return false;
	}
	if (!Mesh->GetSkeleton()) { return Fail(Error, TEXT("The selected mesh has no Skeleton.")); }
	if (!IsValid(BasePawn) || !BasePawn->GeneratedClass || BasePawn->BlueprintType != BPTYPE_Normal
		|| !BasePawn->GeneratedClass->IsChildOf(ATestVehiclePawn::StaticClass())
		|| (BasePawn->Status != BS_UpToDate && BasePawn->Status != BS_UpToDateWithWarnings))
	{
		return Fail(Error, TEXT("Select a compiled vehicle Blueprint derived from TestVehiclePawn."));
	}
	if (!IsValid(BaseDefinition) || BaseDefinition->CollisionShape != EVehicleCollisionShape::ConvexMesh)
	{
		return Fail(Error, TEXT("Select a driving VehicleDefinition with ConvexMesh collision."));
	}
	FString ConfigError;
	if (!BaseDefinition->Validate(ConfigError)) { return Fail(Error, TEXT("Base Vehicle Definition: ") + ConfigError); }
	for (const FString& Package : Out.Packages())
	{
		if (IsOccupied(Package)) { return Fail(Error, TEXT("Output already exists; the complete generation is blocked: ") + Package); }
	}
	return true;
}

bool HALVehicleAssets::Create(USkeletalMesh* Mesh, UBlueprint* BasePawn, UVehicleDefinition* BaseDefinition, FGeneratedAssets& Out, FString& Error)
{
	check(IsInGameThread());
	if (!Out.Assets.IsEmpty()) { return Fail(Error, TEXT("Keep or discard the previous generated set before creating another.")); }
	FOutputPaths Paths;
	if (!Validate(Mesh, BasePawn, BaseDefinition, Paths, Error)) { return false; }
	Out.Paths = Paths;
	const auto Abort = [&]() { DiscardUnsaved(Out); return false; };
	const auto MakePackage = [](const FString& Name) { return CreatePackage(*Name); };
	const auto NameOf = [](const FString& Package) { return FName(*FPackageName::GetLongPackageAssetName(Package)); };

	TStrongObjectPtr<UAnimBlueprintFactory> Factory(NewObject<UAnimBlueprintFactory>());
	Factory->ParentClass = UHALVehicleAnimInstance::StaticClass();
	Factory->TargetSkeleton = Mesh->GetSkeleton();
	Factory->PreviewSkeletalMesh = Mesh;
	Factory->bTemplate = false;
	UPackage* AnimPackage = MakePackage(Paths.AnimBlueprint);
	UAnimBlueprint* Anim = Cast<UAnimBlueprint>(Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(), AnimPackage,
		NameOf(Paths.AnimBlueprint), RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
	if (!Anim) { Fail(Error, TEXT("Unable to create the Animation Blueprint.")); return Abort(); }
	Out.Assets.Emplace(Anim);
	// The factory's default graph is left untouched: no animation nodes or wiring are generated.
	if (!Compile(Anim, Error)) { return Abort(); }

	auto* Presentation = NewObject<UHALVehiclePresentationConfig>(MakePackage(Paths.Presentation), NameOf(Paths.Presentation),
		RF_Public | RF_Standalone | RF_Transactional);
	Out.Assets.Emplace(Presentation);
	Presentation->Mesh = Mesh;
	Presentation->AnimClass = Anim->GeneratedClass;
	Presentation->bReadyForUse = false;

	// Duplicate only this DA. External LocalConfig, collision, visual and material assets remain references.
	auto* Definition = DuplicateObject<UVehicleDefinition>(BaseDefinition, MakePackage(Paths.VehicleDefinition), NameOf(Paths.VehicleDefinition));
	if (!Definition) { Fail(Error, TEXT("Unable to copy the base VehicleDefinition.")); return Abort(); }
	Definition->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
	Definition->ClearFlags(RF_Transient);
	Out.Assets.Emplace(Definition);
	Definition->Presentation = Presentation;
	Definition->bReadyForUse = false;

	UBlueprint* Pawn = FKismetEditorUtilities::CreateBlueprint(BasePawn->GeneratedClass, MakePackage(Paths.VehicleBlueprint),
		NameOf(Paths.VehicleBlueprint), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!Pawn) { Fail(Error, TEXT("Unable to create the vehicle Blueprint.")); return Abort(); }
	Out.Assets.Emplace(Pawn);
	UObject* Defaults = Pawn->GeneratedClass ? Pawn->GeneratedClass->GetDefaultObject() : nullptr;
	auto* DefinitionProperty = FindFProperty<FObjectPropertyBase>(ATestVehiclePawn::StaticClass(), TEXT("Definition"));
	auto* SourceProperty = FindFProperty<FEnumProperty>(ATestVehiclePawn::StaticClass(), TEXT("ConfigurationSource"));
	if (!Defaults || !DefinitionProperty || !SourceProperty)
	{
		Fail(Error, TEXT("The vehicle configuration properties are unavailable."));
		return Abort();
	}
	Defaults->Modify();
	DefinitionProperty->SetObjectPropertyValue_InContainer(Defaults, Definition);
	SourceProperty->GetUnderlyingProperty()->SetIntPropertyValue(SourceProperty->ContainerPtrToValuePtr<void>(Defaults),
		int64(EVehicleConfigurationSource::Definition));
	FBlueprintEditorUtils::MarkBlueprintAsModified(Pawn);
	if (!Compile(Pawn, Error)) { return Abort(); }
	const auto* VehicleDefaults = Cast<ATestVehiclePawn>(Pawn->GeneratedClass->GetDefaultObject());
	if (ReadDefinition(Pawn) != Definition || !VehicleDefaults || VehicleDefaults->GetConfigurationSource() != EVehicleConfigurationSource::Definition)
	{
		Fail(Error, TEXT("The generated vehicle Blueprint did not retain its Definition binding."));
		return Abort();
	}
	for (const auto& Asset : Out.Assets)
	{
		Asset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Asset.Get());
	}
	Out.bPublished = true;
	return true;
}

bool HALVehicleAssets::Save(FGeneratedAssets& Generated, FString& Status)
{
	if (!CanGenerate(Status)) { return false; }
	if (Generated.Assets.Num() != 4 || !Generated.bPublished) { return Fail(Status, TEXT("Create a complete asset set first.")); }
	// Recheck every destination before the first write, including races since Create.
	for (const auto& Asset : Generated.Assets)
	{
		const FString Name = Asset->GetOutermost()->GetName();
		if (!Generated.SavedPackages.Contains(Name) && FPackageName::DoesPackageExist(Name))
		{
			return Fail(Status, TEXT("An external file now occupies an output. No packages were saved: ") + Name);
		}
	}
	TArray<FString> Failures;
	for (const auto& Asset : Generated.Assets)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString Name = Package->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(Name, FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
		{
			Failures.Add(TEXT("Cannot create output directory: ") + Name);
			continue;
		}
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		if (UPackage::SavePackage(Package, Asset.Get(), *Filename, Args)) { Generated.SavedPackages.Add(Name); }
		else { Failures.Add(TEXT("Save failed: ") + Name); }
	}
	Status = FString::Printf(TEXT("Saved %d/4 generated packages. Both DAs remain not ready; complete AnimGraph and calibrate the vehicle before use."), Generated.SavedPackages.Num());
	if (!Failures.IsEmpty()) { Status += TEXT("\n") + FString::Join(Failures, TEXT("\n")); }
	return Failures.IsEmpty();
}

bool HALVehicleAssets::DiscardUnsaved(FGeneratedAssets& Generated)
{
	if (!Generated.SavedPackages.IsEmpty()) { return false; }
	for (const auto& Asset : Generated.Assets)
	{
		if (FPackageName::DoesPackageExist(Asset->GetOutermost()->GetName())) { return false; }
	}
	const FString DiscardRoot = TEXT("/Temp/HALVehicleAssetGenerator/") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	for (const auto& Asset : Generated.Assets)
	{
		if (Generated.bPublished) { FAssetRegistryModule::AssetDeleted(Asset.Get()); }
		UPackage* Package = Asset->GetOutermost();
		ForEachObjectWithPackage(Package, [](UObject* Object) {
			Object->ClearFlags(RF_Public | RF_Standalone);
			Object->SetFlags(RF_Transient);
			return true;
		}, EGetObjectsFlags::IncludeNestedObjects);
		const FString NewName = DiscardRoot / FPackageName::GetLongPackageAssetName(Package->GetName());
		Package->Rename(*NewName, nullptr, REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
		Package->SetDirtyFlag(false);
	}
	Generated.Assets.Reset();
	Generated.Paths = {};
	Generated.bPublished = false;
	return true;
}
