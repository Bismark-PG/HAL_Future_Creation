#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALDamageFeedbackSubsystem.h"
#include "../HALSkillTestDummy.h"
#include "../HALVehiclePresentationComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ContentStreaming.h"
#include "MaterialShaderPrecompileMode.h"
#include "UObject/UnrealType.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALDamageFeedbackRenderTest,"HAL.RenderSmoke.DamageFeedback.RealVehicleOutlineRGBIsolationAndOcclusion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALDamageFeedbackRenderTest::RunTest(const FString&)
{
	if(!FApp::CanEverRender()) { AddError(TEXT("Actual RHI required")); return false; }
	// UE's 8-bit backbuffer quantization dither is independent of ShowFlags.Grain.
	// Suppress only its subpixel noise during this test, then restore the user's CVar.
	auto* Dither=IConsoleManager::Get().FindConsoleVariable(TEXT("r.BackbufferQuantizationDitheringOverride"));
	if(!TestNotNull(TEXT("Backbuffer quantization control exists"),Dither)) { return false; }
	const int32 PreviousDither=Dither->GetInt(); Dither->Set(32,ECVF_SetByCode);
	ON_SCOPE_EXIT { Dither->Set(PreviousDither,ECVF_SetByCode); };
	const auto* Saved=LoadObject<UHALDamageFeedbackConfig>(nullptr,TEXT("/Game/Data/StatusFX/DA_DamageFeedback_Default"));
	if(!TestTrue(TEXT("Global material is reachable"),Saved&&Saved->Material)) { return false; }
	auto* BaseMaterial=Saved->Material->GetMaterial(); BaseMaterial->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(true).ShouldSimulatePhysics(false).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* Config=DuplicateObject<UHALDamageFeedbackConfig>(Saved,GetTransientPackage()); auto* FX=W->GetSubsystem<UHALDamageFeedbackSubsystem>();
	if(!TestNotNull(TEXT("Rendering-world damage subsystem"),FX)) { return false; }
	FX->Config=Config; FX->bPrepared=true;
	const auto Cube=[&](FVector Position,FVector Scale)
	{
		auto* A=W->SpawnActor<AActor>(); auto* C=NewObject<UStaticMeshComponent>(A); A->SetRootComponent(C);
		C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetWorldLocation(Position); C->SetWorldScale3D(Scale); C->RegisterComponent(); return A;
	};
	auto* TestConfig=DuplicateObject<UHALSkillTestDummyConfig>(LoadObject<UHALSkillTestDummyConfig>(nullptr,TEXT("/Game/Data/Testing/DA_SkillTestDummy_Default")),GetTransientPackage()); TestConfig->bShowHealth=false;
	const FTransform Pose(FVector(0,-350,75)); auto* Car=W->SpawnActorDeferred<AHALSkillTestDummy>(AHALSkillTestDummy::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	FindFProperty<FObjectProperty>(Car->GetClass(),TEXT("TestConfig"))->SetObjectPropertyValue_InContainer(Car,TestConfig); Car->FinishSpawning(Pose);
	if(!Car->IsActorInitialized()) { Car->PreInitializeComponents(); Car->InitializeComponents(); Car->PostInitializeComponents(); } Car->DispatchBeginPlay();
	for(int32 I=0;I<60;++I) { W->Tick(LEVELTICK_All,.05f); }
	auto* Other=Cube(FVector(0,350,75),FVector(2,2,1)); auto* OtherMesh=CastChecked<UStaticMeshComponent>(Other->GetRootComponent());
	Cube(FVector(0,0,-15),FVector(20,20,.1));
	auto* LightActor=W->SpawnActor<AActor>(); auto* Light=NewObject<UDirectionalLightComponent>(LightActor); LightActor->SetRootComponent(Light); Light->SetIntensity(4); Light->SetWorldRotation(FRotator(-55,-30,0)); Light->RegisterComponent();
	FHALVehicleFXTargetSnapshot Targets; Car->FindComponentByClass<UHALVehiclePresentationComponent>()->CaptureStatusFXTargets(Targets);
	if(!TestTrue(TEXT("Actual skeletal Body is registered"),!Targets.Targets.IsEmpty()&&Targets.Targets[0].Mesh->IsA<USkeletalMeshComponent>())) { return false; }
	auto* Body=Targets.Targets[0].Mesh.Get(); auto* OriginalOverlay=Body->GetOverlayMaterial();
	FAssetCompilingManager::Get().FinishAllCompilation(); if(GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); }
	for(const auto& T:Targets.Targets) { T.Mesh->SetTextureForceResidentFlag(true); } IStreamingManager::Get().StreamAllResources(10);
	auto* Capture=W->SpawnActor<ASceneCapture2D>(); auto* Camera=Capture->GetCaptureComponent2D(); Camera->bCaptureEveryFrame=false; Camera->bAlwaysPersistRenderingState=true; Camera->CaptureSource=SCS_FinalColorLDR; Camera->ProjectionType=ECameraProjectionMode::Orthographic; Camera->OrthoWidth=1400;
	// Pixel comparisons need deterministic capture output. TemporalAA=false alone selects
	// FXAA rather than disabling AA, and screen-space lighting can retain capture history.
	Camera->ShowFlags.SetAntiAliasing(false); Camera->ShowFlags.SetTemporalAA(false);
	Camera->ShowFlags.SetAmbientOcclusion(false); Camera->ShowFlags.SetScreenSpaceReflections(false);
	Camera->ShowFlags.SetMotionBlur(false); Camera->ShowFlags.SetGrain(false);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod=true; Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
	Camera->ShowFlags.SetEyeAdaptation(false); Camera->ShowFlags.SetLocalExposure(false);
	Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
	Camera->PostProcessSettings.bOverride_AutoExposureBias=true; Camera->PostProcessSettings.AutoExposureBias=0;
	Camera->SetWorldLocation(FVector(0,0,1200)); Camera->SetWorldRotation(FRotator(-90,0,0));
	auto* Target=NewObject<UTextureRenderTarget2D>(); Target->InitCustomFormat(768,512,PF_B8G8R8A8,false); Camera->TextureTarget=Target;
	FString Output; if(!FParse::Value(FCommandLine::Get(),TEXT("HALDamageRenderOutput="),Output)) { Output=FPaths::ProjectSavedDir()/TEXT("DamageFeedback/20261115/Render"); } IFileManager::Get().MakeDirectory(*Output,true);
	const auto Pixels=[&](const TCHAR* Name)
	{ Camera->CaptureScene(); TArray<FColor> P; Target->GameThread_GetRenderTargetResource()->ReadPixels(P); TestTrue(TEXT("Proof bitmap saved"),FFileHelper::CreateBitmap(*(Output/Name),768,512,P.GetData())); return P; };
	const auto Diff=[](const TArray<FColor>& A,const TArray<FColor>& B,int32 MinX,int32 MaxX,int32 Border=0)
	{ int32 N=0; for(int32 Y=Border;Y<512-Border;++Y) for(int32 X=FMath::Max(MinX,Border);X<FMath::Min(MaxX,768-Border);++X) { const int32 I=Y*768+X; N+=FMath::Abs(int32(A[I].R)-B[I].R)+FMath::Abs(int32(A[I].G)-B[I].G)+FMath::Abs(int32(A[I].B)-B[I].B)>9; } return N; };
	const auto Pulse=[&](AActor* A)
	{ FHALDamageFeedbackPulse P; P.Sequence=1; P.AttackId=FGuid::NewGuid(); P.AttackPulse=1; P.ActualDamage=25; P.ServerTime=FX->ServerTime(W)-.015; FX->Receive(A,P); };
	Camera->CaptureScene(); FlushRenderingCommands(); Camera->CaptureScene(); FlushRenderingCommands();
	const auto Bare=Pixels(TEXT("Damage_00_Bare.bmp")); Pulse(Car); const auto Full=Pixels(TEXT("Damage_01_Full.bmp"));
	TestEqual(TEXT("Project enables CustomDepth with stencil"),IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"))->GetInt(),3);
	AddInfo(FString::Printf(TEXT("Damage renderer: target0=%s volumes=%d post=%d materials=%d"),*FX->Material->K2_GetVectorParameterValue(TEXT("Target0")).ToString(),W->PostProcessVolumes.Num(),Camera->ShowFlags.PostProcessing,Camera->ShowFlags.PostProcessMaterial));
	const int32 Changed=Diff(Bare,Full,0,384),OtherChanged=Diff(Bare,Full,410,768);
	TestTrue(TEXT("Actual skeletal vehicle visibly receives all three layers"),Changed>500);
	TestEqual(TEXT("Unhit vehicle and its nearby ground remain unchanged"),OtherChanged,0);
	TestTrue(TEXT("Only registered visible Body participates, physics root remains unmodified"),FX->GetActiveTargetCount()==1&&Body->bRenderCustomDepth);
	TestTrue(TEXT("Damage feedback never steals the electric/status Overlay"),Body->GetOverlayMaterial()==OriginalOverlay);
	const auto Style=Config->Style; FX->ClearTarget(Car); Config->Style.WhiteOpacity=0; Config->Style.RGBOpacity=0; Pulse(Car); const auto Red=Pixels(TEXT("Damage_02_RedOnly.bmp"));
	TestTrue(TEXT("Red fill can be tuned independently"),Diff(Bare,Red,0,384)>300);
	FX->ClearTarget(Car); Config->Style=Style; Config->Style.BodyOpacity=0; Config->Style.RGBOpacity=0; Pulse(Car); const auto White=Pixels(TEXT("Damage_03_WhiteOnly.bmp"));
	const int32 WhitePixels=Diff(Bare,White,0,384); TestTrue(TEXT("White outline has its own visible pixels"),WhitePixels>30);
	FX->ClearTarget(Car); Config->Style=Style; Config->Style.BodyOpacity=0; Config->Style.WhiteOpacity=0; Pulse(Car); const auto RGB=Pixels(TEXT("Damage_04_RGBOnly.bmp"));
	const int32 RGBPixels=Diff(Bare,RGB,0,384); TestTrue(TEXT("RGB separation is local and independently visible"),RGBPixels>30);
	FX->ClearTarget(Car); Config->Style=Style; Pulse(Car); Pulse(Other); Pixels(TEXT("Damage_05_TwoTargets.bmp"));
	TestTrue(TEXT("Two different mesh kinds have independent stencil identities"),FX->GetActiveTargetCount()==2&&Body->CustomDepthStencilValue!=OtherMesh->CustomDepthStencilValue);
	FX->ClearTarget(Car); TestTrue(TEXT("Clearing one target leaves the other active"),FX->GetActiveTargetCount()==1&&!Body->bRenderCustomDepth&&OtherMesh->bRenderCustomDepth); FX->ClearTarget(Other);
	OtherMesh->SetCustomDepthStencilValue(77); OtherMesh->SetRenderCustomDepth(true); Pulse(Other);
	TestTrue(TEXT("An unrelated pre-existing stencil owner is never overwritten"),OtherMesh->CustomDepthStencilValue==77&&OtherMesh->bRenderCustomDepth&&FX->GetActiveTargetCount()==0); FX->ClearTarget(Other); OtherMesh->SetRenderCustomDepth(false);
	OtherMesh->SetCustomDepthStencilValue(Config->Style.FirstStencil); OtherMesh->SetRenderCustomDepth(true); Pulse(Car);
	TestTrue(TEXT("Reserved-range collision allocates another identity instead of coloring foreign geometry"),Body->CustomDepthStencilValue!=OtherMesh->CustomDepthStencilValue&&OtherMesh->CustomDepthStencilValue==Config->Style.FirstStencil);
	FX->ClearTarget(Car); OtherMesh->SetRenderCustomDepth(false);
	Pulse(Car); Config->Style.FirstStencil=120; FX->ClearTarget(Car);
	TestTrue(TEXT("Cleanup restores the captured lease even if the global configuration was edited"),!Body->bRenderCustomDepth); Config->Style.FirstStencil=Style.FirstStencil;
	if(auto* Shock=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Assets/FX/Materials/M_FX_VehicleShock_Overlay")))
	{
		Body->SetOverlayMaterial(Shock); Pulse(Car); TestTrue(TEXT("Existing electric overlay survives a received-damage pulse"),Body->GetOverlayMaterial()==Shock);
		FX->ClearTarget(Car); TestTrue(TEXT("Received-damage cleanup never resets the electric overlay"),Body->GetOverlayMaterial()==Shock); Body->SetOverlayMaterial(OriginalOverlay);
	}
	auto* Wall=Cube(FVector(0,-350,500),FVector(8,4,2)); Pixels(TEXT("Damage_06_OccludedBare.bmp")); Pulse(Car);
	// Compare the same post-process chain with zero/active event weights. Enabling a pass
	// alone may change scene-edge resampling by a few neutral-gray levels; it is not a hit.
	for(int32 I=0;I<8;++I) { FX->Material->SetVectorParameterValue(FName(*FString::Printf(TEXT("Target%d"),I)),FLinearColor(0,0,0,0)); }
	const auto Occluded=Pixels(TEXT("Damage_06b_OccludedNeutralPass.bmp")); FX->UpdateRendering();
	const auto HiddenHit=Pixels(TEXT("Damage_07_OccludedHit.bmp"));
	TestEqual(TEXT("A vehicle behind opaque scenery never flashes through the wall (entire image)"),Diff(Occluded,HiddenHit,0,768),0);
	FX->ClearTarget(Car); FHALDamageFeedbackPulse Old; Old.Sequence=2; Old.AttackId=FGuid::NewGuid(); Old.AttackPulse=1; Old.ActualDamage=25; Old.ServerTime=FX->ServerTime(W)-Config->Style.MaximumEventAge-.1;
	FX->Receive(Car,Old); TestEqual(TEXT("Stale network events never replay a damage flash"),FX->GetActiveTargetCount(),0);
	FX->ClearTarget(Car); Wall->Destroy();
	// Removing the occluder changes scene history; capture a clean control after that
	// edit so expiry is compared against the same current geometry and view history.
	Camera->CaptureScene(); FlushRenderingCommands();
	const auto BeforeFinalHit=Pixels(TEXT("Damage_08a_BeforeFinalHit.bmp")); Pulse(Car);
	for(auto& Slot:FX->Slots) if(Slot.Target.Get()==Car) { Slot.Playback.LastBody-=2; Slot.Playback.LastAccent-=2; Slot.Playback.BurstStart-=2; }
	FX->UpdateRendering(); const auto Gone=Pixels(TEXT("Damage_08_Gone.bmp"));
	TestTrue(TEXT("Envelope completion restores stencil state and leaves no overlay residue"),FX->GetActiveTargetCount()==0&&!Body->bRenderCustomDepth&&Body->GetOverlayMaterial()==OriginalOverlay);
	TestEqual(TEXT("Final image returns to current clean baseline"),Diff(BeforeFinalHit,Gone,0,768),0);
	AddInfo(FString::Printf(TEXT("Damage proof: skeletal=%d unhit=%d outline=%d rgb=%d output=%s"),Changed,OtherChanged,WhitePixels,RGBPixels,*Output)); return true;
}
#endif
