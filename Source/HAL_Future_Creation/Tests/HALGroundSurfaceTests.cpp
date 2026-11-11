#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALGroundSurfacePresentation.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
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
			TPair<FName, FName>(TEXT("InvTileSize"), TEXT("HAL_GroundInvTileSize"))})
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
	TestEqual(TEXT("UV1 carries the smooth weight gradient separately from texture UV0"), Mesh.Attributes()->NumUVLayers(), 2);
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

	TArray<FHALGroundSurfacePoint> Points = {{FVector::ZeroVector, FVector::UpVector, 100, false}};
	Surface->SetMesh(HALGroundSurface::BuildRibbon(Points, FVector::ZeroVector, 2, 320, Settings));
	const auto Bounds = Surface->CalcBounds(FTransform::Identity);
	TestTrue(TEXT("Bounds include the full positive WPO excursion"), Bounds.Origin.Z + Bounds.BoxExtent.Z >= 12.f);
	TestTrue(TEXT("Bounds retain the undeformed ground"), Bounds.Origin.Z - Bounds.BoxExtent.Z <= 2.f);
	Settings.bEnabled = false;
	Surface->ConfigureSurface(Material, 320, Settings);
	TestTrue(TEXT("Opting out restores the supplied material"), Surface->GetMaterial(0) == Material);
	TestFalse(TEXT("Disabling deformation also removes its optional shadow"), Surface->CastShadow);
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
