#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../BasicBallActor.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALBatarang.h"
#include "../HALBatarangAbility.h"
#include "../HALGameplayTags.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALSkillTestDummy.h"
#include "../HALVehiclePresentationComponent.h"
#include "../TestVehiclePawn.h"
#include "../VehicleDefinition.h"
#include "../VehicleHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ContentStreaming.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#endif

struct FHALBatarangTestAccess
{
	static void Advance(AHALBatarang& Dart,double Now) { Dart.Advance(Now); }
	static FTransform Mount(AHALBatarang& Dart, UHALBatarangConfig& Config, ATestVehiclePawn& Target, FVector Outward)
	{
		const auto Hull=Target.GetRootComponent()->CalcBounds(FTransform::Identity);
		Dart.State.Config=&Config;
		Dart.State.Phase=EHALDartPhase::Pinned;
		Dart.State.Target=&Target;
		Dart.State.LocalAnchor=Hull.Origin+Outward*Hull.BoxExtent;
		Dart.State.LocalNormal=Outward;
		Dart.State.PhaseStart=Dart.ServerTime();
		Dart.Present(Dart.ServerTime()+.4); // Inspect the settled blade after the short contact flash.
		return Dart.PinTransform();
	}
	static void Ground(ATestVehiclePawn& Car,bool bGround)
	{
		auto* Move = Car.FindComponentByClass<UArcadeVehicleMovementComponent>();
		Move->bShouldRunPhysics.Store(true);
		auto& Pose = Move->PublishedStability;
		Pose.bValid = true; Pose.bDrivingAllowed = Pose.bGrounded = bGround; Pose.Up = Pose.GroundNormal = FVector::UpVector;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALDartPathTest,"HAL.FutureCreation.Batarang.MirroredLoopAndFourLocalSides",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALDartPathTest::RunTest(const FString&)
{
	const auto Point = [](float T,int8 Sign) { return HALBatarang::Arc(T,1050,480,Sign); };
	TestTrue(TEXT("Leaves forward and ends behind the original muzzle"),Point(.01f,1).X>0 && Point(1,1).X < -400);
	TestTrue(TEXT("Wide lobes on both sides of the release axis"),Point(.2f,1).Y>320 && Point(.6f,1).Y < -400);
	for (int32 I=0; I<=32; ++I)
	{
		const FVector A=Point(I/32.f,1), B=Point(I/32.f,-1);
		TestTrue(TEXT("Opposite steering mirrors the whole arc"),A.Equals(FVector(B.X,-B.Y,0),.01));
	}
	const FVector Extent(170,78,30);
	for (const auto Side : {EHALDartSide::Front,EHALDartSide::Rear,EHALDartSide::Right,EHALDartSide::Left})
	{
		const FVector Out = -HALBatarang::PushDirection(Side,0);
		TestTrue(TEXT("Four physical contact faces match inward impulse"),HALBatarang::HitSide(Out*Extent,Extent,Out)==Side);
		TestTrue(TEXT("90 degree target turn rotates expected impulse"),HALBatarang::PushDirection(Side,90).Equals(FRotator(0,90,0).RotateVector(-Out),.001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALDartPinArtTest,"HAL.FutureCreation.Batarang.EmbeddedBladeOnProductionDummyAndFusePulse",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
#if WITH_EDITOR
namespace
{
	// Photograph the same tilted production dummy with the blade visible and hidden.
	// The difference catches an accidentally buried blade that still has the correct numeric depth.
	void PhotographRearPin(FAutomationTestBase& Test, UWorld& World, UMeshComponent& Body, UStaticMeshComponent& Blade)
	{
		auto* Floor=World.SpawnActor<AActor>();
		auto* Deck=NewObject<UStaticMeshComponent>(Floor);
		Floor->SetRootComponent(Deck);
		Deck->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Deck->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Deck->SetWorldScale3D(FVector(22,22,.1));
		Deck->SetWorldLocation(FVector(0,0,Body.Bounds.GetBox().Min.Z-6));
		Deck->RegisterComponent();
		auto* Lamp=World.SpawnActor<AActor>();
		auto* Light=NewObject<UDirectionalLightComponent>(Lamp);
		Lamp->SetRootComponent(Light);
		Light->SetIntensity(5.f);
		Light->SetWorldRotation(FRotator(-55,-30,0));
		Light->RegisterComponent();
		TInlineComponentArray<UMeshComponent*> CarMeshes(Body.GetOwner());
		for (auto* Mesh : CarMeshes) { Mesh->SetTextureForceResidentFlag(true); }
		Blade.SetTextureForceResidentFlag(true);

		auto* Camera=World.SpawnActor<ASceneCapture2D>()->GetCaptureComponent2D();
		Camera->bCaptureEveryFrame=false;
		Camera->CaptureSource=SCS_FinalColorLDR;
		Camera->ProjectionType=ECameraProjectionMode::Orthographic;
		Camera->OrthoWidth=730;
		Camera->ShowFlags.SetTemporalAA(false);
		Camera->PostProcessSettings.bOverride_AutoExposureMethod=true;
		Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
		const FVector Target=FMath::Lerp(Body.Bounds.Origin,Blade.GetComponentLocation(),.12);
		const FVector Outward=-Blade.GetForwardVector().GetSafeNormal2D();
		const FVector Across=FVector::CrossProduct(FVector::UpVector,Outward);
		const FVector Eye=Target+Outward*360.+Across*560.+FVector(0,0,780);
		Camera->SetWorldLocationAndRotation(Eye,(Target-Eye).Rotation());
		auto* Texture=NewObject<UTextureRenderTarget2D>();
		Texture->InitCustomFormat(960,720,PF_B8G8R8A8,false);
		Camera->TextureTarget=Texture;
		World.SendAllEndOfFrameUpdates();
		Camera->CaptureScene();
		FlushRenderingCommands();
		IStreamingManager::Get().StreamAllResources(10.f);
		FAssetCompilingManager::Get().FinishAllCompilation();
		const FString Folder=FPaths::ProjectSavedDir()/TEXT("Batarang/PinBurst/Tilted");
		IFileManager::Get().MakeDirectory(*Folder,true);
		auto Capture=[&](bool bVisible,const TCHAR* Name)
		{
			Blade.SetVisibility(bVisible);
			World.SendAllEndOfFrameUpdates();
			Camera->CaptureScene();
			TArray<FColor> Pixels;
			Texture->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
			Test.TestTrue(TEXT("Tilted dummy frame saved"),Pixels.Num()==960*720
				&& FFileHelper::CreateBitmap(*(Folder/Name),960,720,Pixels.GetData()));
			return Pixels;
		};
		const TArray<FColor> Bare=Capture(false,TEXT("Rear_Bare.bmp"));
		const TArray<FColor> Embedded=Capture(true,TEXT("Rear_Embedded.bmp"));
		int32 VisiblePixels=0;
		for (int32 I=0; I<FMath::Min(Bare.Num(),Embedded.Num()); ++I)
		{
			const auto& A=Bare[I]; const auto& B=Embedded[I];
			if (FMath::Abs(int32(A.R)-B.R)+FMath::Abs(int32(A.G)-B.G)+FMath::Abs(int32(A.B)-B.B)>30) { ++VisiblePixels; }
		}
		Test.TestTrue(TEXT("Exposed wings are visible on the tilted dummy at 960x720"),VisiblePixels>120);
		Test.AddInfo(FString::Printf(TEXT("Tilted rear mount, visible blade footprint %d pixels"),VisiblePixels));
		// View the broad side of the wing as well as its silhouette next to the bumper.
		const FVector DetailTarget=FMath::Lerp(Blade.GetComponentLocation(),Body.Bounds.Origin,.13);
		const FVector DetailEye=DetailTarget-Blade.GetForwardVector()*320.+Blade.GetUpVector()*440.+FVector::UpVector*110.;
		Camera->OrthoWidth=280;
		Camera->SetWorldLocationAndRotation(DetailEye,(DetailTarget-DetailEye).Rotation());
		Capture(true,TEXT("Rear_Detail.bmp"));
	}
}
#endif
bool FHALDartPinArtTest::RunTest(const FString&)
{
	auto* Config=LoadObject<UHALBatarangConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car12_Batarang"));
	auto* DummyClass=LoadClass<AHALSkillTestDummy>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_SkillTestDummy.BP_SkillTestDummy_C"));
	if (!TestTrue(TEXT("Production art and dummy available"),Config && Config->BladeMesh && Config->BurstCloudMesh && DummyClass)) { return false; }
	TArray<float> Beats;
	float Previous=0;
	for (int32 I=0; I<500; ++I)
	{
		const float Remaining=(1.f-I/500.f)*Config->FuseSeconds;
		const float Pulse=Config->FusePulse(Remaining);
		if (Pulse>.75f && Previous<=.75f)
		{
			Beats.Add(Config->FuseSeconds-Remaining);
		}
		Previous=Pulse;
	}
	TestTrue(TEXT("Production wings beat faster near expiry"),
		Beats.Num()>=4 && Beats.Last()-Beats.Last(1) < .75f*(Beats[2]-Beats[1]));
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
		.CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* Dummy=W->SpawnActor<AHALSkillTestDummy>(DummyClass,FVector(0,0,120),FRotator(12,30,61));
	if (!TestNotNull(TEXT("Tilted skill dummy"),Dummy)) { return false; }
	if (!Dummy->IsActorInitialized()) { Dummy->PreInitializeComponents(); Dummy->InitializeComponents(); Dummy->PostInitializeComponents(); }
	Dummy->DispatchBeginPlay();
#if WITH_EDITOR
	if (FApp::CanEverRender() && FParse::Param(FCommandLine::Get(),TEXT("HALDartPinPhoto")))
	{
		// Let the production presentation initialize its skin before photographing either state.
		for (int32 Frame=0; Frame<30; ++Frame) { W->Tick(LEVELTICK_All,1.f/30.f); }
		Dummy->SetActorTransform(FTransform(FRotator(12,30,61),FVector(0,0,120)),false,nullptr,ETeleportType::TeleportPhysics);
	}
#endif
	FHALVehicleFXTargetSnapshot Targets;
	Dummy->FindComponentByClass<UHALVehiclePresentationComponent>()->CaptureStatusFXTargets(Targets);
	if (!TestTrue(TEXT("Dummy has a visible car body with baked triangle samples"),!Targets.Targets.IsEmpty()
		&& Config->PinProfiles.ContainsByPredicate([&](const FHALBatarangPinProfile& P) { return P.Mesh.Get()==Targets.Targets[0].MeshAsset.Get() && P.Radii.Num()>=4; }))) { return false; }
	for (const FVector Side : {FVector::ForwardVector,FVector::BackwardVector,FVector::LeftVector,FVector::RightVector})
	{
		auto* Dart=W->SpawnActor<AHALBatarang>();
		const FTransform Skin=FHALBatarangTestAccess::Mount(*Dart,*Config,*Dummy,Side);
		auto* Blade=FindObjectFast<UStaticMeshComponent>(Dart,TEXT("ResonanceBlade"));
		if (!TestNotNull(TEXT("Embedded full-size authored mesh"),Blade)) { return false; }
		const FBox Bounds=Config->BladeMesh->GetBoundingBox();
		const FTransform Transform=Blade->GetComponentTransform();
		const auto Depth=[&](double X) { return FVector::DotProduct(Transform.TransformPosition(FVector(X,0,0))-Skin.GetLocation(),Skin.GetUnitAxis(EAxis::X)); };
		TestTrue(TEXT("Nose penetrates the shell by the DA depth, rear wing remains exposed on every side"),
			FMath::IsNearlyEqual(Depth(Bounds.Max.X),double(Config->WingSpan*Config->PinDepthPercent*.01),.12) && Depth(Bounds.Min.X)<-5.);
		AddInfo(FString::Printf(TEXT("Side %s, tip %.2f cm inside, trailing edge %.2f cm outside"),
			*Side.ToCompactString(),Depth(Bounds.Max.X),-Depth(Bounds.Min.X)));
#if WITH_EDITOR
		if (Side==FVector::BackwardVector && FApp::CanEverRender() && FParse::Param(FCommandLine::Get(),TEXT("HALDartPinPhoto")))
		{
			PhotographRearPin(*this,*W,*Targets.Targets[0].Mesh.Get(),*Blade);
		}
#endif
		const FTransform OldPose=Dummy->GetActorTransform();
		const FTransform RelativeBlade=Transform.GetRelativeTransform(OldPose);
		Dummy->SetActorRotation(FRotator(-17,85,-44),ETeleportType::TeleportPhysics);
		FHALBatarangTestAccess::Mount(*Dart,*Config,*Dummy,Side);
		TestTrue(TEXT("The embedded wings follow the target through a change of pitch, yaw and roll"),
			Blade->GetComponentTransform().Equals(RelativeBlade*Dummy->GetActorTransform(),.13));
		Dummy->SetActorRotation(OldPose.GetRotation(),ETeleportType::TeleportPhysics);
		Dart->Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALDartCombatTest,"HAL.FutureCreation.Batarang.ProductionGrantFlightPinFuseAndCover",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALDartCombatTest::RunTest(const FString&)
{
	auto* Class = LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_12.BP_VehiclePawn_Car_12_C"));
	auto* BallClass = LoadClass<ABasicBallActor>(nullptr,TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
	if (!TestNotNull(TEXT("Production Car12"),Class) || !TestNotNull(TEXT("Physical ball"),BallClass)) { return false; }
	const auto* Def = Class->GetDefaultObject<ATestVehiclePawn>()->GetVehicleDefinition();
	if (!Def || !TestEqual(TEXT("Exactly one production grant"),Def->Abilities.Grants.Num(),1)) { return false; }
	auto* Config = Cast<UHALBatarangConfig>(Def->Abilities.Grants[0].SkillConfig);
	if (!TestNotNull(TEXT("Production dart DA"),Config)) { return false; }
	TestTrue(TEXT("Production tuning and capture signal"),Config->HasValidRules() && Config->PinSignal);
	if (!TestNotNull(TEXT("Production sculpted blade"),Config->BladeMesh.Get())) { return false; }
	TestTrue(TEXT("Authored blade has a finish and measurable wingspan"),Config->BladeMesh->GetMaterial(0)
		&& Config->BladeMesh->GetBounds().BoxExtent.Y > UE_SMALL_NUMBER);
	TestTrue(TEXT("Production wake and resonance are independently assigned"),Config->WakeMaterial && Config->ResonanceMaterial
		&& Config->WakeMaterial!=Config->ResonanceMaterial && Config->WakeSeconds>0);
	TestEqual(TEXT("Five seconds from actual pin"),Config->FuseSeconds,5.f);
	TestEqual(TEXT("Ten damage at this car's base attack"),Config->DamageMultiplier*Def->Combat.AttackPower,10.f,.001f);
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* Match = W->SpawnActor<AHALMatchGameState>(); W->SetGameState(Match); Match->SetMatchPhase(EHALMatchPhase::Playing);
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto SpawnCar = [&](int32 Id)
	{
		auto* V = W->SpawnActor<ATestVehiclePawn>(Class,FVector(0,Id*3500,140),FRotator::ZeroRotator,Params);
		V->PreInitializeComponents(); V->InitializeComponents(); V->PostInitializeComponents();
		auto* PC = W->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
		auto* PS = W->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS); PS->SetPlayerId(Id);
		PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay();
		PS->ChangeVehicleAvatar(V,1); V->DispatchBeginPlay(); FHALBatarangTestAccess::Ground(*V,true);
		return V;
	};
	auto* A = SpawnCar(1); auto* B = SpawnCar(2);
	auto* ASC = CastChecked<UHALAbilitySystemComponent>(A->GetAbilitySystemComponent());
	auto* HP = B->FindComponentByClass<UVehicleHealthComponent>();
	const float Full = HP->GetCurrentHP();
	auto Start = [&]() -> AHALBatarang*
	{
		ASC->GrantUltimateEnergy(100);
		if (!A->RequestPrimarySkill())
		{
			const auto* PS = A->GetPlayerState<AHALPlayerState>();
			AddError(FString::Printf(TEXT("Local primary route: local=%d configured=%d owner=%s ready=%d"),A->IsLocallyControlled(),
				A->IsConfigurationValid(),*GetNameSafe(PS),PS && PS->AreVehicleAbilitiesReady(A)));
		}
		auto* D = A->GetActiveBatarang();
		if (D && !D->HasActorBegunPlay())
		{
			if (!D->IsActorInitialized()) { D->PreInitializeComponents(); D->InitializeComponents(); D->PostInitializeComponents(); }
			D->DispatchBeginPlay();
		}
		return D;
	};
	auto Place = [](AActor& V,const FVector& Center)
	{
		auto* Body = CastChecked<UPrimitiveComponent>(V.GetRootComponent());
		V.SetActorLocation(V.GetActorLocation()+Center-Body->Bounds.Origin,false,nullptr,ETeleportType::TeleportPhysics);
	};
	auto Advance = [](AHALBatarang& D,double Seconds) { FHALBatarangTestAccess::Advance(D,D.GetState().PhaseStart+Seconds); };
	FHALBatarangTestAccess::Ground(*A,false); TestNull(TEXT("Airborne release denied"),Start());
	TestEqual(TEXT("Rejected release spends nothing"),ASC->GetUltimateEnergyView().Energy,100.f);
	FHALBatarangTestAccess::Ground(*A,true);
	auto* D = Start();
	if (!TestNotNull(TEXT("Primary starts a real dart"),D)) { return false; }
	TestFalse(TEXT("In-flight taps cannot detonate"),D->RequestDetonation());
	TestEqual(TEXT("One successful full charge"),ASC->GetUltimateEnergyView().Energy,0.f);
	auto* Ball = W->SpawnActor<ABasicBallActor>(BallClass,D->FlightPoint(.12f),FRotator::ZeroRotator,Params);
	Ball->PreInitializeComponents(); Ball->DispatchBeginPlay();
	auto* Obstacle = W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Obstacle);
	Obstacle->SetRootComponent(Box); Box->SetBoxExtent(FVector(45)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Overlap); Box->RegisterComponent(); Obstacle->SetActorLocation(D->FlightPoint(.28f));
	Place(*B,D->FlightPoint(.55f)); Advance(*D,Config->FlightSeconds);
	if (!TestTrue(TEXT("One long server step follows the curved path through ball and trigger"),D->GetState().Target==B
		&& D->GetState().Phase==EHALDartPhase::Pinned)) { return false; }
	TestEqual(TEXT("Pin does zero damage"),HP->GetCurrentHP(),Full);
	const FVector Before = D->PushDirection();
	B->SetActorRotation(FRotator(0,90,0),ETeleportType::TeleportPhysics);
	const FVector After = D->PushDirection();
	TestTrue(TEXT("Arrow rotates with target before detonation"),After.Equals(FRotator(0,90,0).RotateVector(Before),.01));
	FHALBatarangTestAccess::Ground(*A,false);
	A->RequestPrimarySkill();
	TestTrue(TEXT("Airborne second press explodes along the displayed direction"),D->GetState().Phase==EHALDartPhase::Burst && D->GetState().BurstDirection.Equals(After,.001));
	TestEqual(TEXT("Manual burst exactly 10 HP"),HP->GetCurrentHP(),Full-10);
	TestTrue(TEXT("Medium impact reached normal combat pipeline"),B->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact().EffectiveTier==EHALImpactTier::Medium);
	TestFalse(TEXT("Repeat and expiry cannot settle again"),D->RequestDetonation());
	Advance(*D,.55); TestEqual(TEXT("Only one damage event"),HP->GetCurrentHP(),Full-10);
	FHALBatarangTestAccess::Ground(*A,true);
	D=Start(); if (!TestNotNull(TEXT("New cast after old dart is released"),D)) { return false; }
	Box->SetCollisionResponseToAllChannels(ECR_Block); Place(*Obstacle,D->FlightPoint(.2f)); Place(*B,D->FlightPoint(.55f));
	Advance(*D,Config->FlightSeconds);
	TestTrue(TEXT("Blocking wall ends the cast before a vehicle"),!A->GetActiveBatarang());
	TestEqual(TEXT("Wall miss still costs the full charge"),ASC->GetUltimateEnergyView().Energy,0.f);
	Obstacle->Destroy();
	D=Start(); if (!TestNotNull(TEXT("Timed cast"),D)) { return false; }
	Place(*B,D->FlightPoint(.3f)); Advance(*D,Config->FlightSeconds);
	if (!TestTrue(TEXT("Second physical pin"),D->GetState().Phase==EHALDartPhase::Pinned)) { return false; }
	ASC->ApplyStun(6.f,B); TestFalse(TEXT("Stun blocks manual detonation"),D->RequestDetonation());
	Advance(*D,4.99); TestEqual(TEXT("No early expiry"),HP->GetCurrentHP(),Full-10);
	Advance(*D,5.01); TestEqual(TEXT("Automatic burst still happens during stun"),HP->GetCurrentHP(),Full-20);
	Advance(*D,.55);
	auto* C = SpawnCar(3);
	auto* E = SpawnCar(4);
	// Two different casters pin the same moving target. Cleaning up one cast must leave the other intact.
	for (auto* Caster : {C,E})
	{
		Caster->RequestPrimarySkill(); auto* Other = Caster->GetActiveBatarang();
		if (!TestNotNull(TEXT("A separate caster owns its own dart"),Other)) { return false; }
		if (!Other->IsActorInitialized()) { Other->PreInitializeComponents(); Other->InitializeComponents(); Other->PostInitializeComponents(); }
		if (!Other->HasActorBegunPlay()) { Other->DispatchBeginPlay(); }
		Place(*B,Other->FlightPoint(.29f)); Advance(*Other,Config->FlightSeconds);
		TestTrue(TEXT("Another existing mark does not exclude the target"),Other->GetState().Target==B);
	}
	const auto Remaining = TWeakObjectPtr<AHALBatarang>(E->GetActiveBatarang());
	C->GetController()->UnPossess();
	if (auto* Gone = C->GetActiveBatarang()) { Advance(*Gone,5.1); }
	if (!TestTrue(TEXT("Caster exit retires only that caster's dart"),!IsValid(C->GetActiveBatarang()) && Remaining.IsValid())) { return false; }
	Match->SetMatchPhase(EHALMatchPhase::WaitingForPlayers); Advance(*Remaining.Get(),5.1);
	TestFalse(TEXT("Round end cancels a pending automatic burst"),Remaining.IsValid());
	TestEqual(TEXT("Cleanup causes no late damage"),HP->GetCurrentHP(),Full-20);
	return true;
}
#endif
