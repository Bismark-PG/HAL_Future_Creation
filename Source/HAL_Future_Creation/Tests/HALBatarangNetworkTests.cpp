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
		TWeakObjectPtr<ACameraActor> DetailCamera;
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
				if (bHost && !bPositioned && S.Phase==EHALDartPhase::Flight)
				{
					auto* Body=CastChecked<UPrimitiveComponent>(Local->GetRootComponent());
					const FVector Spot=Dart->FlightPoint(.24f);
					Local->SetActorLocation(Local->GetActorLocation()+Spot-Body->Bounds.Origin,false,nullptr,ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					Local->ForceNetUpdate(); bPositioned=true;
				}
				if (S.Phase==EHALDartPhase::Pinned && IsValid(S.Target))
				{
					const double Age=Dart->ServerTime()-S.PhaseStart;
					if (!bPinned)
					{
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
					const bool bCapture = FParse::Param(FCommandLine::Get(),TEXT("HALDartScreens"));
					if (!bHost && Age>.70 && !bCheckedVisible && FMath::Abs(FMath::FindDeltaAngleDegrees(S.Target->GetActorRotation().Yaw,90.))<12.)
					{
						TArray<UUserWidget*> Marks;
						UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Marks,UHALBatarangWidget::StaticClass(),true);
						Test.TestTrue(TEXT("Only the casting client has a live lock HUD and a rendered dart"),Marks.Num()==1 && Dart->FindComponentByClass<UDynamicMeshComponent>()->IsVisible());
						if (bCapture) { FScreenshotRequest::RequestScreenshot(TEXT("Car12_Pinned.png"),true,false); }
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
							if (bCapture) { FScreenshotRequest::RequestScreenshot(TEXT("Car12_Edge.png"),true,false); }
							bCheckedEdge=true;
						}
					}
					if (!bHost && Age>2.15 && DetailCamera.IsValid())
					{
						PC->SetViewTarget(Local); DetailCamera->Destroy(); DetailCamera.Reset();
					}
					if (bHost && !bCreatedSmoke && Age>2.45)
					{
						auto* SmokeConfig=LoadObject<UHALSmokeScreenConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car08_SmokeScreen.DA_Ability_Car08_SmokeScreen"));
						FActorSpawnParameters Params; Params.Owner=S.Target;
						auto* Smoke=W->SpawnActor<AHALSmokeScreen>(Params);
						if (!SmokeConfig || !Smoke || !Smoke->Start(*SmokeConfig,HALSmoke::TargetPosition(*S.Target)-FVector(0,0,28)))
						{ Test.AddError(TEXT("Could not create production smoke over the victim")); return true; }
						bCreatedSmoke=true;
					}
					if (!bHost && !bPressedAgain && Age>3.3 && UHALSmokeScreenView::ShouldHideWorldTarget(S.Target)
						&& !Dart->FindComponentByClass<UDynamicMeshComponent>()->IsVisible())
					{
						Test.TestTrue(TEXT("Visible lock and offscreen edge preceded concealed lock"),bCheckedVisible && bCheckedEdge);
						if (bCapture) { FScreenshotRequest::RequestScreenshot(TEXT("Car12_Smoke.png"),true,false); }
						Local->RequestPrimarySkill(); bPressedAgain=true;
					}
				}
				if (S.Phase==EHALDartPhase::Burst && !bBurst)
				{
					Test.TestTrue(TEXT("Replicated final impulse agrees with indicated direction"),FVector::DotProduct(S.BurstDirection,HALBatarang::PushDirection(S.Side,90))>.95);
					if (bHost)
					{
						TArray<UUserWidget*> Marks; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Marks,UHALBatarangWidget::StaticClass(),true);
						Test.TestTrue(TEXT("Victim never receives the caster's private locator"),Marks.IsEmpty());
					}
					else { Test.TestTrue(TEXT("Manual second press was accepted through smoke before expiry"),bPressedAgain); }
					bBurst=true;
				}
			}
			if (bBurst && !Dart)
			{
				for (TActorIterator<ATestVehiclePawn> I(W); I; ++I)
				{
					const auto* HP=I->FindComponentByClass<UVehicleHealthComponent>();
					if (HP && FMath::IsNearlyEqual(HP->GetCurrentHP(),90.f))
					{ Test.TestTrue(TEXT("One 10 HP hit on the host car"),bHost ? *I==Local : *I!=Local); Finished=Clock; break; }
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
