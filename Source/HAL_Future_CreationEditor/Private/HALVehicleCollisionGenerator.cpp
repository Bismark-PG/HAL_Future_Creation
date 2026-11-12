#include "HALVehicleCollisionGenerator.h"
#include "HALVehicleAlignment.h"
#include "VehicleDefinition.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ComponentReregisterContext.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "CompGeom/ConvexHull3.h"

namespace
{
	constexpr int32 RecipeVersion = 4;
	bool Fail(FString& Error, const TCHAR* Message) { Error = Message; return false; }
	void WriteRecipe(UStaticMesh& Mesh, const FHALVehicleCollisionParameters& P)
	{
		auto& Data = Mesh.GetOutermost()->GetMetaData();
		Data.SetValue(&Mesh, TEXT("HALCollision.Version"), *LexToString(RecipeVersion));
		Data.SetValue(&Mesh, TEXT("HALCollision.GeneratorRevision"), TEXT("5"));
		const auto SetFloat = [&](const TCHAR* Key, float Value) {
			Data.SetValue(&Mesh, Key, *FString::Printf(TEXT("%.9g"), Value));
		};
		Data.RemoveValue(&Mesh, TEXT("HALCollision.LengthCm"));
		SetFloat(TEXT("HALCollision.FrontCm"), P.FrontCm);
		SetFloat(TEXT("HALCollision.RearCm"), P.RearCm);
		SetFloat(TEXT("HALCollision.WidthCm"), P.WidthCm);
		SetFloat(TEXT("HALCollision.HeightCm"), P.HeightCm);
		SetFloat(TEXT("HALCollision.FrontRadiusCm"), P.FrontRadiusCm);
		SetFloat(TEXT("HALCollision.RearRadiusCm"), P.RearRadiusCm);
		SetFloat(TEXT("HALCollision.FrontSideRadiusCm"), P.FrontSideRadiusCm);
		SetFloat(TEXT("HALCollision.RearSideRadiusCm"), P.RearSideRadiusCm);
		Data.SetValue(&Mesh, TEXT("HALCollision.SegmentsPerCorner"), *LexToString(P.SegmentsPerCorner));
		if (P.BottomZCm.IsSet())
		{
			Data.SetValue(&Mesh, TEXT("HALCollision.BottomZCm"), *FString::Printf(TEXT("%.17g"), P.BottomZCm.GetValue()));
		}
		else
		{
			Data.RemoveValue(&Mesh, TEXT("HALCollision.BottomZCm"));
		}
	}

	void BuildRoundedOutline(double MinX, double MaxX, double HalfAcross, double FrontRadius,
		double RearRadius, int32 Segments, TArray<FVector2D>& Outline)
	{
		// Fit rounding inside the authored end planes. Keep the radius ratio when the ends overlap.
		const double Length = MaxX - MinX;
		const double RadiusSum = FrontRadius + RearRadius;
		if (RadiusSum > Length)
		{
			FrontRadius = Length * (FrontRadius / RadiusSum);
			RearRadius = Length - FrontRadius;
		}
		const auto AddPoint = [&](const FVector2D& Point) {
			if (Outline.IsEmpty() || !Outline.Last().Equals(Point, 0.00001)) { Outline.Add(Point); }
		};
		const auto AddCorner = [&](double EndX, double SignX, double SignAcross, double Radius, double StartAngle) {
			const FVector2D Corner(EndX, SignAcross * HalfAcross);
			if (Radius == 0) { AddPoint(Corner); return; }
			const FVector2D Center = Corner - FVector2D(SignX * Radius, SignAcross * Radius);
			for (int32 I = 0; I <= Segments; ++I)
			{
				const double Angle = FMath::DegreesToRadians(StartAngle + 90.0 * I / Segments);
				AddPoint(Center + Radius * FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)));
			}
		};
		// Counterclockwise. Merge coincident endpoints where arcs meet.
		AddCorner(MaxX, 1, 1, FrontRadius, 0);
		AddCorner(MinX, -1, 1, RearRadius, 90);
		AddCorner(MinX, -1, -1, RearRadius, 180);
		AddCorner(MaxX, 1, -1, FrontRadius, 270);
		if (Outline[0].Equals(Outline.Last(), 0.00001)) { Outline.Pop(); }
	}
}

bool FHALVehicleCollisionParameters::Validate(FString& Error) const
{
	Error.Reset();
	const double Length = GetLengthCm();
	if (!FMath::IsFinite(FrontCm) || !FMath::IsFinite(RearCm) || FrontCm < 0 || RearCm < 0
		|| Length < 1 || Length > 10000)
	{
		return Fail(Error, TEXT("Front and Rear must be finite and non-negative, with a combined length of 1 to 10000 cm."));
	}
	for (const float Dimension : { WidthCm, HeightCm })
	{
		if (!FMath::IsFinite(Dimension) || Dimension < 1 || Dimension > 10000)
		{
			return Fail(Error, TEXT("Width and height must be finite and between 1 and 10000 cm."));
		}
	}
	if (BottomZCm.IsSet())
	{
		const double Bottom = BottomZCm.GetValue();
		const double Top = Bottom + HeightCm;
		if (!FMath::IsFinite(Bottom) || FMath::Abs(Bottom) > MAX_flt || FMath::Abs(Top) > MAX_flt
			|| static_cast<float>(Top) <= static_cast<float>(Bottom))
		{
			return Fail(Error, TEXT("The wheel bottom and collision height must produce distinct, finite mesh coordinates."));
		}
	}
	for (const float Radius : { FrontRadiusCm, RearRadiusCm })
	{
		if (!FMath::IsFinite(Radius) || Radius < 0 || Radius > WidthCm * 0.5f)
		{
			return Fail(Error, TEXT("Each corner radius must be between 0 and width / 2."));
		}
	}
	for (const float Radius:{FrontSideRadiusCm,RearSideRadiusCm})
	{
		if (!FMath::IsFinite(Radius) || Radius<0 || Radius>HeightCm*0.5f)
		{ return Fail(Error,TEXT("Side radii must be finite and between 0 and height / 2.")); }
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
	const double MinX = -P.RearCm, MaxX = P.FrontCm;
	const double CenterX = (MinX + MaxX) * 0.5, Y = P.WidthCm * 0.5, Z = P.HeightCm * 0.5;
	BuildRoundedOutline(MinX, MaxX, Y, P.FrontRadiusCm, P.RearRadiusCm, P.SegmentsPerCorner, Out.Outline);
	BuildRoundedOutline(MinX, MaxX, Z, P.FrontSideRadiusCm, P.RearSideRadiusCm, P.SegmentsPerCorner, Out.SideOutline);
	const int32 N = Out.Outline.Num();
	for (const double Height : { -Z, Z })
	{
		for (const FVector2D& Point : Out.Outline) { Out.Vertices.Emplace(Point.X, Point.Y, Height); }
	}
	const int32 Bottom = Out.Vertices.Add(FVector(CenterX, 0, -Z));
	const int32 Top = Out.Vertices.Add(FVector(CenterX, 0, Z));
	for (int32 I = 0; I < N; ++I)
	{
		const int32 J = (I + 1) % N;
		Out.Triangles.Emplace(Top, N + I, N + J);
		Out.Triangles.Emplace(Bottom, J, I);
		Out.Triangles.Emplace(I, J, N + J);
		Out.Triangles.Emplace(I, N + J, N + I);
	}
	if (P.FrontSideRadiusCm>0 || P.RearSideRadiusCm>0)
	{
		// Intersect the XY prism with the rounded XZ profile.
		TArray<double> Heights;
		const auto AddHeight=[&](double Value)
		{ if (!Heights.ContainsByPredicate([&](double V){return FMath::IsNearlyEqual(V,Value,1.e-6);})) { Heights.Add(Value); } };
		for (const auto& Point:Out.SideOutline) { AddHeight(Point.Y); }
		AddHeight(0); Heights.Sort();
		TArray<FVector> Points;
		for (const double Height:Heights)
		{
			double RingMinX=TNumericLimits<double>::Max(),RingMaxX=-TNumericLimits<double>::Max();
			for (int32 I=0;I<Out.SideOutline.Num();++I)
			{
				const auto A=Out.SideOutline[I],B=Out.SideOutline[(I+1)%Out.SideOutline.Num()];
				if (FMath::IsNearlyEqual(A.Y,Height,1.e-7)) { RingMinX=FMath::Min(RingMinX,A.X); RingMaxX=FMath::Max(RingMaxX,A.X); }
				if (FMath::Abs(B.Y-A.Y)>1.e-7 && Height>=FMath::Min(A.Y,B.Y) && Height<=FMath::Max(A.Y,B.Y))
				{ const double V=FMath::Lerp(A.X,B.X,(Height-A.Y)/(B.Y-A.Y)); RingMinX=FMath::Min(RingMinX,V); RingMaxX=FMath::Max(RingMaxX,V); }
			}
			TArray<FVector2D> Ring=Out.Outline;
			for (const bool KeepLess:{true,false})
			{
				const double Limit=KeepLess?RingMaxX:RingMinX;
				TArray<FVector2D> Clipped;
				for (int32 I=0;I<Ring.Num();++I)
				{
					const auto A=Ring[I],B=Ring[(I+1)%Ring.Num()];
					const bool InA=KeepLess?A.X<=Limit:A.X>=Limit,InB=KeepLess?B.X<=Limit:B.X>=Limit;
					if (InA) { Clipped.Add(A); }
					if (InA!=InB) { Clipped.Add(FMath::Lerp(A,B,(Limit-A.X)/(B.X-A.X))); }
				}
				Ring=MoveTemp(Clipped);
			}
			for (const auto& Point:Ring)
			{
				// Arc seams and clipping endpoints can differ by roundoff; weld BEFORE hull
				// triangulation so near-identical rings cannot create zero-area sliver faces.
				const FVector V(Point.X,Point.Y,Height);
				if (!Points.ContainsByPredicate([&](const FVector& Q){return Q.Equals(V,1.e-6);})) { Points.Add(V); }
			}
		}
		UE::Geometry::FConvexHull3d Hull;
		Hull.bSaveTriangleNeighbors=true;
		Hull.SimplificationSettings.DegenerateEdgeTolerance=1.e-6;
		Hull.SimplificationSettings.SkipAtHullDistanceAbsolute=1.e-6;
		if (!Hull.Solve(Points.Num(),[&](int32 I){return Points[I];})) { return Fail(Error,TEXT("Could not build rounded convex hull.")); }
		Out.Vertices.Reset(); Out.Triangles.Reset(); TMap<int32,int32> Remap;
		const auto Index=[&](int32 Original) { if (const auto* Found=Remap.Find(Original)) { return *Found; }
			const int32 Value=Out.Vertices.Add(Points[Original]); Remap.Add(Original,Value); return Value; };
		FVector Center=FVector::ZeroVector; for (const auto& Point:Points) { Center+=Point; } Center/=Points.Num();
		// Merge coplanar facets and discard collinear hull-edge samples before render
		// triangulation. Exact-predicate hulls otherwise retain roundoff-sized slivers.
		TArray<UE::Geometry::FConvexHull3d::FPolygonFace> Faces;
		Hull.GetSimplifiedFaces(Faces,[&](int32 I){return Points[I];},0.01,1.e-6);
		for (const auto& Face:Faces)
		{
			for (int32 I=1;I+1<Face.Num();++I)
			{
				FIntVector Triangle(Index(Face[0]),Index(Face[I]),Index(Face[I+1]));
				const auto A=Out.Vertices[Triangle.X],B=Out.Vertices[Triangle.Y],C=Out.Vertices[Triangle.Z];
				if (FVector::DotProduct(FVector::CrossProduct(B-A,C-A),A-Center)<0) { Swap(Triangle.Y,Triangle.Z); }
				Out.Triangles.Add(Triangle);
			}
		}
	}
	if (P.BottomZCm.IsSet())
	{
		const double WheelBottom = P.BottomZCm.GetValue();
		for (FVector& Vertex : Out.Vertices) { Vertex.Z = WheelBottom + (Vertex.Z + Z); }
		for (FVector2D& Point : Out.SideOutline) { Point.Y = WheelBottom + (Point.Y + Z); }
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
		// Geometry uses the mathematical cross(B-A, C-A) convention. UE derives
		// a face normal with cross(C-A, B-A), so reverse render winding while
		// keeping the outward normal and the independent convex vertices unchanged.
		for (const int32 Index : { Triangle.X, Triangle.Z, Triangle.Y })
		{
			const FVertexInstanceID ID = Description.CreateVertexInstance(IDs[Index]);
			Instances.Add(ID);
			Normals[ID] = Normal;
			Tangents[ID] = Tangent;
			Signs[ID] = 1;
			Colors[ID] = FVector4f(1, 1, 1, 1);
			UVs.Set(ID, 0, FVector2f((Geometry.Vertices[Index].X + P.RearCm) / P.GetLengthCm(),
				Geometry.Vertices[Index].Y / P.WidthCm + 0.5));
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
	// Side rounding produces a hull with more than two rings. Flat caps have two extra render-only center vertices.
	const int32 HullVertexCount = P.FrontSideRadiusCm > 0 || P.RearSideRadiusCm > 0
		? Geometry.Vertices.Num() : Geometry.Vertices.Num() - 2;
	Hull.VertexData.Append(Geometry.Vertices.GetData(), HullVertexCount);
	Hull.UpdateElemBox();
	Setup->InvalidatePhysicsData();
	Setup->CreatePhysicsMeshes();
	if (Setup->bFailedToCreatePhysicsMeshes || Setup->AggGeom.GetScaledVolume(FVector::OneVector) <= 0)
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
	if (!LexTryParseString(Version, *Data.GetValue(&Mesh, TEXT("HALCollision.Version"))) || Version < 1 || Version > RecipeVersion)
	{
		return Fail(Error, TEXT("Select a mesh generated by this version of the HAL Collision Generator."));
	}
	FHALVehicleCollisionParameters P;
	const auto ReadFloat = [&](const TCHAR* Key, float& Value) { return LexTryParseString(Value, *Data.GetValue(&Mesh, Key)); };
	if (!ReadFloat(TEXT("HALCollision.WidthCm"), P.WidthCm)
		|| !ReadFloat(TEXT("HALCollision.HeightCm"), P.HeightCm) || !ReadFloat(TEXT("HALCollision.FrontRadiusCm"), P.FrontRadiusCm)
		|| !ReadFloat(TEXT("HALCollision.RearRadiusCm"), P.RearRadiusCm)
		|| !LexTryParseString(P.SegmentsPerCorner, *Data.GetValue(&Mesh, TEXT("HALCollision.SegmentsPerCorner"))))
	{
		return Fail(Error, TEXT("The saved generator recipe is incomplete."));
	}
	if (Version < 4)
	{
		float LegacyLength = 0;
		if (!ReadFloat(TEXT("HALCollision.LengthCm"), LegacyLength))
		{ return Fail(Error, TEXT("The saved centered length is missing or invalid.")); }
		P.FrontCm = P.RearCm = LegacyLength * 0.5f;
	}
	else if (!ReadFloat(TEXT("HALCollision.FrontCm"), P.FrontCm) || !ReadFloat(TEXT("HALCollision.RearCm"), P.RearCm))
	{ return Fail(Error, TEXT("The saved Front or Rear distance is missing or invalid.")); }
	if (Version>=2 && (!ReadFloat(TEXT("HALCollision.FrontSideRadiusCm"),P.FrontSideRadiusCm)
		|| !ReadFloat(TEXT("HALCollision.RearSideRadiusCm"),P.RearSideRadiusCm)))
	{ return Fail(Error,TEXT("The saved side-radius recipe is incomplete.")); }
	if (Version >= 3)
	{
		const FString BottomText = Data.GetValue(&Mesh, TEXT("HALCollision.BottomZCm"));
		if (!BottomText.IsEmpty())
		{
			double Bottom;
			if (!LexTryParseString(Bottom, *BottomText)) { return Fail(Error, TEXT("The saved wheel bottom Z is invalid.")); }
			P.BottomZCm = Bottom;
		}
	}
	if (!P.Validate(Error)) { return false; }
	Out = P;
	Error.Reset();
	return true;
}

bool HALVehicleCollision::CanUpdateAsset(UStaticMesh& Mesh, FString& Error)
{
	if (GEditor && (GEditor->PlayWorld || GEditor->bIsSimulatingInEditor))
	{
		return Fail(Error, TEXT("Stop PIE / simulation before updating collision assets."));
	}
	if (!Mesh.GetOutermost()->GetName().StartsWith(TEXT("/Game/")) || Mesh.HasAnyFlags(RF_Transient)
		|| !Mesh.HasAllFlags(RF_Public | RF_Standalone) || Mesh.GetOutermost()->HasAnyPackageFlags(PKG_Cooked))
	{
		return Fail(Error, TEXT("Update requires an editable generated Static Mesh asset under /Game/."));
	}
	FHALVehicleCollisionParameters Recipe;
	if (!ReadRecipe(Mesh, Recipe, Error)) { return false; }
	if (Mesh.GetNumSourceModels() != 1 || !Mesh.GetMeshDescription(0) || !Mesh.GetBodySetup()
		|| Mesh.GetStaticMaterials().IsEmpty())
	{
		return Fail(Error, TEXT("Update requires the generated mesh's editable source LOD, material slot and collision body."));
	}
	if (!Mesh.GetSourceModel(0).BuildSettings.BuildScale3D.Equals(FVector::OneVector)
		|| !Mesh.GetBodySetup()->BuildScale3D.Equals(FVector::OneVector))
	{
		return Fail(Error, TEXT("Reset the mesh Build Scale to (1,1,1) before updating dimensions in cm."));
	}
	return true;
}

bool HALVehicleCollision::ResolveUpdateAnchor(UStaticMesh& Mesh, FHALVehicleCollisionParameters& P, FString& Error)
{
	TSet<UVehicleDefinition*> Definitions;
	if (!HALVehicleAlignment::FindVehicleDefinitions(Definitions, Error)) { return false; }
	UVehicleDefinition* First = nullptr;
	double Lowest = 0, Highest = 0;
	for (UVehicleDefinition* Definition : Definitions)
	{
		if (Definition->CollisionShape != EVehicleCollisionShape::ConvexMesh || Definition->CollisionMesh != &Mesh) { continue; }
		HALVehicleAlignment::FReport Report;
		if (!HALVehicleAlignment::AnalyzeWheels(*Definition, Report))
		{
			Error = FString::Printf(TEXT("Cannot measure wheel bottoms for %s: %s"), *Definition->GetName(), *Report.Error);
			return false;
		}
		if (!First)
		{
			First = Definition;
			Lowest = Highest = Report.WheelBottomZ;
		}
		Lowest = FMath::Min(Lowest, Report.WheelBottomZ);
		Highest = FMath::Max(Highest, Report.WheelBottomZ);
		if (Highest - Lowest > 0.0001)
		{
			Error = FString::Printf(TEXT("%s and %s use this mesh with different wheel-bottom heights. Load the intended Vehicle Definition and create a separate mesh."),
				*First->GetName(), *Definition->GetName());
			return false;
		}
	}
	if (First) { P.BottomZCm = Lowest; }
	else if (!P.BottomZCm.IsSet())
	{
		FHALVehicleCollisionParameters Saved;
		if (!ReadRecipe(Mesh, Saved, Error)) { return false; }
		P.BottomZCm = Saved.BottomZCm;
	}
	return true;
}

bool HALVehicleCollision::UpdateAsset(UStaticMesh& Mesh, const FHALVehicleCollisionParameters& Parameters, FString& Error)
{
	FHALVehicleCollisionParameters P = Parameters;
	if (!CanUpdateAsset(Mesh, Error) || !ResolveUpdateAnchor(Mesh, P, Error) || !P.Validate(Error)) { return false; }
	// All predictable generation/cooking failures happen before touching the target.
	TStrongObjectPtr<UStaticMesh> Candidate(NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient));
	if (!BuildMesh(*Candidate, P, Error)) { return false; }
	FStaticMeshCompilingManager::Get().FinishCompilation({ &Mesh, Candidate.Get() });
	FMeshDescription Description(*Candidate->GetMeshDescription(0));
	// Retain the asset's material assignment, including a user-renamed slot.
	FStaticMeshAttributes Attributes(Description);
	const FStaticMaterial& Material = Mesh.GetStaticMaterials()[0];
	for (const FPolygonGroupID Group : Description.PolygonGroups().GetElementIDs())
	{
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = Material.ImportedMaterialSlotName.IsNone()
			? Material.MaterialSlotName : Material.ImportedMaterialSlotName;
	}
	const FMeshDescription PreviousDescription(*Mesh.GetMeshDescription(0));
	UBodySetup* Setup = Mesh.GetBodySetup();
	const FKAggregateGeom PreviousGeometry = Setup->AggGeom;
	const auto PreviousTraceFlag = Setup->CollisionTraceFlag;
	const bool bWasDirty = Mesh.GetOutermost()->IsDirty();
	const bool bWasCustomized = Mesh.GetCustomizedCollision();
	const bool bWasNeverStream = Mesh.NeverStream;
	const FMeshSectionInfoMap PreviousSections = Mesh.GetSectionInfoMap();

	// Detach live editor components until BOTH rendering and Chaos have the new data.
	// Their object references, transforms, overrides and package dirty flags stay unchanged.
	TArray<TUniquePtr<FComponentReregisterContext>> ComponentContexts;
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		if (It->GetStaticMesh() == &Mesh && It->IsRegistered())
		{
			ComponentContexts.Add(MakeUnique<FComponentReregisterContext>(*It));
		}
	}
	const auto Apply = [&](const FMeshDescription& Source, const FKAggregateGeom& Geometry, ECollisionTraceFlag TraceFlag)
	{
		Setup->InvalidatePhysicsData();
		Setup->AggGeom = Geometry;
		Setup->CollisionTraceFlag = TraceFlag;
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		const bool bBuilt = Mesh.BuildFromMeshDescriptions({ &Source }, Params);
		FStaticMeshCompilingManager::Get().FinishCompilation({ &Mesh });
		Setup->CreatePhysicsMeshes();
		return bBuilt && !Setup->bFailedToCreatePhysicsMeshes && Setup->AggGeom.GetScaledVolume(FVector::OneVector) > 0;
	};
	if (!Apply(Description, Candidate->GetBodySetup()->AggGeom, CTF_UseSimpleAndComplex))
	{
		const bool bRestored = Apply(PreviousDescription, PreviousGeometry, PreviousTraceFlag);
		Mesh.NeverStream = bWasNeverStream;
		Mesh.GetSectionInfoMap() = PreviousSections;
		Mesh.SetCustomizedCollision(bWasCustomized);
		Mesh.GetOutermost()->SetDirtyFlag(bWasDirty);
		Mesh.GetOnMeshChanged().Broadcast();
		return Fail(Error, bRestored ? TEXT("Update failed; the previous mesh and collision were restored. See Output Log.")
			: TEXT("Update and restoration failed. Reload this asset from disk before use. See Output Log."));
	}
	Mesh.SetCustomizedCollision(true);
	WriteRecipe(Mesh, P);
	Mesh.MarkPackageDirty();
	Mesh.GetOnMeshChanged().Broadcast();
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
