#pragma once
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "../HALSpecialBall.h"
#include "../HALSpecialBallCue.h"
#include "../HALRoomComponent.h"
#include "../HALPlayerState.h"
#include "../HALPlayerController.h"
#include "../VehicleHealthComponent.h"
#include "EngineUtils.h"
#include "Components/SphereComponent.h"
#include "Engine/NetDriver.h"

/** Opt-in fixture in the real match. Round one uses a physical shot; round two waits for real expiry. */
struct FHALSpecialBallNetworkCheck
{
	const bool bEnabled = FParse::Param(FCommandLine::Get(), TEXT("HALExplosiveBallFixture"));
	uint32 Round = 0;
	bool bSpawned = false, bArrival = false, bSawBody = false, bShot = false, bTerminalCue = false, bDone = false;
	bool bReportedDuplicateImpact = false;
	float InitialHP = 0.f;
	bool Update(FAutomationTestBase& Test, AHALMatchGameState& Match, AHALPlayerController& PC, bool bHost, uint32 Id)
	{
		if (!bEnabled) { return true; }
		const auto& Room = Match.GetRoom()->GetData();
		APawn* Target = Room.Host ? Room.Host->GetPawn() : nullptr;
		auto* Health = Target ? Target->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
		if (!Health || !Health->IsHealthReady()) { return false; }
		UWorld* World = Match.GetWorld();
		if (Round != Id)
		{
			Round = Id;
			bSpawned = bArrival = bSawBody = bShot = bTerminalCue = bDone = false;
			bReportedDuplicateImpact = false;
			InitialHP = Health->GetCurrentHP();
			if (bHost)
			{
				FPacketSimulationSettings Lag;
				Lag.PktLag = 80;
				World->GetNetDriver()->SetPacketSimulationSettings(Lag);
			}
		}
		if (bDone) { return true; }
		const FString ReadyName = FString::Printf(TEXT("SPECIAL_READY_%u"), Id);
		// Wait until every role is in Playing before issuing the first visual event.
		const FString StartName = FString::Printf(TEXT("SPECIAL_MAP_%u"), Id);
		if (!bHost && !bSawBody && PC.PlayerState->GetPlayerName() != StartName) { PC.SetName(StartName); }
		auto AllGuests = [&](const FString& Name)
		{
			for (const auto& Player : Room.Members)
			{ if (!Player || (Player != Room.Host && Player->GetPlayerName() != Name)) { return false; } }
			return true;
		};
		if (bHost && !bSpawned && AllGuests(StartName))
		{
			const auto& Arenas = GetDefault<UHALMatchBallSettings>()->Arenas;
			const auto* Arena = Arenas.FindByPredicate([&](const FHALMatchBallArena& Entry)
			{ return Entry.MatchMap.ToSoftObjectPath().GetLongPackageName() == UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()); });
			if (!Arena) { Test.AddError(TEXT("Missing competition arena for special ball")); return true; }
			World->SpawnActor<AHALExplosiveBall>(Arena->FloorCenter + FVector(1650, -1850, 175), FRotator::ZeroRotator);
			bSpawned = true;
		}
		AHALExplosiveBall* Ball = nullptr;
		int32 Primaries = 0;
		for (TActorIterator<ABasicBallActor> It(World); It; ++It)
		{
			Primaries += It->IsPrimaryMatchBall();
			if (auto* Typed = Cast<AHALExplosiveBall>(*It)) { Ball = Typed; }
		}
		int32 VisibleImpactActors = 0;
		for (TActorIterator<AHALSpecialBallCue> It(World); It; ++It)
		{
			const auto& Cue = It->GetCueState();
			const auto* Type = Cast<UHALExplosiveBallDefinition>(Cue.Type);
			if (!Type || Cue.Radius <= 0) { continue; }
			bArrival |= Cue.Kind == EHALBallCue::Arrival;
			VisibleImpactActors += Cue.Kind == EHALBallCue::Impact;
			const bool bHadTerminalCue = bTerminalCue;
			if (Id == 1 && Cue.Kind == EHALBallCue::Impact && !bTerminalCue)
			{
				Test.TestEqual(TEXT("Replicated impact uses actual type radius"), Cue.Radius, Type->ExplosionRadius);
				Test.TestFalse(TEXT("The match broadcasts one local impact without a second replicated actor"), It->GetIsReplicated());
				Test.TestTrue(TEXT("The local impact cannot open an actor channel"), It->GetRemoteRole() == ROLE_None);
				Test.TestTrue(TEXT("The confirmed fire arcs are playing on this peer"), It->IsActorTickEnabled());
				bTerminalCue = true;
			}
			if (Id == 2 && Cue.Kind == EHALBallCue::Expiration) { bTerminalCue = true; }
			if (bTerminalCue && !bHadTerminalCue)
			{
				UE_LOG(LogTemp, Display, TEXT("Special ball terminal cue observed Host=%d Round=%u Kind=%d Age=%.3f BodyStillPresent=%d HP=%.1f"),
					bHost, Id, int32(Cue.Kind), Match.GetServerWorldTimeSeconds()-Cue.Started, Ball != nullptr, Health->GetCurrentHP());
			}
		}
		if (VisibleImpactActors > 1 && !bReportedDuplicateImpact)
		{
			Test.AddError(TEXT("A single shot produced duplicate impact cues on this peer"));
			bReportedDuplicateImpact = true;
		}
		if (Ball && Ball->RemainingFreeSeconds() > 0 && bArrival && !bSawBody)
		{
			Test.TestTrue(TEXT("Placed primary and live special ball coexist"), Primaries == 1 && !Ball->IsPrimaryMatchBall());
			bSawBody = true;
			UE_LOG(LogTemp, Display, TEXT("Special ball ready Host=%d Round=%u InitialHP=%.1f ObservedHP=%.1f"),
				bHost, Id, InitialHP, Health->GetCurrentHP());
			if (!bHost) { PC.SetName(ReadyName); }
		}
		if (Id == 1 && bHost && bSawBody && !bShot && Ball && AllGuests(ReadyName))
		{
			APawn* Shooter = nullptr;
			for (const auto& Player : Room.Members) { if (Player != Room.Host && Player) { Shooter = Player->GetPawn(); break; } }
			if (!Shooter) { return false; }
			const FVector Forward = Target->GetActorForwardVector().GetSafeNormal2D();
			const FVector From = Target->GetActorLocation() - Forward * 380.f + FVector::UpVector * 18.f;
			Ball->SetActorLocation(From, false, nullptr, ETeleportType::TeleportPhysics);
			bShot = Ball->BeginControl(Shooter) && Ball->LaunchFromControl(Shooter, Shooter, Forward * 3800.f);
			if (!bShot) { Test.AddError(TEXT("Formal special shot could not start in match")); return true; }
		}
		if (!bSawBody || !bTerminalCue || Ball || Primaries != 1) { return false; }
		const auto* Type = GetDefault<UHALMatchBallSettings>()->ExplosiveBallType.LoadSynchronous();
		const float Expected = InitialHP - (Id == 1 ? Type->Gameplay.Damage.VehicleHitDamage : 0.f);
		if (!FMath::IsNearlyEqual(Health->GetCurrentHP(), Expected)) { return false; }
		UE_LOG(LogTemp, Display, TEXT("Special ball verified Host=%d Round=%u Arrival=1 %s HP=%.1f Primary=%d"),
			bHost, Id, Id == 1 ? TEXT("Impact=1") : TEXT("Expiration=1"), Expected, Primaries);
		bDone = true;
		return true;
	}
};
#endif
