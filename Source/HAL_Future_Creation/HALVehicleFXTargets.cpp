#include "HALVehicleFXTargets.h"
#include "HALVehiclePresentationComponent.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

FVector FHALVehicleFXTarget::MeshPointToVehicle(const FVector& Point, const FTransform& VehicleToWorld) const
{
	return VehicleToWorld.InverseTransformPosition(MeshToWorld.TransformPosition(Point));
}

bool HALVehicleFX::BuildBounds(const FBox& LocalBounds, const FTransform& MeshToWorld,
	const FTransform& VehicleToWorld, FBox& OutVehicleBounds)
{
	OutVehicleBounds.Init();
	const auto UsableTransform = [](const FTransform& Transform)
	{
		return !Transform.ContainsNaN() && Transform.GetRotation().IsNormalized()
			&& Transform.GetScale3D().GetAbsMin() > UE_SMALL_NUMBER;
	};
	if (!LocalBounds.IsValid || LocalBounds.Min.ContainsNaN() || LocalBounds.Max.ContainsNaN()
		|| LocalBounds.GetSize().GetMin() < 0 || LocalBounds.GetSize().GetMax() <= UE_SMALL_NUMBER
		|| !UsableTransform(MeshToWorld) || !UsableTransform(VehicleToWorld)) { return false; }
	for (uint8 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Point(Corner & 1 ? LocalBounds.Max.X : LocalBounds.Min.X,
			Corner & 2 ? LocalBounds.Max.Y : LocalBounds.Min.Y, Corner & 4 ? LocalBounds.Max.Z : LocalBounds.Min.Z);
		const FVector VehiclePoint = VehicleToWorld.InverseTransformPosition(MeshToWorld.TransformPosition(Point));
		if (VehiclePoint.ContainsNaN()) { OutVehicleBounds.Init(); return false; }
		OutVehicleBounds += VehiclePoint;
	}
	return true;
}

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorld GHALVehicleFXTargets(TEXT("hal.VehicleFXTargets"),
		TEXT("Log current cosmetic mesh bindings and draw bounds for 10 seconds (snapshot; rerun after movement)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (!World) { return; }
			int32 Providers = 0;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				const auto* Presentation = It->FindComponentByClass<UHALVehiclePresentationComponent>();
				if (!Presentation) { continue; }
				++Providers;
				FHALVehicleFXTargetSnapshot Snapshot;
				Presentation->CaptureStatusFXTargets(Snapshot);
				UE_LOG(LogTemp, Display, TEXT("VehicleFXTargets Owner=%s Role=%d Revision=%u Targets=%d VehicleBounds=%s"),
					*It->GetName(), int32(It->GetLocalRole()), Snapshot.BindingRevision, Snapshot.Targets.Num(), *Snapshot.VehicleBounds.ToString());
				for (const auto& Target : Snapshot.Targets)
				{
					UE_LOG(LogTemp, Display, TEXT("VehicleFXTarget Id=%s Kind=%s Component=%s Asset=%s CenterCm=%s ExtentCm=%s MeshToWorld=%s VehicleBounds=%s"),
						*Target.Id.ToString(), Target.Kind == EHALVehicleFXMeshKind::Skeletal ? TEXT("Skeletal") : TEXT("Static"),
						*GetNameSafe(Target.Mesh.Get()), *GetPathNameSafe(Target.MeshAsset.Get()),
						*Target.LocalBounds.GetCenter().ToString(), *Target.LocalBounds.GetExtent().ToString(),
						*Target.MeshToWorld.ToHumanReadableString(), *Target.VehicleBounds.ToString());
					DrawDebugBox(World, Target.MeshToWorld.TransformPosition(Target.LocalBounds.GetCenter()),
						Target.LocalBounds.GetExtent() * Target.MeshToWorld.GetScale3D().GetAbs(), Target.MeshToWorld.GetRotation(),
						FColor::Cyan, false, 10, 0, 2);
				}
				if (Snapshot.VehicleBounds.IsValid)
				{
					DrawDebugBox(World, Snapshot.VehicleToWorld.TransformPosition(Snapshot.VehicleBounds.GetCenter()),
						Snapshot.VehicleBounds.GetExtent() * Snapshot.VehicleToWorld.GetScale3D().GetAbs(),
						Snapshot.VehicleToWorld.GetRotation(), FColor::Yellow, false, 10, 0, 1);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("VehicleFXTargets Providers=%d World=%s"), Providers, *World->GetName());
		}));
}
#endif
