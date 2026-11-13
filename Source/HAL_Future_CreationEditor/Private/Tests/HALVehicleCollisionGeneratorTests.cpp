#if WITH_DEV_AUTOMATION_TESTS

#include "HALVehicleCollisionGenerator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Materials/Material.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "VehicleDefinition.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCollisionSideRadiusTest,"HAL.Editor.CollisionGenerator.SideRoundedConvexAndLegacyRecipe",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALCollisionSideRadiusTest::RunTest(const FString&)
{
	FString Error; FHALVehicleCollisionParameters Bad;
	Bad.FrontSideRadiusCm=36; TestFalse(TEXT("Side radius bounded by half height"),Bad.Validate(Error));
	Bad.FrontSideRadiusCm=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Nonfinite side radius rejected"),Bad.Validate(Error));
	int32 MaxVertices=0;
	for (const int32 Segments:{1,4,8})
	{
		for (const FVector2D Side:{FVector2D(0,20),FVector2D(35,35),FVector2D(10,30)})
		{
			FHALVehicleCollisionParameters P; P.FrontSideRadiusCm=Side.X; P.RearSideRadiusCm=Side.Y; P.SegmentsPerCorner=Segments;
			HALVehicleCollision::FGeometry G;
			if (!TestTrue(TEXT("Rounded geometry built"),HALVehicleCollision::BuildGeometry(P,G,Error))) { AddError(Error); return false; }
			MaxVertices=FMath::Max(MaxVertices,G.Vertices.Num());
			FBox Bounds(ForceInit); for (const auto& V:G.Vertices) { Bounds+=V; }
			TestTrue(TEXT("Full bounds and centered pivot retained"),Bounds.GetSize().Equals(FVector(P.GetLengthCm(),P.WidthCm,P.HeightCm),0.001) && Bounds.GetCenter().IsNearlyZero(0.001));
			TMap<FIntPoint,int32> Edges; double Volume=0,MinAreaSquared=TNumericLimits<double>::Max();
			for (const auto& T:G.Triangles)
			{
				const auto A=G.Vertices[T.X],B=G.Vertices[T.Y],C=G.Vertices[T.Z]; const auto N=FVector::CrossProduct(B-A,C-A);
				MinAreaSquared=FMath::Min(MinAreaSquared,N.SizeSquared());
				for (const auto& V:G.Vertices) { TestTrue(TEXT("All vertices inside every convex face"),FVector::DotProduct(N.GetSafeNormal(),V-A)<0.001); }
				Volume+=FVector::DotProduct(A,FVector::CrossProduct(B,C))/6;
				for (const auto E:{FIntPoint(T.X,T.Y),FIntPoint(T.Y,T.Z),FIntPoint(T.Z,T.X)})
				{ ++Edges.FindOrAdd(FIntPoint(FMath::Min(E.X,E.Y),FMath::Max(E.X,E.Y))); }
			}
			for (const auto& E:Edges) { TestEqual(TEXT("Rounded manifold closed"),E.Value,2); }
			TestTrue(FString::Printf(TEXT("Rounded faces nondegenerate: segments=%d side=(%.1f,%.1f) minAreaSq=%.9g"),Segments,Side.X,Side.Y,MinAreaSquared),MinAreaSquared>1.e-8);
			TestTrue(TEXT("Positive outward volume"),Volume>0);
			for (const auto& V:G.Vertices)
			{
				if (FMath::IsNearlyEqual(V.X,double(P.FrontCm),0.001))
				{ TestTrue(TEXT("Front flat end height shrinks by authored rounding"),FMath::Abs(V.Z)<=P.HeightCm/2-P.FrontSideRadiusCm+0.001); }
			}
		}
	}
	TestTrue(TEXT("Bounded single-convex vertex count"),MaxVertices<=256);
	FHALVehicleCollisionParameters P; P.FrontSideRadiusCm=35; P.RearSideRadiusCm=20;
	TStrongObjectPtr<UStaticMesh> Mesh(NewObject<UStaticMesh>(GetTransientPackage()));
	if (!TestTrue(TEXT("Rounded render and physics mesh build"),HALVehicleCollision::BuildMesh(*Mesh,P,Error))) { AddError(Error); return false; }
	TestEqual(TEXT("Still one simple physics hull"),Mesh->GetBodySetup()->AggGeom.ConvexElems.Num(),1);
	FHALVehicleCollisionParameters Loaded;
	TestTrue(TEXT("New recipe reads"),HALVehicleCollision::ReadRecipe(*Mesh,Loaded,Error));
	TestEqual(TEXT("Side radius roundtrip"),Loaded.FrontSideRadiusCm,P.FrontSideRadiusCm);
	auto& Metadata = Mesh->GetOutermost()->GetMetaData();
	Metadata.SetValue(Mesh.Get(), TEXT("HALCollision.Version"), TEXT("1"));
	Metadata.SetValue(Mesh.Get(), TEXT("HALCollision.LengthCm"), TEXT("201.5"));
	TestTrue(TEXT("Old recipe still reads"),HALVehicleCollision::ReadRecipe(*Mesh,Loaded,Error));
	TestEqual(TEXT("Old recipe has zero side rounding"),Loaded.FrontSideRadiusCm,0.f);
	TestEqual(TEXT("Legacy centered length loads as half on each end"), Loaded.FrontCm, 100.75f);
	TestEqual(TEXT("Legacy rear matches front"), Loaded.RearCm, Loaded.FrontCm);
	Metadata.SetValue(Mesh.Get(), TEXT("HALCollision.Version"), TEXT("3"));
	Metadata.SetValue(Mesh.Get(), TEXT("HALCollision.BottomZCm"), TEXT("-31.25"));
	TestTrue(TEXT("Wheel-anchored legacy recipe still reads"), HALVehicleCollision::ReadRecipe(*Mesh, Loaded, Error));
	TestTrue(TEXT("Legacy side radius and wheel anchor retained"), Loaded.FrontSideRadiusCm == 35.f
		&& Loaded.BottomZCm.IsSet() && Loaded.BottomZCm.GetValue() == -31.25);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCollisionParameterTest, "HAL.Editor.CollisionGenerator.ParameterValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCollisionParameterTest::RunTest(const FString&)
{
	FString Error;
	FHALVehicleCollisionParameters P;
	TestTrue(TEXT("Default stadium is valid"), P.Validate(Error));
	P.FrontRadiusCm = 0; P.RearRadiusCm = 0;
	TestTrue(TEXT("Zero radius gives a valid square end"), P.Validate(Error));
	P.FrontRadiusCm = -1;
	TestFalse(TEXT("Negative radius rejected"), P.Validate(Error));
	P = {}; P.FrontRadiusCm = P.WidthCm / 2 + 1;
	TestFalse(TEXT("Radius cannot exceed half width"), P.Validate(Error));
	P = {}; P.FrontCm = -1;
	TestFalse(TEXT("Negative end distance rejected"), P.Validate(Error));
	P = {}; P.FrontCm = P.RearCm = 0;
	TestFalse(TEXT("Zero longitudinal span rejected"), P.Validate(Error));
	P = {}; P.HeightCm = 0;
	TestFalse(TEXT("Flat volume rejected"), P.Validate(Error));
	P = {}; P.RearCm = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Infinite dimension rejected"), P.Validate(Error));
	P = {}; P.RearRadiusCm = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN radius rejected"), P.Validate(Error));
	P = {}; P.SegmentsPerCorner = 9;
	TestFalse(TEXT("Vertex budget is bounded"), P.Validate(Error));
	TestTrue(TEXT("Project output path"), HALVehicleCollision::ValidateOutput(TEXT("/Game/Meshes/Collision"), TEXT("SM_Probe"), Error));
	for (const FString Folder : { TEXT("/Engine/Meshes"), TEXT("C:/Meshes"), TEXT("/Game/Meshes/"), TEXT("/Game/../Engine") })
	{
		TestFalse(TEXT("Nonproject or invalid output rejected"), HALVehicleCollision::ValidateOutput(Folder, TEXT("SM_Probe"), Error));
	}
	TestFalse(TEXT("Asset name is not a nested path"), HALVehicleCollision::ValidateOutput(TEXT("/Game/Meshes"), TEXT("Sub/Probe"), Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCollisionGeometryTest, "HAL.Editor.CollisionGenerator.ConvexClosedGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCollisionGeometryTest::RunTest(const FString&)
{
	for (const int32 Segments : { 1, 4, 8 })
	{
		for (const FVector2D Radii : { FVector2D(0,0), FVector2D(70,70), FVector2D(50,10), FVector2D(0,70) })
		{
			FHALVehicleCollisionParameters P;
			P.FrontRadiusCm = Radii.X; P.RearRadiusCm = Radii.Y; P.SegmentsPerCorner = Segments;
			HALVehicleCollision::FGeometry G;
			FString Error;
			if (!TestTrue(TEXT("Geometry generated"), HALVehicleCollision::BuildGeometry(P, G, Error))) { return false; }
			const int32 N = G.Outline.Num();
			TestTrue(TEXT("Bounded convex vertex count"), N >= 4 && N <= 36);
			FBox Bounds(ForceInit);
			for (const auto& V : G.Vertices) { TestFalse(TEXT("Finite vertex"), V.ContainsNaN()); Bounds += V; }
			TestTrue(TEXT("Requested full dimensions"), Bounds.GetSize().Equals(FVector(P.GetLengthCm(), P.WidthCm, P.HeightCm), 0.0001));
			TestTrue(TEXT("Centered pivot"), Bounds.GetCenter().IsNearlyZero(0.0001));
			double TwiceArea = 0, Volume = 0;
			for (int32 I = 0; I < N; ++I)
			{
				const auto& A = G.Outline[I]; const auto& B = G.Outline[(I + 1) % N];
				TwiceArea += A.X * B.Y - A.Y * B.X;
				for (const auto& Q : G.Outline)
				{
					TestTrue(TEXT("Every point is inside each CCW edge: no concavity"), FVector2D::CrossProduct(B-A, Q-A) >= -0.0001);
				}
				TestEqual(TEXT("Flat bottom"), G.Vertices[I].Z, -P.HeightCm * 0.5, 0.0001);
				TestEqual(TEXT("Flat top"), G.Vertices[N + I].Z, P.HeightCm * 0.5, 0.0001);
			}
			TMap<FIntPoint, int32> EdgeCounts;
			for (const auto& T : G.Triangles)
			{
				const auto& A = G.Vertices[T.X]; const auto& B = G.Vertices[T.Y]; const auto& C = G.Vertices[T.Z];
				TestTrue(TEXT("Nondegenerate triangle"), FVector::CrossProduct(B-A,C-A).SizeSquared() > 0.000001);
				Volume += FVector::DotProduct(A, FVector::CrossProduct(B,C)) / 6;
				for (const auto Edge : { FIntPoint(T.X,T.Y), FIntPoint(T.Y,T.Z), FIntPoint(T.Z,T.X) })
				{
					++EdgeCounts.FindOrAdd(FIntPoint(FMath::Min(Edge.X,Edge.Y), FMath::Max(Edge.X,Edge.Y)));
				}
			}
			for (const auto& Pair : EdgeCounts) { TestEqual(TEXT("Closed manifold edge"), Pair.Value, 2); }
			TestTrue(TEXT("Outward winding and volume equal extruded outline"), FMath::IsNearlyEqual(Volume, TwiceArea * 0.5 * P.HeightCm, 0.001));
		}
	}
	// Arcs can also meet along the sides, not only at front/rear tips.
	FHALVehicleCollisionParameters P; P.FrontCm = P.RearCm = 70;
	HALVehicleCollision::FGeometry G;
	FString Error;
	TestTrue(TEXT("Circle boundary case"), HALVehicleCollision::BuildGeometry(P, G, Error));
	for (int32 I = 0; I < G.Outline.Num(); ++I)
	{
		TestFalse(TEXT("Meeting arcs do not leave duplicate endpoints"), G.Outline[I].Equals(G.Outline[(I+1)%G.Outline.Num()], 0.00001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCollisionEndsTest, "HAL.Editor.CollisionGenerator.IndependentEndPlanes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCollisionEndsTest::RunTest(const FString&)
{
	FHALVehicleCollisionParameters P;
	P.BottomZCm = -45;
	HALVehicleCollision::FGeometry Geometry;
	FString Error;
	const auto CheckGeometry = [&]() {
		if (!TestTrue(TEXT("Build asymmetric geometry"), HALVehicleCollision::BuildGeometry(P, Geometry, Error)))
		{ AddError(Error); return false; }
		for (const auto& Outline : { &Geometry.Outline, &Geometry.SideOutline })
		{
			FBox2D Bounds(ForceInit);
			for (const FVector2D& Vertex : *Outline) { Bounds += Vertex; }
			TestEqual(TEXT("Preview front at requested +X"), Bounds.Max.X, double(P.FrontCm), 0.001);
			TestEqual(TEXT("Preview rear at requested -X"), Bounds.Min.X, -double(P.RearCm), 0.001);
		}
		FBox Bounds(ForceInit);
		for (const FVector& Vertex : Geometry.Vertices) { Bounds += Vertex; }
		TestEqual(TEXT("Geometry front at requested +X"), Bounds.Max.X, double(P.FrontCm), 0.001);
		TestEqual(TEXT("Geometry rear at requested -X"), Bounds.Min.X, -double(P.RearCm), 0.001);
		TestEqual(TEXT("Wheel-bottom anchor stays in place"), Bounds.Min.Z, -45.0, 0.001);
		TestEqual(TEXT("Total height stays in place"), Bounds.Max.Z, 25.0, 0.001);
		for (const FIntVector& Triangle : Geometry.Triangles)
		{
			const FVector& A = Geometry.Vertices[Triangle.X];
			const FVector& B = Geometry.Vertices[Triangle.Y];
			const FVector& C = Geometry.Vertices[Triangle.Z];
			TestTrue(TEXT("Face remains nondegenerate"), FVector::CrossProduct(B - A, C - A).SizeSquared() > 1.e-8);
		}
		return true;
	};
	P.FrontCm = 175; P.RearCm = 60;
	if (!CheckGeometry()) { return false; }
	P.FrontCm = 0;
	if (!CheckGeometry()) { return false; }
	P.FrontSideRadiusCm = 30; P.RearSideRadiusCm = 20;
	P.RearCm = 25; // Both rounding pairs must fit without moving either end plane.
	if (!CheckGeometry()) { return false; }
	TestTrue(TEXT("Shortening fits the side radii proportionally around the end planes"),
		Geometry.SideOutline.ContainsByPredicate([](const FVector2D& Point) { return Point.Equals(FVector2D(-15, 25), 0.001); }));
	P.FrontCm = 175;
	if (!CheckGeometry()) { return false; }
	TestTrue(TEXT("Extending again restores the authored front side radius"),
		Geometry.SideOutline.ContainsByPredicate([](const FVector2D& Point) { return Point.Equals(FVector2D(145, 25), 0.001); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCollisionBodyTest, "HAL.Editor.CollisionGenerator.SinglePhysicsBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCollisionBodyTest::RunTest(const FString&)
{
	TStrongObjectPtr<UStaticMesh> Mesh(NewObject<UStaticMesh>());
	FHALVehicleCollisionParameters P;
	FString Error;
	if (!TestTrue(TEXT("Render source and Chaos body generated"), HALVehicleCollision::BuildMesh(*Mesh, P, Error))) { AddError(Error); return false; }
	const UBodySetup* Setup = Mesh->GetBodySetup();
	TestEqual(TEXT("One simple convex only"), Setup->AggGeom.GetElementCount(), 1);
	TestEqual(TEXT("No redundant box"), Setup->AggGeom.BoxElems.Num(), 0);
	TestEqual(TEXT("One convex"), Setup->AggGeom.ConvexElems.Num(), 1);
	TestTrue(TEXT("Cooked Chaos convex has volume"), Setup->AggGeom.GetScaledVolume(FVector::OneVector) > 0);
	TestEqual(TEXT("Physics-compatible collision mode"), Setup->GetCollisionTraceFlag(), CTF_UseSimpleAndComplex);
	TestNotNull(TEXT("Editable source retained for future rebuild/cook"), Mesh->GetMeshDescription(0));
	TestEqual(TEXT("One material section"), Mesh->GetStaticMaterials().Num(), 1);
	// Exercise the engine's actual face convention, rather than just signed mathematical volume.
	FMeshDescription RenderSource(*Mesh->GetMeshDescription(0));
	FStaticMeshAttributes RenderAttributes(RenderSource);
	RenderAttributes.RegisterTriangleNormalAndTangentAttributes();
	FStaticMeshOperations::ComputeTriangleTangentsAndNormals(RenderSource);
	const auto EngineNormals = RenderAttributes.GetTriangleNormals();
	const auto InstanceNormals = RenderAttributes.GetVertexInstanceNormals();
	for (const FTriangleID Triangle : RenderSource.Triangles().GetElementIDs())
	{
		const auto Instances = RenderSource.GetTriangleVertexInstances(Triangle);
		FVector3f Center = FVector3f::ZeroVector;
		for (const FVertexInstanceID Instance : Instances)
		{
			Center += RenderSource.GetVertexPosition(RenderSource.GetVertexInstanceVertex(Instance)) / 3;
			TestTrue(TEXT("UE-derived face normal matches the stored outward vertex normal"),
				FVector3f::DotProduct(EngineNormals[Triangle], InstanceNormals[Instance]) > 0.99f);
		}
		TestTrue(TEXT("UE renderer sees the outside of the closed convex proxy"), FVector3f::DotProduct(EngineNormals[Triangle], Center) > 0);
	}
	TestFalse(TEXT("Generator refuses to rebuild existing mesh data"), HALVehicleCollision::BuildMesh(*Mesh, P, Error));

	const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
		.ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
	AActor* Actor = World->SpawnActor<AActor>();
	UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(Actor);
	Actor->SetRootComponent(Body);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetStaticMesh(Mesh.Get());
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true);
	Body->BodyInstance.SetMassOverride(800, true);
	Body->RegisterComponent();
	TestTrue(TEXT("Generated mesh creates a real dynamic body"), Body->BodyInstance.IsValidBodyInstance());
	TestTrue(TEXT("Vehicle-compatible simulation"), Body->IsSimulatingPhysics());
	TestEqual(TEXT("Authored mass can be retained"), Body->GetMass(), 800.f, 0.1f);
	AActor* Floor = World->SpawnActor<AActor>();
	UBoxComponent* FloorBody = NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(FloorBody);
	FloorBody->InitBoxExtent(FVector(2000, 2000, 50));
	FloorBody->SetCollisionProfileName(TEXT("BlockAll"));
	FloorBody->SetWorldLocation(FVector(0, 0, -50));
	FloorBody->RegisterComponent();
	TestFalse(TEXT("Old 70 cm body clears a flat floor at center Z=92"),
		World->EncroachingBlockingGeometry(Actor, FVector(0, 0, 92), FRotator::ZeroRotator));
	TStrongObjectPtr<UStaticMesh> TallMesh(NewObject<UStaticMesh>());
	P.FrontCm = P.RearCm = 228; P.WidthCm = 288; P.HeightCm = 269;
	P.FrontRadiusCm = 120; P.RearRadiusCm = 120;
	if (TestTrue(TEXT("Tall centered proxy generated"), HALVehicleCollision::BuildMesh(*TallMesh, P, Error)))
	{
		Body->SetStaticMesh(TallMesh.Get());
		TestTrue(TEXT("269 cm body intersects the floor at center Z=92"),
			World->EncroachingBlockingGeometry(Actor, FVector(0, 0, 92), FRotator::ZeroRotator));
		TestFalse(TEXT("269 cm body clears the floor at center Z=200"),
			World->EncroachingBlockingGeometry(Actor, FVector(0, 0, 200), FRotator::ZeroRotator));
	}
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCollisionAssetTest, "HAL.Editor.CollisionGenerator.RecipePersistenceAndUniqueAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCollisionAssetTest::RunTest(const FString&)
{
	// Saved/ only for the serialization probe. Never save into project Content or edit a team asset.
	const FString ID = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CollisionGenerator/RecipeProbe") / ID) + TEXT("/");
	const FString Mount = TEXT("/Game/__HALCollisionProbe_") + ID + TEXT("/");
	IFileManager::Get().MakeDirectory(*Directory, true);
	FPackageName::RegisterMountPoint(Mount, Directory);
	struct FCleanup
	{
		FString Mount, Directory;
		TArray<UStaticMesh*> Assets;
		~FCleanup()
		{
			for (auto* Mesh : Assets)
			{
				if (!Mesh) { continue; }
				FAssetRegistryModule::AssetDeleted(Mesh);
				Mesh->ClearFlags(RF_Public | RF_Standalone);
				Mesh->GetOutermost()->SetDirtyFlag(false);
			}
			FPackageName::UnRegisterMountPoint(Mount, Directory);
		}
	} Cleanup { Mount, Directory, {} };
	TStrongObjectPtr<UHALVehicleCollisionGeneratorSettings> Settings(NewObject<UHALVehicleCollisionGeneratorSettings>());
	Settings->OutputFolder = Mount.LeftChop(1);
	Settings->AssetName = TEXT("SM_Probe_") + ID;
	Settings->Shape.FrontCm = Settings->Shape.RearCm = 160.5f;
	Settings->Shape.FrontRadiusCm = 35; Settings->Shape.RearRadiusCm = 12;
	FString Error;
	UStaticMesh* First = HALVehicleCollision::CreateAsset(*Settings, Error); Cleanup.Assets.Add(First);
	if (!TestNotNull(TEXT("First new asset"), First)) { AddError(Error); return false; }
	UStaticMesh* Second = HALVehicleCollision::CreateAsset(*Settings, Error); Cleanup.Assets.Add(Second);
	if (!TestNotNull(TEXT("Second new asset"), Second)) { AddError(Error); return false; }
	TestTrue(TEXT("Repeated generation never overwrites"), First->GetPathName() != Second->GetPathName());
	const FString OriginalPath = First->GetPathName();
	Settings->Shape.FrontCm = 250; Settings->Shape.RearCm = 152;
	Settings->Shape.HeightCm = 96;
	Settings->Shape.FrontSideRadiusCm = 35;
	Settings->Shape.RearSideRadiusCm = 18;
	Settings->Shape.BottomZCm = -41.25;
	if (!TestTrue(TEXT("Update original asset before saving"), HALVehicleCollision::UpdateAsset(*First, Settings->Shape, Error))) { AddError(Error); return false; }
	TestEqual(TEXT("Update retains original object path"), First->GetPathName(), OriginalPath);
	FHALVehicleCollisionParameters Untouched;
	TestTrue(TEXT("Other generated version stays readable"), HALVehicleCollision::ReadRecipe(*Second, Untouched, Error));
	TestEqual(TEXT("Other version dimensions untouched"), Untouched.GetLengthCm(), 321.0);
	FHALVehicleCollisionParameters Recipe;
	if (!TestTrue(TEXT("Recipe exists before serialization"), HALVehicleCollision::ReadRecipe(*First, Recipe, Error))) { AddError(Error); return false; }
	const FString SavedAssetName = First->GetName();
	const FString Filename = Directory / (First->GetName() + TEXT(".uasset"));
	FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
	if (!TestTrue(TEXT("Save generated mesh and recipe under Saved"), UPackage::SavePackage(First->GetOutermost(), First, *Filename, Args))) { return false; }
	// Metadata keys are soft object paths. Keep the serialized package path stable, just as Ctrl+S does.
	// Move the original in-memory package aside, so LoadPackage cannot return its cached objects.
	const FString OldObjectPath = First->GetPathName();
	ResetLoaders(First->GetOutermost());
	if (!TestTrue(TEXT("Move memory fixture aside"), First->GetOutermost()->Rename(*(Mount + TEXT("PreviousMemoryPackage")),
		nullptr, REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional))) { return false; }
	FAssetRegistryModule::AssetRenamed(First, OldObjectPath);
	UPackage* Loaded = LoadPackage(nullptr, *(Mount + SavedAssetName), LOAD_None);
	UStaticMesh* Copy = Loaded ? FindObject<UStaticMesh>(Loaded, *SavedAssetName) : nullptr;
	if (!TestNotNull(TEXT("Reload generated asset from disk at the saved package path"), Copy)) { return false; }
	TestTrue(TEXT("Reload does not reuse the in-memory mesh"), Copy != First);
	Cleanup.Assets.Add(Copy);
	TestTrue(TEXT("Read versioned recipe after serialization"), HALVehicleCollision::ReadRecipe(*Copy, Recipe, Error));
	TestEqual(TEXT("Saved front distance"), Recipe.FrontCm, 250.f);
	TestEqual(TEXT("Saved rear distance"), Recipe.RearCm, 152.f);
	TestEqual(TEXT("Saved independent front radius"), Recipe.FrontRadiusCm, 35.f);
	TestEqual(TEXT("Saved independent rear radius"), Recipe.RearRadiusCm, 12.f);
	TestEqual(TEXT("Updated side radius persisted"), Recipe.FrontSideRadiusCm, 35.f);
	TestEqual(TEXT("Updated rear side radius persisted"), Recipe.RearSideRadiusCm, 18.f);
	TestTrue(TEXT("Wheel-bottom anchor persisted"), Recipe.BottomZCm.IsSet() && Recipe.BottomZCm.GetValue() == -41.25);
	TestTrue(TEXT("Updated render bounds persisted"), Copy->GetBounds().BoxExtent.Equals(FVector(201,70,48), 0.01));
	TestEqual(TEXT("Reloaded render front is at +250"), Copy->GetBounds().GetBox().Max.X, 250.0, 0.001);
	TestEqual(TEXT("Reloaded render rear is at -152"), Copy->GetBounds().GetBox().Min.X, -152.0, 0.001);
	TestEqual(TEXT("Reloaded render bottom retains anchor"), Copy->GetBounds().GetBox().Min.Z, -41.25, 0.001);
	TestNotNull(TEXT("Reloaded editable source"), Copy->GetMeshDescription(0));
	TestEqual(TEXT("Reloaded single convex"), Copy->GetBodySetup()->AggGeom.ConvexElems.Num(), 1);
	TestEqual(TEXT("Reloaded collision front is at +250"), Copy->GetBodySetup()->AggGeom.ConvexElems[0].ElemBox.Max.X, 250.0, 0.001);
	TestEqual(TEXT("Reloaded collision rear is at -152"), Copy->GetBodySetup()->AggGeom.ConvexElems[0].ElemBox.Min.X, -152.0, 0.001);
	TestEqual(TEXT("Reloaded physics bottom retains anchor"), Copy->GetBodySetup()->AggGeom.ConvexElems[0].ElemBox.Min.Z, -41.25, 0.001);
	TestTrue(TEXT("Reloaded physics volume"), Copy->GetBodySetup()->AggGeom.GetScaledVolume(FVector::OneVector) > 0);
	Copy->GetOutermost()->GetMetaData().SetValue(Copy, TEXT("HALCollision.Version"), TEXT("999"));
	Recipe.FrontCm = 777;
	TestFalse(TEXT("Unknown future recipe rejected"), HALVehicleCollision::ReadRecipe(*Copy, Recipe, Error));
	TestEqual(TEXT("Failed read preserves panel values"), Recipe.FrontCm, 777.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCollisionUpdateTest, "HAL.Editor.CollisionGenerator.UpdateInPlaceAndRefreshPhysics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCollisionUpdateTest::RunTest(const FString&)
{
	const FString PackageName = TEXT("/Game/__HALCollisionUpdate_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UPackage* Package = CreatePackage(*PackageName);
	TStrongObjectPtr<UStaticMesh> Mesh(NewObject<UStaticMesh>(Package, TEXT("SM_Update"), RF_Public | RF_Standalone));
	struct FCleanup { UStaticMesh* Mesh; ~FCleanup() { Mesh->ClearFlags(RF_Public | RF_Standalone); Mesh->GetOutermost()->SetDirtyFlag(false); } } Cleanup { Mesh.Get() };
	FString Error;
	FHALVehicleCollisionParameters P;
	if (!TestTrue(TEXT("Generated fixture"), HALVehicleCollision::BuildMesh(*Mesh, P, Error))) { AddError(Error); return false; }
	UBodySetup* OriginalSetup = Mesh->GetBodySetup();
	const FString OriginalPath = Mesh->GetPathName();
	const FGuid OriginalGuid = OriginalSetup->BodySetupGuid;
	Mesh->GetStaticMaterials()[0].MaterialSlotName = TEXT("CustomProxySlot");
	Mesh->GetStaticMaterials()[0].ImportedMaterialSlotName = TEXT("CustomProxySlot");
	UMaterialInterface* OriginalMaterial = Mesh->GetStaticMaterials()[0].MaterialInterface;
	OriginalSetup->DefaultInstance.SetMassOverride(910, true);
	Package->GetMetaData().SetValue(Mesh.Get(), TEXT("User.Note"), TEXT("preserve me"));
	TStrongObjectPtr<UVehicleDefinition> FirstVehicle(NewObject<UVehicleDefinition>());
	TStrongObjectPtr<UVehicleDefinition> SecondVehicle(NewObject<UVehicleDefinition>());
	const auto* Seed = LoadObject<UVehicleDefinition>(nullptr, TEXT("/Game/Data/MVP_B/DA_Vehicle_Car_01.DA_Vehicle_Car_01"));
	if (!TestTrue(TEXT("Wheel configuration for shared update fixture"), Seed && Seed->Presentation)) { return false; }
	FirstVehicle->Presentation = SecondVehicle->Presentation = Seed->Presentation;
	FirstVehicle->CollisionMesh = Mesh.Get(); SecondVehicle->CollisionMesh = Mesh.Get();
	const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
		.ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
	struct FWorldCleanup { UWorld* World; ~FWorldCleanup() { World->DestroyWorld(false); } } WorldCleanup { World };
	AActor* Actor = World->SpawnActor<AActor>();
	UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(Actor);
	Actor->SetRootComponent(Body); Body->SetMobility(EComponentMobility::Movable);
	Body->SetStaticMesh(Mesh.Get()); Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true); Body->BodyInstance.SetMassOverride(800, true); Body->RegisterComponent();
	TestTrue(TEXT("Original body exists"), Body->BodyInstance.IsValidBodyInstance());
	TestFalse(TEXT("Original shape misses trace beyond its front"), World->LineTraceTestByChannel(FVector(160,0,500), FVector(160,0,-500), ECC_Visibility));
	P.FrontCm = 300; P.HeightCm = 90; P.FrontSideRadiusCm = 30; P.RearSideRadiusCm = 15;
	if (!TestTrue(TEXT("Update same mesh"), HALVehicleCollision::UpdateAsset(*Mesh, P, Error))) { AddError(Error); return false; }
	TestEqual(TEXT("Object path retained"), Mesh->GetPathName(), OriginalPath);
	TestTrue(TEXT("Both vehicle definitions retain identical mesh reference"), FirstVehicle->CollisionMesh == Mesh.Get() && SecondVehicle->CollisionMesh == Mesh.Get());
	TestTrue(TEXT("BodySetup object and component mesh references retained"), Mesh->GetBodySetup() == OriginalSetup && Body->GetStaticMesh() == Mesh.Get());
	TestTrue(TEXT("Changed physics GUID prevents old cook reuse"), OriginalSetup->BodySetupGuid != OriginalGuid);
	TestTrue(TEXT("Render bounds rebuilt"), Mesh->GetBounds().BoxExtent.Equals(FVector(200,70,45), 0.01));
	TestTrue(TEXT("Existing component bounds refreshed"), Body->Bounds.BoxExtent.Equals(FVector(200,70,45), 0.01));
	TestEqual(TEXT("Extending front preserves rear collision plane"), OriginalSetup->AggGeom.ConvexElems[0].ElemBox.Min.X, -100.0, 0.001);
	TestTrue(TEXT("Registered component physics recreated"), Body->IsRegistered() && Body->BodyInstance.IsValidBodyInstance());
	TestTrue(TEXT("Actual new collision reaches the extended front"), World->LineTraceTestByChannel(FVector(160,0,500), FVector(160,0,-500), ECC_Visibility));
	TestEqual(TEXT("Component mass override retained"), Body->GetMass(), 800.f, 0.1f);
	TestEqual(TEXT("Body defaults retained"), OriginalSetup->DefaultInstance.GetMassOverride(), 910.f);
	TestTrue(TEXT("Material assignment retained"), Mesh->GetStaticMaterials()[0].MaterialInterface == OriginalMaterial);
	TestEqual(TEXT("Custom material slot retained"), Mesh->GetStaticMaterials()[0].MaterialSlotName, FName(TEXT("CustomProxySlot")));
	TestEqual(TEXT("One convex retained"), OriginalSetup->AggGeom.GetElementCount(), 1);
	TestEqual(TEXT("Unrelated metadata retained"), Package->GetMetaData().GetValue(Mesh.Get(), TEXT("User.Note")), FString(TEXT("preserve me")));
	TestTrue(TEXT("Update is unsaved and marked dirty"), Package->IsDirty());
	P.FrontCm = 120; P.RearCm = 200; P.HeightCm = 70; P.FrontSideRadiusCm = P.RearSideRadiusCm = 0;
	TestTrue(TEXT("Repeated update returns to flat-side profile"), HALVehicleCollision::UpdateAsset(*Mesh, P, Error));
	TestFalse(TEXT("Recreated physics loses removed front volume"), World->LineTraceTestByChannel(FVector(160,0,500), FVector(160,0,-500), ECC_Visibility));
	TestTrue(TEXT("Recreated physics reaches extended rear"), World->LineTraceTestByChannel(FVector(-160,0,500), FVector(-160,0,-500), ECC_Visibility));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCollisionUpdateValidationTest, "HAL.Editor.CollisionGenerator.UpdateRejectsInvalidWithoutMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCollisionUpdateValidationTest::RunTest(const FString&)
{
	UPackage* Package = CreatePackage(*(TEXT("/Game/__HALCollisionInvalid_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TStrongObjectPtr<UStaticMesh> Mesh(NewObject<UStaticMesh>(Package, TEXT("SM_Update"), RF_Public | RF_Standalone));
	struct FCleanup { UStaticMesh* Mesh; ~FCleanup() { Mesh->ClearFlags(RF_Public | RF_Standalone); Mesh->GetOutermost()->SetDirtyFlag(false); } } Cleanup { Mesh.Get() };
	FString Error; FHALVehicleCollisionParameters P;
	if (!TestTrue(TEXT("Generated validation fixture"), HALVehicleCollision::BuildMesh(*Mesh, P, Error))) { return false; }
	const FGuid OriginalGuid = Mesh->GetBodySetup()->BodySetupGuid;
	const FBoxSphereBounds OriginalBounds = Mesh->GetBounds();
	Package->SetDirtyFlag(false);
	P.WidthCm = 10;
	TestFalse(TEXT("Invalid corner dimensions rejected"), HALVehicleCollision::UpdateAsset(*Mesh, P, Error));
	TestTrue(TEXT("Failure has explanation"), !Error.IsEmpty());
	TestFalse(TEXT("Failed update does not dirty package"), Package->IsDirty());
	TestEqual(TEXT("Failed update preserves physics cook"), Mesh->GetBodySetup()->BodySetupGuid, OriginalGuid);
	TestTrue(TEXT("Failed update preserves render bounds"), Mesh->GetBounds().BoxExtent.Equals(OriginalBounds.BoxExtent));
	FHALVehicleCollisionParameters Recipe;
	TestTrue(TEXT("Failed update retains readable recipe"), HALVehicleCollision::ReadRecipe(*Mesh, Recipe, Error));
	TestEqual(TEXT("Failed update preserves recipe dimensions"), Recipe.WidthCm, 140.f);
	P = {};
	Package->GetMetaData().SetValue(Mesh.Get(), TEXT("HALCollision.Version"), TEXT("999"));
	TestFalse(TEXT("Unknown recipe cannot overwrite mesh"), HALVehicleCollision::UpdateAsset(*Mesh, P, Error));
	Package->GetMetaData().RemoveValue(Mesh.Get(), TEXT("HALCollision.Version"));
	TestFalse(TEXT("Non-generator mesh cannot be overwritten"), HALVehicleCollision::UpdateAsset(*Mesh, P, Error));
	Package->GetMetaData().SetValue(Mesh.Get(), TEXT("HALCollision.Version"), TEXT("1"));
	Package->GetMetaData().SetValue(Mesh.Get(), TEXT("HALCollision.LengthCm"), TEXT("200"));
	TestTrue(TEXT("Legacy recipe remains updateable"), HALVehicleCollision::CanUpdateAsset(*Mesh, Error));
	Mesh->SetFlags(RF_Transient);
	TestFalse(TEXT("Transient meshes rejected"), HALVehicleCollision::CanUpdateAsset(*Mesh, Error));
	Mesh->ClearFlags(RF_Transient);
	Package->SetPackageFlags(PKG_Cooked);
	TestFalse(TEXT("Cooked assets rejected"), HALVehicleCollision::CanUpdateAsset(*Mesh, Error));
	Package->ClearPackageFlags(PKG_Cooked);
	if (GEditor)
	{
		const bool bPreviousSimulation = GEditor->bIsSimulatingInEditor;
		GEditor->bIsSimulatingInEditor = true;
		TestFalse(TEXT("Simulation cannot update assets"), HALVehicleCollision::UpdateAsset(*Mesh, P, Error));
		GEditor->bIsSimulatingInEditor = bPreviousSimulation;
	}
	TestEqual(TEXT("All rejected updates preserve collision"), Mesh->GetBodySetup()->BodySetupGuid, OriginalGuid);
	TestFalse(TEXT("All rejected updates preserve clean package state"), Package->IsDirty());
	return true;
}

#endif
