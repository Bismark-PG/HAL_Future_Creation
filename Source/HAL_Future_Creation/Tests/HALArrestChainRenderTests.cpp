#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALPursuitTargetComponent.h"
#include "../HALPolicePursuitAbility.h"
#include "../HALSkillTestDummy.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AssetCompilingManager.h"
#include "MaterialShaderPrecompileMode.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"
#include "ContentStreaming.h"
#include "../HALVehiclePresentationComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALArrestChainRenderTest,"HAL.RenderSmoke.Pursuit.CrossedChainsOnVehicleEntryReleaseAndRotation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALArrestChainRenderTest::RunTest(const FString&)
{
	if(!FApp::CanEverRender()) { AddError(TEXT("Actual RHI required")); return false; }
	const auto* Config=LoadObject<UHALPolicePursuitConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car06_PolicePursuit"));
	if(!TestTrue(TEXT("Chain resources reachable from the skill DA"),Config&&Config->FX.Chains.LinkMesh&&Config->FX.Chains.Material)) { return false; }
	auto* M=CastChecked<UMaterial>(Config->FX.Chains.Material); M->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous); FAssetCompilingManager::Get().FinishAllCompilation();
	TestEqual(TEXT("Toon metal keeps three colors through an Unlit masked surface"),EBlendMode(M->BlendMode),BLEND_Masked);
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(true).ShouldSimulatePhysics(false).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* TestConfig=DuplicateObject<UHALSkillTestDummyConfig>(LoadObject<UHALSkillTestDummyConfig>(nullptr,TEXT("/Game/Data/Testing/DA_SkillTestDummy_Default")),GetTransientPackage()); TestConfig->bShowHealth=false;
	const FTransform Pose(FVector(0,0,75)); auto* D=W->SpawnActorDeferred<AHALSkillTestDummy>(AHALSkillTestDummy::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	FindFProperty<FObjectProperty>(D->GetClass(),TEXT("TestConfig"))->SetObjectPropertyValue_InContainer(D,TestConfig); D->FinishSpawning(Pose);
	if(!D->IsActorInitialized()) { D->PreInitializeComponents(); D->InitializeComponents(); D->PostInitializeComponents(); } D->DispatchBeginPlay();
	for(int32 I=0;I<60;++I) { W->Tick(LEVELTICK_All,.05f); } // Give absolute replicated presentation timestamps a nonnegative origin.
	auto* Floor=W->SpawnActor<AActor>(); auto* FloorMesh=NewObject<UStaticMeshComponent>(Floor); Floor->SetRootComponent(FloorMesh); FloorMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); FloorMesh->SetWorldScale3D(FVector(18,18,.1)); FloorMesh->SetWorldLocation(FVector(0,0,-12)); FloorMesh->RegisterComponent();
	auto* LightActor=W->SpawnActor<AActor>(); auto* Light=NewObject<UDirectionalLightComponent>(LightActor); LightActor->SetRootComponent(Light); Light->SetIntensity(4); Light->SetWorldRotation(FRotator(-55,-30,0)); Light->RegisterComponent();
	FAssetCompilingManager::Get().FinishAllCompilation();
	FHALVehicleFXTargetSnapshot VisualTargets; D->FindComponentByClass<UHALVehiclePresentationComponent>()->CaptureStatusFXTargets(VisualTargets);
	for(const auto& T:VisualTargets.Targets) { AddInfo(FString::Printf(TEXT("Chain fit target=%s bounds=%s"),*T.Id.ToString(),*T.VehicleBounds.ToString())); }
	TArray<UMeshComponent*> Meshes; D->GetComponents(Meshes); for(auto* Mesh:Meshes) { Mesh->SetTextureForceResidentFlag(true); }
	IStreamingManager::Get().StreamAllResources(10.f);
	auto* Capture=W->SpawnActor<ASceneCapture2D>(); auto* Camera=Capture->GetCaptureComponent2D(); Camera->bCaptureEveryFrame=false; Camera->CaptureSource=SCS_FinalColorLDR; Camera->ProjectionType=ECameraProjectionMode::Orthographic; Camera->OrthoWidth=620;
	Camera->ShowFlags.SetTemporalAA(false); Camera->PostProcessSettings.bOverride_AutoExposureMethod=true; Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
	Camera->SetWorldLocation(FVector(0,-350,1000)); Camera->SetWorldRotation((FVector(0,0,100)-Camera->GetComponentLocation()).Rotation());
	auto* Target=NewObject<UTextureRenderTarget2D>(); Target->InitCustomFormat(1024,768,PF_B8G8R8A8,false); Camera->TextureTarget=Target;
	FString Output; if(!FParse::Value(FCommandLine::Get(),TEXT("HALChainRenderOutput="),Output)) { Output=FPaths::ProjectSavedDir()/TEXT("Car06Chains/20261115/Render"); } IFileManager::Get().MakeDirectory(*Output,true);
	const auto Pixels=[&](const TCHAR* Name) { Camera->CaptureScene(); TArray<FColor> P; Target->GameThread_GetRenderTargetResource()->ReadPixels(P); TestTrue(TEXT("Proof image written"),FFileHelper::CreateBitmap(*(Output/Name),1024,768,P.GetData())); return P; };
	const auto Difference=[](const TArray<FColor>& A,const TArray<FColor>& B) { int32 N=0; for(int32 I=0;I<A.Num();++I) { N+=FMath::Abs(int32(A[I].R)-B[I].R)+FMath::Abs(int32(A[I].G)-B[I].G)+FMath::Abs(int32(A[I].B)-B[I].B)>25; } return N; };
	const auto Bare=Pixels(TEXT("Chain_00_Bare.bmp")); auto* C=D->FindComponentByClass<UHALPursuitTargetComponent>(); C->State.FX=Config->FX; C->State.bArrested=true; C->State.ArrestStart=C->PresentationTime()-.6; C->OnRep_State();
	const auto Tight=Pixels(TEXT("Chain_01_Tight_Top.bmp")); const int32 Visible=Difference(Bare,Tight);
	TestTrue(TEXT("Actual 3D chain links visibly wrap a real vehicle"),Visible>500&&C->Chains&&C->Chains->GetInstanceCount()==Config->FX.Chains.LinksPerLoop*2);
	if(!TestNotNull(TEXT("Chain instance component"),C->Chains.Get())) { return false; }
	TestEqual(TEXT("Chain geometry never participates in physics collision"),C->Chains->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
	const auto SavedControls=C->State.FX.Chains; C->State.FX.Chains.FlowSpeed=0; C->State.ArrestStart=C->PresentationTime()-2;
	C->State.FX.Chains.WireRadius=.8f; C->UpdateMarker(); const auto Thin=Pixels(TEXT("Chain_07_ThinWire.bmp"));
	FTransform CenterlineBefore,CenterlineAfter; C->Chains->GetInstanceTransform(0,CenterlineBefore);
	C->State.FX.Chains.WireRadius=4.2f; C->UpdateMarker(); const auto Thick=Pixels(TEXT("Chain_08_ThickWire.bmp")); C->Chains->GetInstanceTransform(0,CenterlineAfter);
	const int32 ThinArea=Difference(Bare,Thin),ThickArea=Difference(Bare,Thick),WireChanged=Difference(Thin,Thick);
	TestTrue(TEXT("Wire radius expands visible metal without changing the ring transforms"),CenterlineBefore.Equals(CenterlineAfter,.0001)&&ThickArea>ThinArea*1.2&&WireChanged>500);
	TestTrue(TEXT("Independent radius is actually bound to the material"),FMath::IsNearlyEqual(C->Material->K2_GetScalarParameterValue(TEXT("WireRadius")),4.2f));
	C->State.FX.Chains=SavedControls; C->State.ArrestStart=C->PresentationTime()-2; C->UpdateMarker(); const auto FlowA=Pixels(TEXT("Chain_09_Flow_A.bmp")); C->Chains->GetInstanceTransform(0,CenterlineBefore);
	C->State.ArrestStart-=.13; C->UpdateMarker(); const auto FlowB=Pixels(TEXT("Chain_10_Flow_B.bmp")); C->Chains->GetInstanceTransform(0,CenterlineAfter);
	const int32 FlowChanged=Difference(FlowA,FlowB);
	TestTrue(TEXT("Circulation moves actual links and visible pixels along fixed wrapping paths"),FVector::Distance(CenterlineBefore.GetLocation(),CenterlineAfter.GetLocation())>1&&FlowChanged>200);
	C->State.ArrestStart=C->PresentationTime()-.06; C->UpdateMarker(); const auto Loose=Pixels(TEXT("Chain_02_Entry_Loose.bmp")); TestTrue(TEXT("Tightening changes geometry rather than a static icon"),Difference(Tight,Loose)>200);
	C->State.ArrestStart=C->PresentationTime()-1; C->UpdateMarker(); Camera->SetWorldLocation(FVector(0,-700,330)); Camera->SetWorldRotation((FVector(0,0,90)-Camera->GetComponentLocation()).Rotation()); Pixels(TEXT("Chain_03_Side.bmp"));
	D->SetActorRotation(FRotator(18,35,55)); C->UpdateMarker(); Pixels(TEXT("Chain_04_Tilted.bmp"));
	TestTrue(TEXT("Full pitch/yaw/roll follows car, never faces the camera"),C->Chains->GetComponentQuat().Equals(D->GetActorQuat(),.001f));
	C->State.bArrested=false; C->State.ReleaseStart=C->PresentationTime()-Config->FX.Chains.ReleaseSeconds*.5; C->OnRep_State(); Pixels(TEXT("Chain_05_Release.bmp"));
	TestTrue(TEXT("Release remains a short cosmetic tail after arrest ended"),C->Chains->GetInstanceCount()>0&&C->Material->K2_GetScalarParameterValue(TEXT("Visibility"))<1);
	C->State.ReleaseStart=C->PresentationTime()-2; C->OnRep_State(); Pixels(TEXT("Chain_06_Gone.bmp")); TestEqual(TEXT("Release completes with zero instances"),C->Chains->GetInstanceCount(),0);
	C->State.bArrested=true; C->State.ArrestStart=C->PresentationTime()-1; C->OnRep_State(); C->Clear(); TestTrue(TEXT("Avatar clear hides immediately without release leftovers"),!C->Chains->IsVisible()&&C->Chains->GetInstanceCount()==0);
	AddInfo(FString::Printf(TEXT("Chain proof: changed=%d instances=%d thin=%d thick=%d wireChanged=%d flowChanged=%d output=%s"),Visible,Config->FX.Chains.LinksPerLoop*2,ThinArea,ThickArea,WireChanged,FlowChanged,*Output)); return true;
}
#endif
