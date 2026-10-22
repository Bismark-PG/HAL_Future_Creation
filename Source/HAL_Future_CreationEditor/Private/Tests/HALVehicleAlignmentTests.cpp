#if WITH_DEV_AUTOMATION_TESTS

#include "HALVehicleAlignment.h"
#include "VehicleDefinition.h"
#include "HALVehiclePresentationConfig.h"
#include "BoneWeights.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SkeletalMeshAttributes.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	void SourceWheels(FMeshDescription& Mesh, const FName (&Names)[4], double RaisedRear = 0, bool bBlend = false)
	{
		FSkeletalMeshAttributes A(Mesh); A.Register();
		const FBoneID Body = A.CreateBone(); A.GetBoneNames()[Body] = TEXT("Body");
		int32 IDs[4];
		// Bone ordering deliberately differs from wheel ordering: resolve by source bone name.
		for (int32 I : { 3, 0, 2, 1 }) { const FBoneID ID = A.CreateBone(); A.GetBoneNames()[ID] = Names[I]; IDs[I] = ID.GetValue(); }
		const FPolygonGroupID Group = Mesh.CreatePolygonGroup();
		const FIntVector Faces[] = { {0,1,3},{0,3,2},{4,6,7},{4,7,5},{0,4,5},{0,5,1},{2,3,7},{2,7,6},{0,2,6},{0,6,4},{1,5,7},{1,7,3} };
		for (int32 I = 0; I < 4; ++I)
		{
			FVertexID Vertices[8];
			for (int32 V = 0; V < 8; ++V)
			{
				Vertices[V] = Mesh.CreateVertex();
				A.GetVertexPositions()[Vertices[V]] = FVector3f((I < 2 ? 50 : -50) + ((V&1) ? 10 : -10),
					(I%2 ? 30 : -30) + ((V&2) ? 5 : -5), ((V&4) ? 20 : 0) + (I == 2 ? RaisedRear : 0));
				TArray<UE::AnimationCore::FBoneWeight> Weights;
				Weights.Emplace(IDs[I], bBlend && I == 0 ? .5f : 1.f);
				if (bBlend && I == 0) { Weights.Emplace(Body.GetValue(), .5f); }
				A.GetVertexSkinWeights().Set(Vertices[V], UE::AnimationCore::FBoneWeights::Create(MakeArrayView(Weights)));
			}
			for (const auto& Face : Faces)
			{
				TArray<FVertexInstanceID> Instances;
				for (int32 V : { Face.X, Face.Y, Face.Z }) { Instances.Add(Mesh.CreateVertexInstance(Vertices[V])); }
				Mesh.CreateTriangle(Group, Instances);
			}
		}
	}
	UVehicleDefinition* TeamDefinition()
	{
		return LoadObject<UVehicleDefinition>(nullptr, TEXT("/Game/Data/MVP_B/DA_Vehicle_Player_TestBaseline_v1.DA_Vehicle_Player_TestBaseline_v1"));
	}
	struct FProbe
	{
		UPackage* Package = nullptr;
		TStrongObjectPtr<UVehicleDefinition> Definition;
		UHALVehiclePresentationConfig* Config = nullptr;
		explicit FProbe(UVehicleDefinition* Seed)
		{
			Package = CreatePackage(*(TEXT("/Game/__HALAlignmentProbe_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			Definition.Reset(DuplicateObject<UVehicleDefinition>(Seed,Package,TEXT("Vehicle")));
			Config = DuplicateObject<UHALVehiclePresentationConfig>(Seed->Presentation,Package,TEXT("Presentation"));
			Definition->Presentation = Config;
			FVector T = Config->RelativeTransform.GetTranslation(); T.Z += 100;
			Config->RelativeTransform.SetTranslation(T);
			Package->SetDirtyFlag(false);
		}
		~FProbe() { Package->SetDirtyFlag(false); Config->ClearFlags(RF_Standalone); Definition->ClearFlags(RF_Standalone); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAlignmentGeometryTest, "HAL.Editor.VehicleAlignment.ReferenceGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAlignmentGeometryTest::RunTest(const FString&)
{
	const FName Names[4] = { TEXT("FL"), TEXT("FR"), TEXT("BL"), TEXT("BR") };
	FMeshDescription Mesh; SourceWheels(Mesh,Names);
	const FBox Collision(FVector(-228,-144,-134.5),FVector(228,144,134.5));
	const FTransform Transform(FRotator(0,90,0),FVector(12,-34,77),FVector(2));
	HALVehicleAlignment::FReport Report;
	if (!TestTrue(TEXT("Source wheels measured under rotation and two layers of uniform scale"),HALVehicleAlignment::Measure(Mesh,Names,Transform,FVector(3),Collision,Report))) { AddError(Report.Error); return false; }
	TestEqual(TEXT("Absolute suggestion removes prior arbitrary offset"),Report.SuggestedZ,-134.5,.0001);
	TestEqual(TEXT("Delta is relative to current offset"),Report.DeltaZ,-211.5,.0001);
	TestTrue(TEXT("Input transform unchanged"),Report.CurrentTransform.Equals(Transform));
	for (const auto& Wheel : Report.Wheels)
	{
		TestEqual(TEXT("Named wheel identity survives reordered source bone IDs"),Wheel.Vertices,8);
		TestEqual(TEXT("All four proposed bottoms meet collision baseline"),Wheel.Bounds.Min.Z+Report.DeltaZ,Collision.Min.Z,.0001);
	}
	FTransform Aligned = Transform;
	FVector T = Aligned.GetTranslation(); T.Z = Report.SuggestedZ; Aligned.SetTranslation(T);
	TestTrue(TEXT("Repeat calculation accepted"),HALVehicleAlignment::Measure(Mesh,Names,Aligned,FVector(3),Collision,Report));
	TestEqual(TEXT("Repeat is idempotent"),Report.DeltaZ,0.,.0001);
	FSkeletalMeshAttributes Attributes(Mesh);
	for (const FVertexID Vertex : Mesh.Vertices().GetElementIDs()) { Attributes.GetVertexPositions()[Vertex] += FVector3f(0,0,7.5f); }
	TestTrue(TEXT("Different imported model origin measured"),HALVehicleAlignment::Measure(Mesh,Names,Aligned,FVector(3),Collision,Report));
	TestEqual(TEXT("Source bottom and visual scale affect absolute calibration"),Report.SuggestedZ,-149.5,.0001);
	TestEqual(TEXT("Existing calibrated asset receives only the newly required change"),Report.DeltaZ,-15.,.0001);
	TestTrue(TEXT("Non-centered collision origin measured"),HALVehicleAlignment::Measure(Mesh,Names,Aligned,FVector(3),Collision.ShiftBy(FVector(5,12,50)),Report));
	TestEqual(TEXT("Uses actual convex minimum, not minus half height"),Report.SuggestedZ,-99.5,.0001);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAlignmentInvalidTest, "HAL.Editor.VehicleAlignment.RejectAmbiguousGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAlignmentInvalidTest::RunTest(const FString&)
{
	const FName Names[4] = { TEXT("FL"), TEXT("FR"), TEXT("BL"), TEXT("BR") };
	const FBox Collision(FVector(-100,-70,-35),FVector(100,70,35));
	FMeshDescription Mesh; SourceWheels(Mesh,Names);
	HALVehicleAlignment::FReport R;
	TestFalse(TEXT("Nonuniform root scale rejected"),HALVehicleAlignment::Measure(Mesh,Names,FTransform::Identity,FVector(1,2,1),Collision,R));
	TestFalse(TEXT("Negative model scale rejected"),HALVehicleAlignment::Measure(Mesh,Names,FTransform(FQuat::Identity,FVector::ZeroVector,FVector(-1)),FVector::OneVector,Collision,R));
	TestFalse(TEXT("Empty source rejected without a radius fallback"),HALVehicleAlignment::Measure(FMeshDescription(),Names,FTransform::Identity,FVector::OneVector,Collision,R));
	FMeshDescription Raised; SourceWheels(Raised,Names,1);
	TestFalse(TEXT("Noncoplanar wheel bottoms rejected"),HALVehicleAlignment::Measure(Raised,Names,FTransform::Identity,FVector::OneVector,Collision,R));
	TestTrue(TEXT("Rejected plane still supplies a diagnostic preview"),R.bMeasured && R.SpreadCm > .5 && !R.bCanApply);
	FMeshDescription Blended; SourceWheels(Blended,Names,0,true);
	TestFalse(TEXT("Wheel/body blend rejected, not silently discarded"),HALVehicleAlignment::Measure(Blended,Names,FTransform::Identity,FVector::OneVector,Collision,R));
	FMeshDescription NearlyFlat; SourceWheels(NearlyFlat,Names,.2);
	TestTrue(TEXT("Small source tolerance accepted"),HALVehicleAlignment::Measure(NearlyFlat,Names,FTransform::Identity,FVector::OneVector,Collision,R));
	TestFalse(TEXT("Tolerance is world cm, including root scale"),HALVehicleAlignment::Measure(NearlyFlat,Names,FTransform::Identity,FVector(3),Collision,R));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAlignmentTeamTest, "HAL.Editor.VehicleAlignment.SavedVehicleReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAlignmentTeamTest::RunTest(const FString&)
{
	auto* Vehicle = TeamDefinition();
	if (!TestNotNull(TEXT("Team vehicle loads"),Vehicle) || !TestNotNull(TEXT("Presentation exists"),Vehicle->Presentation.Get())) { return false; }
	const FTransform Before = Vehicle->Presentation->RelativeTransform;
	const bool bDirty = Vehicle->Presentation->GetOutermost()->IsDirty();
	HALVehicleAlignment::FReport R;
	if (!TestTrue(TEXT("Actual saved reference geometry analyzed"),HALVehicleAlignment::Analyze(*Vehicle,R))) { AddError(R.Error); return false; }
	TestTrue(TEXT("Analyzing never changes authored transform"),Vehicle->Presentation->RelativeTransform.Equals(Before));
	TestEqual(TEXT("Analyzing never marks asset dirty"),Vehicle->Presentation->GetOutermost()->IsDirty(),bDirty);
	for (const auto& Wheel : R.Wheels)
	{
		TestTrue(TEXT("Real tyre geometry measured"),Wheel.Vertices > 0);
		TestTrue(TEXT("Proposed wheel bottom is on/just above support plane"),
			Wheel.Bounds.Min.Z+R.DeltaZ >= R.CollisionBounds.Min.Z-.0001
			&& (Wheel.Bounds.Min.Z+R.DeltaZ-R.CollisionBounds.Min.Z)*R.RootScale.Z <= HALVehicleAlignment::CoplanarToleranceCm);
	}
	const FString Directory = FPaths::ProjectSavedDir()/TEXT("VehicleAlignment/20261022");
	IFileManager::Get().MakeDirectory(*Directory,true);
	TestTrue(TEXT("Read-only team measurement report saved outside Content"),FFileHelper::SaveStringToFile(R.Describe(),*(Directory/TEXT("TeamAssetAnalysis.txt"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAlignmentApplyTest, "HAL.Editor.VehicleAlignment.ApplyUndoStaleAndSharing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAlignmentApplyTest::RunTest(const FString&)
{
	auto* Seed = TeamDefinition();
	if (!Seed || !Seed->Presentation || !GEditor) { AddError(TEXT("Team seed and editor required.")); return false; }
	FProbe Probe(Seed);
	HALVehicleAlignment::FReport R;
	if (!TestTrue(TEXT("Unsaved fixture analysis"),HALVehicleAlignment::Analyze(*Probe.Definition.Get(),R))) { AddError(R.Error); return false; }
	const FTransform Before = Probe.Config->RelativeTransform;
	const FVector OriginalPhysicsScale = Probe.Definition->Physics.Scale;
	UStaticMesh* OriginalCollision = Probe.Definition->CollisionMesh;
	bool bChanged = false; FString Error;
	if (!TestTrue(TEXT("Explicit Apply modifies in-memory fixture"),HALVehicleAlignment::Apply(R,false,bChanged,Error))) { AddError(Error); return false; }
	TestTrue(TEXT("Apply reports a change and dirty package"),bChanged && Probe.Package->IsDirty());
	FTransform Expected = Before; FVector T = Before.GetTranslation(); T.Z = R.SuggestedZ; Expected.SetTranslation(T);
	TestTrue(TEXT("Only presentation Z changes"),Probe.Config->RelativeTransform.Equals(Expected));
	TestTrue(TEXT("Physics configuration untouched"),Probe.Definition->Physics.Scale == OriginalPhysicsScale && Probe.Definition->CollisionMesh == OriginalCollision);
	TestTrue(TEXT("Undo transaction available"),GEditor->UndoTransaction());
	TestTrue(TEXT("Undo restores original offset"),Probe.Config->RelativeTransform.Equals(Before));
	TestTrue(TEXT("Redo transaction available"),GEditor->RedoTransaction());
	TestTrue(TEXT("Redo restores calibrated offset"),Probe.Config->RelativeTransform.Equals(Expected));
	TestTrue(TEXT("Fresh repeat analysis"),HALVehicleAlignment::Analyze(*Probe.Definition.Get(),R));
	TestTrue(TEXT("Repeat application succeeds without drift"),HALVehicleAlignment::Apply(R,false,bChanged,Error));
	TestFalse(TEXT("Already aligned creates no change"),bChanged);
	T = Probe.Config->RelativeTransform.GetTranslation(); T.X += 7; Probe.Config->RelativeTransform.SetTranslation(T);
	TestFalse(TEXT("Stale preview cannot overwrite an edit"),HALVehicleAlignment::Apply(R,false,bChanged,Error));
	TestEqual(TEXT("Stale rejection preserves the newer X"),Probe.Config->RelativeTransform.GetTranslation().X,T.X);
	T.Z += 50; Probe.Config->RelativeTransform.SetTranslation(T);
	TStrongObjectPtr<UVehicleDefinition> Other(DuplicateObject<UVehicleDefinition>(Probe.Definition.Get(),Probe.Package,TEXT("OtherVehicle")));
	TestTrue(TEXT("Compatible shared asset analyzes"),HALVehicleAlignment::Analyze(*Probe.Definition.Get(),R));
	TestTrue(TEXT("Shared user is reported"),!R.SharedUsers.IsEmpty());
	TestFalse(TEXT("Compatible shared DA still needs explicit acknowledgement"),HALVehicleAlignment::Apply(R,false,bChanged,Error));
	TestTrue(TEXT("Acknowledged compatible shared DA can apply"),HALVehicleAlignment::Apply(R,true,bChanged,Error));
	Other->CollisionMesh = NewObject<UStaticMesh>(Probe.Package);
	TestTrue(TEXT("Incompatible sharing still has diagnostic measurements"),HALVehicleAlignment::Analyze(*Probe.Definition.Get(),R));
	TestTrue(TEXT("Different collider sharing blocks writes"),!R.bCanApply && !R.IncompatibleUsers.IsEmpty());
	TestFalse(TEXT("Acknowledgement cannot bypass incompatible sharing"),HALVehicleAlignment::Apply(R,true,bChanged,Error));
	Other->ClearFlags(RF_Standalone);
	return true;
}

#endif
