#pragma once

#include "HALVehicleCollisionGeneratorSettings.h"

class UStaticMesh;

namespace HALVehicleCollision
{
	struct FGeometry
	{
		TArray<FVector2D> Outline;
		TArray<FVector2D> SideOutline;
		TArray<FVector> Vertices;
		TArray<FIntVector> Triangles;
	};

	bool BuildGeometry(const FHALVehicleCollisionParameters& Parameters, FGeometry& Out, FString& Error);
	/** Only fills a new mesh. No asset lookup, registry, file writes or gameplay configuration changes. */
	bool BuildMesh(UStaticMesh& Mesh, const FHALVehicleCollisionParameters& Parameters, FString& Error);
	bool ReadRecipe(UStaticMesh& Mesh, FHALVehicleCollisionParameters& Out, FString& Error);
	/** Accepts only editable /Game meshes with a supported generator recipe and one source LOD. */
	bool CanUpdateAsset(UStaticMesh& Mesh, FString& Error);
	/** Re-measures wheel bottoms for every Vehicle Definition currently using this mesh. */
	bool ResolveUpdateAnchor(UStaticMesh& Mesh, FHALVehicleCollisionParameters& Parameters, FString& Error);
	/** Rebuilds the same asset in memory after staging/cooking; preserves references. Does not save. */
	bool UpdateAsset(UStaticMesh& Mesh, const FHALVehicleCollisionParameters& Parameters, FString& Error);
	bool ValidateOutput(const FString& Folder, const FString& Name, FString& Error);
	/** Creates a unique unsaved /Game asset. Saving and assigning to a VehicleDefinition are explicit user actions. */
	UStaticMesh* CreateAsset(const UHALVehicleCollisionGeneratorSettings& Settings, FString& Error);
}
