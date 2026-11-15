#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALPolicePursuit.h"
#include "../HALPolicePursuitAbility.h"
#include "../HALPursuitTargetComponent.h"
#include "../HALPursuitHUDWidget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Sound/SoundWave.h"
#include "AssetCompilingManager.h"
#include "MaterialShaderPrecompileMode.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SLeafWidget.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
class SPursuitProofStars : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SPursuitProofStars) {} SLATE_ARGUMENT(float,Fraction) SLATE_END_ARGS()
	void Construct(const FArguments& A) { Fraction=A._Fraction; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(220,64); }
	virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& E,int32 L,const FWidgetStyle&,bool) const override
	{ UHALPursuitHUDWidget::PaintStars(G,E,L,FVector2D(110,32),Fraction,FHALPursuitFX()); return L+2; }
	float Fraction=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPursuitRenderTest,"HAL.RenderSmoke.Pursuit.ScanBeaconsArrestAndLeftToRightStars",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALPursuitRenderTest::RunTest(const FString&)
{
	if(!FApp::CanEverRender()) { AddError(TEXT("Actual RHI required")); return false; }
	const auto* Config=LoadObject<UHALPolicePursuitConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car06_PolicePursuit.DA_Ability_Car06_PolicePursuit"));
	if(!TestNotNull(TEXT("Saved Car06 skill DA"),Config)) { return false; }
	for(auto* M:{Cast<UMaterial>(Config->FX.RangeMaterial),Cast<UMaterial>(Config->FX.SignalMaterial)})
	{ if(!TestNotNull(TEXT("Native saved material"),M)) { return false; } M->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous); }
	FAssetCompilingManager::Get().FinishAllCompilation(); const auto* Wave=Cast<USoundWave>(Config->FX.SirenSound);
	TestTrue(TEXT("Original looping siren resource available"),Wave&&Wave->bLooping&&Wave->Duration>1);
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(true).ShouldSimulatePhysics(false).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	const auto Box=[&](FVector At,FVector Extent)
	{ auto* A=W->SpawnActor<AActor>(); auto* C=NewObject<UBoxComponent>(A); A->SetRootComponent(C); C->SetBoxExtent(Extent); C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); C->SetCollisionObjectType(ECC_WorldStatic); C->SetCollisionResponseToAllChannels(ECR_Block); C->SetWorldLocation(At); C->RegisterComponent(); return A; };
	Box(FVector(0,0,-20),FVector(3000,3000,20)); auto* Owner=Box(FVector(0,0,80),FVector(5));
	auto* P=W->SpawnActor<AHALPolicePursuit>(); P->SetOwner(Owner); P->State.Activation=FGuid::NewGuid(); P->State.Rules=Config->Rules; P->State.FX=Config->FX; P->State.FX.SirenSound=nullptr;
	auto* Capture=W->SpawnActor<ASceneCapture2D>(); auto* Camera=Capture->GetCaptureComponent2D(); Camera->bCaptureEveryFrame=false; Camera->CaptureSource=SCS_FinalColorLDR; Camera->ProjectionType=ECameraProjectionMode::Orthographic; Camera->OrthoWidth=2300;
	Camera->ShowFlags.SetTemporalAA(false); Camera->PostProcessSettings.bOverride_AutoExposureMethod=true; Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
	Camera->SetWorldLocation(FVector(0,-1100,1800)); Camera->SetWorldRotation((-Camera->GetComponentLocation()).Rotation());
	auto* Target=NewObject<UTextureRenderTarget2D>(); Target->InitCustomFormat(768,768,PF_B8G8R8A8,false); Camera->TextureTarget=Target;
	FString Output; if(!FParse::Value(FCommandLine::Get(),TEXT("HALPursuitRenderOutput="),Output)) { Output=FPaths::ProjectSavedDir()/TEXT("Car06Pursuit/20261115/Render"); } IFileManager::Get().MakeDirectory(*Output,true);
	const auto CapturePixels=[&](const TCHAR* Name) { Camera->CaptureScene(); TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels); TestTrue(TEXT("Proof saved"),FFileHelper::CreateBitmap(*(Output/Name),768,768,Pixels.GetData())); return Pixels; };
	P->State.Start=P->ServerTime()-.01; P->UpdatePresentation(); const auto A=CapturePixels(TEXT("Pursuit_RedScan.bmp"));
	TestTrue(TEXT("Red roof phase"),P->Lights.Num()==2&&P->Lights[0]->Intensity>P->Lights[1]->Intensity);
	TestTrue(TEXT("Range mesh uses authoritative radius"),P->Range&&FMath::IsNearlyEqual(P->Range->GetComponentScale().X,Config->Rules.Radius/100));
	P->State.Start=P->ServerTime()-Config->FX.FlashHalfPeriod; P->UpdatePresentation(); const auto B=CapturePixels(TEXT("Pursuit_BlueScan.bmp"));
	TestTrue(TEXT("Blue roof phase alternates"),P->Lights[1]->Intensity>P->Lights[0]->Intensity);
	int32 Changed=0; for(int32 I=0;I<A.Num();++I) { Changed+=FMath::Abs(int32(A[I].R)-B[I].R)>5; } TestTrue(TEXT("Actual scan moves on the red disk"),Changed>300);
	for(int32 I=0;I<40;++I) { W->Tick(LEVELTICK_All,.05f); }
	auto* ArrestOwner=Box(FVector(300,100,80),FVector(130,80,45)); auto* Receiver=NewObject<UHALPursuitTargetComponent>(ArrestOwner); Receiver->RegisterComponent(); Receiver->State.bArrested=true; Receiver->State.ArrestStart=Receiver->PresentationTime()-.6; Receiver->State.FX=Config->FX; Receiver->UpdateMarker();
	const auto Arrest=CapturePixels(TEXT("Pursuit_Arrest.bmp")); int32 MarkerChanged=0; for(int32 I=0;I<A.Num();++I) { MarkerChanged+=FMath::Abs(int32(Arrest[I].B)-B[I].B)>8; }
	TestTrue(TEXT("Actual crossed chain mesh is visible"),Receiver->Chains&&Receiver->Chains->GetInstanceCount()==Config->FX.Chains.LinksPerLoop*2&&MarkerChanged>50);
	FWidgetRenderer Renderer(true,false); auto* Stars=Renderer.DrawWidget(SNew(SPursuitProofStars).Fraction(4.f/6),FVector2D(220,64)); TArray<FColor> Pixels; Stars->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
	TestTrue(TEXT("Star proof saved"),FFileHelper::CreateBitmap(*(Output/TEXT("Pursuit_Stars_4of6.bmp")),220,64,Pixels.GetData()));
	const auto Mean=[&](int32 Index){const int32 X=58+Index*21; int32 Total=0; for(int32 Y=30;Y<=34;++Y) for(int32 J=X-2;J<=X+2;++J) { Total+=Pixels[Y*220+J].G; } return Total/25;};
	TestTrue(TEXT("Four left stars fill before the two right stars"),Mean(0)>Mean(4)+40&&Mean(3)>Mean(5)+40);
	AddInfo(FString::Printf(TEXT("Pursuit render: scan=%d marker=%d starCenters=%d/%d/%d/%d/%d/%d"),Changed,MarkerChanged,Mean(0),Mean(1),Mean(2),Mean(3),Mean(4),Mean(5)));
	return true;
}
#endif
