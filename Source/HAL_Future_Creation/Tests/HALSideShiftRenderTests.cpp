#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HALSideShiftTestSupport.h"
#include "AssetCompilingManager.h"
#include "MaterialShaderPrecompileMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/Material.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraDataSetAccessor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "RenderingThread.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSideShiftRenderTest,"HAL.RenderSmoke.SideShift.ActualCar07JetsSmokeAndCleanup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSideShiftRenderTest::RunTest(const FString&)
{
	if(!FApp::CanEverRender()) { AddError(TEXT("SideShift render proof requires a real RHI")); return false; }
	auto* Settings=LoadObject<UHALSideShiftConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car07_SideShift"));
	auto* P=LoadObject<UHALVehiclePresentationConfig>(nullptr,TEXT("/Game/Data/Car_Presentation/DA_Car_07_Presentation")); FString Error;
	if(!TestTrue(TEXT("Saved Car07 skill and presentation assets validate"),Settings&&Settings->Validate(Error)&&P&&P->Validate(Error))) { return false; }
	for(auto* S:{Settings->FX.Smoke.Get(),Settings->FX.Sparks.Get()})
	{ S->RequestCompile(false); S->WaitForCompilationComplete(); TestTrue(TEXT("Saved Niagara system cold loads and compiles"),S->IsReadyToRun()); }
	for(const TCHAR* Path:{TEXT("/Game/Assets/FX/SideShift/M_SideShift_ToonSmoke"),TEXT("/Game/Assets/FX/BullCharge/M_BullCharge_ToonSpark")})
	{
		auto* Material=LoadObject<UMaterial>(nullptr,Path); if(!TestNotNull(TEXT("Saved particle material"),Material)) { return false; }
		Material->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	}
	FAssetCompilingManager::Get().FinishAllCompilation();
	auto* Dither=IConsoleManager::Get().FindConsoleVariable(TEXT("r.BackbufferQuantizationDitheringOverride"));
	const int32 OldDither=Dither?Dither->GetInt():0; if(Dither) { Dither->Set(32,ECVF_SetByCode); }
	ON_SCOPE_EXIT { if(Dither) { Dither->Set(OldDither,ECVF_SetByCode); } };
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(true).ShouldSimulatePhysics(false).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* A=W->SpawnActor<AActor>(); auto* Root=NewObject<UBoxComponent>(A); A->SetRootComponent(Root); Root->SetCollisionEnabled(ECollisionEnabled::NoCollision); Root->RegisterComponent();
	auto* Fallback=NewObject<UStaticMeshComponent>(A); Fallback->SetupAttachment(Root); Fallback->RegisterComponent();
	auto* Mesh=NewObject<USkeletalMeshComponent>(A); Mesh->SetupAttachment(Root); Mesh->RegisterComponent();
	auto* Presentation=NewObject<UHALVehiclePresentationComponent>(A); A->AddInstanceComponent(Presentation); Presentation->RegisterComponent(); Presentation->Configure(P,Root,Fallback,Mesh,true); Mesh->RefreshBoneTransforms();
	auto* C=NewObject<UHALSideShiftComponent>(A); A->AddInstanceComponent(C); C->RegisterComponent();
	auto* Light=W->SpawnActor<ADirectionalLight>(); Light->SetActorRotation(FRotator(-65,-25,0)); Light->GetLightComponent()->SetIntensity(4);
	auto* Capture=W->SpawnActor<ASceneCapture2D>(); auto* Camera=Capture->GetCaptureComponent2D(); Camera->bCaptureEveryFrame=false;
	Camera->CaptureSource=SCS_FinalColorLDR; Camera->ProjectionType=ECameraProjectionMode::Orthographic; Camera->OrthoWidth=1100;
	Camera->PostProcessSettings.bOverride_AutoExposureMethod=true; Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureBias=true; Camera->PostProcessSettings.AutoExposureBias=0;
	Camera->SetWorldLocation(FVector(500,-700,900)); Camera->SetWorldRotation((FVector(0,0,-50)-Camera->GetComponentLocation()).Rotation());
	auto* RT=NewObject<UTextureRenderTarget2D>(); RT->InitCustomFormat(768,768,PF_B8G8R8A8,false); Camera->TextureTarget=RT;
	const FString Out=FPaths::ProjectSavedDir()/TEXT("Car07SideShift/20261116/Render"); IFileManager::Get().MakeDirectory(*Out,true);
	const auto Pixels=[&](const TCHAR* Name)
	{ W->SendAllEndOfFrameUpdates(); FlushRenderingCommands(); Camera->CaptureScene(); TArray<FColor> Data; RT->GameThread_GetRenderTargetResource()->ReadPixels(Data); TestTrue(TEXT("Image saved"),FFileHelper::CreateBitmap(*(Out/Name),768,768,Data.GetData())); return Data; };
	const auto Difference=[](const TArray<FColor>& X,const TArray<FColor>& Y)
	{ int32 Count=0; for(int32 I=0;I<X.Num();++I) { Count+=FMath::Abs(int32(X[I].R)-Y[I].R)+FMath::Abs(int32(X[I].G)-Y[I].G)+FMath::Abs(int32(X[I].B)-Y[I].B)>30; } return Count; };
	const auto Baseline=Pixels(TEXT("Car07_NoSkill.bmp"));
	for(int8 Side:{int8(-1),int8(1)})
	{
		FHALSideShiftTestAccess::ClearFX(*C); FHALSideShiftTestAccess::Visual(*C,Settings->FX,Side,.2); C->TickComponent(.016f,LEVELTICK_All,nullptr);
		const auto Jets=FHALSideShiftTestAccess::Jets(*C); TestEqual(TEXT("Exactly sparks and smoke on the reaction side"),Jets.Num(),2);
		if(Jets.Num()!=2||!Jets[0]||!Jets[1]) { AddError(TEXT("Missing actual mounted Niagara components")); return false; }
		for(const auto& Jet:Jets) { Jet->AdvanceSimulation(12,1.f/60); }
		int32 Counts[2]={};
		for(int32 Layer=0;Layer<2;++Layer)
		{
			TestTrue(TEXT("Jets attach to registered cosmetic mesh, never collision root"),Jets[Layer]->GetAttachParent()==Mesh);
			if(auto Controller=Jets[Layer]->GetSystemInstanceController()) if(auto* System=Controller->GetSystemInstance_Unsafe())
			for(const auto& E:System->GetEmitters())
			{
				const auto& Data=E->GetParticleData(); const auto V=FNiagaraDataSetAccessor<FVector3f>::CreateReader(Data,TEXT("Velocity"));
				TestTrue(TEXT("Actual simulated velocity available"),V.IsValid()); Counts[Layer]+=E->GetNumParticles();
				if(V.IsValid()) for(int32 I=0;I<E->GetNumParticles();++I) { TestTrue(TEXT("Particle flow opposes the side-step"),V[I].Y*Side<0); }
				if(Layer==0&&E->GetNumParticles()>0)
				{
					const auto Size=FNiagaraDataSetAccessor<FVector2f>::CreateReader(Data,TEXT("SpriteSize"));
					const auto Color=FNiagaraDataSetAccessor<FLinearColor>::CreateReader(Data,TEXT("Color"));
					const auto Style=FNiagaraDataSetAccessor<FVector4f>::CreateReader(Data,TEXT("DynamicMaterialParameter"));
					AddInfo(FString::Printf(TEXT("Actual spark Size=%s Color=%s Style=%s"),Size.IsValid()?*Size[0].ToString():TEXT("missing"),Color.IsValid()?*Color[0].ToString():TEXT("missing"),Style.IsValid()?*Style[0].ToString():TEXT("missing")));
				}
			}
			TestTrue(TEXT("This layer actually produced particles"),Counts[Layer]>0);
		}
		const auto Both=Pixels(Side<0?TEXT("Car07_LeftShift_RightJets.bmp"):TEXT("Car07_RightShift_LeftJets.bmp"));
		Jets[0]->SetVisibility(false); const auto SmokeOnly=Pixels(Side<0?TEXT("Car07_Left_Smoke.bmp"):TEXT("Car07_Right_Smoke.bmp"));
		TestTrue(TEXT("Toon sparks add visible pixels"),Difference(Both,SmokeOnly)>30);
		TestTrue(TEXT("Three-dimensional cartoon smoke adds visible pixels"),Difference(SmokeOnly,Baseline)>100);
		Jets[0]->SetVisibility(true);
		AddInfo(FString::Printf(TEXT("Car07 FX side=%d sparks=%d smoke=%d sparkPixels=%d smokePixels=%d"),int32(Side),Counts[0],Counts[1],Difference(Both,SmokeOnly),Difference(SmokeOnly,Baseline)));
		FHALSideShiftTestAccess::EndFX(*C); TestTrue(TEXT("End disables skill tire-mark demand"),C->GetActiveTireMarks()==nullptr);
		for(const auto& Jet:Jets) if(IsValid(Jet))
		{
			Jet->AdvanceSimulation(9,1.f/60);
			if(auto Controller=Jet->GetSystemInstanceController()) if(auto* System=Controller->GetSystemInstance_Unsafe())
			for(const auto& E:System->GetEmitters()) { TestEqual(TEXT("Short side jets leave no particles 0.15 seconds after stop"),E->GetNumParticles(),0); }
		}
	}
	FHALSideShiftTestAccess::ClearFX(*C); const auto Clean=Pixels(TEXT("Car07_Cleaned.bmp"));
	TestTrue(TEXT("Cleanup restores baseline image"),Difference(Clean,Baseline)<20);
	return true;
}
#endif
