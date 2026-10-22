#include "HALVehicleAlignment.h"

#include "VehicleDefinition.h"
#include "HALVehiclePresentationConfig.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "ScopedTransaction.h"
#include "SkeletalMeshAttributes.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace
{
	bool PositiveUniform(const FVector& Scale)
	{
		return !Scale.ContainsNaN() && Scale.X > SMALL_NUMBER && Scale.Equals(FVector(Scale.X), 0.0001);
	}
	bool Fail(HALVehicleAlignment::FReport& Out, const FString& Error)
	{
		Out.Error = Error; Out.bCanApply = false; return false;
	}
	bool BoundsEqual(const FBox& A, const FBox& B)
	{
		return A.IsValid == B.IsValid && A.Min.Equals(B.Min, .0001) && A.Max.Equals(B.Max, .0001);
	}
	bool SamePreview(const HALVehicleAlignment::FReport& A, const HALVehicleAlignment::FReport& B)
	{
		if (A.Presentation != B.Presentation || A.Model != B.Model || A.CollisionMesh != B.CollisionMesh
			|| !A.CurrentTransform.Equals(B.CurrentTransform, .0001) || !A.RootScale.Equals(B.RootScale, .0001)
			|| !BoundsEqual(A.CollisionBounds, B.CollisionBounds) || !BoundsEqual(A.ModelBounds, B.ModelBounds)
			|| !FMath::IsNearlyEqual(A.SuggestedZ, B.SuggestedZ, .0001) || A.SharedUsers != B.SharedUsers) { return false; }
		for (int32 I = 0; I < 4; ++I)
		{
			if (A.Wheels[I].SpinBone != B.Wheels[I].SpinBone || A.Wheels[I].Vertices != B.Wheels[I].Vertices
				|| !BoundsEqual(A.Wheels[I].Bounds, B.Wheels[I].Bounds)) { return false; }
		}
		return true;
	}
}

bool HALVehicleAlignment::Measure(const FMeshDescription& Source, const FName (&SpinBones)[4],
	const FTransform& Transform, const FVector& RootScale, const FBox& ConvexBounds, FReport& Out)
{
	Out = {};
	Out.CurrentTransform = Transform; Out.RootScale = RootScale; Out.CollisionBounds = ConvexBounds;
	if (Transform.ContainsNaN() || !Transform.GetRotation().IsNormalized() || !PositiveUniform(Transform.GetScale3D())
		|| !PositiveUniform(RootScale) || !ConvexBounds.IsValid || ConvexBounds.Min.ContainsNaN() || ConvexBounds.Max.ContainsNaN()
		|| ConvexBounds.GetSize().GetMin() <= SMALL_NUMBER)
	{ return Fail(Out, TEXT("Use finite transforms, positive uniform model/root scale and a nonzero convex volume.")); }
	const FSkeletalMeshConstAttributes Attributes(Source);
	const auto Weights = Attributes.GetVertexSkinWeights();
	if (!Attributes.HasBones() || !Attributes.HasBoneNameAttribute() || !Weights.IsValid() || Source.Triangles().Num() == 0)
	{ return Fail(Out, TEXT("LOD0 source triangles, named bones and default skin weights are required. No bounds/radius fallback is used.")); }
	TMap<int32, int32> BoneSlots;
	const auto Names = Attributes.GetBoneNames();
	TSet<FName> UniqueNames;
	for (int32 I = 0; I < 4; ++I)
	{
		if (SpinBones[I].IsNone() || UniqueNames.Contains(SpinBones[I])) { return Fail(Out, TEXT("Four distinct configured wheel spin bones are required.")); }
		UniqueNames.Add(SpinBones[I]); Out.Wheels[I].SpinBone = SpinBones[I];
		int32 Found = 0;
		for (const FBoneID Bone : Attributes.Bones().GetElementIDs())
		{
			if (Names[Bone] == SpinBones[I]) { BoneSlots.Add(Bone.GetValue(), I); ++Found; }
		}
		if (Found != 1) { return Fail(Out, FString::Printf(TEXT("Spin bone %s must occur exactly once in LOD0 source bone names."), *SpinBones[I].ToString())); }
	}
	TSet<FVertexID> Used;
	for (const FTriangleID Triangle : Source.Triangles().GetElementIDs())
	{ for (const FVertexID Vertex : Source.GetTriangleVertices(Triangle)) { Used.Add(Vertex); } }
	// Remove translation Z while solving. This gives an absolute result, independent of the previous manual offset.
	FTransform Base = Transform;
	FVector Translation = Base.GetTranslation(); Translation.Z = 0; Base.SetTranslation(Translation);
	FBox WheelBase[4] = { FBox(ForceInit), FBox(ForceInit), FBox(ForceInit), FBox(ForceInit) };
	const auto Positions = Attributes.GetVertexPositions();
	for (const FVertexID Vertex : Used)
	{
		const FVector Point = Base.TransformPosition(FVector(Positions[Vertex]));
		if (Point.ContainsNaN()) { return Fail(Out, TEXT("LOD0 contains a non-finite transformed vertex.")); }
		Out.ModelBounds += Point + FVector(0, 0, Transform.GetTranslation().Z);
		const auto Influences = Weights.Get(Vertex);
		int32 Slot = INDEX_NONE;
		bool bOther = false;
		float Sum = 0;
		for (int32 I = 0; I < Influences.Num(); ++I)
		{
			const auto Influence = Influences.Get(I);
			if (Influence.GetWeight() <= 0) { continue; }
			if (!Attributes.IsBoneValid(FBoneID(Influence.GetBoneIndex()))) { return Fail(Out, TEXT("Source skin weights refer to an invalid source bone.")); }
			const int32* Found = BoneSlots.Find(Influence.GetBoneIndex());
			if (!Found) { bOther = true; }
			else if (Slot != INDEX_NONE && Slot != *Found) { return Fail(Out, TEXT("A vertex is blended between different wheel spin bones.")); }
			else { Slot = *Found; }
			Sum += Influence.GetWeight();
		}
		if (Slot == INDEX_NONE) { continue; }
		if (bOther || !FMath::IsNearlyEqual(Sum, 1.f, .0001f))
		{ return Fail(Out, TEXT("Wheel geometry must be fully weighted to its configured spin bone. Blended wheel/body or deforming wheel rigs need manual calibration.")); }
		WheelBase[Slot] += Point; ++Out.Wheels[Slot].Vertices;
	}
	double Lowest = TNumericLimits<double>::Max(), Highest = TNumericLimits<double>::Lowest();
	for (int32 I = 0; I < 4; ++I)
	{
		if (Out.Wheels[I].Vertices < 3 || !WheelBase[I].IsValid || WheelBase[I].GetSize().GetMin() <= SMALL_NUMBER)
		{ return Fail(Out, FString::Printf(TEXT("Wheel %d has no reliable nonzero source geometry."), I)); }
		Lowest = FMath::Min(Lowest, WheelBase[I].Min.Z); Highest = FMath::Max(Highest, WheelBase[I].Min.Z);
		Out.Wheels[I].Bounds = WheelBase[I].ShiftBy(FVector(0, 0, Transform.GetTranslation().Z));
	}
	Out.SuggestedZ = ConvexBounds.Min.Z - Lowest;
	Out.DeltaZ = Out.SuggestedZ - Transform.GetTranslation().Z;
	Out.SpreadCm = (Highest - Lowest) * RootScale.Z;
	Out.bMeasured = true;
	if (Out.SpreadCm > CoplanarToleranceCm)
	{ return Fail(Out, FString::Printf(TEXT("Wheel bottom spread %.3f world cm exceeds %.1f cm. A single Z offset cannot fix this pose."), Out.SpreadCm, CoplanarToleranceCm)); }
	if (ConvexBounds.GetSize().Z > Out.ModelBounds.GetSize().Z + 1 / RootScale.Z)
	{ Out.Warnings.Add(TEXT("Collision is taller than the visual model. Upright alignment does not fit roof/side contacts; review collision dimensions separately.")); }
	Out.bCanApply = true;
	return true;
}

bool HALVehicleAlignment::Analyze(UVehicleDefinition& Definition, FReport& Out)
{
	Out = {};
	UHALVehiclePresentationConfig* Presentation = Definition.Presentation;
	if (Definition.CollisionShape != EVehicleCollisionShape::ConvexMesh || !Definition.CollisionMesh || !Presentation || !Presentation->Mesh)
	{ return Fail(Out, TEXT("Select a Vehicle Definition with a Convex Mesh and a Presentation DA containing a Skeletal Mesh.")); }
	const UBodySetup* Setup = Definition.CollisionMesh->GetBodySetup();
	if (!Setup || !Setup->BuildScale3D.Equals(FVector::OneVector, .0001) || Setup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple || Setup->AggGeom.ConvexElems.IsEmpty()
		|| Setup->AggGeom.GetElementCount() != Setup->AggGeom.ConvexElems.Num())
	{ return Fail(Out, TEXT("Collision must use simple convex hulls only with BodySetup BuildScale3D=1; complex-as-simple and additional primitive types are unsupported.")); }
	FBox ConvexBounds(ForceInit);
	for (const auto& Hull : Setup->AggGeom.ConvexElems)
	{
		if (Hull.VertexData.Num() < 4 || Hull.GetTransform().ContainsNaN()) { return Fail(Out, TEXT("A convex hull has missing or invalid source vertices.")); }
		for (const FVector& Vertex : Hull.VertexData)
		{
			const FVector P = Hull.GetTransform().TransformPosition(Vertex);
			if (P.ContainsNaN()) { return Fail(Out, TEXT("Non-finite convex collision vertex.")); }
			ConvexBounds += P;
		}
	}
	if (Presentation->Wheels.Num() != 4) { return Fail(Out, TEXT("Presentation must configure exactly FL, FR, BL and BR.")); }
	FName SpinBones[4];
	bool Seen[4] = {};
	const auto& Skeleton = Presentation->Mesh->GetRefSkeleton();
	const int32 Root = Skeleton.FindBoneIndex(Presentation->RootBone);
	if (Root == INDEX_NONE) { return Fail(Out, TEXT("Configured root bone is missing.")); }
	for (const auto& Wheel : Presentation->Wheels)
	{
		const int32 Slot = uint8(Wheel.Wheel);
		if (Slot > 3 || Seen[Slot]) { return Fail(Out, TEXT("Wheel identities must be unique FL/FR/BL/BR.")); }
		Seen[Slot] = true; SpinBones[Slot] = Wheel.SpinBone;
		const int32 Suspension = Skeleton.FindBoneIndex(Wheel.SuspensionBone), Steering = Skeleton.FindBoneIndex(Wheel.SteeringBone), Spin = Skeleton.FindBoneIndex(Wheel.SpinBone);
		if (Suspension == INDEX_NONE || Steering == INDEX_NONE || Spin == INDEX_NONE || Skeleton.GetParentIndex(Suspension) != Root
			|| Skeleton.GetParentIndex(Steering) != Suspension || Skeleton.GetParentIndex(Spin) != Steering)
		{ return Fail(Out, TEXT("Wheel controls must form Root -> suspension -> steer -> spin. Check the configured bone names.")); }
	}
	const FMeshDescription* Source = Presentation->Mesh->GetMeshDescription(0);
	if (!Source) { return Fail(Out, TEXT("Skeletal Mesh LOD0 editable source geometry is unavailable.")); }
	const bool bMeasured = Measure(*Source, SpinBones, Presentation->RelativeTransform, Definition.Physics.Scale, ConvexBounds, Out);
	Out.Definition = &Definition; Out.Presentation = Presentation; Out.CollisionMesh = Definition.CollisionMesh; Out.Model = Presentation->Mesh;
	if (!bMeasured) { return false; }
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	if (Registry.IsLoadingAssets()) { return Fail(Out, TEXT("Asset discovery is still running. Wait for the scan, then analyze again to check shared DA users.")); }
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UVehicleDefinition::StaticClass()->GetClassPathName(), Assets, true);
	TSet<UVehicleDefinition*> Definitions;
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.PackageName.ToString().StartsWith(TEXT("/Game/")))
		{
			auto* Other = Cast<UVehicleDefinition>(Asset.GetAsset());
			if (!Other) { return Fail(Out, TEXT("A project Vehicle Definition could not be loaded; shared-DA safety cannot be verified.")); }
			Definitions.Add(Other);
		}
	}
	for (TObjectIterator<UVehicleDefinition> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && !It->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor)) { Definitions.Add(*It); }
	}
	for (auto* Other : Definitions)
	{
		if (Other == &Definition || Other->Presentation != Presentation) { continue; }
		Out.SharedUsers.Add(Other->GetPathName());
		if (Other->CollisionShape != Definition.CollisionShape || Other->CollisionMesh != Definition.CollisionMesh || !Other->Physics.Scale.Equals(Definition.Physics.Scale, .0001))
		{ Out.IncompatibleUsers.Add(Other->GetPathName()); }
	}
	Out.SharedUsers.Sort(); Out.IncompatibleUsers.Sort();
	if (!Out.IncompatibleUsers.IsEmpty())
	{ Out.bCanApply = false; Out.Error = TEXT("Presentation DA is shared by a different collision configuration. Duplicate/reassign that DA manually before applying."); }
	return true;
}

bool HALVehicleAlignment::Apply(const FReport& Preview, bool bAcknowledgeShared, bool& bChanged, FString& Error)
{
	bChanged = false;
	if (!GEditor || GEditor->PlayWorld || IsRunningGame() || IsRunningCommandlet()) { Error = TEXT("Apply is available only in an idle Editor, outside PIE/simulation/game/commandlet."); return false; }
	if (!Preview.Definition.IsValid() || !Preview.Presentation.IsValid() || !Preview.bCanApply) { Error = TEXT("No applicable analysis. Select a Vehicle Definition and analyze first."); return false; }
	FReport Fresh;
	if (!Analyze(*Preview.Definition.Get(), Fresh) || !Fresh.bCanApply) { Error = Fresh.Error; return false; }
	if (!SamePreview(Preview, Fresh)) { Error = TEXT("Configuration, geometry or shared DA users changed after preview. Analyze again before applying."); return false; }
	if (!Fresh.SharedUsers.IsEmpty() && !bAcknowledgeShared) { Error = TEXT("Acknowledge the listed compatible shared DA users before applying."); return false; }
	auto* Config = Fresh.Presentation.Get();
	if (!Config->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))) { Error = TEXT("Apply requires a project /Game Presentation DA; engine/transient assets are read-only in this tool."); return false; }
	if (FMath::Abs(Fresh.DeltaZ) <= .0001) { return true; }
	const FScopedTransaction Transaction(NSLOCTEXT("HALVehicleAlignment", "ApplyTransaction", "Align vehicle visual wheel bottoms"));
	Config->SetFlags(RF_Transactional);
	Config->Modify();
	FVector Translation = Config->RelativeTransform.GetTranslation(); Translation.Z = Fresh.SuggestedZ;
	Config->RelativeTransform.SetTranslation(Translation);
	FPropertyChangedEvent Changed(FindFProperty<FProperty>(UHALVehiclePresentationConfig::StaticClass(), GET_MEMBER_NAME_CHECKED(UHALVehiclePresentationConfig, RelativeTransform)));
	Config->PostEditChangeProperty(Changed); Config->MarkPackageDirty();
	bChanged = true;
	return true;
}

FString HALVehicleAlignment::FReport::Describe() const
{
	FString Text;
	if (Definition.IsValid()) { Text += FString::Printf(TEXT("Vehicle: %s\nPresentation: %s\n\n"), *Definition->GetPathName(), *GetNameSafe(Presentation.Get())); }
	if (bMeasured)
	{
		Text += FString::Printf(TEXT("Current local Z: %.3f cm\nSuggested local Z: %.3f cm\nChange: %+.3f cm\nRoot scale: %.3f\nCollision bottom (root local): %.3f cm\nWheel bottom spread: %.4f world cm (limit %.1f)\n\n"),
			CurrentTransform.GetTranslation().Z, SuggestedZ, DeltaZ, RootScale.Z, CollisionBounds.Min.Z, SpreadCm, CoplanarToleranceCm);
		const TCHAR* Labels[] = { TEXT("FL"), TEXT("FR"), TEXT("BL"), TEXT("BR") };
		for (int32 I = 0; I < 4; ++I)
		{
			Text += FString::Printf(TEXT("%s  bottom %.3f -> %.3f cm   [%d source vertices]\n"), Labels[I], Wheels[I].Bounds.Min.Z, Wheels[I].Bounds.Min.Z + DeltaZ, Wheels[I].Vertices);
		}
	}
	for (const auto& User : SharedUsers) { Text += TEXT("\nShared DA user: ") + User; }
	for (const auto& User : IncompatibleUsers) { Text += TEXT("\nINCOMPATIBLE: ") + User; }
	for (const auto& Warning : Warnings) { Text += TEXT("\n\nNote: ") + Warning; }
	if (!Error.IsEmpty()) { Text += TEXT("\n\nCannot apply: ") + Error; }
	return Text;
}
