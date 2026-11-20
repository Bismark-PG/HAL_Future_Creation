#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALWaterCannon.h"
#include "../HALWaterCannonAbility.h"
#include "../TestVehiclePawn.h"
#include "AssetCompilingManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShaderPrecompileMode.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALWaterRenderTest,"HAL.RenderSmoke.WaterCannon.FlowTonesAndContact",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALWaterRenderTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("Water render check requires RHI")); return false; }
	auto* Config = LoadObject<UHALWaterCannonConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car09_WaterCannon.DA_Ability_Car09_WaterCannon"));
	auto* Model = LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_09.BP_VehiclePawn_Car_09_C"));
	if (!TestNotNull(TEXT("Fire engine"),Model) || !TestNotNull(TEXT("Water cannon config"),Config) || !Config->WaterMaterial) { return false; }
	Config->WaterMaterial->GetMaterial()->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	FAssetCompilingManager::Get().FinishAllCompilation();
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).ShouldSimulatePhysics(false)
		.CreateNavigation(false).CreateAISystem(false).SetTransactional(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* Car = W->SpawnActor<ATestVehiclePawn>(Model);
	Car->PreInitializeComponents(); Car->InitializeComponents(); Car->PostInitializeComponents();
	auto* Jet = W->SpawnActor<AHALWaterCannon>(); Jet->SetOwner(Car); Jet->Config = Config;
	if (!TestTrue(TEXT("Nozzle is resolved from the production model"),Config->ResolveNozzle(*Car,Jet->LocalNozzle))) { return false; }
	Jet->View.bFlowing = true; Jet->View.Reach.Init(255,AHALWaterCannon::LaneCount);
	auto* Light = W->SpawnActor<ADirectionalLight>(); Light->SetActorRotation(FRotator(-58,-78,0));
	auto* Camera = W->SpawnActor<ASceneCapture2D>()->GetCaptureComponent2D();
	Camera->bCaptureEveryFrame = false; Camera->CaptureSource = SCS_FinalColorLDR;
	Camera->ProjectionType = ECameraProjectionMode::Orthographic; Camera->OrthoWidth = Config->Range+560;
	Camera->ShowFlags.SetTemporalAA(false);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true; Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	const FVector Focus = Jet->Frame(false).GetLocation()+FVector(Config->Range*.33f,0,0);
	Camera->SetWorldLocation(Focus + FVector(0,-680,820)); Camera->SetWorldRotation((Focus-Camera->GetComponentLocation()).Rotation());
	constexpr int32 Width = 900, Height = 480;
	auto* Texture = NewObject<UTextureRenderTarget2D>(); Texture->InitCustomFormat(Width,Height,PF_B8G8R8A8,false); Camera->TextureTarget = Texture;
	const FString Output = FPaths::ProjectSavedDir()/TEXT("WaterCannon/Render");
	IFileManager::Get().MakeDirectory(*Output,true);
	auto Capture = [&](float Age,const TCHAR* Name)
	{
		Jet->StartTime = Jet->ServerTime()-Age; Jet->Present();
		W->SendAllEndOfFrameUpdates();
		Camera->CaptureScene();
		TArray<FColor> Pixels;
		if (Texture->GameThread_GetRenderTargetResource()->ReadPixels(Pixels) && Pixels.Num() == Width*Height)
		{ TestTrue(TEXT("Capture saved"),FFileHelper::CreateBitmap(*(Output/Name),Width,Height,Pixels.GetData())); }
		return Pixels;
	};
	const auto First = Capture(.3f,TEXT("WaterJet.bmp"));
	const auto Later = Capture(.47f,TEXT("Flow.bmp"));
	Jet->View.ContactMask = (1u<<AHALWaterCannon::LaneCount)-1; Jet->View.Reach.Init(148,AHALWaterCannon::LaneCount);
	const auto Contact = Capture(.52f,TEXT("Contact.bmp"));
	if (First.Num() != Width*Height || Later.Num() != First.Num() || Contact.Num() != First.Num()) { AddError(TEXT("RHI pixel read failed")); return false; }
	int32 Dark = 0, Mid = 0, LightPixels = 0, Changed = 0, Clipped = 0;
	for (int32 I = 0; I < First.Num(); ++I)
	{
		const auto A = First[I];
		if (A.B > A.R+25 && A.B > 70)
		{
			if (A.R < 72) { ++Dark; } else if (A.R < 150) { ++Mid; } else { ++LightPixels; }
			Changed += FMath::Abs(int32(A.R)-Later[I].R) > 12;
			Clipped += int32(A.B)-Contact[I].B > 60;
		}
	}
	AddInfo(FString::Printf(TEXT("Water readback: shade=%d mid=%d highlight=%d moving=%d clipped=%d local nozzle=%s"),Dark,Mid,LightPixels,Changed,Clipped,*Jet->LocalNozzle.ToString()));
	TestTrue(TEXT("Multiple blue values are readable on the jet"),Dark+Mid+LightPixels>500 && Mid+LightPixels>80);
	TestTrue(TEXT("Flow travels through unchanged geometry"),Changed>95);
	TestTrue(TEXT("Contact shortens the visible reach"),Clipped>250);
	return true;
}
#endif
