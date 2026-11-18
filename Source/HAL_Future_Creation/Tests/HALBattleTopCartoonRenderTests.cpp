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
#include "NiagaraDataSetAccessor.h"
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
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"

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
	FString RenderOutput=FPaths::ProjectSavedDir()/TEXT("BattleTop/Cartoon_20261113");
	FParse::Value(FCommandLine::Get(),TEXT("HALBattleTopRenderOutput="),RenderOutput);
	IFileManager::Get().MakeDirectory(*RenderOutput,true);
	const auto Pixels=[&](const TCHAR* Name){Camera->CaptureScene();TArray<FColor> P;Target->GameThread_GetRenderTargetResource()->ReadPixels(P);
		TestTrue(TEXT("Render evidence is saved"),FFileHelper::CreateBitmap(*(RenderOutput/Name),512,512,P.GetData()));return P;};
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
	// Check that the configured angle is actually reached, not merely that the angle is below a cap.
	MID->SetScalarParameterValue(TEXT("Envelope"),1);
	C->SetVariableVec3(TEXT("User.PositionMin"),FVector(0,0,-11.25));
	C->SetVariableVec3(TEXT("User.PositionMax"),FVector(0,0,33.75));
	TArray<FColor> FlatImage;
	for(const float RequestedTilt:{0.f,12.f,30.f,60.f,80.f})
	{
		C->SetVariableFloat(TEXT("User.TiltDegrees"),RequestedTilt);
		// Retain the old input in the fixture so a pre-fix resource reproduces the real attenuation.
		const float LegacyTilt=RequestedTilt/(360.f*FMath::Sqrt(2.f));
		C->SetVariableVec3(TEXT("User.RotationMin"),FVector(-LegacyTilt,-LegacyTilt,0));
		C->SetVariableVec3(TEXT("User.RotationMax"),FVector(LegacyTilt,LegacyTilt,1));
		const float Speed=RequestedTilt>=60?3600.f:1440.f;
		ResetWind(Speed);
		if(RequestedTilt==0)FlatImage=Pixels(TEXT("WindTilt_0.bmp"));
		if(RequestedTilt==30)
			{
				const auto Tilted=Pixels(TEXT("WindTilt_30.bmp"));
				TestTrue(TEXT("Thirty degrees produces a visibly different silhouette from flat arcs"),Difference(FlatImage,Tilted)>1000);
			}
		float MinimumTilt=180,MaximumTilt=0,MinimumYawAdvance=360;int32 Compared=0,Observed=0;
		TMap<int32,FQuat4f> Previous;
		for(int32 Frame=0;Frame<120;++Frame)
		{
			if(auto Controller=C->GetSystemInstanceController())if(auto* Instance=Controller->GetSystemInstance_Unsafe())
				for(const auto& E:Instance->GetEmitters())
				{
					const auto& D=E->GetParticleData();
					const auto Q=FNiagaraDataSetAccessor<FQuat4f>::CreateReader(D,TEXT("MeshOrientation"));
					const auto ID=FNiagaraDataSetAccessor<int32>::CreateReader(D,TEXT("UniqueID"));
					if(!Q.IsValid()||!ID.IsValid()){AddError(TEXT("Missing actual particle orientation/identity data"));return false;}
					TMap<int32,FQuat4f> Current;
					for(int32 I=0;I<E->GetNumParticles();++I)
					{
						const auto Rotation=Q[I].GetNormalized();
						const float ActualTilt=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Rotation.RotateVector(FVector3f(0,0,1)).Z,-1.f,1.f)));
						MinimumTilt=FMath::Min(MinimumTilt,ActualTilt);MaximumTilt=FMath::Max(MaximumTilt,ActualTilt);++Observed;
						if(const auto* Old=Previous.Find(ID[I]))
						{
							const auto Delta=(Rotation*Old->Inverse()).GetNormalized();
							const auto X=Delta.RotateVector(FVector3f(1,0,0));
							MinimumYawAdvance=FMath::Min(MinimumYawAdvance,FMath::RadiansToDegrees(FMath::Atan2(X.Y,X.X)));
							TestTrue(TEXT("Arc advances around vertical without pitching or rolling during life"),Delta.RotateVector(FVector3f(0,0,1)).Equals(FVector3f(0,0,1),.001f));++Compared;
						}
						Current.Add(ID[I],Rotation);
					}
					Previous=MoveTemp(Current);
				}
			C->AdvanceSimulation(1,1.f/60);
		}
		TestTrue(TEXT("Multiple complete particle lifetimes were checked"),Observed>500&&Compared>400);
		TestTrue(TEXT("Every arc reaches the configured tilt without yaw attenuation"),FMath::IsNearlyEqual(MinimumTilt,RequestedTilt,.1f)&&FMath::IsNearlyEqual(MaximumTilt,RequestedTilt,.1f));
		TestTrue(TEXT("High-speed airflow advances in the intended direction at the requested rate"),FMath::IsNearlyEqual(MinimumYawAdvance,Speed/60,.01f));
		AddInfo(FString::Printf(TEXT("Wind tilt requested=%.1f actual=%.3f..%.3f speed=%.1f minimum step=%.3f samples=%d comparisons=%d"),RequestedTilt,MinimumTilt,MaximumTilt,Speed,MinimumYawAdvance,Observed,Compared));
	}
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
