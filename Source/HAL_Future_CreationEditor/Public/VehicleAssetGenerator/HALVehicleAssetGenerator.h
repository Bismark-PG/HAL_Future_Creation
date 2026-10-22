#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UBlueprint;
class USkeletalMesh;
class UVehicleDefinition;
class SWidget;

namespace HALVehicleAssets
{
	struct FOutputPaths
	{
		FString Model;
		FString Presentation;
		FString VehicleDefinition;
		FString AnimBlueprint;
		FString VehicleBlueprint;
		TArray<FString> Packages() const { return { AnimBlueprint, Presentation, VehicleDefinition, VehicleBlueprint }; }
	};

	/** Only the four newly generated assets. Strong ownership survives tab refreshes and GC. */
	struct FGeneratedAssets
	{
		FOutputPaths Paths;
		TArray<TStrongObjectPtr<UObject>> Assets;
		TSet<FString> SavedPackages;
		bool bPublished = false;
	};

	bool BuildOutputPaths(const FString& MeshName, FOutputPaths& Out, FString& Error);
	UVehicleDefinition* ReadDefinition(const UBlueprint* Blueprint);
	bool Validate(USkeletalMesh* Mesh, UBlueprint* BasePawn, UVehicleDefinition* BaseDefinition, FOutputPaths& Out, FString& Error);
	/** Creates unsaved assets and publishes them only after all links and both compilations succeed. */
	bool Create(USkeletalMesh* Mesh, UBlueprint* BasePawn, UVehicleDefinition* BaseDefinition, FGeneratedAssets& Out, FString& Error);
	/** Saves just this generation; never saves source/shared packages. Partial save results are retained for retry. */
	bool Save(FGeneratedAssets& Generated, FString& Status);
	/** Cleanup of this generation's unsaved packages only; refuses any package with a disk file. */
	bool DiscardUnsaved(FGeneratedAssets& Generated);

	TSharedRef<SWidget> MakePanel();
}
