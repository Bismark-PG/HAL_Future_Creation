#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALGroundSurfacePresentation.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include <limits>

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "MaterialShaderPrecompileMode.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/ScopeExit.h"

namespace
{
	UMaterial* DeformationTestMaterial()
	{
		auto* Material = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
		Material->SetShadingModel(MSM_Unlit);
		Material->TwoSided = true;
		auto* Deform = NewObject<UMaterialExpressionCustom>(Material);
		Material->GetExpressionCollection().AddExpression(Deform);
		Deform->OutputType = CMOT_Float3;
		Deform->Code = TEXT("return float3(0,0,saturate(Weight)*(Height+Amplitude*sin(6.28318530718*(dot(UV,WaveUV.xy)+Phase))));");
		const auto Connect = [&](FName Name, UMaterialExpression* Expression, int32 Output = 0)
		{
			Material->GetExpressionCollection().AddExpression(Expression);
			auto& Input = Deform->Inputs.AddDefaulted_GetRef();
			Input.InputName = Name;
			Input.Input.Connect(Output, Expression);
		};
		Connect(TEXT("Weight"), NewObject<UMaterialExpressionVertexColor>(Material), 1);
		Connect(TEXT("UV"), NewObject<UMaterialExpressionTextureCoordinate>(Material));
		for (const auto& Pair : {TPair<FName, FName>(TEXT("Height"), TEXT("HAL_GroundBulgeHeight")),
			TPair<FName, FName>(TEXT("Amplitude"), TEXT("HAL_GroundWaveAmplitude")),
			TPair<FName, FName>(TEXT("Phase"), TEXT("HAL_GroundWavePhase")),
			TPair<FName, FName>(TEXT("InvTileSize"), TEXT("HAL_GroundInvTileSize")),
			TPair<FName, FName>(TEXT("Dissolve"), TEXT("HAL_GroundDissolveProgress")),
			TPair<FName, FName>(TEXT("PaintEdge"), TEXT("HAL_GroundPaintEdgeEnabled"))})
		{
			auto* Parameter = NewObject<UMaterialExpressionScalarParameter>(Material);
			Parameter->ParameterName = Pair.Value;
			Parameter->DefaultValue = 0;
			Connect(Pair.Key, Parameter);
		}
		auto* WaveUV = NewObject<UMaterialExpressionVectorParameter>(Material);
		WaveUV->ParameterName = TEXT("HAL_GroundWaveUV");
		WaveUV->DefaultValue = FLinearColor::Black;
		Connect(TEXT("WaveUV"), WaveUV);
		Material->GetEditorOnlyData()->WorldPositionOffset.Connect(0, Deform);
		auto* Color = NewObject<UMaterialExpressionVectorParameter>(Material);
		Color->ParameterName = TEXT("Tint");
		Color->DefaultValue = FLinearColor::Green;
		Material->GetExpressionCollection().AddExpression(Color);
		Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Color);
		Material->PostEditChange();
		if (FApp::CanEverRender()) { Material->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous); }
		FAssetCompilingManager::Get().FinishAllCompilation();
		return Material;
	}
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALGroundSurfaceMeshTest, "HAL.FutureCreation.GroundSurface.SubdivisionWeightsAndGaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALGroundSurfaceMeshTest::RunTest(const FString&)
{
	FHALGroundSurfaceDeformation Settings;
	Settings.bEnabled = true;
	const FVector Normal = FVector(0, -.25, 1).GetSafeNormal();
	TArray<FHALGroundSurfacePoint> Points = {
		{FVector(0, 0, 0), Normal, 100, false},
		{FVector(200, 0, 0), Normal, 60, true},
		{FVector(500, 0, 0), Normal, 40, false}};
	const auto Mesh = HALGroundSurface::BuildRibbon(Points, FVector::ZeroVector, 2, 320, Settings);
	if (!TestTrue(TEXT("Subdivided strip and rounded caps form a valid mesh"), Mesh.TriangleCount() > 0 && Mesh.CheckValidity())) { return false; }
	const auto* Colors = Mesh.Attributes()->PrimaryColors();
	TestEqual(TEXT("UV1 gradient and UV2 paint metadata remain separate from texture UV0"), Mesh.Attributes()->NumUVLayers(), 3);
	const auto* Gradients = Mesh.Attributes()->GetUVLayer(1);
	bool bRaisedInterior = false, bPinnedEdge = false, bInteriorCapRing = false;
	for (const int32 Vertex : Mesh.VertexIndicesItr())
	{
		const FVector Position = Mesh.GetVertex(Vertex);
		const float Weight = Colors->GetElement(Vertex).X;
		const FVector2f Gradient = Gradients->GetElement(Vertex);
		TestTrue(TEXT("Smooth gradients are finite"), FMath::IsFinite(Gradient.X) && FMath::IsFinite(Gradient.Y));
		if (Weight > .99f) { TestTrue(TEXT("The bulge crest has a flat analytic tangent"), Gradient.SizeSquared() < .00001f); }
		if (Position.X < 0) { TestTrue(TEXT("Rounded end gradients point towards the raised center"), Gradient.X >= 0); }
		TestTrue(TEXT("Deformation weights are bounded"), Weight >= 0 && Weight <= 1);
		TestTrue(TEXT("Base mesh remains on its sampled ground plane"), FMath::IsNearlyEqual(Position.Z, Position.Y * .25 + 2, .001));
		bRaisedInterior |= Weight > .99f;
		bPinnedEdge |= Weight == 0;
		bInteriorCapRing |= Position.X < 0 && Weight > .1f && Weight < .9f;
		TestTrue(TEXT("Disconnected patch does not bridge the landing gap"), Position.X <= 260.001 || Position.X >= 459.999);
	}
	TestTrue(TEXT("Strip has deformable central vertices and pinned outer vertices"), bRaisedInterior && bPinnedEdge);
	TestTrue(TEXT("Rounded ends have interior rings for smooth displacement"), bInteriorCapRing);

	Settings.WidthSubdivisions = 16;
	Points.SetNum(256);
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		Points[Index] = {FVector(Index * 1500., 0, 0), FVector::UpVector, 100, Index % 2 != 0};
	}
	const auto Long = HALGroundSurface::BuildRibbon(Points, FVector::ZeroVector, 2, 320, Settings);
	TestTrue(TEXT("Worst-case source count stays below 65536 vertices"), Long.VertexCount() > 0 && Long.VertexCount() < 65536);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALGroundSurfacePaintEdgeTest, "HAL.FutureCreation.GroundSurface.PaintMarginAndCorePreservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALGroundSurfacePaintEdgeTest::RunTest(const FString&)
{
	FHALGroundSurfacePaintEdge Edge;
	TestTrue(TEXT("Generic surfaces keep an opt-in paint margin"), Edge.IsValid() && !Edge.bEnabled);
	Edge.bEnabled = true;
	TestTrue(TEXT("Small patches use the relative cap"), FMath::IsNearlyEqual(Edge.Padding(70), 5.6f));
	TestEqual(TEXT("Large patches use the centimeter cap"), Edge.Padding(900), 15.f);
	FHALGroundSurfaceDeformation Settings;
	Settings.bEnabled = true;
	TArray<FHALGroundSurfacePoint> Points = {
		{FVector::ZeroVector, FVector::UpVector, 100, false},
		{FVector(200, 0, 0), FVector::UpVector, 100, true},
		{FVector(600, 0, 0), FVector::UpVector, 80, false}};
	const auto Core = HALGroundSurface::BuildRibbon(Points, FVector::ZeroVector, 2, 320, Settings);
	const auto Paint = HALGroundSurface::BuildRibbon(Points, FVector::ZeroVector, 2, 320, Settings, Edge);
	if (!TestTrue(TEXT("Paint annulus, caps and gaps form a valid mesh"), Paint.CheckValidity())) { return false; }
	TestTrue(TEXT("Visual bounds include the thin margin"), FMath::IsNearlyEqual(Paint.GetBounds().Max.Y, 108., .001));
	for (const int32 Vertex : Core.VertexIndicesItr())
	{
		bool bPreserved = false;
		for (const int32 Candidate : Paint.VertexIndicesItr())
		{
			if (Core.GetVertex(Vertex).Equals(Paint.GetVertex(Candidate), .001)
				&& Core.Attributes()->PrimaryUV()->GetElement(Vertex).Equals(Paint.Attributes()->PrimaryUV()->GetElement(Candidate), .0001f)
				&& Core.Attributes()->GetUVLayer(1)->GetElement(Vertex).Equals(Paint.Attributes()->GetUVLayer(1)->GetElement(Candidate), .0001f)
				&& Core.Attributes()->PrimaryColors()->GetElement(Vertex).Equals(Paint.Attributes()->PrimaryColors()->GetElement(Candidate), .0001f))
			{ bPreserved = true; break; }
		}
		if (!TestTrue(TEXT("Original core vertices, texture coordinates, weights and normals survive"), bPreserved)) { return false; }
	}
	for (const int32 Vertex : Paint.VertexIndicesItr())
	{
		const auto Meta = Paint.Attributes()->GetUVLayer(2)->GetElement(Vertex);
		TestTrue(TEXT("Visual margin is bounded in its own metadata"), Meta.Y >= 0 && Meta.Y <= 15 && Meta.X <= Meta.Y + .001f);
		if (Meta.X > 0)
		{
			TestEqual(TEXT("Overflow remains flat on the ground"), Paint.Attributes()->PrimaryColors()->GetElement(Vertex).X, 0.f);
			TestTrue(TEXT("Overflow does not inherit the core's bulge gradient"),
				Paint.Attributes()->GetUVLayer(1)->GetElement(Vertex).SizeSquared() == 0);
		}
		const auto P = Paint.GetVertex(Vertex);
		TestTrue(TEXT("Paint does not bridge a disconnected landing gap"), P.X <= 308.001 || P.X >= 513.599);
	}
	Points.SetNum(256);
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		Points[Index] = {FVector(Index * 1500., 0, 0), FVector::UpVector, 100, false};
	}
	for (const int32 Across : {2, 8, 16})
	{
		Settings.WidthSubdivisions = Across;
		const auto Maximum = HALGroundSurface::BuildRibbon(Points, FVector::ZeroVector, 2, 320, Settings, Edge);
		TestTrue(TEXT("Paint caps remain below the existing vertex budget"), Maximum.VertexCount() < 65536);
	}
	Edge.MaximumOutset = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite paint limits are rejected"), Edge.IsValid());
	// Optional render-fixture input: export production vertices and attributes, rather than recreating them in Python.
	if (FParse::Param(FCommandLine::Get(), TEXT("HALPaintEdgeExport")))
	{
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("PaintEdgeFixture");
		IFileManager::Get().MakeDirectory(*Directory, true);
		Settings.WidthSubdivisions = 8;
		Settings.VertexSpacing = 40;
		Points = {{FVector::ZeroVector, FVector::UpVector, 280, false}};
		for (const bool bPaint : {false, true})
		{
			Edge = {};
			Edge.bEnabled = bPaint;
			const auto Export = HALGroundSurface::BuildRibbon(Points, FVector::ZeroVector, 2, 320, Settings, Edge);
			FString CSV;
			for (const int32 Vertex : Export.VertexIndicesItr())
			{
				const auto P = Export.GetVertex(Vertex);
				const auto UV = Export.Attributes()->PrimaryUV()->GetElement(Vertex);
				const auto Gradient = Export.Attributes()->GetUVLayer(1)->GetElement(Vertex);
				const auto Meta = Export.Attributes()->GetUVLayer(2)->GetElement(Vertex);
				const auto Normal = Export.Attributes()->PrimaryNormals()->GetElement(Vertex);
				CSV += FString::Printf(TEXT("v,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n"),
					P.X, P.Y, P.Z, UV.X, UV.Y, Gradient.X, Gradient.Y, Meta.X, Meta.Y,
					Export.Attributes()->PrimaryColors()->GetElement(Vertex).X, Normal.X, Normal.Y, Normal.Z);
			}
			for (const int32 Triangle : Export.TriangleIndicesItr())
			{
				const auto T = Export.GetTriangle(Triangle);
				CSV += FString::Printf(TEXT("t,%d,%d,%d\n"), T.A, T.B, T.C);
			}
			TestTrue(TEXT("Production render fixture export succeeds"), FFileHelper::SaveStringToFile(CSV,
				*(Directory / (bPaint ? TEXT("Paint.csv") : TEXT("Legacy.csv"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALGroundSurfaceConfigTest, "HAL.FutureCreation.GroundSurface.ConfigMaterialAndBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALGroundSurfaceConfigTest::RunTest(const FString&)
{
	FHALGroundSurfaceDeformation Settings;
	TestTrue(TEXT("Defaults are valid and opt in"), Settings.IsValid() && !Settings.bEnabled);
	Settings.WidthSubdivisions = 7;
	TestFalse(TEXT("Odd counts cannot supply a shared center row"), Settings.IsValid());
	Settings = {};
	Settings.WaveAmplitude = Settings.BulgeHeight + 1;
	TestFalse(TEXT("Wave cannot pull the interior beneath the ground"), Settings.IsValid());
	Settings = {};
	Settings.WaveLength = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite values are rejected"), Settings.IsValid());
	Settings = {};
	Settings.bEnabled = true;
	auto* Surface = NewObject<UHALGroundSurfaceComponent>();
	UMaterial* Material = UMaterial::GetDefaultMaterial(MD_Surface);
#if WITH_EDITOR
	Material = DeformationTestMaterial();
#endif
	Surface->ConfigureSurface(Material, 320, Settings);
	TestFalse(TEXT("Existing ground surfaces retain their shadow-free default"), Surface->CastShadow);
	Settings.bCastShadow = true;
	Surface->ConfigureSurface(Material, 320, Settings);
	TestTrue(TEXT("A valid raised surface can opt into real shadows"), Surface->CastShadow);
	auto* MID = Cast<UMaterialInstanceDynamic>(Surface->GetMaterial(0));
	if (!TestNotNull(TEXT("Each surface gets its own WPO material instance"), MID)) { return false; }
	TestEqual(TEXT("Height comes from the area's config"), MID->K2_GetScalarParameterValue(TEXT("HAL_GroundBulgeHeight")), 8.f);
	TestTrue(TEXT("Wave normals use the same world-to-UV scale as the displacement"),
		FMath::IsNearlyEqual(MID->K2_GetScalarParameterValue(TEXT("HAL_GroundInvTileSize")), 1.f / 320));
	Surface->SetSurfaceAge(.75f);
	TestTrue(TEXT("Server age reaches the material phase"), FMath::IsNearlyEqual(MID->K2_GetScalarParameterValue(TEXT("HAL_GroundWavePhase")), .25f));
	Surface->SetSurfaceAge(3.75f);
	TestTrue(TEXT("Late observation samples the same phase"), FMath::IsNearlyEqual(MID->K2_GetScalarParameterValue(TEXT("HAL_GroundWavePhase")), .25f));
	const auto Other = NewObject<UHALGroundSurfaceComponent>();
	Other->ConfigureSurface(Material, 320, Settings);
	TestTrue(TEXT("Two effects do not share a mutable material"), Other->GetMaterial(0) != MID);
	Surface->SetDissolveProgress(.4f);
	TestEqual(TEXT("Dissolve affects only this area's material"), MID->K2_GetScalarParameterValue(TEXT("HAL_GroundDissolveProgress")), .4f);
	TestEqual(TEXT("Another area's dissolve remains intact"),
		CastChecked<UMaterialInstanceDynamic>(Other->GetMaterial(0))->K2_GetScalarParameterValue(TEXT("HAL_GroundDissolveProgress")), 0.f);

	TArray<FHALGroundSurfacePoint> Points = {{FVector::ZeroVector, FVector::UpVector, 100, false}};
	Surface->SetMesh(HALGroundSurface::BuildRibbon(Points, FVector::ZeroVector, 2, 320, Settings));
	const auto Bounds = Surface->CalcBounds(FTransform::Identity);
	TestTrue(TEXT("Bounds include the full positive WPO excursion"), Bounds.Origin.Z + Bounds.BoxExtent.Z >= 12.f);
	TestTrue(TEXT("Bounds retain the undeformed ground"), Bounds.Origin.Z - Bounds.BoxExtent.Z <= 2.f);
	Settings.bEnabled = false;
	Surface->ConfigureSurface(Material, 320, Settings);
	TestTrue(TEXT("Opting out restores the supplied material"), Surface->GetMaterial(0) == Material);
	TestFalse(TEXT("Disabling deformation also removes its optional shadow"), Surface->CastShadow);
	Surface->SetDissolveProgress(.5f);
	TestTrue(TEXT("An undeformed surface can still dissolve with its own MID"), Surface->GetMaterial(0) != Material);
	TestEqual(TEXT("Undeformed dissolution keeps its supplied progress"),
		CastChecked<UMaterialInstanceDynamic>(Surface->GetMaterial(0))->K2_GetScalarParameterValue(TEXT("HAL_GroundDissolveProgress")), .5f);
	Settings.bEnabled = true;
	Surface->ConfigureSurface(nullptr, 320, Settings);
	TestFalse(TEXT("An absent material cannot leave a shadow-only surface"), Surface->CastShadow);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALGroundSurfacePixelsTest, "HAL.RenderSmoke.GroundSurface.Displacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALGroundSurfacePixelsTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("Ground-surface pixel verification requires a real RHI.")); return false; }
	const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).SetTransactional(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto* Actor = World->SpawnActor<AActor>();
	auto* Surface = NewObject<UHALGroundSurfaceComponent>(Actor);
	Actor->SetRootComponent(Surface);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FHALGroundSurfaceDeformation Settings;
	Settings.bEnabled = true;
	Settings.BulgeHeight = 40;
	Settings.WaveAmplitude = 10;
	Settings.WaveLength = 2000;
	Settings.WaveDirection = 0;
	const TArray<FHALGroundSurfacePoint> Points = {{FVector::ZeroVector, FVector::UpVector, 100, false}};
	Surface->SetMesh(HALGroundSurface::BuildRibbon(Points, FVector::ZeroVector, 2, 320, Settings));
	auto* Material = DeformationTestMaterial();
	Surface->ConfigureSurface(Material, 320, {});
	Surface->RegisterComponent();
	auto* Target = NewObject<UTextureRenderTarget2D>(Actor);
	Target->InitAutoFormat(128, 128);
	Target->UpdateResourceImmediate(true);
	auto* Capture = World->SpawnActor<ASceneCapture2D>();
	const FVector CameraPosition(0, -300, 150);
	Capture->SetActorLocation(CameraPosition);
	Capture->SetActorRotation((FVector(0, 0, 10) - CameraPosition).Rotation());
	auto* Camera = Capture->GetCaptureComponent2D();
	Camera->TextureTarget = Target;
	Camera->bCaptureEveryFrame = Camera->bCaptureOnMovement = false;
	Camera->FOVAngle = 45;
	Camera->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Camera->ShowFlags.SetAntiAliasing(false);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	const auto Pixels = [&]()
	{
		Camera->CaptureScene();
		TArray<FColor> Result;
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Result);
		return Result;
	};
	const auto Flat = Pixels();
	Surface->ConfigureSurface(Material, 320, Settings);
	Surface->SetSurfaceAge(.75f);
	const auto Raised = Pixels();
	Surface->SetSurfaceAge(2.25f);
	const auto Low = Pixels();
	int32 Visible = 0, BulgeChanges = 0, WaveChanges = 0;
	for (int32 Index = 0; Index < Flat.Num(); ++Index)
	{
		const auto IsGreen = [](const FColor& Pixel) { return Pixel.G > 40 && Pixel.G > Pixel.R * 2 && Pixel.G > Pixel.B * 2; };
		Visible += IsGreen(Flat[Index]);
		if (Raised.IsValidIndex(Index) && Low.IsValidIndex(Index))
		{
			BulgeChanges += IsGreen(Flat[Index]) != IsGreen(Raised[Index]);
			WaveChanges += IsGreen(Raised[Index]) != IsGreen(Low[Index]);
		}
	}
	AddInfo(FString::Printf(TEXT("Actual ground WPO pixels Visible=%d BulgeChanges=%d WaveChanges=%d"), Visible, BulgeChanges, WaveChanges));
	TestTrue(TEXT("Ground material produces real visible pixels"), Visible > 100);
	TestTrue(TEXT("Enabling the authored height changes the rendered silhouette"), BulgeChanges > 20);
	TestTrue(TEXT("Server phase changes the rendered wave"), WaveChanges > 10);
	return true;
}
#endif
#endif
