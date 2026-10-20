#pragma once

#include "HALVehicleCollisionGeneratorSettings.h"

class UStaticMesh;

namespace HALVehicleCollision
{
	struct FGeometry
	{
		TArray<FVector2D> Outline;
		TArray<FVector> Vertices;
		TArray<FIntVector> Triangles;
	};

	bool BuildGeometry(const FHALVehicleCollisionParameters& Parameters, FGeometry& Out, FString& Error);
	/** Only fills a new mesh. No asset lookup, registry, file writes or gameplay configuration changes. */
	bool BuildMesh(UStaticMesh& Mesh, const FHALVehicleCollisionParameters& Parameters, FString& Error);
	bool ReadRecipe(UStaticMesh& Mesh, FHALVehicleCollisionParameters& Out, FString& Error);
	bool ValidateOutput(const FString& Folder, const FString& Name, FString& Error);
	/** Creates a unique unsaved /Game asset. Saving and assigning to a VehicleDefinition are explicit user actions. */
	UStaticMesh* CreateAsset(const UHALVehicleCollisionGeneratorSettings& Settings, FString& Error);
}
