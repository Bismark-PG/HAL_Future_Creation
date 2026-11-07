#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALVehicleStatusFXComponent.h"
#include "../HALVehicleStatusFXRenderer.h"
#include "../HALStatusFXSurfaceLayout.h"
#include "../HALStatusFXParticleBinding.h"
#include "../HALVehicleStatusFXDefinition.h"
#include "../HALGameplayTags.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "AssetCompilingManager.h"
#include "MaterialShaderPrecompileMode.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraDataInterfaceSkeletalMesh.h"
#include "NiagaraFunctionLibrary.h"
#include "StaticMeshResources.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
	UMaterial* TestMaterial()
	{
		auto* M = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
		M->SetShadingModel(MSM_Unlit); M->BlendMode = BLEND_Translucent; M->TranslucencyPass = MTP_BeforeDOF;
		UMaterialExpression* Total = nullptr;
		for (int32 Slot = 0; Slot < 2; ++Slot)
		{
			const FString Prefix = FString::Printf(TEXT("FX%d_"), Slot);
			UMaterialExpression* Sum = nullptr;
			for (const TCHAR* Name : {TEXT("Intensity"), TEXT("Age"), TEXT("Residue"), TEXT("Power")})
			{
				auto* P = NewObject<UMaterialExpressionScalarParameter>(M); P->ParameterName = FName(*(Prefix + Name)); P->DefaultValue = 0;
				M->GetExpressionCollection().AddExpression(P);
				if (!Sum) { Sum = P; } else { auto* A = NewObject<UMaterialExpressionAdd>(M); A->A.Connect(0, Sum); A->B.Connect(0, P); M->GetExpressionCollection().AddExpression(A); Sum = A; }
			}
			auto* Color = NewObject<UMaterialExpressionVectorParameter>(M); Color->ParameterName = FName(*(Prefix + TEXT("Tint")));
			Color->DefaultValue = Slot == 0 ? FLinearColor::Red : FLinearColor::Blue; M->GetExpressionCollection().AddExpression(Color);
			auto* Mul = NewObject<UMaterialExpressionMultiply>(M); Mul->A.Connect(0, Sum); Mul->B.Connect(0, Color); M->GetExpressionCollection().AddExpression(Mul);
			if (!Total) { Total = Mul; } else { auto* A = NewObject<UMaterialExpressionAdd>(M); A->A.Connect(0, Total); A->B.Connect(0, Mul); M->GetExpressionCollection().AddExpression(A); Total = A; }
		}
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, Total);
		M->GetEditorOnlyData()->Opacity.UseConstant = true; M->GetEditorOnlyData()->Opacity.Constant = 1;
		M->PostEditChange();
		if (FApp::CanEverRender()) { M->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous); }
		FAssetCompilingManager::Get().FinishAllCompilation(); return M;
	}
	struct FRenderWorld
	{
		UWorld* World;
		AActor* Car;
		UStaticMeshComponent* Mesh;
		TStrongObjectPtr<UHALVehicleStatusFXRenderer> Renderer{NewObject<UHALVehicleStatusFXRenderer>()};
		TStrongObjectPtr<UHALStatusFXSurfaceLayout> Layout{NewObject<UHALStatusFXSurfaceLayout>()};
		explicit FRenderWorld(bool WithMaterial = true)
		{
			const auto Opt = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
				.CreateAISystem(false).CreateFXSystem(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Opt);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Car = World->SpawnActor<AActor>(); Mesh = NewObject<UStaticMeshComponent>(Car); Car->SetRootComponent(Mesh);
			Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent();
			if (WithMaterial) { Layout->Material = TestMaterial(); }
		}
		~FRenderWorld() { Renderer->ClearAll(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		FHALStatusFXRenderRequest Request(FGameplayTag Tag, FName Channel, int32 Priority = 0)
		{
			auto* D = NewObject<UHALVehicleStatusFXDefinition>(Layout.Get()); D->bEnableRendering = true; D->SurfaceLayout = Layout.Get();
			D->SurfaceChannel = Channel; D->SurfacePriority = Priority;
			FHALStatusFXRenderRequest R; R.Recipe = D; R.State.Tag = Tag; R.State.Phase = EHALStatusFXPhase::Active;
			R.State.bAvailable = true; R.State.Intensity = 1; R.State.InstanceId = 1;
			R.Target.Id = TEXT("Body"); R.Target.Mesh = Mesh; R.Target.MeshAsset = Mesh->GetStaticMesh();
			R.Target.LocalBounds = Mesh->CalcBounds(FTransform::Identity).GetBox(); R.Target.MeshToWorld = Mesh->GetComponentTransform();
			R.Surface = UMaterialInstanceDynamic::Create(Layout->Material, Renderer.Get()); return R;
		}
	};
	float Scalar(UMaterialInstanceDynamic* M, const TCHAR* Name)
	{ float Out = -99; M->GetScalarParameterValue(FMaterialParameterInfo(Name), Out); return Out; }
	UNiagaraSystem* Template(UObject* Outer)
	{
		auto* S = NewObject<UNiagaraSystem>(Outer);
		for (const TCHAR* N : {TEXT("User.FX_Intensity"), TEXT("User.FX_Age"), TEXT("User.FX_Residue"), TEXT("User.FX_SpawnRate")})
		{ S->GetExposedParameters().AddParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), FName(N))); }
		for (const TCHAR* N : {TEXT("User.FX_BoundsCenter"), TEXT("User.FX_BoundsExtent")})
		{ S->GetExposedParameters().AddParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), FName(N))); }
		S->GetExposedParameters().AddParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("User.FX_MaxParticles")));
		return S;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXLayoutTest,"HAL.FutureCreation.StatusFX.Renderer.LayoutAndIndependentCapacity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALFXLayoutTest::RunTest(const FString&)
{
	TStrongObjectPtr<UHALStatusFXSurfaceLayout> L(NewObject<UHALStatusFXSurfaceLayout>()); FString Error;
	TestEqual(TEXT("First edition declares two display channels"),L->Channels.Num(),2);
	L->Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
	TestTrue(TEXT("Default structure valid"),L->Validate(Error));
	auto& C=L->Channels.AddDefaulted_GetRef(); C.Id=TEXT("Third"); C.Prefix=TEXT("FX2_");
	TestTrue(TEXT("Display capacity can expand independently of ledger"),L->Validate(Error));
	C.Prefix=TEXT("FX0_Extra"); TestFalse(TEXT("Overlapping namespaces rejected"),L->Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXCompositeTest,"HAL.FutureCreation.StatusFX.Renderer.TwoChannelsAndExitOrder",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALFXCompositeTest::RunTest(const FString&)
{
	FRenderWorld F; auto A=F.Request(HALGameplayTags::State_Visual_DebugA,TEXT("Primary")),B=F.Request(HALGameplayTags::State_Visual_DebugB,TEXT("Secondary"));
	B.Surface=A.Surface; A.State.ClockSeconds=2; A.Recipe->Scalars.Add({TEXT("Power"),3}); B.State.Intensity=.4f;
	F.Renderer->RenderSurfaces({A,B});
	TestTrue(TEXT("One output MID on the actual mesh"),F.Mesh->GetOverlayMaterial()==A.Surface.Get());
	TestEqual(TEXT("Primary envelope"),Scalar(A.Surface.Get(),TEXT("FX0_Intensity")),1.f);
	TestEqual(TEXT("Independent secondary envelope"),Scalar(A.Surface.Get(),TEXT("FX1_Intensity")),.4f);
	TestEqual(TEXT("Instance age reaches material"),Scalar(A.Surface.Get(),TEXT("FX0_Age")),2.f);
	auto C=F.Request(FGameplayTag::RequestGameplayTag(TEXT("State.Visual.StatusFXComponentSecond")),TEXT("Primary"),-1);
	C.Surface=A.Surface; C.State.ClockSeconds=7;
	F.Renderer->RenderSurfaces({A,B,C});
	TestEqual(TEXT("Third effective request does not add a third surface channel"),F.Renderer->GetGenericChannelCount(),2);
	F.Renderer->RenderSurfaces({B,C}); TestEqual(TEXT("Previously suppressed third request resumes at its own age"),Scalar(A.Surface.Get(),TEXT("FX0_Age")),7.f);
	F.Renderer->RenderSurfaces({B});
	TestEqual(TEXT("Departed primary is zeroed"),Scalar(B.Surface.Get(),TEXT("FX0_Intensity")),0.f);
	F.Renderer->RenderSurfaces({A});
	TestEqual(TEXT("Reverse order also zeroes secondary"),Scalar(A.Surface.Get(),TEXT("FX1_Intensity")),0.f);
	F.Renderer->RenderSurfaces({}); TestNull(TEXT("Original empty overlay restored"),F.Mesh->GetOverlayMaterial());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXLegacyOwnerTest,"HAL.FutureCreation.StatusFX.Renderer.LegacyLeaseAndOriginalRestore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALFXLegacyOwnerTest::RunTest(const FString&)
{
	FRenderWorld F; auto A=F.Request(HALGameplayTags::State_Visual_DebugA,TEXT("Primary"));
	auto* Original=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial")); F.Mesh->SetOverlayMaterial(Original);
	auto* Legacy=UMaterialInstanceDynamic::Create(Original,F.Car);
	F.Renderer->RenderSurfaces({A}); F.Renderer->SetLegacy(F.Mesh,Legacy);
	A.State.ClockSeconds=5; F.Renderer->RenderSurfaces({A});
	TestTrue(TEXT("Legacy holds the sole surface while new time advances"),F.Mesh->GetOverlayMaterial()==Legacy);
	F.Renderer->SetLegacy(F.Mesh,nullptr); TestTrue(TEXT("Current new style resumes"),F.Mesh->GetOverlayMaterial()==A.Surface.Get());
	TestEqual(TEXT("Resume uses current clock"),Scalar(A.Surface.Get(),TEXT("FX0_Age")),5.f);
	F.Renderer->ClearGeneric(); TestTrue(TEXT("All effects restore exact original material"),F.Mesh->GetOverlayMaterial()==Original);
	F.Renderer->SetLegacy(F.Mesh,Legacy); F.Renderer->ClearGeneric(); TestTrue(TEXT("Generic rebind cannot clear live legacy"),F.Mesh->GetOverlayMaterial()==Legacy);
	F.Renderer->ClearAll(); TestTrue(TEXT("Owner exit restores baseline"),F.Mesh->GetOverlayMaterial()==Original);
	F.Renderer->RenderSurfaces({A}); F.Mesh->SetOverlayMaterial(nullptr); // Assembly seeds a new base before provider invalidation.
	F.Renderer->ClearGeneric(); TestNull(TEXT("Rebind does not restore a superseded vehicle base overlay"),F.Mesh->GetOverlayMaterial());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXPriorityTest,"HAL.FutureCreation.StatusFX.Renderer.PriorityResidueAndParameterReset",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALFXPriorityTest::RunTest(const FString&)
{
	FRenderWorld F; auto A=F.Request(HALGameplayTags::State_Visual_DebugA,TEXT("Primary"),1),B=F.Request(HALGameplayTags::State_Visual_DebugB,TEXT("Primary"),10);
	B.Surface=A.Surface; A.Recipe->Scalars.Add({TEXT("Power"),9}); A.State.ClockSeconds=4;
	F.Renderer->RenderSurfaces({B,A}); TestEqual(TEXT("Higher priority owns channel"),Scalar(A.Surface.Get(),TEXT("FX0_Age")),0.f);
	F.Renderer->RenderSurfaces({A}); TestEqual(TEXT("Suppressed style resumes without time restart"),Scalar(A.Surface.Get(),TEXT("FX0_Age")),4.f);
	F.Renderer->RenderSurfaces({B}); TestEqual(TEXT("Missing parameter in replacement restores parent default"),Scalar(A.Surface.Get(),TEXT("FX0_Power")),0.f);
	B.State.Phase=EHALStatusFXPhase::Residue; B.State.Intensity=0; B.Recipe->ResidueSeconds=2; B.State.PhaseSeconds=1;
	F.Renderer->RenderSurfaces({A,B}); TestEqual(TEXT("Active style beats high-priority residue"),Scalar(A.Surface.Get(),TEXT("FX0_Age")),4.f);
	F.Renderer->RenderSurfaces({B}); TestEqual(TEXT("Residue has separate channel weight"),Scalar(A.Surface.Get(),TEXT("FX0_Residue")),.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXInvalidContractTest,"HAL.FutureCreation.StatusFX.Renderer.InvalidContractAndShockReservation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALFXInvalidContractTest::RunTest(const FString&)
{
	FRenderWorld F; auto A=F.Request(HALGameplayTags::State_Visual_Shocked,TEXT("Primary"));
	F.Renderer->RenderSurfaces({A}); TestNull(TEXT("Shocked stays shadow-only until FX5"),F.Mesh->GetOverlayMaterial());
	A.State.Tag=HALGameplayTags::State_Visual_DebugA; A.Recipe->Scalars.Add({TEXT("NoSuchParameter"),1});
	F.Renderer->RenderSurfaces({A}); TestEqual(TEXT("Missing contract is diagnosed per surface"),F.Renderer->RejectedSurfaces,1);
	TestNull(TEXT("Invalid style never replaces original"),F.Mesh->GetOverlayMaterial());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXBoundsBindingTest,"HAL.FutureCreation.StatusFX.Renderer.BoundsTemplateContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALFXBoundsBindingTest::RunTest(const FString&)
{
	FRenderWorld F(false); auto* C=NewObject<UNiagaraComponent>(F.Car); C->SetAutoActivate(false); C->SetAsset(Template(F.Car));
	FHALVehicleFXTarget T; T.Mesh=F.Mesh; T.LocalBounds=F.Mesh->CalcBounds(FTransform::Identity).GetBox();
	FHALStatusFXNiagaraLayer L; FString Error;
	TestTrue(TEXT("Valid CPU bounds template binds actual visual component"),HALStatusFXParticle::Configure(C,T,L,Error));
	TestTrue(TEXT("Attachment uses mesh rather than physics/root approximation"),C->GetAttachParent()==F.Mesh);
	TestFalse(TEXT("Configuration alone never activates"),C->IsActive());
	C->SetAsset(NewObject<UNiagaraSystem>(F.Car));
	TestFalse(TEXT("Missing User parameters fail clearly"),HALStatusFXParticle::Configure(C,T,L,Error));
	TestTrue(TEXT("Failure has actionable parameter name"),Error.Contains(TEXT("User.FX_")));
	C->DestroyComponent(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXSurfaceDataTest,"HAL.FutureCreation.StatusFX.Renderer.SurfaceSourceAndCPUDataGuard",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALFXSurfaceDataTest::RunTest(const FString&)
{
	FRenderWorld F(false); auto* C=NewObject<UNiagaraComponent>(F.Car); C->SetAutoActivate(false);
	auto* S=Template(F.Car); FNiagaraVariable V(FNiagaraTypeDefinition(UNiagaraDataInterfaceSkeletalMesh::StaticClass()),TEXT("User.FX_SourceMesh"));
	S->GetExposedParameters().AddParameter(V); auto* Prototype=NewObject<UNiagaraDataInterfaceSkeletalMesh>(S);
	S->GetExposedParameters().SetDataInterface(Prototype,V); C->SetAsset(S);
	auto* Rig=NewObject<USkeletalMeshComponent>(F.Car); Rig->SetupAttachment(F.Mesh);
	auto* Mesh=NewObject<USkeletalMesh>(F.Car); Mesh->SetSkeleton(NewObject<USkeleton>(Mesh)); Mesh->AddLODInfo(); Mesh->GetLODInfo(0)->bAllowCPUAccess=true;
	Mesh->AllocateResourceForRendering(); auto* LOD=new FSkeletalMeshLODRenderData(); Mesh->GetResourceForRendering()->LODRenderData.Add(LOD);
	LOD->StaticVertexBuffers.PositionVertexBuffer.Init(3, true); LOD->RenderSections.AddDefaulted_GetRef().NumTriangles=1;
	LOD->SkinWeightVertexBuffer.SetNeedsCPUAccess(true); TArray<FSkinWeightInfo> Weights; Weights.AddZeroed(3); LOD->SkinWeightVertexBuffer=Weights;
	LOD->MultiSizeIndexContainer.RebuildIndexBuffer(2,TArray<uint32>{0,1,2});
	Rig->SetSkeletalMeshAsset(Mesh);
	FHALVehicleFXTarget T; T.Mesh=Rig; T.Kind=EHALVehicleFXMeshKind::Skeletal; FHALStatusFXNiagaraLayer L; L.Binding=EHALStatusFXBinding::MeshSurface; FString Error;
	const bool bBound=HALStatusFXParticle::Configure(C,T,L,Error);
	TestTrue(FString(TEXT("Skeletal CPU adapter assigns a real component: "))+Error,bBound);
	auto* DI=UNiagaraFunctionLibrary::GetSkeletalMeshDataInterface(C,TEXT("User.FX_SourceMesh"));
	TestTrue(TEXT("DI resolves intended mesh, not an asset preview"),DI && DI->GetSourceComponent()==Rig);
	TestNull(TEXT("Shared Niagara prototype stays unchanged"),Prototype->GetSourceComponent());
	Mesh->GetLODInfo(0)->bAllowCPUAccess=false;
	TestFalse(TEXT("Missing CPU data stops only that layer"),HALStatusFXParticle::Configure(C,T,L,Error));
	TestTrue(TEXT("CPU requirement is diagnosed"),Error.Contains(TEXT("CPU Access")));
	C->DestroyComponent(); Rig->DestroyComponent(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXStaticSourceTest,"HAL.FutureCreation.StatusFX.Renderer.StaticSourceAndLOD",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALFXStaticSourceTest::RunTest(const FString&)
{
	FRenderWorld F(false);
	auto* Class=FindObject<UClass>(nullptr,TEXT("/Script/Niagara.NiagaraDataInterfaceStaticMesh"));
	if (!TestNotNull(TEXT("Standard static mesh data interface is loaded"),Class)) { return false; }
	auto* S=Template(F.Car); FNiagaraVariable V(FNiagaraTypeDefinition(Class),TEXT("User.FX_SourceMesh"));
	S->GetExposedParameters().AddParameter(V); auto* Prototype=NewObject<UNiagaraDataInterface>(S,Class);
	S->GetExposedParameters().SetDataInterface(Prototype,V);
	auto* C=NewObject<UNiagaraComponent>(F.Car); C->SetAutoActivate(false); C->SetAsset(S);
	auto* Static=NewObject<UStaticMeshComponent>(F.Car); auto* Mesh=NewObject<UStaticMesh>(F.Car); Mesh->bAllowCPUAccess=true;
	auto Data=MakeUnique<FStaticMeshRenderData>(); auto* LOD=new FStaticMeshLODResources(); Data->LODResources.Add(LOD);
	LOD->IndexBuffer.SetIndices(TArray<uint32>{0,1,2},EIndexBufferStride::Force16Bit); LOD->IndexBuffer.TrySetAllowCPUAccess(true);
	LOD->VertexBuffers.PositionVertexBuffer.Init(3,true); LOD->Sections.AddDefaulted_GetRef().NumTriangles=1;
	Mesh->SetRenderData(MoveTemp(Data)); Static->SetStaticMesh(Mesh);
	FHALVehicleFXTarget T; T.Mesh=Static; FHALStatusFXNiagaraLayer L; L.Binding=EHALStatusFXBinding::MeshSurface; FString Error;
	const bool bBound=HALStatusFXParticle::Configure(C,T,L,Error);
	TestTrue(FString(TEXT("Static adapter accepts retained CPU triangle LOD: "))+Error,bBound);
	auto* DI=UNiagaraFunctionLibrary::GetDataInterface(Class,C,TEXT("User.FX_SourceMesh"));
	const auto* Source=FindFProperty<FObjectPropertyBase>(Class,TEXT("SourceComponent"));
	TestTrue(TEXT("Local DI uses actual component and leaves prototype untouched"),DI && Source
		&& Source->GetObjectPropertyValue_InContainer(DI)==Static && Source->GetObjectPropertyValue_InContainer(Prototype)==nullptr);
	L.SamplingLOD=1; TestFalse(TEXT("Invalid sampling LOD is rejected"),HALStatusFXParticle::Configure(C,T,L,Error));
	C->DestroyComponent(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXRenderedPixelsTest,"HAL.RenderSmoke.StatusFX.TwoChannelPixels",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALFXRenderedPixelsTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("This test requires a real RHI, run without -nullrhi.")); return false; }
	FRenderWorld F; auto A=F.Request(HALGameplayTags::State_Visual_DebugA,TEXT("Primary")), B=F.Request(HALGameplayTags::State_Visual_DebugB,TEXT("Secondary")); B.Surface=A.Surface;
	auto* RT=NewObject<UTextureRenderTarget2D>(F.Car); RT->InitAutoFormat(64,64); RT->UpdateResourceImmediate(true);
	auto* Capture=F.World->SpawnActor<ASceneCapture2D>(); Capture->SetActorLocation(FVector(300,0,0)); Capture->SetActorRotation(FRotator(0,180,0));
	auto* Cam=Capture->GetCaptureComponent2D(); Cam->TextureTarget=RT; Cam->bCaptureEveryFrame=false; Cam->bCaptureOnMovement=false;
	Cam->ProjectionType=ECameraProjectionMode::Perspective; Cam->FOVAngle=45; Cam->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
	Cam->PostProcessSettings.bOverride_AutoExposureMethod=true; Cam->PostProcessSettings.AutoExposureMethod=AEM_Manual;
	Cam->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Cam->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
	Cam->PostProcessSettings.bOverride_AutoExposureBias=true; Cam->PostProcessSettings.AutoExposureBias=0;
	Cam->ShowFlags.SetTranslucency(true); Cam->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	const auto Pixel=[&]() { Cam->CaptureScene(); TArray<FColor> Pixels; RT->GameThread_GetRenderTargetResource()->ReadPixels(Pixels); return Pixels.IsValidIndex(32*64+32)?Pixels[32*64+32]:FColor::Black; };
	const auto Base=Pixel(); F.Renderer->RenderSurfaces({A,B}); const auto Both=Pixel(); F.Renderer->RenderSurfaces({A}); const auto Red=Pixel();
	AddInfo(FString::Printf(TEXT("Actual RHI readback Base=%s Both=%s PrimaryOnly=%s Scene=%d"),*Base.ToString(),*Both.ToString(),*Red.ToString(),F.World->Scene!=nullptr));
	TestTrue(TEXT("Both channels contribute actual red and blue pixels"),Both.R>30 && Both.B>30 && Both.G<Both.R && Both.G<Both.B);
	TestTrue(TEXT("Secondary exit changes actual blue output"),Red.R>30 && int32(Red.B)+20<int32(Both.B));
	return true;
}
#endif
