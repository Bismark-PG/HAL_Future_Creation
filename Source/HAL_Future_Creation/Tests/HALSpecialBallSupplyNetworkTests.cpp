#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "../HALSpecialBallPad.h"
#include "../HALBlackHoleBall.h"
#include "../HALMeteorBall.h"
#include "../HALTimedBombBall.h"
#include "../HALHomingBall.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "UnrealClient.h"

namespace
{
	class FSupplyNetworkRun : public IAutomationLatentCommand
	{
	public:
		explicit FSupplyNetworkRun(FAutomationTestBase& InTest) : Test(InTest) {}
		bool Update() override;
	private:
		FAutomationTestBase& Test;
		double Deadline = FPlatformTime::Seconds() + 100.;
		double Start = -1.;
		double Finished = -1.;
		double MinZ = MAX_dbl;
		double MaxZ = -MAX_dbl;
		bool bNamed = false;
		bool bPlacedCar = false;
		bool bCrossed = false;
		bool bWarnShot = false;
		double LastWarningAt = -1.;
		double DeliveryPhotoAt = -1.;
		EHALPadBall Announced = EHALPadBall::None;
		TWeakObjectPtr<AHALSpecialBallPad> Site;
		TWeakObjectPtr<AHALSpecialBallPad> PreviousSite;
		TSet<FName> Deliveries;
		TWeakObjectPtr<ACameraActor> Camera;
	};

	bool FSupplyNetworkRun::Update()
	{
		const double Now = FPlatformTime::Seconds();
		if (Finished > 0.) { return Now > Finished + 2.; }
		if (Now > Deadline) { Test.AddError(FString::Printf(TEXT("Supply test timed out: named=%d started=%.1f crossed=%d deliveries=%d"), bNamed, Start, bCrossed, Deliveries.Num())); return true; }
		UWorld* W = nullptr;
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			auto* World = Context.World();
			if (World && (World->IsNetMode(NM_ListenServer) || World->IsNetMode(NM_Client)) && World->IsGameWorld()) { W = World; break; }
		}
		if (!W) { return false; }
		auto* PC = Cast<AHALPlayerController>(W->GetFirstPlayerController());
		auto* Match = W->GetGameState<AHALMatchGameState>();
		auto* Car = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
		if (!Match || !Car || !PC->IsLocalController()) { return false; }
		const bool bHost = W->IsNetMode(NM_ListenServer);
		if (!bHost && !bNamed) { PC->SetName(TEXT("SupplyObserver")); bNamed = true; }
		if (bHost && Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers)
		{
			for (const auto& PS : Match->PlayerArray)
			{
				if (PS && PS->GetPlayerName() == TEXT("SupplyObserver")) { PC->HALStartMatch(); break; }
			}
		}
		if (Match->GetMatchPhase() != EHALMatchPhase::Playing) { return false; }
		TArray<AHALSpecialBallPad*> Pads;
		for (TActorIterator<AHALSpecialBallPad> It(W); It; ++It) { Pads.Add(*It); }
		if (!Test.TestEqual(TEXT("Two saved pad actors in the formal map"), Pads.Num(), 2)) { return true; }
		Pads.Sort([](const AHALSpecialBallPad& A, const AHALSpecialBallPad& B) { return A.RotationOrder < B.RotationOrder; });
		const FVector Anchor = Pads[0]->GetActorLocation();
		const bool bPhotos = !bHost && FParse::Param(FCommandLine::Get(), TEXT("HALSupplyScreens"));
		const auto Clock = W->GetTimeSeconds();
		if (Start < 0.)
		{
			Start = Clock;
			for (const auto* Pad : Pads)
			{
				auto* Mesh = Pad->FindComponentByClass<UStaticMeshComponent>();
				if (!Test.TestTrue(TEXT("Formal pad has production mesh and no collision or overlaps"),
					Mesh && Mesh->GetStaticMesh() && Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Mesh->GetGenerateOverlapEvents())) { return true; }
				for (double Offset : {-145., 0., 145.})
				{
					const FVector At = Pad->GetActorLocation() + FVector(Offset,0,0);
					FHitResult Hit;
					const bool bFloor = W->LineTraceSingleByChannel(Hit,At+FVector(0,0,80),At-FVector(0,0,30),ECC_Visibility);
					if (!Test.TestTrue(TEXT("Ground probe at hub and rims reaches unchanged driving plane"), bFloor && Hit.GetActor()!=Pad && FMath::Abs(Hit.ImpactPoint.Z-At.Z)<1.)) { return true; }
				}
			}
			if (bPhotos)
			{
				const FVector Eye = Anchor + FVector(-450,-520,760);
				Camera = W->SpawnActor<ACameraActor>(Eye,(Anchor+FVector(0,0,40)-Eye).Rotation());
				Camera->GetCameraComponent()->SetFieldOfView(49.f);
				PC->SetViewTarget(Camera.Get());
			}
		}
		if (bHost && !bCrossed)
		{
			auto* Body = CastChecked<UPrimitiveComponent>(Car->GetRootComponent());
			if (!bPlacedCar)
			{
				Car->SetActorLocationAndRotation(Anchor + FVector(-460,0,100), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
				Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				bPlacedCar = true;
			}
			const double X = Car->GetActorLocation().X - Anchor.X;
			if (Clock - Start > 2. && X < 400.)
			{
				Body->SetPhysicsLinearVelocity(FVector(700.,0.,Body->GetPhysicsLinearVelocity().Z));
				if (FMath::Abs(X)<190.) { MinZ=FMath::Min(MinZ,Car->GetActorLocation().Z); MaxZ=FMath::Max(MaxZ,Car->GetActorLocation().Z); }
			}
			else if (X >= 400.)
			{
				bCrossed = true;
				Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Test.TestTrue(TEXT("Driving all the way over the empty pad produces no step"), MaxZ >= MinZ && MaxZ-MinZ < 12.);
				Test.AddInfo(FString::Printf(TEXT("Actual suspension crossing height variation %.2f cm."), MaxZ-MinZ));
			}
		}
		if (bPhotos && Clock-Start>5. && !bPlacedCar) { FScreenshotRequest::RequestScreenshot(TEXT("SpecialBallPad_Idle.png"),false,false); bPlacedCar=true; }
		for (auto* Pad : Pads)
		{
			const auto& Warning = Pad->GetWarning();
			if (Warning.Ball == EHALPadBall::None || (Pad == PreviousSite.Get() && Warning.StartedAt <= LastWarningAt)) { continue; }
			if (!Site.IsValid())
			{
				if (!Test.TestTrue(TEXT("Real warning alternates sites"), Pad != PreviousSite.Get())) { return true; }
				Site = Pad; Announced = Warning.Ball; LastWarningAt = Warning.StartedAt;
			}
			if (!Test.TestTrue(TEXT("Exactly the announced site charges"), Pad == Site.Get() && Warning.Ball == Announced)) { return true; }
			if (bPhotos && Deliveries.IsEmpty() && !bWarnShot && Match->GetServerWorldTimeSeconds()-Warning.StartedAt > Warning.Duration*.6f)
			{
				FScreenshotRequest::RequestScreenshot(TEXT("SpecialBallPad_Warning.png"),false,false); bWarnShot = true;
			}
		}
		for (TActorIterator<AHALSpecialBall> It(W); It; ++It)
		{
			if (Deliveries.Contains(It->GetFName())) { continue; }
			const EHALPadBall Actual = It->IsA<AHALHomingBall>() ? EHALPadBall::Homing : It->IsA<AHALTimedBombBall>() ? EHALPadBall::TimedBomb :
				It->IsA<AHALBlackHoleBall>() ? EHALPadBall::BlackHole : It->IsA<AHALMeteorBall>() ? EHALPadBall::Meteor : EHALPadBall::Explosive;
			if (!Test.TestTrue(TEXT("Free replicated ball matches the warned type and pad"),
				Site.IsValid() && Actual == Announced && It->GetBallState()==EBasicBallState::Free && !It->IsPrimaryMatchBall()
				&& FVector::Dist2D(It->GetActorLocation(),Site->GetActorLocation()) <
					(Actual == EHALPadBall::TimedBomb ? 15.f + CastChecked<UHALTimedBombBallDefinition>(It->GetTypeDefinition())->PatrolSpeed * .35f : 15.f))) { return true; }
			Deliveries.Add(It->GetFName());
			Test.AddInfo(FString::Printf(TEXT("Drop %d saw %s at pad %d, %.2f seconds since Playing."),Deliveries.Num(),*It->GetClass()->GetName(),Site->RotationOrder,Clock-Start));
			if (bPhotos && Deliveries.Num()==1) { DeliveryPhotoAt = Clock+.28; }
			PreviousSite=Site; Site.Reset(); Announced=EHALPadBall::None;
		}
		if (DeliveryPhotoAt > 0. && Clock >= DeliveryPhotoAt)
		{
			FScreenshotRequest::RequestScreenshot(TEXT("SpecialBallPad_Delivery.png"),false,false); DeliveryPhotoAt = -1.;
		}
		if (Deliveries.Num() >= 2)
		{
			int32 Primaries = 0;
			for (TActorIterator<ABasicBallActor> It(W); It; ++It) { Primaries += It->IsPrimaryMatchBall(); }
			Test.TestEqual(TEXT("Original primary remains unique on both peers"), Primaries,1);
			if (bHost) { Test.TestTrue(TEXT("Vehicle traversed the empty pad"),bCrossed); }
			Finished = Now;
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSpecialBallSupplyNetworkTest,"HAL.NetworkSmoke.SpecialBallSupply.FormalMatch",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALSpecialBallSupplyNetworkTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("HALSupplyFixture"))) { AddWarning(TEXT("Requires a listen host and client with -HALSupplyFixture.")); return true; }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSupplyNetworkRun>(*this)); return true;
}
#endif
