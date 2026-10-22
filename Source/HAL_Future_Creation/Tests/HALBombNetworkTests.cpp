#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALBombActor.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALImpactReceiverComponent.h"
#include "../TestVehiclePawn.h"
#include "../VehicleHealthComponent.h"
#include "../SpringPunchGimmick.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

/** Opt-in: client console request -> authority spawn/instant fuse -> range/HP/impact replication. */
class FHALBombLiveTest : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	double Deadline=FPlatformTime::Seconds()+60, Completed=-1;
	float Aligned=-1, InitialOwnHP=0, InitialOtherHP=0, FirstFuseEnd=0;
	bool bPrepared=false,bSpawnReady=false,bRequested=false,bSawArmed=false,bSawExplosion=false;
	FString Diagnostics;
public:
	explicit FHALBombLiveTest(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		if (Completed>0) { return FPlatformTime::Seconds()-Completed>4; }
		if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Bomb body network timeout: ")+Diagnostics); return true; }
		UWorld* World=nullptr;
		for (const auto& Context:GEngine->GetWorldContexts())
		{
			auto* W=Context.World();
			if (W && W->IsGameWorld() && (W->GetNetMode()==NM_ListenServer || W->GetNetMode()==NM_Client)) { World=W; break; }
		}
		if (!World) { return false; }
		const bool bHost=World->GetNetMode()==NM_ListenServer;
		AHALPlayerController* PC=nullptr;
		for (auto It=World->GetPlayerControllerIterator();It;++It)
		{
			auto* C=Cast<AHALPlayerController>(It->Get()); if (C && C->IsLocalController()) { PC=C; break; }
		}
		auto* Own=PC?Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr;
		ATestVehiclePawn* Other=nullptr;
		for (TActorIterator<ATestVehiclePawn> It(World);It;++It) { if (*It!=Own && It->GetPlayerState()) { Other=*It; break; } }
		auto* Match=World->GetGameState<AHALMatchGameState>();
		if (!Own || !Other || !Own->GetPlayerState() || !Other->GetPlayerState() || !Match
			|| !Own->FindComponentByClass<UVehicleHealthComponent>()->IsHealthReady()
			|| !Other->FindComponentByClass<UVehicleHealthComponent>()->IsHealthReady()) { return false; }
		if (!bPrepared)
		{
			InitialOwnHP=Own->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP();
			InitialOtherHP=Other->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP();
			PC->SetName(bHost?TEXT("HAL_BombBodyHostReady"):TEXT("HAL_BombBodyClientReady")); bPrepared=true;
		}
		if (!Other->GetPlayerState()->GetPlayerName().StartsWith(TEXT("HAL_BombBody"))) { return false; }
		if (bHost && Aligned<0)
		{
			for (TActorIterator<ASpringPunchGimmick> It(World);It;++It) { It->Destroy(); }
			PC->HALStartMatch();
			int32 Index=0;
			for (auto* V:{Own,Other})
			{
				auto* Body=CastChecked<UPrimitiveComponent>(V->GetRootComponent());
				Body->SetWorldLocationAndRotation(FVector(Index++?600:-600,-2500,250),FRotator(0,90,0),false,nullptr,ETeleportType::TeleportPhysics);
				Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); V->ForceNetUpdate();
			}
			Aligned=World->GetTimeSeconds();
		}
		if (bHost && !bSpawnReady && Aligned>=0 && World->GetTimeSeconds()-Aligned>1)
		{
			PC->SetName(TEXT("HAL_BombBodyHostSpawnReady")); bSpawnReady=true;
		}
		if (!bHost && !bRequested && Other->GetPlayerState()->GetPlayerName()==TEXT("HAL_BombBodyHostSpawnReady"))
		{
			PC->HALSpawnBomb(); bRequested=true; PC->SetName(TEXT("HAL_BombBodyClientRequested"));
		}
		int32 Bombs=0;
		for (TActorIterator<AHALBombActor> It(World);It;++It)
		{
			++Bombs;
			if (It->IsArmed())
			{
				bSawArmed=true;
				if (FirstFuseEnd==0) { FirstFuseEnd=It->GetFuseEndServerTime(); }
				if (FirstFuseEnd!=It->GetFuseEndServerTime()) { Test->AddError(TEXT("Fuse end changed after arming")); return true; }
			}
		}
		if (Bombs>1) { Test->AddError(TEXT("One command produced multiple bombs")); return true; }
		for (TActorIterator<AHALBombExplosion> It(World);It;++It) { bSawExplosion=true; }
		const float HP1=Own->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP();
		const float HP2=Other->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP();
		Diagnostics=FString::Printf(TEXT("Role=%d Request=%d Armed=%d Explosion=%d Bombs=%d HP=%.1f/%.1f Initial=%.1f/%.1f"),bHost,bRequested,bSawArmed,bSawExplosion,Bombs,HP1,HP2,InitialOwnHP,InitialOtherHP);
		if (!bSawArmed || !bSawExplosion || Bombs || HP1!=InitialOwnHP-(bHost?0.f:25.f) || HP2!=InitialOtherHP-(bHost?25.f:0.f)) { return false; }
		const auto* Struck=bHost?Other:Own;
		if (Struck->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact().EffectiveTier!=EHALImpactTier::Heavy) { return false; }
		if (bHost && Other->GetPlayerState()->GetPlayerName()!=TEXT("HAL_BombBodyClientDone")) { return false; }
		Test->TestTrue(TEXT("Console bomb fuse/destruction/range/damage/Heavy replicate"),true);
		if (!bHost) { PC->SetName(TEXT("HAL_BombBodyClientDone")); }
		UE_LOG(LogTemp,Display,TEXT("HAL Bomb body network complete: %s"),*Diagnostics);
		Completed=FPlatformTime::Seconds(); return false;
	}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBombNetworkTest,"HAL.NetworkSmoke.Bomb",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALBombNetworkTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHALBombLiveTest>(this)); return true;
}
#endif