#include "HALVehicleFXMount.h"
#include "HALVehicleFXTargets.h"
#include "Components/MeshComponent.h"

bool FHALVehicleFXMount::IsValid() const
{
	const FVector S = Offset.GetScale3D();
	return !PartId.IsNone() && !Offset.ContainsNaN() && Offset.GetRotation().IsNormalized()
		&& S.GetMin() > 0 && S.GetMax() <= 100 && Offset.GetLocation().GetAbsMax() <= 10000;
}

bool HALVehicleFX::ResolveMount(const FHALVehicleFXTargetSnapshot& Targets,
	const FHALVehicleFXMount& Mount, FHALResolvedFXMount& Out)
{
	Out = {};
	if (!Mount.IsValid()) { return false; }
	for (const auto& Target : Targets.Targets)
	{
		auto* Mesh = Target.Mesh.Get();
		if (Target.Id != Mount.PartId || !IsValid(Mesh)) { continue; }
		const bool bMissing = !Mount.SocketOrBone.IsNone() && !Mesh->DoesSocketExist(Mount.SocketOrBone);
		if (bMissing && !Mount.bFallbackToMeshOrigin) { return false; }
		Out.Mesh = Mesh;
		Out.Socket = bMissing ? NAME_None : Mount.SocketOrBone;
		Out.Relative = Mount.Offset;
		Out.World = Mount.Offset * Mesh->GetSocketTransform(Out.Socket, RTS_World);
		Out.bUsedFallback = bMissing;
		return !Out.World.ContainsNaN();
	}
	return false;
}
