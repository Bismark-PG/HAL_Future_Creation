#include "HALVehiclePresentationComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"

namespace
{
	const FName BodyId(TEXT("Body"));

	UObject* GetFXAsset(UMeshComponent* Component)
	{
		if (auto* Static = Cast<UStaticMeshComponent>(Component)) { return Static->GetStaticMesh(); }
		if (auto* Skeletal = Cast<USkeletalMeshComponent>(Component)) { return Skeletal->GetSkeletalMeshAsset(); }
		return nullptr;
	}
}

void UHALVehiclePresentationComponent::CaptureStatusFXTargets(FHALVehicleFXTargetSnapshot& Out) const
{
	check(IsInGameThread());
	Out = {};
	Out.Provider = const_cast<UHALVehiclePresentationComponent*>(this);
	Out.BindingRevision = StatusFXBindingRevision;
	const AActor* Owner = GetOwner();
	if (!bStatusFXBound || !IsRegistered() || !IsValid(Owner) || Owner->IsActorBeingDestroyed() || Owner->IsHidden()
		|| !IsValid(Body) || !Body->IsRegistered()) { return; }
	Out.VehicleToWorld = Body->GetComponentTransform();
	const auto AddTarget = [&](FName Id, UMeshComponent* Component)
	{
		if (!IsValid(Component) || Component == Body || Component->GetOwner() != Owner || !Component->IsRegistered()
			|| Component->IsSimulatingPhysics() || !Component->IsVisible() || Component->bHiddenInGame) { return; }
		UObject* Asset = GetFXAsset(Component);
		if (!IsValid(Asset)) { return; }
		FHALVehicleFXTarget Target;
		Target.Id = Id; Target.Mesh = Component; Target.MeshAsset = Asset;
		Target.Kind = Cast<USkeletalMeshComponent>(Component) ? EHALVehicleFXMeshKind::Skeletal : EHALVehicleFXMeshKind::Static;
		Target.LocalBounds = Component->CalcBounds(FTransform::Identity).GetBox();
		Target.MeshToWorld = Component->GetComponentTransform();
		if (!HALVehicleFX::BuildBounds(Target.LocalBounds, Target.MeshToWorld, Out.VehicleToWorld, Target.VehicleBounds)) { return; }
		Out.VehicleBounds += Target.VehicleBounds;
		Out.Targets.Add(MoveTemp(Target));
	};
	AddTarget(BodyId, bConfigured ? static_cast<UMeshComponent*>(Mesh.Get()) : Fallback.Get());
	TArray<FName> PartIds;
	StatusFXParts.GetKeys(PartIds);
	PartIds.Sort(FNameLexicalLess());
	for (const FName Id : PartIds) { AddTarget(Id, StatusFXParts.FindChecked(Id).Get()); }
}

bool UHALVehiclePresentationComponent::RegisterStatusFXPart(FName Id, UMeshComponent* Part)
{
	check(IsInGameThread());
	if (!bStatusFXBound || Id.IsNone() || Id == BodyId || !IsValid(Part) || !IsValid(GetFXAsset(Part))
		|| Part->IsSimulatingPhysics() || Part->GetOwner() != GetOwner() || Part == Body || Part == Fallback || Part == Mesh) { return false; }
	if (const auto* Existing = StatusFXParts.Find(Id))
	{
		if (Existing->Get() == Part) { return true; }
		if (Existing->IsValid()) { return false; }
	}
	for (const auto& Entry : StatusFXParts) { if (Entry.Value.Get() == Part) { return false; } }
	StatusFXParts.Add(Id, Part);
	++StatusFXBindingRevision;
	StatusFXBindingsChanged.Broadcast();
	return true;
}

void UHALVehiclePresentationComponent::UnregisterStatusFXPart(FName Id)
{
	check(IsInGameThread());
	if (StatusFXParts.Remove(Id)) { ++StatusFXBindingRevision; StatusFXBindingsChanged.Broadcast(); }
}

void UHALVehiclePresentationComponent::ClearStatusFXBindings()
{
	check(IsInGameThread());
	if (!bStatusFXBound && StatusFXParts.IsEmpty()) { return; }
	bStatusFXBound = false;
	StatusFXParts.Reset();
	++StatusFXBindingRevision;
	StatusFXBindingsChanged.Broadcast();
}

bool UHALVehiclePresentationComponent::AreStatusFXBindingsCurrent(const FHALVehicleFXTargetSnapshot& Snapshot) const
{
	check(IsInGameThread());
	if (Snapshot.Provider.Get() != this || Snapshot.BindingRevision != StatusFXBindingRevision) { return false; }
	FHALVehicleFXTargetSnapshot Current;
	CaptureStatusFXTargets(Current);
	if (Snapshot.Targets.Num() != Current.Targets.Num()) { return false; }
	for (int32 Index = 0; Index < Current.Targets.Num(); ++Index)
	{
		const auto& Old = Snapshot.Targets[Index]; const auto& New = Current.Targets[Index];
		if (Old.Id != New.Id || Old.Mesh != New.Mesh || Old.MeshAsset != New.MeshAsset) { return false; }
	}
	return true;
}
