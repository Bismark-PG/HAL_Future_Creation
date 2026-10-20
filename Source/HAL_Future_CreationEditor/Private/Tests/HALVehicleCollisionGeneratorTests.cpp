#if WITH_DEV_AUTOMATION_TESTS

#include "HALVehicleCollisionGenerator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

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
	P = {}; P.LengthCm = 100;
	TestFalse(TEXT("Overlapping corner arcs rejected"), P.Validate(Error));
	P = {}; P.HeightCm = 0;
	TestFalse(TEXT("Flat volume rejected"), P.Validate(Error));
	P = {}; P.LengthCm = std::numeric_limits<float>::infinity();
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
			TestTrue(TEXT("Requested full dimensions"), Bounds.GetSize().Equals(FVector(P.LengthCm, P.WidthCm, P.HeightCm), 0.0001));
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
	FHALVehicleCollisionParameters P; P.LengthCm = 140;
	HALVehicleCollision::FGeometry G;
	FString Error;
	TestTrue(TEXT("Circle boundary case"), HALVehicleCollision::BuildGeometry(P, G, Error));
	for (int32 I = 0; I < G.Outline.Num(); ++I)
	{
		TestFalse(TEXT("Meeting arcs do not leave duplicate endpoints"), G.Outline[I].Equals(G.Outline[(I+1)%G.Outline.Num()], 0.00001));
	}
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
	Settings->Shape.LengthCm = 321; Settings->Shape.FrontRadiusCm = 35; Settings->Shape.RearRadiusCm = 12;
	FString Error;
	UStaticMesh* First = HALVehicleCollision::CreateAsset(*Settings, Error); Cleanup.Assets.Add(First);
	if (!TestNotNull(TEXT("First new asset"), First)) { AddError(Error); return false; }
	UStaticMesh* Second = HALVehicleCollision::CreateAsset(*Settings, Error); Cleanup.Assets.Add(Second);
	if (!TestNotNull(TEXT("Second new asset"), Second)) { AddError(Error); return false; }
	TestTrue(TEXT("Repeated generation never overwrites"), First->GetPathName() != Second->GetPathName());
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
	TestEqual(TEXT("Saved length"), Recipe.LengthCm, Settings->Shape.LengthCm);
	TestEqual(TEXT("Saved independent front radius"), Recipe.FrontRadiusCm, 35.f);
	TestEqual(TEXT("Saved independent rear radius"), Recipe.RearRadiusCm, 12.f);
	TestNotNull(TEXT("Reloaded editable source"), Copy->GetMeshDescription(0));
	TestEqual(TEXT("Reloaded single convex"), Copy->GetBodySetup()->AggGeom.ConvexElems.Num(), 1);
	TestTrue(TEXT("Reloaded physics volume"), Copy->GetBodySetup()->AggGeom.GetScaledVolume(FVector::OneVector) > 0);
	Copy->GetOutermost()->GetMetaData().SetValue(Copy, TEXT("HALCollision.Version"), TEXT("999"));
	Recipe.LengthCm = 777;
	TestFalse(TEXT("Unknown future recipe rejected"), HALVehicleCollision::ReadRecipe(*Copy, Recipe, Error));
	TestEqual(TEXT("Failed read preserves panel values"), Recipe.LengthCm, 777.f);
	return true;
}

#endif
