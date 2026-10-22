#pragma once

#include "CoreMinimal.h"

class UVehicleDefinition;
class UHALVehiclePresentationConfig;
class UStaticMesh;
class USkeletalMesh;
struct FMeshDescription;

namespace HALVehicleAlignment
{
	/** World cm. A single Z translation cannot repair a non-planar set of wheel bottoms. */
	constexpr double CoplanarToleranceCm = 0.5;
	struct FWheel
	{
		FName SpinBone;
		int32 Vertices = 0;
		FBox Bounds = FBox(ForceInit); // CollisionRoot local space, current presentation transform applied.
	};
	struct FReport
	{
		TWeakObjectPtr<UVehicleDefinition> Definition;
		TWeakObjectPtr<UHALVehiclePresentationConfig> Presentation;
		TWeakObjectPtr<UStaticMesh> CollisionMesh;
		TWeakObjectPtr<USkeletalMesh> Model;
		FTransform CurrentTransform;
		FVector RootScale = FVector::OneVector;
		FBox CollisionBounds = FBox(ForceInit), ModelBounds = FBox(ForceInit);
		FWheel Wheels[4]; // FL, FR, BL, BR.
		double SuggestedZ = 0, DeltaZ = 0, SpreadCm = 0;
		bool bMeasured = false, bCanApply = false;
		TArray<FString> SharedUsers, IncompatibleUsers, Warnings;
		FString Error;
		FString Describe() const;
	};

	/** Pure source-geometry calculation. Only fully rigid vertices on the four configured spin bones are accepted. */
	bool Measure(const FMeshDescription& Source, const FName (&SpinBones)[4], const FTransform& ModelTransform,
		const FVector& RootScale, const FBox& ConvexBounds, FReport& Out);
	/** Read-only: imports no files, changes no asset properties, and uses no animated pose or configured RadiusCm. */
	bool Analyze(UVehicleDefinition& Definition, FReport& Out);
	/** Team-triggered Editor operation only. Fresh analysis, shared-DA guard, transaction/Undo, dirty but never saved. */
	bool Apply(const FReport& Preview, bool bAcknowledgeShared, bool& bChanged, FString& Error);
}
