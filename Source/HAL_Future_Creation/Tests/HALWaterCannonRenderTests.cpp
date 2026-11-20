#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALWaterCannon.h"
#include "../HALWaterCannonAbility.h"
#include "../TestVehiclePawn.h"
#include "AssetCompilingManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
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
	const auto Photograph = [&](const TCHAR* Name)
	{
		W->SendAllEndOfFrameUpdates();
		Camera->CaptureScene();
		TArray<FColor> Pixels;
		if (Texture->GameThread_GetRenderTargetResource()->ReadPixels(Pixels) && Pixels.Num() == Width*Height)
		{ TestTrue(TEXT("Capture saved"),FFileHelper::CreateBitmap(*(Output/Name),Width,Height,Pixels.GetData())); }
		return Pixels;
	};
	const auto Capture = [&](float Age,const TCHAR* Name)
	{
		Jet->StartTime=Jet->ServerTime()-Age; Jet->Present();
		return Photograph(Name);
	};
	const auto Dry=Capture(0,TEXT("NoFlow.bmp"));
	const auto Opening=Capture(Config->SprayInSeconds*.4f,TEXT("Opening.bmp"));
	const auto First = Capture(.3f,TEXT("WaterJet.bmp"));
	const auto Later = Capture(.47f,TEXT("Flow.bmp"));
	Jet->View.ContactMask = (1u<<AHALWaterCannon::LaneCount)-1; Jet->View.Reach.Init(148,AHALWaterCannon::LaneCount);
	auto& Surface=Jet->View.Contacts.AddDefaulted_GetRef();
	Surface.Point=FVector(Config->Range*148.f/255.f,0,0); Surface.Normal=FVector::BackwardVector; Surface.LastLane=AHALWaterCannon::LaneCount-1;
	const auto Contact = Capture(.52f,TEXT("Contact.bmp"));
	Jet->View.Contacts.Reset(); Jet->LastMeshAge=-1;
	const auto WithoutSplash=Capture(.52f,TEXT("ContactWithoutFoam.bmp"));
	Jet->View.ContactMask=0; Jet->View.Reach.Init(0,AHALWaterCannon::LaneCount);
	const auto ClosedMuzzle=Capture(.57f,TEXT("ClosedMuzzle.bmp"));
	Jet->View.ContactMask=0; Jet->View.Reach.Init(255,AHALWaterCannon::LaneCount);
	Jet->LastMeshAge=-1;
	Capture(.6f,TEXT("BeforeRelease.bmp")); Jet->ReleaseVisual();
	TActorIterator<AHALWaterCannonTail> Tail(W);
	if(!TestTrue(TEXT("Stopping leaves a local visual tail"),bool(Tail))) { return false; }
	Tail->Tick(Config->SprayOutSeconds*.49f);
	const auto Draining=Photograph(TEXT("Draining.bmp"));
	Tail->Tick(Config->SprayOutSeconds);
	const auto End=Photograph(TEXT("End.bmp"));
	for(const auto* Frame:{&Dry,&Opening,&First,&Later,&Contact,&WithoutSplash,&ClosedMuzzle,&Draining,&End})
	{ if(Frame->Num()!=Width*Height) { AddError(TEXT("RHI pixel read failed")); return false; } }
	const auto Different=[](const TArray<FColor>& A,const TArray<FColor>& B)
	{
		int32 N=0;
		for(int32 I=0;I<A.Num();++I)
		{ if(FMath::Abs(int32(A[I].B)-B[I].B)+FMath::Abs(int32(A[I].G)-B[I].G)>30) { ++N; } }
		return N;
	};
	const int32 Full=Different(First,Dry), Early=Different(Opening,Dry), TailPixels=Different(Draining,Dry), Foam=Different(Contact,WithoutSplash);
	AddInfo(FString::Printf(TEXT("Water silhouettes: opening=%d full=%d draining=%d contact foam=%d end=%d"),Early,Full,TailPixels,Foam,Different(End,Dry)));
	TestTrue(TEXT("Water travels outward then drains to zero"),Early>60&&Early<Full*.8f&&TailPixels>60&&TailPixels<Full*.9f&&Different(End,Dry)<30);
	TestTrue(TEXT("Splash adds visible pixels at a clipped tip"),Foam>50);
	TestTrue(TEXT("A blocked muzzle emits no forward sheet"),Different(ClosedMuzzle,Dry)<30);
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

	// Put a visible wall across just one side. Both the spray and its photographed location
	// now come from the production sweep instead of the fixed all-lanes sample above.
	auto* Wall=W->SpawnActor<AStaticMeshActor>();
	auto* WallMesh=Wall->GetStaticMeshComponent();
	WallMesh->SetMobility(EComponentMobility::Movable);
	WallMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	WallMesh->SetCollisionProfileName(TEXT("BlockAll"));
	if (auto* SurfaceMaterial=WallMesh->CreateDynamicMaterialInstance(0))
	{ SurfaceMaterial->SetVectorParameterValue(TEXT("Color"),FLinearColor(.13f,.17f,.21f)); }
	const FTransform Aim=Jet->Frame(false);
	Wall->SetActorTransform(FTransform(Aim.GetRotation(),Aim.TransformPosition(FVector(Config->Range*.58f,81,0)),FVector(.12,1.55,1.2)));
	Jet->SampleJet(Jet->ServerTime());
	const uint32 AllLanes=(1u<<AHALWaterCannon::LaneCount)-1;
	TestTrue(TEXT("A real wall clips one side and gives the spray an outward normal"),
		Jet->View.ContactMask!=0 && Jet->View.ContactMask!=AllLanes && Jet->View.Contacts.ContainsByPredicate(
			[](const FHALWaterJetContact& Hit){return Hit.Normal.X<-.85 && Hit.Point.X>0;}));
	Jet->StartTime=Jet->ServerTime()-.72; Jet->Present();
	Capture(1.1f,TEXT("WallEdge.bmp"));
	Wall->SetActorLocation(Aim.TransformPosition(FVector(Config->Range*.58f,0,0)));
	Jet->SampleJet(Jet->ServerTime());
	TestTrue(TEXT("Moving that same wall across the nozzle line stops all lanes"),Jet->View.ContactMask==AllLanes);
	Capture(1.19f,TEXT("WallImpact.bmp"));
	return true;
}
#endif
