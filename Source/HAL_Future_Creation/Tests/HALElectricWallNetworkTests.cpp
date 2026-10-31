#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALElectricWallGimmick.h"
#include "../HALShockedPresentationComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALHealthAttributeSet.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace HALElectricWallNetworkTests
{
	class FRun : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline=FPlatformTime::Seconds()+100,AlignedAt=-1,Complete=-1;
		bool Prepared=false,ReadySent=false,Aligned=false,Triggered=false,SawOwn=false,SawRemote=false;
		float OwnPeak=0;
		FGuid PreviousImpactId;
	public:
		explicit FRun(FAutomationTestBase* T):Test(T) {}
		bool Update() override
		{
			if (Complete>0) { return FPlatformTime::Seconds()-Complete>4; }
			if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Electric wall network timeout; paired host/client fixture required")); return true; }
			UWorld* World=nullptr;
			for (const auto& Context:GEngine->GetWorldContexts())
			{ auto* W=Context.World(); if (W && W->IsGameWorld() && (W->GetNetMode()==NM_Client || W->GetNetMode()==NM_ListenServer)) { World=W; break; } }
			if (!World) { return false; }
			const bool Host=World->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC=nullptr;
			for (auto It=World->GetPlayerControllerIterator();It;++It)
			{ if (auto* C=Cast<AHALPlayerController>(It->Get()); C && C->IsLocalController()) { PC=C; break; } }
			auto* Own=PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other=nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World);It;++It) { if (*It!=Own && It->GetPlayerState()) { Other=*It; break; } }
			auto* ASC=Own ? Cast<UHALAbilitySystemComponent>(Own->GetAbilitySystemComponent()) : nullptr;
			auto* Remote=Other ? Cast<UHALAbilitySystemComponent>(Other->GetAbilitySystemComponent()) : nullptr;
			auto* Match=World->GetGameState<AHALMatchGameState>();
			if (!ASC || !Remote || !Match || !ASC->IsHealthInitialized() || !Remote->IsHealthInitialized()) { return false; }
			auto* Movement=Own->FindComponentByClass<UArcadeVehicleMovementComponent>();
			const auto Physics=Movement->GetMovementEffectStatus();
			if (!Prepared)
			{
				for (auto* V:{Own,Other}) { auto* Body=CastChecked<UPrimitiveComponent>(V->GetRootComponent()); Body->SetEnableGravity(false); Body->SetLinearDamping(0); }
				FVehicleInputCmd Warmup; Warmup.Throttle=.1f; Movement->SetInputCommand(Warmup); Prepared=true;
			}
			if (!Host && !ReadySent && Physics.bReady) { PC->SetName(TEXT("HAL_Electric_ReadyC")); ReadySent=true; }
			if (Host && Match->GetMatchPhase()!=EHALMatchPhase::Playing) { PC->HALStartMatch(); return false; }
			const FString OtherName=Other->GetPlayerState()->GetPlayerName();
			const double Now=World->GetTimeSeconds();
			AHALElectricWallGimmick* Wall=nullptr;
			// The team's map can already contain electric-wall Blueprint instances. Select only
			// this test's native aerial fixture, never the first unrelated mechanism in the level.
			for (TActorIterator<AHALElectricWallGimmick> It(World);It;++It)
			{ if (It->GetClass()==AHALElectricWallGimmick::StaticClass() && It->GetActorLocation().Equals(FVector(0,0,10000),1.f)) { Wall=*It; break; } }
			if (Host && !Aligned && Physics.bReady && OtherName==TEXT("HAL_Electric_ReadyC"))
			{
				int32 Index=0;
				for (auto* V:{Own,Other})
				{
					auto* Body=CastChecked<UPrimitiveComponent>(V->GetRootComponent());
					Body->SetWorldLocationAndRotation(FVector(-600,Index++ ? 500 : -500,10300),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					V->GetAbilitySystemComponent()->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),100);
					V->ForceNetUpdate();
				}
				FHALElectricWallConfig Config; Config.PillarSpacing=2400; Config.Height=600; Config.BounceBackDistance=100; Config.BounceBackSeconds=.3f;
				const FTransform Pose(FRotator(0,90,0),FVector(0,0,10000));
				Wall=World->SpawnActorDeferred<AHALElectricWallGimmick>(AHALElectricWallGimmick::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!Wall || !Wall->Configure(Config)) { Test->AddError(TEXT("Cannot create paired electric wall fixture")); return true; }
				Wall->FinishSpawning(Pose); Wall->ForceNetUpdate();
				Aligned=true; AlignedAt=Now; return false;
			}
			if (!Host && !Aligned && Wall && Own->GetActorLocation().Z>10200 && Other->GetActorLocation().Z>10200 && Physics.bReady)
			{ Aligned=true; PC->SetName(TEXT("HAL_Electric_AlignedC")); }
			if (Host && Aligned && !Triggered && OtherName==TEXT("HAL_Electric_AlignedC") && Now-AlignedAt>.8)
			{
				PreviousImpactId=Own->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact().EventId;
				FPacketSimulationSettings Settings; Settings.PktLag=80; World->GetNetDriver()->SetPacketSimulationSettings(Settings);
				for (auto* V:{Own,Other})
				{ const auto P=V->GetActorLocation(); V->SetActorLocation(FVector(0,P.Y,P.Z),false,nullptr,ETeleportType::TeleportPhysics);
						CastChecked<UPrimitiveComponent>(V->GetRootComponent())->SetPhysicsLinearVelocity(FVector(4000,0,0));
						V->GetAbilitySystemComponent()->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune); }
				Triggered=true; UE_LOG(LogTemp,Display,TEXT("HAL ElectricWall smoke: Host=1 Entered=1 Lag=80ms"));
			}
			if (!Wall) { return false; }
			auto* OwnVisual=Own->FindComponentByClass<UHALShockedPresentationComponent>();
			auto* OtherVisual=Other->FindComponentByClass<UHALShockedPresentationComponent>();
			SawOwn |= ASC->IsStunned() && OwnVisual->IsShocked() && Physics.bReady && Physics.Value.bControlsBlocked;
			SawRemote |= Remote->IsStunned() && OtherVisual->IsShocked();
			if (Aligned) { OwnPeak=FMath::Max(OwnPeak,float(-CastChecked<UPrimitiveComponent>(Own->GetRootComponent())->GetPhysicsLinearVelocity().X)); }
			if (!SawOwn || !SawRemote || ASC->IsStunned() || Remote->IsStunned() || OwnVisual->IsShocked() || OtherVisual->IsShocked()
				|| !Physics.bReady || Physics.Value.bControlsBlocked || Wall->GetAcceptedContactCount()!=2 || OwnPeak<1) { return false; }
			if (Host && OtherName!=TEXT("HAL_Electric_DoneC")) { return false; }
			Test->TestEqual(TEXT("Group contact applied once per vehicle"),Wall->GetAcceptedContactCount(),uint32(2));
			Test->TestEqual(TEXT("No HP damage"),ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute()),100.f);
			const auto Impact=Own->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact();
			if (Host) { Test->TestEqual(TEXT("Bounce leaves authority Impact event unchanged"),Impact.EventId,PreviousImpactId); }

			Test->TestTrue(TEXT("Eastbound contact produces actual westward motion"),OwnPeak>1);
			Movement->ResetInputCommand(); if (!Host) { PC->SetName(TEXT("HAL_Electric_DoneC")); }
			Complete=FPlatformTime::Seconds();
			UE_LOG(LogTemp,Display,TEXT("HAL ElectricWall smoke: Host=%d Complete=1 Contacts=%u PeakWest=%.3f HP=100 Stun=0 Shocked=0 Resimulations=%d Rewinds=%d"),
				Host,Wall->GetAcceptedContactCount(),OwnPeak,Movement->GetResimulationRunCount(),Physics.RewindRequests);
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALElectricWallNetworkTest,"HAL.NetworkSmoke.ElectricWall",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALElectricWallNetworkTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALElectricWallNetworkTests::FRun>(this)); return true;
}
#endif
