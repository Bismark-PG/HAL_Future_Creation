#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALBullChargeAbility.h"
#include "../HALBullChargeComponent.h"
#include "../HALVehiclePresentationComponent.h"
#include "../HALVehiclePresentationConfig.h"
#include "AssetCompilingManager.h"
#include "MaterialShaderPrecompileMode.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraDataSetAccessor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/DirectionalLight.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBullChargeFXRenderTest, "HAL.RenderSmoke.BullCharge.ThreeLayersAndWorldSpaceExhaust",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBullChargeFXRenderTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("This test requires a real RHI.")); return false; }
	const auto Load = [](const TCHAR* Name) { return LoadObject<UNiagaraSystem>(nullptr, *(FString(TEXT("/Game/Assets/FX/BullCharge/")) + Name + TEXT(".") + Name)); };
	auto* Wind = Load(TEXT("NS_BullCharge_FrontWind")); auto* Rear = Load(TEXT("NS_BullCharge_RearSparks")); auto* Impact = Load(TEXT("NS_BullCharge_ToonImpact"));
	if (!TestNotNull(TEXT("Saved front airflow"), Wind) || !TestNotNull(TEXT("Saved rear sparks"), Rear) || !TestNotNull(TEXT("Saved three-layer impact"), Impact)) { return false; }
	TestEqual(TEXT("Contact has sparks, flash and shock emitters"), Impact->GetEmitterHandles().Num(), 3);
	for (const auto& H : Rear->GetEmitterHandles()) { TestFalse(TEXT("Rear particles stay in world space after spawning"), H.GetInstance().Emitter->GetLatestEmitterData()->bLocalSpace); }
	for (const auto* Name : {TEXT("M_BullCharge_FrontWind"), TEXT("M_BullCharge_ToonSpark"), TEXT("M_BullCharge_Flash"), TEXT("M_BullCharge_Shock")})
	{
		auto* M = LoadObject<UMaterial>(nullptr, *(FString(TEXT("/Game/Assets/FX/BullCharge/")) + Name + TEXT(".") + Name));
		if (!TestNotNull(TEXT("Saved standalone material"), M)) { return false; }
		M->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	}
	FAssetCompilingManager::Get().FinishAllCompilation();
	for (auto* S : {Wind, Rear, Impact}) { S->RequestCompile(false); S->WaitForCompilationComplete(); TestTrue(TEXT("Niagara recipe compiles"), S->IsReadyToRun()); }
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(true).SetTransactional(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* Owner = W->SpawnActor<AActor>(); auto* Root = NewObject<USceneComponent>(Owner); Owner->SetRootComponent(Root); Root->RegisterComponent();
	const auto Make = [&](UNiagaraSystem* S, FVector Point)
	{
		auto* C = NewObject<UNiagaraComponent>(Owner); C->SetAutoActivate(false); C->SetAsset(S);
		C->SetupAttachment(Root); C->SetRelativeLocation(Point); C->RegisterComponent(); return C;
	};
	auto* C = Make(Wind, FVector(120, 0, 40));
	auto* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/Assets/FX/BullCharge/M_BullCharge_FrontWind.M_BullCharge_FrontWind"));
	auto* MID = UMaterialInstanceDynamic::Create(Material, C); C->SetVariableMaterial(TEXT("User.WindMaterial"), MID);
	C->Activate(true); C->AdvanceSimulation(25, 1.f / 60);
	auto* Capture = W->SpawnActor<ASceneCapture2D>(); auto* Camera = Capture->GetCaptureComponent2D(); Camera->bCaptureEveryFrame = false;
	Camera->CaptureSource = SCS_FinalColorLDR; Camera->ProjectionType = ECameraProjectionMode::Orthographic; Camera->OrthoWidth = 700;
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true; Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true; Camera->PostProcessSettings.AutoExposureBias = 0;
	Camera->SetWorldLocation(FVector(420, -650, 750)); Camera->SetWorldRotation((-Camera->GetComponentLocation()).Rotation());
	auto* Target = NewObject<UTextureRenderTarget2D>(); Target->InitCustomFormat(768, 768, PF_B8G8R8A8, false); Camera->TextureTarget = Target;
	FString Output;
	if (!FParse::Value(FCommandLine::Get(), TEXT("HALBullChargeRenderOutput="), Output)) { Output = FPaths::ProjectSavedDir() / TEXT("Car04FX/20261114/Render"); }
	IFileManager::Get().MakeDirectory(*Output, true);
	const auto Pixels = [&](const TCHAR* Name)
	{
		Camera->CaptureScene(); TArray<FColor> P; Target->GameThread_GetRenderTargetResource()->ReadPixels(P);
		TestTrue(TEXT("Render proof saved"), FFileHelper::CreateBitmap(*(Output / Name), 768, 768, P.GetData())); return P;
	};
	const auto Visible = [](const TArray<FColor>& P) { int32 N = 0; for (const auto C : P) { N += FMath::Max3(C.R, C.G, C.B) > 24; } return N; };
	const auto Difference = [](const TArray<FColor>& X, const TArray<FColor>& Y) { int32 N = 0; for (int32 I = 0; I < X.Num(); ++I) { N += FMath::Abs(int32(X[I].R) - Y[I].R) + FMath::Abs(int32(X[I].G) - Y[I].G) + FMath::Abs(int32(X[I].B) - Y[I].B) > 30; } return N; };
	const auto WindImage = Pixels(TEXT("Car04_FrontWind.bmp"));
	TestTrue(TEXT("Achromatic curved wind is visible"), Visible(WindImage) > 300);
	MID->SetScalarParameterValue(TEXT("Envelope"), 0); const auto Hidden = Pixels(TEXT("Car04_FrontWind_Paused.bmp"));
	TestTrue(TEXT("Pause envelope actually hides the headwind"), Difference(WindImage, Hidden) > 300);
	C->DestroyComponent();
	auto* Tail = Make(Rear, FVector(-120, 0, 40));
	Tail->SetVariableFloat(TEXT("User.SpawnRate"), 65); Tail->SetVariableFloat(TEXT("User.LifetimeMin"), .65f); Tail->SetVariableFloat(TEXT("User.LifetimeMax"), .8f);
	Tail->SetVariableVec3(TEXT("User.VelocityMin"), FVector(-650, -30, 30)); Tail->SetVariableVec3(TEXT("User.VelocityMax"), FVector(-550, 30, 80));
	Tail->SetVariableVec2(TEXT("User.SizeMin"), FVector2D(9, 23)); Tail->SetVariableVec2(TEXT("User.SizeMax"), FVector2D(13, 32));
	Tail->Activate(true); Tail->AdvanceSimulation(20, 1.f / 60);
	TestTrue(TEXT("Toon exhaust is visible"), Visible(Pixels(TEXT("Car04_RearSparks.bmp"))) > 100);
	TMap<int32, FVector3f> Before;
	if (auto Controller = Tail->GetSystemInstanceController()) if (auto* Instance = Controller->GetSystemInstance_Unsafe())
		for (const auto& E : Instance->GetEmitters())
		{
			const auto& Data = E->GetParticleData(); const auto P = FNiagaraDataSetAccessor<FNiagaraPosition>::CreateReader(Data, TEXT("Position"));
			const auto ID = FNiagaraDataSetAccessor<int32>::CreateReader(Data, TEXT("UniqueID"));
			if (!TestTrue(TEXT("Actual world particle position data is available"), P.IsValid() && ID.IsValid())) { return false; }
			for (int32 I = 0; I < E->GetNumParticles(); ++I) { Before.Add(ID[I], P[I]); }
		}
	Root->SetWorldLocationAndRotation(FVector(450, 320, 0), FRotator(0, 90, 0)); Tail->AdvanceSimulation(1, 1.f / 60);
	int32 Compared = 0;
	if (auto Controller = Tail->GetSystemInstanceController()) if (auto* Instance = Controller->GetSystemInstance_Unsafe())
		for (const auto& E : Instance->GetEmitters())
		{
			const auto& Data = E->GetParticleData(); const auto P = FNiagaraDataSetAccessor<FNiagaraPosition>::CreateReader(Data, TEXT("Position"));
			const auto ID = FNiagaraDataSetAccessor<int32>::CreateReader(Data, TEXT("UniqueID"));
			for (int32 I = 0; I < E->GetNumParticles(); ++I) if (const auto* Old = Before.Find(ID[I]))
			{
				TestTrue(TEXT("Existing sparks do not follow a sudden 450cm chassis move or its rotation"), FVector3f::Distance(*Old, P[I]) < 40); ++Compared;
			}
		}
	TestTrue(TEXT("Several live sparks were observed across the turn"), Compared > 5);
	Tail->DestroyComponent(); Root->SetWorldTransform(FTransform::Identity);
	auto* Hit = Make(Impact, FVector(0, 0, 60)); Hit->SetVariableInt(TEXT("User.BurstCount"), 48);
	Hit->SetVariableVec3(TEXT("User.VelocityMin"), FVector(400, -300, -100)); Hit->SetVariableVec3(TEXT("User.VelocityMax"), FVector(1000, 300, 500));
	Hit->Activate(true); Hit->AdvanceSimulation(3, 1.f / 60);
	TestTrue(TEXT("Initial local flash and sparks are visible"), Visible(Pixels(TEXT("Car04_Contact_005.bmp"))) > 500);
	Hit->AdvanceSimulation(9, 1.f / 60);
	TestTrue(TEXT("Shock remains visible after short flash"), Visible(Pixels(TEXT("Car04_Contact_020.bmp"))) > 150);
	Hit->DestroyComponent();
	// Assemble the saved model and all three recipes for a human-readable production preview.
	const auto* P = LoadObject<UHALVehiclePresentationConfig>(nullptr, TEXT("/Game/Data/Car_Presentation/DA_Car_04_Presentation.DA_Car_04_Presentation"));
	const auto* Skill = LoadObject<UHALBullChargeConfig>(nullptr, TEXT("/Game/Data/Abilities/DA_Ability_Car04_BullCharge.DA_Ability_Car04_BullCharge"));
	if (!TestNotNull(TEXT("Car04 presentation asset"), P) || !TestNotNull(TEXT("Saved skill FX recipe"), Skill)) { return false; }
	auto* Body = NewObject<USkeletalMeshComponent>(Owner); Body->SetupAttachment(Root); Body->SetSkeletalMeshAsset(P->Mesh); Body->SetRelativeTransform(P->RelativeTransform); Body->RegisterComponent();
	auto* Light = W->SpawnActor<ADirectionalLight>(); Light->SetActorRotation(FRotator(-65, -25, 0)); Light->GetLightComponent()->SetIntensity(4);
	Camera->OrthoWidth = 1100;
	const auto Mounted = [&](UNiagaraSystem* S, const FHALVehicleFXMount& M)
	{
		auto* N = NewObject<UNiagaraComponent>(Owner); N->SetAutoActivate(false); N->SetAsset(S); N->SetupAttachment(Body, M.SocketOrBone);
		N->SetRelativeTransform(M.Offset); N->RegisterComponent(); return N;
	};
	auto* Front = Mounted(Skill->FX.FrontWind ? Skill->FX.FrontWind.Get() : Wind, Skill->FX.FrontMount);
	const FVector WindScale(Skill->FX.WindSizeScale * Skill->FX.WindLengthScale, Skill->FX.WindSizeScale, Skill->FX.WindSizeScale);
	Front->SetVariableVec3(TEXT("User.ScaleMin"), WindScale * .85f);
	Front->SetVariableVec3(TEXT("User.ScaleMax"), WindScale * 1.15f);
	Front->SetVariableVec3(TEXT("User.WindScale"), WindScale);
	Front->SetVariableVec3(TEXT("User.PressureScale"), Skill->FX.PressureSize / 100.f);
	Front->SetVariableVec3(TEXT("User.FlowOrigin"), FVector(Skill->FX.PressureSize.X, 0, 0));
	const auto PreviewMaterial = [&](const TCHAR* Name, bool bShell)
	{
		const FNiagaraVariable Variable(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()), Name);
		if (auto* Source = Cast<UMaterialInterface>(Front->GetAsset()->GetExposedParameters().GetUObject(Variable)))
		{
			auto* M = UMaterialInstanceDynamic::Create(Source, Front); Front->SetVariableMaterial(Name, M);
			M->SetScalarParameterValue(TEXT("UseRuntimeTime"), 1); M->SetScalarParameterValue(TEXT("MotionTime"), 1);
			M->SetScalarParameterValue(TEXT("FlowSpeed"), Skill->FX.WindFlowSpeed);
			M->SetScalarParameterValue(TEXT("Turbulence"), bShell ? Skill->FX.PressureTurbulence : Skill->FX.WindTurbulence / Skill->FX.WindSizeScale);
			if (bShell)
			{
				M->SetVectorParameterValue(TEXT("ShadowColor"), Skill->FX.PressureShadowColor);
				M->SetVectorParameterValue(TEXT("MidColor"), Skill->FX.PressureMidColor);
				M->SetVectorParameterValue(TEXT("LightColor"), Skill->FX.PressureLightColor);
				M->SetScalarParameterValue(TEXT("ShadowThreshold"), Skill->FX.PressureShadowThreshold);
				M->SetScalarParameterValue(TEXT("LightThreshold"), Skill->FX.PressureLightThreshold);
				M->SetScalarParameterValue(TEXT("ToneSoftness"), Skill->FX.PressureToneSoftness);
			}
			else { M->SetScalarParameterValue(TEXT("ContourWidth"), Skill->FX.WindContourWidth); M->SetVectorParameterValue(TEXT("ContourColor"), Skill->FX.WindContourColor); }
			M->SetScalarParameterValue(TEXT("Opacity"), Skill->FX.WindOpacity); M->SetScalarParameterValue(TEXT("PressureOpacity"), Skill->FX.PressureOpacity);
			M->SetScalarParameterValue(TEXT("Feather"), Skill->FX.PressureFeather);
		}
	};
	PreviewMaterial(TEXT("User.WindMaterial"), false); PreviewMaterial(TEXT("User.PressureMaterial"), true);
	Front->Activate(true); Front->AdvanceSimulation(20, 1.f / 60);
	for (const auto& M : Skill->FX.RearMounts)
	{
		auto* N = Mounted(Rear, M);
		FVector Min, Max; HALBullChargeFX::VelocityBox(-N->GetForwardVector(), Skill->FX.RearSpeed, Skill->FX.RearSpreadDegrees, Min, Max);
		N->SetVariableFloat(TEXT("User.SpawnRate"), Skill->FX.RearSpawnRate);
		N->SetVariableFloat(TEXT("User.LifetimeMin"), Skill->FX.RearLifetime * .65f); N->SetVariableFloat(TEXT("User.LifetimeMax"), Skill->FX.RearLifetime);
		N->SetVariableVec3(TEXT("User.VelocityMin"), Min); N->SetVariableVec3(TEXT("User.VelocityMax"), Max);
		N->SetVariableVec2(TEXT("User.SizeMin"), Skill->FX.RearSize * .65); N->SetVariableVec2(TEXT("User.SizeMax"), Skill->FX.RearSize);
		N->SetVariableLinearColor(TEXT("User.SparkColor"), Skill->FX.SparkColor);
		N->Activate(true); N->AdvanceSimulation(20, 1.f / 60);
	}
	Pixels(TEXT("Car04_Skill_ModelPreview.bmp"));
	// Use the current DA's real lifetime, rather than the long trajectory-observation fixture above.
	auto* ShortTail = Make(Rear, FVector(-120, 0, 40));
	ShortTail->SetVariableFloat(TEXT("User.SpawnRate"), Skill->FX.RearSpawnRate);
	ShortTail->SetVariableFloat(TEXT("User.LifetimeMin"), Skill->FX.RearLifetime * .65f); ShortTail->SetVariableFloat(TEXT("User.LifetimeMax"), Skill->FX.RearLifetime);
	ShortTail->Activate(true); ShortTail->AdvanceSimulation(6, 1.f / 60);
	int32 ShortCount = 0;
	if (auto Controller = ShortTail->GetSystemInstanceController()) if (auto* Instance = Controller->GetSystemInstance_Unsafe())
		for (const auto& E : Instance->GetEmitters()) { ShortCount += E->GetNumParticles(); }
	TestTrue(TEXT("Shortened tail still spawns live particles"), ShortCount > 0);
	ShortTail->SetVariableFloat(TEXT("User.SpawnRate"), 0); ShortTail->AdvanceSimulation(16, 1.f / 60);
	int32 Remaining = 0;
	if (auto Controller = ShortTail->GetSystemInstanceController()) if (auto* Instance = Controller->GetSystemInstance_Unsafe())
		for (const auto& E : Instance->GetEmitters()) { Remaining += E->GetNumParticles(); }
	TestEqual(TEXT("Current DA tail particles die before .267 seconds after emission stops"), Remaining, 0);
	ShortTail->DestroyComponent();
	AddInfo(FString::Printf(TEXT("Car04 render proof: %s; exhaust particles checked=%d"), *Output, Compared));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBullChargePressureWindTest, "HAL.RenderSmoke.BullCharge.ContinuousPressureWind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBullChargePressureWindTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("Requires actual RHI")); return false; }
	auto* System = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/FX/BullCharge/NS_BullCharge_ContourWind.NS_BullCharge_ContourWind"));
	if (!TestNotNull(TEXT("Saved pressure wind"), System)) { return false; }
	TestEqual(TEXT("Flow strips and soft shell use two persistent emitters"), System->GetEmitterHandles().Num(), 2);
	for (const auto& H : System->GetEmitterHandles()) { TestTrue(TEXT("Both layers remain local to the nose mount"), H.GetEmitterData()->bLocalSpace); }
	auto* Flow = LoadObject<UMaterial>(nullptr, TEXT("/Game/Assets/FX/BullCharge/M_BullCharge_ContourWind.M_BullCharge_ContourWind"));
	auto* Shell = LoadObject<UMaterial>(nullptr, TEXT("/Game/Assets/FX/BullCharge/M_BullCharge_ContourShell.M_BullCharge_ContourShell"));
	if (!Flow || !Shell) { AddError(TEXT("Pressure materials missing")); return false; }
	bool bSurfaceShader = false;
	for (const auto& E : Shell->GetExpressionCollection().Expressions)
	{
		if (const auto* H = Cast<UMaterialExpressionCustom>(E.Get()); H && H->OutputType == CMOT_Float4)
		{
			bSurfaceShader = H->Code.Contains(TEXT("shadowToMid")) && !H->Code.Contains(TEXT("outline"));
			for (const auto& I : H->Inputs) { TestTrue(TEXT("Shell shading has no airflow contour inputs"), I.InputName != TEXT("ContourWidth") && I.InputName != TEXT("Ink")); }
		}
	}
	if (!TestTrue(TEXT("Saved shell uses soft whole-surface three-tone shading"), bSurfaceShader)) { return false; }
	for (auto* M : {Flow, Shell}) { M->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous); }
	FAssetCompilingManager::Get().FinishAllCompilation(); System->RequestCompile(false); System->WaitForCompilationComplete();
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(true).SetTransactional(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* A = W->SpawnActor<AActor>(); auto* C = NewObject<UNiagaraComponent>(A); A->SetRootComponent(C);
	C->SetAutoActivate(false); C->SetAsset(System);
	auto* F = UMaterialInstanceDynamic::Create(Flow, C); auto* S = UMaterialInstanceDynamic::Create(Shell, C);
	C->SetVariableMaterial(TEXT("User.WindMaterial"), F); C->SetVariableMaterial(TEXT("User.PressureMaterial"), S);
	C->SetVariableVec3(TEXT("User.FlowOrigin"), FVector(100, 0, 0));
	C->RegisterComponent(); C->Activate(true);
	int32 Frames = 0;
	for (int32 I = 0; I < 360; ++I)
	{
		C->AdvanceSimulation(1, 1.f / 60);
		if (I < 2) { continue; }
		int32 Count = 0;
		if (auto Controller = C->GetSystemInstanceController()) if (auto* Instance = Controller->GetSystemInstance_Unsafe())
			for (const auto& E : Instance->GetEmitters()) { Count += E->GetNumParticles(); }
		if (Count != 2) { AddError(FString::Printf(TEXT("Persistent geometry blinked at frame %d: %d particles"), I, Count)); return false; }
		++Frames;
	}
	if (auto Controller = C->GetSystemInstanceController()) if (auto* Instance = Controller->GetSystemInstance_Unsafe())
	{
		int32 Index = 0;
		for (const auto& E : Instance->GetEmitters())
		{
			const auto Position = FNiagaraDataSetAccessor<FNiagaraPosition>::CreateReader(E->GetParticleData(), TEXT("Position"));
			if (!TestTrue(TEXT("Persistent mesh position data is available"), Position.IsValid())) { return false; }
			TestTrue(TEXT("Flow origin aligns with the cap apex independently of wind scale"), FVector(Position[0]).Equals(Index++ == 0 ? FVector(100, 0, 0) : FVector::ZeroVector, .01));
		}
	}
	auto* Geometry = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Assets/FX/BullCharge/SM_BullCharge_ConnectedFlow.SM_BullCharge_ConnectedFlow"));
	if (!Geometry || !Geometry->GetRenderData()) { AddError(TEXT("Connected flow geometry missing")); return false; }
	const auto& LOD = Geometry->GetRenderData()->LODResources[0]; int32 ApexVertices = 0;
	for (uint32 I = 0; I < LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices(); ++I)
	{
		if (LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(I, 0).X < .0001f)
		{
			TestTrue(TEXT("Every authored lane begins at the shared, turbulence-pinned origin"), LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(I).Size() < .01f); ++ApexVertices;
		}
	}
	TestTrue(TEXT("All seven lane roots were inspected"), ApexVertices >= 7);
	// A light unlit floor makes dark contour contrast measurable instead of judging on black only.
	auto* GroundMaterial = NewObject<UMaterial>(); GroundMaterial->SetShadingModel(MSM_Unlit);
	auto* Grey = NewObject<UMaterialExpressionConstant3Vector>(GroundMaterial); Grey->Constant = FLinearColor(.5f, .5f, .5f);
	GroundMaterial->GetExpressionCollection().AddExpression(Grey); GroundMaterial->GetEditorOnlyData()->EmissiveColor.Connect(0, Grey);
	GroundMaterial->PostEditChange(); GroundMaterial->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	FAssetCompilingManager::Get().FinishAllCompilation();
	auto* Ground = NewObject<UStaticMeshComponent>(A); Ground->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Ground->SetMaterial(0, GroundMaterial); Ground->SetWorldLocation(FVector(0, 0, -160)); Ground->SetWorldScale3D(FVector(20, 20, .05)); Ground->RegisterComponent();
	auto* Capture = W->SpawnActor<ASceneCapture2D>(); auto* Camera = Capture->GetCaptureComponent2D();
	Camera->bCaptureEveryFrame = false; Camera->CaptureSource = SCS_FinalColorLDR; Camera->ProjectionType = ECameraProjectionMode::Orthographic;
	Camera->OrthoWidth = 700; Camera->ShowFlags.SetTemporalAA(false);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true; Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->SetWorldLocation(FVector(420, -650, 650)); Camera->SetWorldRotation((-Camera->GetComponentLocation()).Rotation());
	auto* Target = NewObject<UTextureRenderTarget2D>(); Target->InitCustomFormat(768, 768, PF_B8G8R8A8, false); Camera->TextureTarget = Target;
	FString Output;
	if (!FParse::Value(FCommandLine::Get(), TEXT("HALBullChargeRenderOutput="), Output)) { Output = FPaths::ProjectSavedDir() / TEXT("Car04FX/Polish_20261114/Render"); }
	IFileManager::Get().MakeDirectory(*Output, true);
	const auto Pixels = [&](const TCHAR* Name)
	{
		Camera->CaptureScene(); TArray<FColor> P; Target->GameThread_GetRenderTargetResource()->ReadPixels(P);
		TestTrue(TEXT("Pressure wind proof saved"), FFileHelper::CreateBitmap(*(Output / Name), 768, 768, P.GetData())); return P;
	};
	const auto Difference = [](const TArray<FColor>& X, const TArray<FColor>& Y) { int32 N = 0; for (int32 I = 0; I < X.Num(); ++I) { N += FMath::Abs(int32(X[I].R) - Y[I].R) > 8; } return N; };
	const auto Full = Pixels(TEXT("PressureWind_All.bmp"));
	F->SetScalarParameterValue(TEXT("ContourWidth"), 0); S->SetScalarParameterValue(TEXT("ContourWidth"), 0);
	const auto NoContour = Pixels(TEXT("PressureWind_NoContour.bmp")); int32 Darker = 0;
	for (int32 I = 0; I < Full.Num(); ++I) { Darker += int32(Full[I].R) + 10 < int32(NoContour[I].R); }
	TestTrue(TEXT("Cartoon contours add visible darker shading on a light floor"), Darker > 300);
	F->SetScalarParameterValue(TEXT("ContourWidth"), .12f); S->SetScalarParameterValue(TEXT("ContourWidth"), .12f);
	S->SetScalarParameterValue(TEXT("PressureOpacity"), 0); const auto Strips = Pixels(TEXT("PressureWind_StripsOnly.bmp"));
	// Coverage is measured against black: the requested pale, low-contrast shell can intentionally
	// match the light floor. Keep the light-floor proof above, without reintroducing a dark rim.
	Ground->SetVisibility(false); const auto BlackStrips = Pixels(TEXT("PressureWind_Black_Strips.bmp"));
	S->SetScalarParameterValue(TEXT("PressureOpacity"), .42f); const auto BlackFull = Pixels(TEXT("PressureWind_Black_All.bmp"));
	const int32 SurfaceCoverage = Difference(BlackFull, BlackStrips);
	TestTrue(TEXT("The soft pressure cap adds real surface coverage around the nose"), SurfaceCoverage > 1000);
	Ground->SetVisibility(true); S->SetScalarParameterValue(TEXT("PressureOpacity"), 0);
	C->AdvanceSimulation(10, 1.f / 60); const auto Moved = Pixels(TEXT("PressureWind_FlowMoved.bmp"));
	TestTrue(TEXT("Material flow actually advances without new particle births"), Difference(Strips, Moved) > 100);
	F->SetScalarParameterValue(TEXT("FlowSpeed"), 0); const auto Still = Pixels(TEXT("PressureWind_FlowStopped.bmp"));
	C->AdvanceSimulation(10, 1.f / 60); const auto StillLater = Pixels(TEXT("PressureWind_FlowStoppedLater.bmp"));
	TestTrue(TEXT("Flow speed zero removes animation rather than changing emission cadence"), Difference(Still, StillLater) < 20);
	// Hold particle age fixed and advance the explicit runtime clock: the silhouette must move.
	F->SetScalarParameterValue(TEXT("UseRuntimeTime"), 1); F->SetScalarParameterValue(TEXT("FlowSpeed"), 1.8f);
	F->SetScalarParameterValue(TEXT("MotionTime"), 0); const auto MotionA = Pixels(TEXT("PressureWind_Turbulence_A.bmp"));
	F->SetScalarParameterValue(TEXT("MotionTime"), .23f); const auto MotionB = Pixels(TEXT("PressureWind_Turbulence_B.bmp"));
	F->SetScalarParameterValue(TEXT("Envelope"), 0); const auto Background = Pixels(TEXT("PressureWind_Background.bmp"));
	const auto SilhouetteChange = [&](const TArray<FColor>& A0, const TArray<FColor>& A1)
	{
		int32 N = 0; for (int32 I = 0; I < Background.Num(); ++I) { const bool X = FMath::Abs(int32(A0[I].R) - Background[I].R) > 12;
			const bool Y = FMath::Abs(int32(A1[I].R) - Background[I].R) > 12; N += X != Y; } return N;
	};
	F->SetScalarParameterValue(TEXT("Envelope"), 1); F->SetScalarParameterValue(TEXT("Turbulence"), 0);
	F->SetScalarParameterValue(TEXT("MotionTime"), 0); const auto RigidA = Pixels(TEXT("PressureWind_Rigid_A.bmp"));
	F->SetScalarParameterValue(TEXT("MotionTime"), .23f); const auto RigidB = Pixels(TEXT("PressureWind_Rigid_B.bmp"));
	const int32 MovingEdge = SilhouetteChange(MotionA, MotionB), RigidEdge = SilhouetteChange(RigidA, RigidB);
	TestTrue(TEXT("WPO produces actual edge motion beyond texture-only modulation"), MovingEdge > RigidEdge + 100);
	// Isolate the shell: artist controls affect its broad surface, never the wind contour.
	F->SetScalarParameterValue(TEXT("Envelope"), 0); S->SetScalarParameterValue(TEXT("PressureOpacity"), .42f);
	S->SetScalarParameterValue(TEXT("FlowSpeed"), 0); S->SetScalarParameterValue(TEXT("Turbulence"), 0);
	Pixels(TEXT("PressureShell_SoftDefault_LightFloor.bmp"));
	Ground->SetVisibility(false); const auto SoftShell = Pixels(TEXT("PressureShell_SoftDefault.bmp"));
	S->SetScalarParameterValue(TEXT("ContourWidth"), .3f); S->SetVectorParameterValue(TEXT("ContourColor"), FLinearColor(1, 0, 1));
	const auto IgnoreContour = Pixels(TEXT("PressureShell_IgnoreWindContour.bmp"));
	TestTrue(TEXT("Old airflow contour color and width cannot create a ring on the shell"), Difference(SoftShell, IgnoreContour) < 20);
	// Strong diagnostic colors and wider shadow coverage make all three surface bands measurable.
	S->SetScalarParameterValue(TEXT("ShadowThreshold"), .5f); S->SetScalarParameterValue(TEXT("LightThreshold"), .82f);
	const auto BaseTones = Pixels(TEXT("PressureShell_ToneBaseline.bmp"));
	const FHALBullChargeFXConfig Style;
	int32 ColorChanges[3] {}; int32 ColorIndex = 0;
	for (const auto& Pair : TArray<TPair<FName, FLinearColor>>{
		{TEXT("ShadowColor"), Style.PressureShadowColor}, {TEXT("MidColor"), Style.PressureMidColor}, {TEXT("LightColor"), Style.PressureLightColor}})
	{
		S->SetVectorParameterValue(Pair.Key, FLinearColor(.02f, 1, .02f));
		const auto Changed = Pixels(*FString::Printf(TEXT("PressureShell_%s.bmp"), *Pair.Key.ToString()));
		ColorChanges[ColorIndex++] = Difference(BaseTones, Changed);
		TestTrue(TEXT("Each DA color changes a visible region of the curved shell"), ColorChanges[ColorIndex - 1] > 80);
		S->SetVectorParameterValue(Pair.Key, Pair.Value);
	}
	S->SetVectorParameterValue(TEXT("ShadowColor"), FLinearColor(.1f, .1f, .1f));
	S->SetVectorParameterValue(TEXT("LightColor"), FLinearColor(1, 1, 1));
	S->SetScalarParameterValue(TEXT("ToneSoftness"), .02f); const auto NarrowBlend = Pixels(TEXT("PressureShell_NarrowBlend.bmp"));
	S->SetScalarParameterValue(TEXT("ToneSoftness"), .32f); const auto WideBlend = Pixels(TEXT("PressureShell_WideBlend.bmp"));
	const int32 SoftnessChanges = Difference(NarrowBlend, WideBlend);
	TestTrue(TEXT("Transition width changes actual rendered shading"), SoftnessChanges > 80);
	S->SetScalarParameterValue(TEXT("Feather"), .1f); const auto NarrowFade = Pixels(TEXT("PressureShell_NarrowFade.bmp"));
	S->SetScalarParameterValue(TEXT("Feather"), .5f); const auto WideFade = Pixels(TEXT("PressureShell_WideFade.bmp"));
	TestTrue(TEXT("Outer silhouette retains a soft opacity fade independently of tone blending"), Difference(NarrowFade, WideFade) > 100);
	AddInfo(FString::Printf(TEXT("Soft shell surface color changed pixels: shadow=%d mid=%d light=%d; transition width=%d; contour coupling=%d"),
		ColorChanges[0], ColorChanges[1], ColorChanges[2], SoftnessChanges, Difference(SoftShell, IgnoreContour)));
	Ground->SetVisibility(true);
	S->SetScalarParameterValue(TEXT("PressureOpacity"), .42f); F->SetScalarParameterValue(TEXT("Envelope"), 0); S->SetScalarParameterValue(TEXT("Envelope"), 0);
	const auto Hidden = Pixels(TEXT("PressureWind_Paused.bmp"));
	TestTrue(TEXT("Pause hides both persistent layers"), Difference(Full, Hidden) > 1000);
	AddInfo(FString::Printf(TEXT("Pressure wind persistent frames=%d; shared apex vertices=%d; wind contour pixels=%d; moving silhouette=%d rigid silhouette=%d; shell coverage on black=%d; low-contrast light floor difference=%d"), Frames, ApexVertices, Darker, MovingEdge, RigidEdge, SurfaceCoverage, Difference(Full, Strips)));
	return true;
}
struct FHALBullChargeFXRenderAccess
{
	static void Begin(UHALBullChargeComponent& C, const FHALBullChargeFXConfig& FX, float RampSeconds)
	{
		C.VisualState.FX = FX; C.VisualState.Activation = FGuid::NewGuid();
		C.VisualState.StartServerTime = C.GetWorld()->GetTimeSeconds(); C.VisualState.DurationSeconds = 30;
		C.VisualState.RampSeconds = RampSeconds; C.VisualState.bDriving = true; C.OnRep_VisualState();
	}
	static void Sample(UHALBullChargeComponent& C, double Elapsed, bool bDriving = true)
	{
		C.VisualState.StartServerTime = C.GetWorld()->GetTimeSeconds() - Elapsed;
		C.VisualState.bDriving = bDriving; C.UpdateParticles(1.f / 60);
	}
	static UNiagaraComponent* Front(UHALBullChargeComponent& C) { return C.FrontWindComponent; }
	static UNiagaraComponent* Rear(UHALBullChargeComponent& C) { return C.RearSparkComponents.IsEmpty() ? nullptr : C.RearSparkComponents[0].Get(); }
	static float Envelope(UHALBullChargeComponent& C, bool bShell = false)
	{
		auto* M = bShell ? C.PressureMaterial.Get() : C.FrontMaterial.Get();
		return M ? M->K2_GetScalarParameterValue(TEXT("Envelope")) : -1;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBullChargeLengthRampRenderTest, "HAL.RenderSmoke.BullCharge.RuntimeLengthAndAccelerationEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBullChargeLengthRampRenderTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("Requires actual RHI")); return false; }
	const auto* Saved = LoadObject<UHALBullChargeConfig>(nullptr, TEXT("/Game/Data/Abilities/DA_Ability_Car04_BullCharge.DA_Ability_Car04_BullCharge"));
	if (!TestNotNull(TEXT("Current saved skill"), Saved)) { return false; }
	for (auto* System : {Saved->FX.FrontWind.Get(), Saved->FX.RearSparks.Get()})
	{
		if (!TestNotNull(TEXT("Current saved continuous recipe"), System)) { return false; }
		System->RequestCompile(false); System->WaitForCompilationComplete();
	}
	FAssetCompilingManager::Get().FinishAllCompilation();
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(true).SetTransactional(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* A = W->SpawnActor<AActor>(); auto* Body = NewObject<USphereComponent>(A); A->SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Body->SetSimulatePhysics(true); Body->SetEnableGravity(false); Body->RegisterComponent();
	Body->SetPhysicsLinearVelocity(FVector(Saved->FX.WindFullSpeed, 0, 0));
	if (!TestTrue(TEXT("Fixture starts at full speed to expose activation pops"), Body->GetPhysicsLinearVelocity().X > 0)) { return false; }
	auto* Visual = NewObject<UStaticMeshComponent>(A); Visual->SetupAttachment(Body);
	Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->RegisterComponent();
	auto* Presentation = NewObject<UHALVehiclePresentationComponent>(A); Presentation->RegisterComponent();
	Presentation->Configure(nullptr, Body, Visual, nullptr, true);
	auto* Charge = NewObject<UHALBullChargeComponent>(A); Charge->RegisterComponent();
	auto FX = Saved->FX; FX.FrontMount = {}; FX.RearMounts = {FHALVehicleFXMount()};
	FX.WindSizeScale = 1; FX.WindLengthScale = 1; FX.WindResponseSeconds = 0; FX.WindFlowSpeed = 0;
	FX.WindTurbulence = 0; FX.PressureTurbulence = 0;
	const auto ReadParticles = [&](UNiagaraComponent& N, float Length)
	{
		N.AdvanceSimulation(3, 1.f / 60); int32 Layers = 0;
		if (auto Controller = N.GetSystemInstanceController()) if (auto* Instance = Controller->GetSystemInstance_Unsafe())
			for (const auto& E : Instance->GetEmitters())
			{
				const auto Scale = FNiagaraDataSetAccessor<FVector3f>::CreateReader(E->GetParticleData(), TEXT("Scale"));
				const auto Point = FNiagaraDataSetAccessor<FNiagaraPosition>::CreateReader(E->GetParticleData(), TEXT("Position"));
				if (!TestTrue(TEXT("Runtime particle scale and position exist"), E->GetNumParticles() == 1 && Scale.IsValid() && Point.IsValid())) { return false; }
				const bool bWind = Layers++ == 0;
				TestTrue(TEXT("Independent length affects actual wind X only; pressure dimensions stay unchanged"),
					FVector(Scale[0]).Equals(bWind ? FVector(Length, 1, 1) : FX.PressureSize / 100, .001));
				TestTrue(TEXT("Changing length preserves the common cap apex"),
					FVector(Point[0]).Equals(bWind ? FVector(FX.PressureSize.X, 0, 0) : FVector::ZeroVector, .001));
			}
		return TestEqual(TEXT("Both actual persistent mesh particles were inspected"), Layers, 2);
	};
	for (const float Length : {.5f, 1.f, 2.f, 4.f})
	{
		FX.WindLengthScale = Length; FHALBullChargeFXRenderAccess::Begin(*Charge, FX, 2);
		FHALBullChargeFXRenderAccess::Sample(*Charge, 2);
		auto* N = FHALBullChargeFXRenderAccess::Front(*Charge);
		if (!TestNotNull(TEXT("Production UpdateParticles creates the wind component"), N) || !ReadParticles(*N, Length)) { return false; }
	}
	FX.WindLengthScale = 1; FHALBullChargeFXRenderAccess::Begin(*Charge, FX, 2);
	auto* Capture = W->SpawnActor<ASceneCapture2D>(); auto* Camera = Capture->GetCaptureComponent2D();
	Camera->bCaptureEveryFrame = false; Camera->CaptureSource = SCS_FinalColorLDR;
	Camera->ProjectionType = ECameraProjectionMode::Orthographic; Camera->OrthoWidth = 700;
	Camera->ShowFlags.SetTemporalAA(false); Camera->HiddenComponents.Add(Body); Camera->HiddenComponents.Add(Visual);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true; Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->SetWorldLocation(FVector(420, -650, 650)); Camera->SetWorldRotation((-Camera->GetComponentLocation()).Rotation());
	auto* Target = NewObject<UTextureRenderTarget2D>(); Target->InitCustomFormat(512, 512, PF_B8G8R8A8, false); Camera->TextureTarget = Target;
	FString Output; FParse::Value(FCommandLine::Get(), TEXT("HALBullChargeRenderOutput="), Output);
	if (Output.IsEmpty()) { Output = FPaths::ProjectSavedDir() / TEXT("Car04FX/LengthRamp_20261114/Render"); }
	IFileManager::Get().MakeDirectory(*Output, true);
	uint64 PreviousLight = 0;
	for (const float Elapsed : {0.f, .5f, 1.f, 2.f})
	{
		FHALBullChargeFXRenderAccess::Sample(*Charge, Elapsed);
		TestTrue(TEXT("Wind opacity follows acceleration time even when activating at full road speed"),
			FMath::IsNearlyEqual(FHALBullChargeFXRenderAccess::Envelope(*Charge), Elapsed / 2, .001f));
		TestTrue(TEXT("Pressure shell shares the exact acceleration envelope"),
			FMath::IsNearlyEqual(FHALBullChargeFXRenderAccess::Envelope(*Charge, true), Elapsed / 2, .001f));
		bool bValid = false; const auto* Rear = FHALBullChargeFXRenderAccess::Rear(*Charge);
		if (!TestNotNull(TEXT("Production rear emitter"), Rear)) { return false; }
		const float Rate = Rear->GetVariableFloat(TEXT("User.SpawnRate"), bValid);
		TestTrue(TEXT("Rear emission also starts at zero and follows acceleration"), bValid && FMath::IsNearlyEqual(Rate, FX.RearSpawnRate * Elapsed / 2, .001f));
		FHALBullChargeFXRenderAccess::Front(*Charge)->AdvanceSimulation(3, 1.f / 60);
		Camera->CaptureScene(); TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		TestTrue(TEXT("Progressive appearance proof saved"), FFileHelper::CreateBitmap(*(Output / FString::Printf(TEXT("Ramp_%.2f.bmp"), Elapsed)), 512, 512, Pixels.GetData()));
		uint64 Light = 0; for (const auto Color : Pixels) { Light += Color.R + Color.G + Color.B; }
		if (Elapsed > 0) { TestTrue(TEXT("Real rendered wind and shell progressively brighten rather than popping at entry"), Light > PreviousLight + 1000); }
		PreviousLight = Light;
		AddInfo(FString::Printf(TEXT("Acceleration sample %.2fs / 2s: envelope=%.3f rear rate=%.1f rendered RGB sum=%llu"),
			Elapsed, FHALBullChargeFXRenderAccess::Envelope(*Charge), Rate, Light));
	}
	// A later observer uses the original server phase; suspension still hides immediately.
	FHALBullChargeFXRenderAccess::Begin(*Charge, FX, 2); FHALBullChargeFXRenderAccess::Sample(*Charge, 3);
	TestEqual(TEXT("Late observer does not restart the acceleration fade"), FHALBullChargeFXRenderAccess::Envelope(*Charge), 1.f);
	FHALBullChargeFXRenderAccess::Sample(*Charge, 3.1, false);
	TestEqual(TEXT("Suspended driving hides wind without waiting for fade"), FHALBullChargeFXRenderAccess::Envelope(*Charge), 0.f);
	TestEqual(TEXT("Suspended driving hides pressure without waiting for fade"), FHALBullChargeFXRenderAccess::Envelope(*Charge, true), 0.f);
	return true;
}
#endif
