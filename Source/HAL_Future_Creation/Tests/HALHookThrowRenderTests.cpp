#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALHookThrow.h"
#include "../HALHookThrowAbility.h"
#include "../TestVehiclePawn.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShaderPrecompileMode.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHookMetalRenderTest,"HAL.RenderSmoke.HookThrow.InterlockedMetalOutboundAndReturn",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALHookMetalRenderTest::RunTest(const FString&)
{
	if(!FApp::CanEverRender()) { AddError(TEXT("Real RHI required")); return false; }
	const auto* C=LoadObject<UHALHookThrowConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_HookThrow"));
	if(!TestNotNull(TEXT("Authored hook recipe"),C)) { return false; }
	auto* Material=Cast<UMaterial>(C->FX.MetalMaterial.Get());
	if(!TestNotNull(TEXT("Toon metal"),Material)) { return false; }
	Material->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	FAssetCompilingManager::Get().FinishAllCompilation(); if(GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); }
	const auto Options=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::SM6,&Options);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* Owner=W->SpawnActor<AActor>(); auto* Root=NewObject<UBoxComponent>(Owner); Owner->SetRootComponent(Root); Root->RegisterComponent(); Root->SetWorldLocation(FVector(-700,0,100));
	auto* H=W->SpawnActor<AHALHookThrow>(); H->SetOwner(Owner); H->Rules=C->Rules; H->FX=C->FX;
	H->State.Activation=FGuid::NewGuid(); H->State.Phase=EHALHookPhase::Outbound; H->State.Point=H->Outlet(); H->State.Forward=FVector::ForwardVector;
	auto* Camera=W->SpawnActor<ASceneCapture2D>()->GetCaptureComponent2D(); Camera->bCaptureEveryFrame=false; Camera->bAlwaysPersistRenderingState=true;
	Camera->ProjectionType=ECameraProjectionMode::Orthographic; Camera->OrthoWidth=1900; Camera->CaptureSource=SCS_FinalColorLDR;
	Camera->SetWorldLocation(FVector(200,-650,1800)); Camera->SetWorldRotation((FVector(200,0,100)-Camera->GetComponentLocation()).Rotation());
	Camera->ShowFlags.SetAntiAliasing(false); Camera->ShowFlags.SetTemporalAA(false); Camera->ShowFlags.SetMotionBlur(false); Camera->ShowFlags.SetEyeAdaptation(false);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod=true; Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
	auto* Target=NewObject<UTextureRenderTarget2D>(); Target->InitCustomFormat(1024,512,PF_B8G8R8A8,false); Camera->TextureTarget=Target;
	const FString Directory=FPaths::ProjectSavedDir()/TEXT("HookSwing/20261117/Render"); IFileManager::Get().MakeDirectory(*Directory,true);
	const auto Sample=[&](double Age,const TCHAR* Name)
	{
		H->State.StartTime=H->State.PhaseTime=H->ServerTime()-Age; H->UpdateVisual(); Camera->CaptureScene();
		TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		TestTrue(TEXT("Real render proof saved"),FFileHelper::CreateBitmap(*(Directory/Name),1024,512,Pixels.GetData())); return Pixels;
	};
	const auto Outbound=Sample(.5,TEXT("Hook_Outbound.bmp"));
	TestTrue(TEXT("Actual ISM chain and hook rendered"),H->Chain->GetInstanceCount()>30&&H->Hook->GetStaticMesh()!=nullptr);
	int32 Visible=0,Dark=0,Mid=0,Light=0;
	for(const auto P:Outbound) { if(FMath::Max3(P.R,P.G,P.B)>18) { ++Visible; const int32 V=FMath::Max3(P.R,P.G,P.B); Dark+=V<90; Mid+=V>=90&&V<180; Light+=V>=180; } }
	TestTrue(TEXT("Real pixels contain metal volume and all three tonal bands"),Visible>1000&&Dark>30&&Mid>50&&Light>50);
	const int32 Count=H->Chain->GetInstanceCount();
	auto* Captured=W->SpawnActor<ATestVehiclePawn>(); Captured->SetActorHiddenInGame(true);
	H->State.Phase=EHALHookPhase::Swinging; H->State.Target=Captured; H->State.TargetOffset=FVector(0,0,40);
	Captured->SetActorLocation(FVector(500,0,100)); Sample(.8,TEXT("Hook_Swing_A.bmp")); const FVector Before=H->Hook->GetComponentLocation();
	Captured->SetActorLocationAndRotation(FVector(350,350,100),FRotator(0,30,0)); Sample(.9,TEXT("Hook_Swing_B.bmp"));
	TestTrue(TEXT("Rendered hook follows the real target attachment during the full swing"),FVector::Dist(H->Hook->GetComponentLocation(),Captured->GetActorTransform().TransformPosition(H->State.TargetOffset))<.1&&FVector::Dist(Before,H->Hook->GetComponentLocation())>300);
	FTransform LastLink; H->Chain->GetInstanceTransform(H->Chain->GetInstanceCount()-1,LastLink,true);
	TestTrue(TEXT("The actual rendered chain stays connected to the moving head"),FVector::Dist(LastLink.GetLocation(),H->Hook->GetComponentLocation())<120);
	H->State.Target=nullptr;
	H->State.Phase=EHALHookPhase::Returning; H->State.Point=H->Outlet()+FVector(1000,0,0);
	Sample(.1,TEXT("Hook_Returning.bmp")); TestTrue(TEXT("Reel-in retracts physical-looking links"),H->Chain->GetInstanceCount()<Count);
	return true;
}
#endif
