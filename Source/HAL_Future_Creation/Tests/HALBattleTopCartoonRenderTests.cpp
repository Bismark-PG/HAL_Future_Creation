#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "AssetCompilingManager.h"
#include "MaterialShaderPrecompileMode.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraEmitter.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBattleTopCartoonPixels,"HAL.RenderSmoke.BattleTop.CartoonShapesAndLayeredWind",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALBattleTopCartoonPixels::RunTest(const FString&)
{
	if(!FApp::CanEverRender()){AddError(TEXT("Requires a real RHI"));return false;}
	const auto Load=[](const TCHAR* Name){return LoadObject<UNiagaraSystem>(nullptr,*(FString(TEXT("/Game/Assets/FX/BattleTop/"))+Name+TEXT(".")+Name));};
	auto* Wind=Load(TEXT("NS_BattleTop_ToonWind"));auto* Contact=Load(TEXT("NS_BattleTop_ToonContact"));
	auto* Mat=LoadObject<UMaterial>(nullptr,TEXT("/Game/Assets/FX/BattleTop/M_BattleTop_ToonWind.M_BattleTop_ToonWind"));
	auto* ContactMat=LoadObject<UMaterial>(nullptr,TEXT("/Game/Assets/FX/BattleTop/M_BattleTop_ToonContact.M_BattleTop_ToonContact"));
	auto* Arc=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Assets/FX/BattleTop/SM_BattleTop_WindArc.SM_BattleTop_WindArc"));
	if(!TestNotNull(TEXT("Saved mesh wind"),Wind)||!TestNotNull(TEXT("Saved cartoon contact"),Contact)
		||!TestNotNull(TEXT("Saved wind material"),Mat)||!TestNotNull(TEXT("Saved contact material"),ContactMat)||!TestNotNull(TEXT("Saved curved arc"),Arc))return false;
	TestTrue(TEXT("Arc mesh has real nonzero cross-section height"),Arc->GetBoundingBox().GetExtent().Z>2);
	bool bMeshRenderer=false;
	for(const auto& H:Wind->GetEmitterHandles())if(auto* E=H.GetInstance().Emitter.Get())
		for(const auto* R:E->GetLatestEmitterData()->GetRenderers())if(const auto* Mesh=Cast<UNiagaraMeshRendererProperties>(R))
		{
			bMeshRenderer=true;
			TestTrue(TEXT("Default mesh index selects the actual arc"),Mesh->Meshes.Num()==1&&Mesh->Meshes[0].Mesh==Arc);
		}
	TestTrue(TEXT("Wind uses mesh particles rather than a flat sprite"),bMeshRenderer);
	for(auto* M:{Mat,ContactMat})M->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	FAssetCompilingManager::Get().FinishAllCompilation();
	for(auto* S:{Wind,Contact}){S->RequestCompile(false);S->WaitForCompilationComplete();TestTrue(TEXT("New Niagara system compiles"),S->IsReadyToRun());}
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(true).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT{W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
	auto* A=W->SpawnActor<AActor>();auto* C=NewObject<UNiagaraComponent>(A);A->SetRootComponent(C);C->SetAutoActivate(false);C->SetAsset(Wind);C->RegisterComponent();
	auto* MID=UMaterialInstanceDynamic::Create(Mat,A);C->SetVariableMaterial(TEXT("User.WindMaterial"),MID);
	MID->SetScalarParameterValue(TEXT("Opacity"),.9f);MID->SetScalarParameterValue(TEXT("Envelope"),1);
	auto* Target=NewObject<UTextureRenderTarget2D>(A);Target->InitAutoFormat(512,512);Target->UpdateResourceImmediate(true);
	auto* Capture=W->SpawnActor<ASceneCapture2D>();Capture->SetActorLocation(FVector(0,-450,280));Capture->SetActorRotation((FVector(0,0,20)-Capture->GetActorLocation()).Rotation());
	auto* Camera=Capture->GetCaptureComponent2D();Camera->TextureTarget=Target;Camera->bCaptureEveryFrame=Camera->bCaptureOnMovement=false;
	Camera->ProjectionType=ECameraProjectionMode::Orthographic;Camera->OrthoWidth=450;Camera->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;Camera->ShowFlags.SetAntiAliasing(false);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod=true;Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
	const auto Pixels=[&](const TCHAR* Name){Camera->CaptureScene();TArray<FColor> P;Target->GameThread_GetRenderTargetResource()->ReadPixels(P);
		FFileHelper::CreateBitmap(*(FPaths::ProjectSavedDir()/TEXT("BattleTop/Cartoon_20261113")/Name),512,512,P.GetData());return P;};
	const auto Difference=[](const TArray<FColor>& X,const TArray<FColor>& Y){int32 N=0;for(int32 I=0;I<X.Num();++I)N+=FMath::Abs(int32(X[I].R)-Y[I].R)>12;return N;};
	const auto Warm=[](const TArray<FColor>& X){int32 N=0;for(const auto P:X)N+=P.R>25&&P.R>int32(P.B)*1.5;return N;};
	const auto ResetWind=[&](float Speed){C->DeactivateImmediate();C->SetVariableFloat(TEXT("User.SpawnRate"),5.f/1.2f);C->SetVariableFloat(TEXT("User.YawRate"),Speed);C->Activate(true);C->AdvanceSimulation(20,1.f/60);};
	ResetWind(240);const auto First=Pixels(TEXT("CartoonWind_Default.bmp"));
	if(auto Controller=C->GetSystemInstanceController())if(auto* Instance=Controller->GetSystemInstance_Unsafe())
		for(const auto& E:Instance->GetEmitters())
		{
			AddInfo(FString::Printf(TEXT("Wind particles=%d spawned=%d state=%d"),E->GetNumParticles(),E->GetTotalSpawnedParticles(),int32(E->GetExecutionState())));
			TestTrue(TEXT("Wind continuously spawns live arc particles"),E->GetNumParticles()>0);
		}
	TestTrue(TEXT("Layered wind has visible warm body pixels"),Warm(First)>200);
	MID->SetScalarParameterValue(TEXT("BandWidthScale"),2);const auto Wide=Pixels(TEXT("CartoonWind_Wide.bmp"));
	TestTrue(TEXT("Width changes mesh coverage without changing its radius input"),Difference(First,Wide)>100);
	MID->SetScalarParameterValue(TEXT("BandWidthScale"),1);MID->SetScalarParameterValue(TEXT("SpinAngle"),.37f);
	const auto Spin=Pixels(TEXT("CartoonWind_SpinIgnored.bmp"));
	TestEqual(TEXT("Vehicle spin angle cannot rotate the new airflow"),Difference(First,Spin),0);
	ResetWind(0);const auto Still=Pixels(TEXT("CartoonWind_ZeroSpeed.bmp"));
	TestTrue(TEXT("Independent Niagara rotation rate changes the wind image"),Difference(First,Still)>100);
	C->SetVariableVec3(TEXT("User.PositionMin"),FVector(0,0,70));C->SetVariableVec3(TEXT("User.PositionMax"),FVector(0,0,100));
	ResetWind(240);const auto Raised=Pixels(TEXT("CartoonWind_Raised.bmp"));
	TestTrue(TEXT("Particle layer height changes its actual side-view silhouette"),Difference(First,Raised)>100);
	MID->SetScalarParameterValue(TEXT("Envelope"),0);const auto Hidden=Pixels(TEXT("CartoonWind_Hidden.bmp"));
	TestTrue(TEXT("Skill exit envelope hides mesh wind"),Warm(Hidden)<10);
	C->DeactivateImmediate();C->SetAsset(Contact);C->SetVariableInt(TEXT("User.BurstCount"),7);
	C->SetVariableVec4(TEXT("User.StyleParameters"),FVector4(1,0,0,0));
	C->SetVariableLinearColor(TEXT("User.SparkColor"),FLinearColor(1,.72f,.22f));
	C->SetVariableVec3(TEXT("User.VelocityMin"),FVector(-150,0,250));C->SetVariableVec3(TEXT("User.VelocityMax"),FVector(150,200,650));
	C->Activate(true);C->AdvanceSimulation(9,1.f/60);const auto Sparks=Pixels(TEXT("CartoonContact_Default.bmp"));
	TestTrue(TEXT("Seven original-strength cartoon strokes are visible"),Warm(Sparks)>35);
	C->AdvanceSimulation(65,1.f/60);const auto Gone=Pixels(TEXT("CartoonContact_Gone.bmp"));
	TestTrue(TEXT("Original contact lifetimes still clear every particle"),Warm(Gone)<10);
	AddInfo(FString::Printf(TEXT("Cartoon FX: Wind=%d WidthDelta=%d LayerDelta=%d Contact=%d"),Warm(First),Difference(First,Wide),Difference(First,Raised),Warm(Sparks)));
	return true;
}
#endif
