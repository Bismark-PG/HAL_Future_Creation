#include "HALVehicleCollisionGenerator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/StaticMesh.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace
{
	constexpr int32 RecipeVersion = 1;
	bool Fail(FString& Error, const TCHAR* Message) { Error = Message; return false; }
	void WriteRecipe(UStaticMesh& Mesh, const FHALVehicleCollisionParameters& P)
	{
		auto& Data = Mesh.GetOutermost()->GetMetaData();
		Data.SetValue(&Mesh, TEXT("HALCollision.Version"), TEXT("1"));
		const auto SetFloat = [&](const TCHAR* Key, float Value) {
			Data.SetValue(&Mesh, Key, *FString::Printf(TEXT("%.9g"), Value));
		};
		SetFloat(TEXT("HALCollision.LengthCm"), P.LengthCm);
		SetFloat(TEXT("HALCollision.WidthCm"), P.WidthCm);
		SetFloat(TEXT("HALCollision.HeightCm"), P.HeightCm);
		SetFloat(TEXT("HALCollision.FrontRadiusCm"), P.FrontRadiusCm);
		SetFloat(TEXT("HALCollision.RearRadiusCm"), P.RearRadiusCm);
		Data.SetValue(&Mesh, TEXT("HALCollision.SegmentsPerCorner"), *LexToString(P.SegmentsPerCorner));
	}
}

bool FHALVehicleCollisionParameters::Validate(FString& Error) const
{
	Error.Reset();
	for (const float Dimension : { LengthCm, WidthCm, HeightCm })
	{
		if (!FMath::IsFinite(Dimension) || Dimension < 1 || Dimension > 10000)
		{
			return Fail(Error, TEXT("Length, width and height must be finite and between 1 and 10000 cm."));
		}
	}
	for (const float Radius : { FrontRadiusCm, RearRadiusCm })
	{
		if (!FMath::IsFinite(Radius) || Radius < 0 || Radius > WidthCm * 0.5f)
		{
			return Fail(Error, TEXT("Each corner radius must be between 0 and width / 2."));
		}
	}
	if (FrontRadiusCm + RearRadiusCm > LengthCm)
	{
		return Fail(Error, TEXT("Front radius + rear radius must not exceed length."));
	}
	if (SegmentsPerCorner < 1 || SegmentsPerCorner > 8)
	{
		return Fail(Error, TEXT("Segments per corner must be between 1 and 8."));
	}
	return true;
}

bool HALVehicleCollision::BuildGeometry(const FHALVehicleCollisionParameters& P, FGeometry& Out, FString& Error)
{
	Out = {};
	if (!P.Validate(Error)) { return false; }
	const double X = P.LengthCm * 0.5, Y = P.WidthCm * 0.5, Z = P.HeightCm * 0.5;
	const auto AddPoint = [&](const FVector2D& Point) {
		if (Out.Outline.IsEmpty() || !Out.Outline.Last().Equals(Point, 0.00001)) { Out.Outline.Add(Point); }
	};
	const auto AddCorner = [&](double SignX, double SignY, double Radius, double StartAngle) {
		const FVector2D Corner(SignX * X, SignY * Y);
		if (Radius == 0) { AddPoint(Corner); return; }
		const FVector2D Center = Corner - FVector2D(SignX * Radius, SignY * Radius);
		for (int32 I = 0; I <= P.SegmentsPerCorner; ++I)
		{
			const double Angle = FMath::DegreesToRadians(StartAngle + 90.0 * I / P.SegmentsPerCorner);
			AddPoint(Center + Radius * FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)));
		}
	};
	// Counterclockwise in XY. Coincident endpoints at semicircular tips are merged.
	AddCorner(1, 1, P.FrontRadiusCm, 0);
	AddCorner(-1, 1, P.RearRadiusCm, 90);
	AddCorner(-1, -1, P.RearRadiusCm, 180);
	AddCorner(1, -1, P.FrontRadiusCm, 270);
	if (Out.Outline[0].Equals(Out.Outline.Last(), 0.00001)) { Out.Outline.Pop(); }
	const int32 N = Out.Outline.Num();
	for (const double Height : { -Z, Z })
	{
		for (const FVector2D& Point : Out.Outline) { Out.Vertices.Emplace(Point.X, Point.Y, Height); }
	}
	const int32 Bottom = Out.Vertices.Add(FVector(0, 0, -Z));
	const int32 Top = Out.Vertices.Add(FVector(0, 0, Z));
	for (int32 I = 0; I < N; ++I)
	{
		const int32 J = (I + 1) % N;
		Out.Triangles.Emplace(Top, N + I, N + J);
		Out.Triangles.Emplace(Bottom, J, I);
		Out.Triangles.Emplace(I, J, N + J);
		Out.Triangles.Emplace(I, N + J, N + I);
	}
	return true;
}

bool HALVehicleCollision::BuildMesh(UStaticMesh& Mesh, const FHALVehicleCollisionParameters& P, FString& Error)
{
	if (Mesh.GetNumSourceModels() > 0 || Mesh.GetBodySetup())
	{
		return Fail(Error, TEXT("BuildMesh requires a new empty mesh; existing mesh data is preserved."));
	}
	FGeometry Geometry;
	if (!BuildGeometry(P, Geometry, Error)) { return false; }
	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();
	auto Positions = Attributes.GetVertexPositions();
	auto Normals = Attributes.GetVertexInstanceNormals();
	auto Tangents = Attributes.GetVertexInstanceTangents();
	auto Signs = Attributes.GetVertexInstanceBinormalSigns();
	auto Colors = Attributes.GetVertexInstanceColors();
	auto UVs = Attributes.GetVertexInstanceUVs();
	UVs.SetNumChannels(1);
	TArray<FVertexID> IDs;
	for (const FVector& Point : Geometry.Vertices)
	{
		const FVertexID ID = Description.CreateVertex();
		Positions[ID] = FVector3f(Point);
		IDs.Add(ID);
	}
	const FName Slot(TEXT("CollisionProxy"));
	const FPolygonGroupID Group = Description.CreatePolygonGroup();
	Attributes.GetPolygonGroupMaterialSlotNames()[Group] = Slot;
	for (const FIntVector& Triangle : Geometry.Triangles)
	{
		const FVector& A = Geometry.Vertices[Triangle.X];
		const FVector& B = Geometry.Vertices[Triangle.Y];
		const FVector& C = Geometry.Vertices[Triangle.Z];
		const FVector3f Normal(FVector::CrossProduct(B - A, C - A).GetSafeNormal());
		const FVector3f Tangent((B - A).GetSafeNormal());
		TArray<FVertexInstanceID, TInlineAllocator<3>> Instances;
		for (const int32 Index : { Triangle.X, Triangle.Y, Triangle.Z })
		{
			const FVertexInstanceID ID = Description.CreateVertexInstance(IDs[Index]);
			Instances.Add(ID);
			Normals[ID] = Normal;
			Tangents[ID] = Tangent;
			Signs[ID] = 1;
			Colors[ID] = FVector4f(1, 1, 1, 1);
			UVs.Set(ID, 0, FVector2f(Geometry.Vertices[Index].X / P.LengthCm + 0.5, Geometry.Vertices[Index].Y / P.WidthCm + 0.5));
		}
		Description.CreateTriangle(Group, Instances);
	}
	Mesh.GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface), Slot, Slot));
	Mesh.SetNumSourceModels(1);
	auto& Build = Mesh.GetSourceModel(0).BuildSettings;
	Build.bRecomputeNormals = false;
	Build.bRecomputeTangents = false;
	Build.bGenerateLightmapUVs = false;
	UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
	BuildParams.bBuildSimpleCollision = false;
	BuildParams.bCommitMeshDescription = true;
	BuildParams.bMarkPackageDirty = false;
	if (!Mesh.BuildFromMeshDescriptions({ &Description }, BuildParams))
	{
		return Fail(Error, TEXT("Static Mesh build failed. See Output Log."));
	}
	Mesh.CreateBodySetup();
	UBodySetup* Setup = Mesh.GetBodySetup();
	Setup->AggGeom.EmptyElements();
	Setup->CollisionTraceFlag = CTF_UseSimpleAndComplex;
	FKConvexElem& Hull = Setup->AggGeom.ConvexElems.AddDefaulted_GetRef();
	// Only the two boundary rings are required for the convex; cap centers are render vertices.
	Hull.VertexData.Append(Geometry.Vertices.GetData(), Geometry.Outline.Num() * 2);
	Hull.UpdateElemBox();
	Setup->InvalidatePhysicsData();
	Setup->CreatePhysicsMeshes();
	if (Setup->AggGeom.GetScaledVolume(FVector::OneVector) <= 0)
	{
		return Fail(Error, TEXT("Chaos convex cooking failed. See Output Log."));
	}
	Mesh.SetCustomizedCollision(true);
	WriteRecipe(Mesh, P);
	return true;
}

bool HALVehicleCollision::ReadRecipe(UStaticMesh& Mesh, FHALVehicleCollisionParameters& Out, FString& Error)
{
	auto& Data = Mesh.GetOutermost()->GetMetaData();
	int32 Version = 0;
	if (!LexTryParseString(Version, *Data.GetValue(&Mesh, TEXT("HALCollision.Version"))) || Version != RecipeVersion)
	{
		return Fail(Error, TEXT("Select a mesh generated by this version of the HAL Collision Generator."));
	}
	FHALVehicleCollisionParameters P;
	const auto ReadFloat = [&](const TCHAR* Key, float& Value) { return LexTryParseString(Value, *Data.GetValue(&Mesh, Key)); };
	if (!ReadFloat(TEXT("HALCollision.LengthCm"), P.LengthCm) || !ReadFloat(TEXT("HALCollision.WidthCm"), P.WidthCm)
		|| !ReadFloat(TEXT("HALCollision.HeightCm"), P.HeightCm) || !ReadFloat(TEXT("HALCollision.FrontRadiusCm"), P.FrontRadiusCm)
		|| !ReadFloat(TEXT("HALCollision.RearRadiusCm"), P.RearRadiusCm)
		|| !LexTryParseString(P.SegmentsPerCorner, *Data.GetValue(&Mesh, TEXT("HALCollision.SegmentsPerCorner"))))
	{
		return Fail(Error, TEXT("The saved generator recipe is incomplete."));
	}
	if (!P.Validate(Error)) { return false; }
	Out = P;
	Error.Reset();
	return true;
}

bool HALVehicleCollision::ValidateOutput(const FString& Folder, const FString& Name, FString& Error)
{
	if (!Folder.StartsWith(TEXT("/Game/")) || Folder.EndsWith(TEXT("/")) || Name.IsEmpty() || Name.Contains(TEXT("/")))
	{
		return Fail(Error, TEXT("Output folder must be under /Game/ without a trailing slash; asset name must be a single name."));
	}
	FText Reason;
	const FString PackageName = Folder / Name;
	if (!FPackageName::IsValidLongPackageName(PackageName, false, &Reason)
		|| !FPackageName::IsValidObjectPath(PackageName + TEXT(".") + Name, &Reason))
	{
		Error = Reason.ToString();
		return false;
	}
	Error.Reset();
	return true;
}

UStaticMesh* HALVehicleCollision::CreateAsset(const UHALVehicleCollisionGeneratorSettings& Settings, FString& Error)
{
	if (!Settings.Shape.Validate(Error) || !ValidateOutput(Settings.OutputFolder, Settings.AssetName, Error)) { return nullptr; }
	FString PackageName, AssetName;
	FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateUniqueAssetName(
		Settings.OutputFolder / Settings.AssetName, TEXT(""), PackageName, AssetName);
	UPackage* Package = CreatePackage(*PackageName);
	UStaticMesh* Mesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!BuildMesh(*Mesh, Settings.Shape, Error))
	{
		Mesh->ClearFlags(RF_Public | RF_Standalone);
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Mesh);
	Mesh->MarkPackageDirty();
	return Mesh;
}
