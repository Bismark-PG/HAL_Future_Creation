#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "AssetCompilingManager.h"
#include "MaterialShaderPrecompileMode.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "../HALBattleTopFXTypes.h"

// Real RHI only: saved Niagara graphs, shader compilation and rendered output, without modifying assets.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBattleTopRenderedTest, "HAL.RenderSmoke.BattleTop.WarmWindAndDirectionalSparks",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBattleTopRenderedTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender())
	{
		AddError(TEXT("Requires a real RHI; omit -nullrhi."));
		return false;
	}
	auto* Wind =
		LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/FX/BattleTop/NS_BattleTop_Wind.NS_BattleTop_Wind"));
	auto* Sparks =
		LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/FX/BattleTop/NS_BattleTop_Sparks.NS_BattleTop_Sparks"));
	auto* Material =
		LoadObject<UMaterial>(nullptr, TEXT("/Game/Assets/FX/BattleTop/M_BattleTop_Wind.M_BattleTop_Wind"));
	auto* SparkMaterial =
		LoadObject<UMaterial>(nullptr, TEXT("/Game/Assets/FX/BattleTop/M_BattleTop_Spark.M_BattleTop_Spark"));
	if (!TestNotNull(TEXT("Saved wind"), Wind) || !TestNotNull(TEXT("Saved sparks"), Sparks) ||
		!TestNotNull(TEXT("Saved wind material"), Material) ||
		!TestNotNull(TEXT("Saved spark material"), SparkMaterial))
	{
		return false;
	}
	Material->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	SparkMaterial->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	FAssetCompilingManager::Get().FinishAllCompilation();
	Wind->RequestCompile(false);
	Wind->WaitForCompilationComplete();
	Sparks->RequestCompile(false);
	Sparks->WaitForCompilationComplete();
	TestTrue(TEXT("Both saved systems compile"), Wind->IsReadyToRun() && Sparks->IsReadyToRun());
	const auto Init = UWorld::InitializationValues()
						  .AllowAudioPlayback(false)
						  .CreatePhysicsScene(false)
						  .CreateNavigation(false)
						  .CreateAISystem(false)
						  .CreateFXSystem(true)
						  .SetTransactional(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT
	{
		W->DestroyWorld(false);
		GEngine->DestroyWorldContext(W);
	};
	auto* A = W->SpawnActor<AActor>();
	auto* C = NewObject<UNiagaraComponent>(A);
	A->SetRootComponent(C);
	C->SetAutoActivate(false);
	C->SetAsset(Wind);
	C->RegisterComponent();
	auto* MID = UMaterialInstanceDynamic::Create(Material, A);
	C->SetVariableMaterial(TEXT("User.WindMaterial"), MID);
	C->Activate(true);
	C->AdvanceSimulation(10, 1.f / 60.f);
	auto* RT = NewObject<UTextureRenderTarget2D>(A);
	RT->InitAutoFormat(512, 512);
	RT->UpdateResourceImmediate(true);
	auto* Capture = W->SpawnActor<ASceneCapture2D>();
	Capture->SetActorLocation(FVector(0, 0, 500));
	Capture->SetActorRotation(FRotator(-90, 0, 0));
	auto* Camera = Capture->GetCaptureComponent2D();
	Camera->TextureTarget = RT;
	Camera->bCaptureEveryFrame = Camera->bCaptureOnMovement = false;
	Camera->ProjectionType = ECameraProjectionMode::Orthographic;
	Camera->OrthoWidth = 260;
	Camera->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Camera->ShowFlags.SetAntiAliasing(false);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	const auto Pixels = [&](const TCHAR* Name) {
		Camera->CaptureScene();
		TArray<FColor> P;
		RT->GameThread_GetRenderTargetResource()->ReadPixels(P);
		FFileHelper::CreateBitmap(*(FPaths::ProjectSavedDir() / TEXT("BattleTop/Polish_20261113") / Name), 512, 512,
								  P.GetData());
		return P;
	};
	MID->SetScalarParameterValue(TEXT("Envelope"), 1);
	const auto First = Pixels(TEXT("Wind_0.bmp"));
	MID->SetScalarParameterValue(TEXT("SpinAngle"), .13f);
	const auto Turned = Pixels(TEXT("Wind_Turned.bmp"));
	MID->SetScalarParameterValue(TEXT("Envelope"), 0);
	const auto Hidden = Pixels(TEXT("Wind_Hidden.bmp"));
	int32 Warm = 0, Changed = 0, Faded = 0;
	for (int32 I = 0; I < First.Num(); ++I)
	{
		Warm += First[I].R > 35 && First[I].R > First[I].B * 2;
		Changed += FMath::Abs(int32(First[I].R) - Turned[I].R) > 20;
		Faded += int32(First[I].R) > int32(Hidden[I].R) + 20;
	}
	TestTrue(TEXT("Actual Niagara wind has warm pixels"), Warm > 400);
	TestTrue(TEXT("Shared spin phase rotates the flow"), Changed > 400);
	TestTrue(TEXT("Exit envelope hides the flow"), Faded > 400);
	MID->SetScalarParameterValue(TEXT("Envelope"), 1);
	C->SetVariableVec2(TEXT("User.WindSize"), FVector2D(100, 100));
	C->ReinitializeSystem();
	C->AdvanceSimulation(10, 1.f / 60.f);
	const auto Small = Pixels(TEXT("Wind_Small.bmp"));
	int32 SmallWarm = 0;
	for (const auto& P : Small)
	{
		SmallWarm += P.R > 35 && P.R > P.B * 2;
	}
	TestTrue(TEXT("Authored diameter really changes Niagara sprite size"), SmallWarm > 100 && SmallWarm < Warm / 2);
	C->DestroyComponent();
	auto* S = NewObject<UNiagaraComponent>(A);
	S->SetAsset(Sparks);
	S->SetAutoActivate(false);
	S->SetWorldRotation(FQuat::FindBetweenNormals(FVector::UpVector, FVector::ForwardVector));
	S->SetVariableInt(TEXT("User.BurstCount"), 20);
	S->SetVariableVec3(TEXT("User.VelocityMin"), FVector(0, 0, 550));
	S->SetVariableVec3(TEXT("User.VelocityMax"), FVector(0, 0, 550));
	S->RegisterComponent();
	S->Activate(true);
	S->AdvanceSimulation(6, 1.f / 60.f);
	const auto Burst = Pixels(TEXT("Sparks_Forward.bmp"));
	int32 SparkPixels = 0;
	for (const auto& P : Burst)
	{
		SparkPixels += P.R > 45 && P.R > P.B * 2;
	}
	TestTrue(TEXT("Saved spark graph renders actual moving streaks"), SparkPixels > 5);
	AddInfo(FString::Printf(TEXT("BattleTop RHI pixels Warm=%d Rotated=%d Faded=%d Sparks=%d"), Warm, Changed, Faded,
							SparkPixels));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBattleTopSprayRenderedTest,
								 "HAL.RenderSmoke.BattleTop.ContactSprayPixelsAndMotion",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBattleTopSprayRenderedTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender())
	{
		AddError(TEXT("Requires actual RHI"));
		return false;
	}
	auto* System = LoadObject<UNiagaraSystem>(
		nullptr, TEXT("/Game/Assets/FX/BattleTop/NS_BattleTop_ContactSpray.NS_BattleTop_ContactSpray"));
	auto* Material = LoadObject<UMaterial>(
		nullptr, TEXT("/Game/Assets/FX/BattleTop/M_BattleTop_ContactSpray.M_BattleTop_ContactSpray"));
	if (!TestNotNull(TEXT("Contact spray system"), System) || !TestNotNull(TEXT("Contact spray material"), Material))
	{
		return false;
	}
	Material->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	FAssetCompilingManager::Get().FinishAllCompilation();
	System->RequestCompile(false);
	System->WaitForCompilationComplete();
	const auto Init = UWorld::InitializationValues()
						  .AllowAudioPlayback(false)
						  .CreatePhysicsScene(false)
						  .CreateNavigation(false)
						  .CreateAISystem(false)
						  .CreateFXSystem(true)
						  .SetTransactional(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT
	{
		W->DestroyWorld(false);
		GEngine->DestroyWorldContext(W);
	};
	auto* A = W->SpawnActor<AActor>();
	auto* RT = NewObject<UTextureRenderTarget2D>(A);
	RT->InitAutoFormat(768, 512);
	RT->UpdateResourceImmediate(true);
	auto* CameraActor = W->SpawnActor<ASceneCapture2D>();
	CameraActor->SetActorLocation(FVector(700, -900, 700));
	CameraActor->SetActorRotation((FVector(0, 150, 80) - CameraActor->GetActorLocation()).Rotation());
	auto* Camera = CameraActor->GetCaptureComponent2D();
	Camera->TextureTarget = RT;
	Camera->bCaptureEveryFrame = Camera->bCaptureOnMovement = false;
	Camera->ProjectionType = ECameraProjectionMode::Orthographic;
	Camera->OrthoWidth = 1800;
	Camera->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Camera->ShowFlags.SetAntiAliasing(false);
	Camera->ShowFlags.SetMotionBlur(false);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	FHALBattleTopFXConfig FX;
	FX.ContactCount = 32;
	const auto Motion = HALBattleTopFX::ContactSprayMotion(FX, FVector::RightVector, FVector::ForwardVector);
	const auto Spawn = [&](int32 Count, FVector Min, FVector Max, bool Forces) {
		auto* C = NewObject<UNiagaraComponent>(A);
		C->SetAsset(System);
		C->SetAutoActivate(false);
		C->SetWorldLocation(FVector(0, 0, 120));
		C->SetWorldRotation(Motion.Rotation);
		C->SetRandomSeedOffset(17);
		C->SetVariableInt(TEXT("User.BurstCount"), Count);
		C->SetVariableVec3(TEXT("User.VelocityMin"), Min);
		C->SetVariableVec3(TEXT("User.VelocityMax"), Max);
		C->SetVariableVec2(TEXT("User.SizeMin"), FX.ContactSizeMin);
		C->SetVariableVec2(TEXT("User.SizeMax"), FX.ContactSizeMax);
		C->SetVariableFloat(TEXT("User.LifetimeMin"), .55f);
		C->SetVariableFloat(TEXT("User.LifetimeMax"), .55f);
		C->SetVariableVec3(TEXT("User.Gravity"), Forces ? Motion.Gravity : FVector::ZeroVector);
		C->SetVariableFloat(TEXT("User.Drag"), Forces ? FX.ContactDrag : 0);
		C->RegisterComponent();
		C->Activate(true);
		return C;
	};
	const auto Pixels = [&](const TCHAR* Name) {
		Camera->CaptureScene();
		TArray<FColor> P;
		RT->GameThread_GetRenderTargetResource()->ReadPixels(P);
		FFileHelper::CreateBitmap(*(FPaths::ProjectSavedDir() / TEXT("BattleTop/Spray_20261113") / Name), 768, 512,
								  P.GetData());
		return P;
	};
	const auto Visible = [](const FColor& P) { return P.R > 50 && P.R > P.B * 1.3; };
	const auto CountPixels = [&](const TArray<FColor>& P) { return P.FilterByPredicate(Visible).Num(); };
	auto* C = Spawn(32, Motion.VelocityMin, Motion.VelocityMax, false);
	C->AdvanceSimulation(12, 1.f / 60.f);
	const auto Ballistic = Pixels(TEXT("Spray_Ballistic_20.bmp"));
	C->DestroyComponent();
	C = Spawn(32, Motion.VelocityMin, Motion.VelocityMax, true);
	C->AdvanceSimulation(12, 1.f / 60.f);
	const auto Spray = Pixels(TEXT("Spray_20.bmp"));
	C->AdvanceSimulation(10, 1.f / 60.f);
	const auto Late = Pixels(TEXT("Spray_37.bmp"));
	C->AdvanceSimulation(30, 1.f / 60.f);
	const auto Expired = Pixels(TEXT("Spray_Expired.bmp"));
	C->DestroyComponent();
	C = Spawn(7, FVector(-80, -80, 550), FVector(80, 80, 1000), false);
	C->AdvanceSimulation(12, 1.f / 60.f);
	const auto Narrow = Pixels(TEXT("Spray_Narrow_20.bmp"));
	C->DestroyComponent();
	int32 MotionPixels = 0;
	for (int32 I = 0; I < Spray.Num(); ++I)
	{
		MotionPixels += FMath::Abs(int32(Spray[I].R) - Ballistic[I].R) > 20;
	}
	const int32 Bright = CountPixels(Spray), Thin = CountPixels(Narrow), Tail = CountPixels(Late),
				Dead = CountPixels(Expired);
	AddInfo(FString::Printf(TEXT("Spray pixels Wide=%d Narrow=%d Motion=%d Tail=%d Expired=%d"), Bright, Thin,
							MotionPixels, Tail, Dead));
	TestTrue(TEXT("Warm spray is visible at gameplay-like viewing scale"), Bright > 150);
	TestTrue(TEXT("Wide dense burst covers more pixels than a seven-particle narrow beam"), Bright > Thin * 1.5);
	TestTrue(TEXT("Gravity and drag alter actual particle motion"), MotionPixels > 60);
	TestTrue(TEXT("Visible trailing sparks survive beyond the old 0.22 s lifetime"), Tail > 20);
	TestTrue(TEXT("Burst naturally expires without a new emitter loop"), Dead < 5);
	return true;
}
#endif
