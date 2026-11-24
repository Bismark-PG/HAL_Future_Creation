#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "../HALBatarang.h"
#include "../HALMatchGameMode.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALSmokeScreen.h"
#include "../HALSmokeScreenAbility.h"
#include "../HALSmokeScreenView.h"
#include "../TestVehiclePawn.h"
#include "../VehicleHealthComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "../HALBatarangWidget.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "UnrealClient.h"

namespace
{
	struct FDartFixture
	{
		FDartFixture()
		{
			FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* W,const UWorld::InitializationValues&)
			{
				if (!W->IsGameWorld() || !FParse::Param(FCommandLine::Get(),TEXT("HALBatarangFixture"))) { return; }
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* A)
				{
					if (auto* Mode = Cast<AHALMatchGameMode>(A))
					{
						Mode->DefaultPawnClass = LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_12.BP_VehiclePawn_Car_12_C"));
					}
				}));
			});
		}
	} DartFixture;

	class FDartNetworkRun : public IAutomationLatentCommand
	{
		FAutomationTestBase& Test;
		double Deadline = FPlatformTime::Seconds()+90, NextPress = 0, Finished = 0;
		bool bPositioned = false, bPinned = false, bRotated = false, bPressedAgain = false, bBurst = false;
		bool bCheckedVisible = false, bCheckedEdge = false, bCreatedSmoke = false;
		const bool bOpenArt = FParse::Param(FCommandLine::Get(),TEXT("HALDartOpenArt"));
		const bool bCapture = FParse::Param(FCommandLine::Get(),TEXT("HALDartScreens"));
		bool bFlightShot = false, bPinShot = false, bWarningShot = false, bShockShot = false;
		bool bDetailShot = false, bRestoredOverview = false, bFlightDetailShot = false;
		TWeakObjectPtr<ACameraActor> DetailCamera;
		TWeakObjectPtr<ACameraActor> ArtCamera;
		FTransform Overview;
	public:
		explicit FDartNetworkRun(FAutomationTestBase& T) : Test(T) {}
		bool Update() override
		{
			const double Clock=FPlatformTime::Seconds();
			if (Finished) { return Clock>Finished+2.; }
			if (Clock>Deadline)
			{
				Test.AddError(FString::Printf(TEXT("Timed out: positioned=%d pin=%d rotated=%d press=%d burst=%d"),bPositioned,bPinned,bRotated,bPressedAgain,bBurst)); return true;
			}
			UWorld* W=nullptr;
			for (const auto& C : GEngine->GetWorldContexts())
			{
				auto* V=C.World();
				if (V && V->IsGameWorld() && (V->GetNetMode()==NM_ListenServer || V->GetNetMode()==NM_Client)) { W=V; break; }
			}
			if (!W) { return false; }
			const bool bHost=W->GetNetMode()==NM_ListenServer;
			auto* PC=Cast<AHALPlayerController>(W->GetFirstPlayerController());
			auto* Local=PC?Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr;
			auto* GS=W->GetGameState<AHALMatchGameState>();
			if (!Local || !PC->IsLocalController() || !GS) { return false; }
			if (bHost && W->GetNumPlayerControllers()>=2 && GS->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers) { PC->HALStartMatch(); }
			if (GS->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
			AHALBatarang* Dart=nullptr;
			for (TActorIterator<AHALBatarang> I(W); I; ++I) { if (I->GetOwner() && I->GetState().Config) { Dart=*I; break; } }
			if (!bHost && !Dart && !bPinned && Clock>NextPress) { PC->HALUseSkill(); NextPress=Clock+.65; }
			if (Dart)
			{
				const auto& S=Dart->GetState();
				const double VisualAge = Dart->ServerTime()-S.PhaseStart;
				if (bOpenArt && !bHost && !ArtCamera.IsValid())
				{
					const FRotator View(-68,S.Yaw+5,0);
					const FVector Focus=(Local->GetActorLocation()+Dart->FlightPoint(.55f))*.5;
					auto* Camera=W->SpawnActor<ACameraActor>(Focus-View.Vector()*1700,View);
					if (!Camera) { Test.AddError(TEXT("Art proof camera missing")); return true; }
					Camera->GetCameraComponent()->SetFieldOfView(76); Overview=Camera->GetActorTransform(); ArtCamera=Camera; PC->SetViewTarget(Camera);
				}
				if (bOpenArt && !bHost && S.Phase==EHALDartPhase::Flight && VisualAge>.12 && !bFlightShot)
				{
					const auto* Ribbon=FindObjectFast<UDynamicMeshComponent>(Dart,TEXT("FlightWake"));
					Test.TestTrue(TEXT("Curved flight wake is visible in flight"),Ribbon && Ribbon->IsVisible());
					if (bCapture) { FScreenshotRequest::RequestScreenshot(TEXT("Car12_ArtFlight.png"),true,false); }
					bFlightShot=true;
				}
				if (bOpenArt && !bHost && bCapture && S.Phase==EHALDartPhase::Flight && VisualAge>.27 && ArtCamera.IsValid())
				{
					const auto* Blade=FindObjectFast<UStaticMeshComponent>(Dart,TEXT("ResonanceBlade"));
					if (Blade)
					{
						const FVector From=Dart->FlightPoint(.20f), To=Dart->FlightPoint(.33f);
						const FVector Focus=FMath::Lerp(From,To,.58);
						const FRotator View(-73,(To-From).Rotation().Yaw-90.,0);
						ArtCamera->SetActorLocationAndRotation(Focus-View.Vector()*725,View);
						ArtCamera->GetCameraComponent()->SetFieldOfView(58);
						if (VisualAge>.42 && !bFlightDetailShot && !FScreenshotRequest::IsScreenshotRequested())
						{
							FVector2D Pixel;
							PC->ProjectWorldLocationToScreen(Blade->GetComponentLocation(),Pixel);
							Test.AddInfo(FString::Printf(TEXT("Flight detail: blade at (%.0f, %.0f), %.2f s after release"),Pixel.X,Pixel.Y,VisualAge));
							FScreenshotRequest::RequestScreenshot(TEXT("Car12_ArtFlightDetail.png"),true,false);
							bFlightDetailShot=true;
						}
					}
				}
				if (bHost && !bPositioned && S.Phase==EHALDartPhase::Flight)
				{
					auto* Body=CastChecked<UPrimitiveComponent>(Local->GetRootComponent());
					const FVector Spot=Dart->FlightPoint(bOpenArt ? .55f : .24f);
					Local->SetActorLocation(Local->GetActorLocation()+Spot-Body->Bounds.Origin,false,nullptr,ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					Local->ForceNetUpdate(); bPositioned=true;
				}
				if (S.Phase==EHALDartPhase::Pinned && IsValid(S.Target))
				{
					const double Age=Dart->ServerTime()-S.PhaseStart;
					if (!bPinned)
					{
						if (ArtCamera.IsValid())
						{
							ArtCamera->SetActorTransform(Overview);
							ArtCamera->GetCameraComponent()->SetFieldOfView(76);
						}
						Test.TestTrue(TEXT("Client owns the dart, host owns the pinned car"),bHost ? (Dart->GetOwner()!=Local && S.Target==Local) : (Dart->GetOwner()==Local && S.Target!=Local));
						Test.TestEqual(TEXT("Physical pin has dealt zero damage"),S.Target->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(),100.f);
						bPinned=true;
					}
					if (bHost && !bRotated && Age>.28)
					{
						S.Target->SetActorRotation(FRotator(0,90,0),ETeleportType::TeleportPhysics);
						CastChecked<UPrimitiveComponent>(S.Target->GetRootComponent())->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
						S.Target->ForceNetUpdate(); bRotated=true;
					}
					if (bOpenArt && !bHost && bCapture && !bPinShot && Age>.09 && Age<.6)
					{
						FScreenshotRequest::RequestScreenshot(TEXT("Car12_ArtContact.png"),true,false);
						bPinShot=true;
					}
					if (!bHost && Age>.70 && !bCheckedVisible && FMath::Abs(FMath::FindDeltaAngleDegrees(S.Target->GetActorRotation().Yaw,90.))<12.)
					{
						TArray<UUserWidget*> Marks;
						UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Marks,UHALBatarangWidget::StaticClass(),true);
						const auto* Art=FindObjectFast<UStaticMeshComponent>(Dart,TEXT("ResonanceBlade"));
						const auto* Ribbon=FindObjectFast<UDynamicMeshComponent>(Dart,TEXT("FlightWake"));
						Test.TestTrue(TEXT("Casting client has a lock and an attached authored mesh; the wake has finished"),Marks.Num()==1 && Art && Art->GetStaticMesh() && Art->IsVisible() && Ribbon && !Ribbon->IsVisible());
						if (bCapture) { FScreenshotRequest::RequestScreenshot(bOpenArt ? TEXT("Car12_ArtPinned.png") : TEXT("Car12_Pinned.png"),true,false); }
						Test.TestTrue(TEXT("Client's live indicator uses the rotated local pin"),FVector::DotProduct(Dart->PushDirection(),HALBatarang::PushDirection(S.Side,90))>.97);
						bRotated=bCheckedVisible=true;
					}
					if (!bHost && bCheckedVisible && !bCheckedEdge && Age>1.25 && Age<2.15)
					{
						// Keep the caster centred while the victim crosses the camera frame. No gameplay teleport.
						if (!DetailCamera.IsValid())
						{
							const FVector ToTarget = S.Target->GetActorLocation()-Local->GetActorLocation();
							const FRotator View(-89.5,ToTarget.Rotation().Yaw,0);
							const FVector Focus = Local->GetActorLocation()-ToTarget.GetSafeNormal2D()*ToTarget.Size2D()*.18;
							auto* Camera=W->SpawnActor<ACameraActor>(Focus-View.Vector()*ToTarget.Size2D()*2.1,View);
							if (!Camera) { Test.AddError(TEXT("Edge camera missing")); return true; }
							Camera->GetCameraComponent()->SetFieldOfView(65);
							DetailCamera=Camera; PC->SetViewTarget(Camera);
						}
						FVector2D Pixel; int32 Width=0,Height=0; PC->GetViewportSize(Width,Height);
						if (Age>1.60 && PC->ProjectWorldLocationToScreen(S.Target->GetActorLocation(),Pixel)
							&& (Pixel.X<0 || Pixel.Y<0 || Pixel.X>Width || Pixel.Y>Height))
						{
							if (bCapture) { FScreenshotRequest::RequestScreenshot(bOpenArt ? TEXT("Car12_ArtEdge.png") : TEXT("Car12_Edge.png"),true,false); }
							bCheckedEdge=true;
						}
					}
					if (!bHost && Age>2.15 && DetailCamera.IsValid())
					{
						PC->SetViewTarget(ArtCamera.IsValid() ? static_cast<AActor*>(ArtCamera.Get()) : Local); DetailCamera->Destroy(); DetailCamera.Reset();
					}
					if (bOpenArt && !bHost && bCapture && Age>2.45 && Age<3.55 && ArtCamera.IsValid())
					{
						// Inspect the real embedded mesh from its exposed face, then restore the play-scale view.
						const auto* Art=FindObjectFast<UStaticMeshComponent>(Dart,TEXT("ResonanceBlade"));
						if (Art)
						{
							const FVector Focus=Art->GetComponentLocation();
							const FVector Eye=Focus-Art->GetForwardVector().GetSafeNormal2D()*135+FVector(0,0,140);
							ArtCamera->SetActorLocationAndRotation(Eye,(Focus-Eye).Rotation());
							ArtCamera->GetCameraComponent()->SetFieldOfView(49);
							if (Age>2.80 && !bDetailShot)
							{
								FScreenshotRequest::RequestScreenshot(TEXT("Car12_ArtDetail.png"),true,false);
								bDetailShot=true;
							}
						}
					}
					if (bOpenArt && !bHost && bDetailShot && !bRestoredOverview && Age>3.55 && ArtCamera.IsValid())
					{
						const FRotator View(-67,S.Yaw-21,0);
						const FVector Focus=S.Target->GetActorLocation()+Dart->PushDirection()*100;
						ArtCamera->SetActorLocationAndRotation(Focus-View.Vector()*1150,View);
						ArtCamera->GetCameraComponent()->SetFieldOfView(70);
						bRestoredOverview=true;
					}
					if (!bOpenArt && bHost && !bCreatedSmoke && Age>2.45)
					{
						auto* SmokeConfig=LoadObject<UHALSmokeScreenConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car08_SmokeScreen.DA_Ability_Car08_SmokeScreen"));
						FActorSpawnParameters Params; Params.Owner=S.Target;
						auto* Smoke=W->SpawnActor<AHALSmokeScreen>(Params);
						if (!SmokeConfig || !Smoke || !Smoke->Start(*SmokeConfig,HALSmoke::TargetPosition(*S.Target)-FVector(0,0,28)))
						{ Test.AddError(TEXT("Could not create production smoke over the victim")); return true; }
						bCreatedSmoke=true;
					}
					if (bOpenArt && !bHost && !bWarningShot && Age>4.03)
					{
						if (bCapture) { FScreenshotRequest::RequestScreenshot(TEXT("Car12_ArtWarning.png"),true,false); }
						bWarningShot=true;
					}
					if (!bOpenArt && !bHost && !bPressedAgain && Age>3.3 && UHALSmokeScreenView::ShouldHideWorldTarget(S.Target))
					{
						TInlineComponentArray<UMeshComponent*> Meshes(Dart);
						for (auto* Mesh : Meshes) { Test.TestFalse(TEXT("Every public dart layer hides in smoke"),Mesh->IsVisible()); }
						Test.TestTrue(TEXT("Visible lock and offscreen edge preceded concealed lock"),bCheckedVisible && bCheckedEdge);
						if (bCapture) { FScreenshotRequest::RequestScreenshot(TEXT("Car12_Smoke.png"),true,false); }
						Local->RequestPrimarySkill(); bPressedAgain=true;
					}
				}
				if (bOpenArt && !bHost && !bShockShot && S.Phase==EHALDartPhase::Burst && VisualAge>.13)
				{
					const auto* Stamp=FindObjectFast<UDynamicMeshComponent>(Dart,TEXT("DartWings"));
					Test.TestTrue(TEXT("Directional open-air shock front is visible"),Stamp && Stamp->IsVisible()
						&& FVector::DotProduct(Stamp->GetForwardVector(),S.BurstDirection)>.99);
					if (bCapture) { FScreenshotRequest::RequestScreenshot(TEXT("Car12_ArtShock.png"),true,false); }
					bShockShot=true;
				}
				if (S.Phase==EHALDartPhase::Burst && !bBurst)
				{
					Test.TestTrue(TEXT("Replicated final impulse agrees with indicated direction"),FVector::DotProduct(S.BurstDirection,HALBatarang::PushDirection(S.Side,90))>.95);
					if (bHost)
					{
						TArray<UUserWidget*> Marks; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Marks,UHALBatarangWidget::StaticClass(),true);
						Test.TestTrue(TEXT("Victim never receives the caster's private locator"),Marks.IsEmpty());
					}
					else if (bOpenArt) { Test.TestTrue(TEXT("Automatic fuse followed the flight, edge and final warning"),bFlightShot && bCheckedEdge && bWarningShot && !bPressedAgain); }
					else
					{
						Test.TestTrue(TEXT("Manual second press was accepted through smoke before expiry"),bPressedAgain);
						TInlineComponentArray<UMeshComponent*> Meshes(Dart);
						for (const auto* Mesh : Meshes) { Test.TestFalse(TEXT("Concealed detonation has no public crest or blade"),Mesh->IsVisible()); }
					}
					bBurst=true;
				}
			}
			if (bBurst && !Dart)
			{
				for (TActorIterator<ATestVehiclePawn> I(W); I; ++I)
				{
					const auto* HP=I->FindComponentByClass<UVehicleHealthComponent>();
					if (HP && FMath::IsNearlyEqual(HP->GetCurrentHP(),90.f))
					{
						Test.TestTrue(TEXT("One 10 HP hit on the host car"),bHost ? *I==Local : *I!=Local);
						if (bOpenArt && !bHost) { Test.TestTrue(TEXT("Saw the open directional burst"),bShockShot); }
						if (ArtCamera.IsValid()) { PC->SetViewTarget(Local); ArtCamera->Destroy(); }
						Finished=Clock; break;
					}
				}
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALDartNetworkTest,"HAL.NetworkSmoke.Batarang.ClientPinsTurnsAndDetonates",
	EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALDartNetworkTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("HALBatarangFixture"))) { AddWarning(TEXT("Requires the Car12 listen/client fixture.")); return true; }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDartNetworkRun>(*this)); return true;
}
#endif
