#pragma once
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "../HALSpecialBall.h"
#include "../HALBlackHoleBall.h"
#include "../HALMeteorBall.h"
#include "../TestVehiclePawn.h"
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
	const bool bBlackHole = FParse::Param(FCommandLine::Get(), TEXT("HALBlackHoleBallFixture"));
	const bool bMeteor = FParse::Param(FCommandLine::Get(), TEXT("HALMeteorBallFixture"));
	const bool bEnabled = bBlackHole || bMeteor || FParse::Param(FCommandLine::Get(), TEXT("HALExplosiveBallFixture"));
	uint32 Round = 0;
	bool bSpawned = false, bArrival = false, bSawBody = false, bShot = false, bTerminalCue = false, bDone = false;
	bool bReportedDuplicateImpact = false, bReportedFieldEnd = false;
	float InitialHP = 0.f;
	double FieldEndTime = 0.;
	TWeakObjectPtr<UVehicleHealthComponent> MeteorVictim;
	TOptional<FVector> LockedGround;
	bool Update(FAutomationTestBase& Test, AHALMatchGameState& Match, AHALPlayerController& PC, bool bHost, uint32 Id)
	{
		if (!bEnabled) { return true; }
		const auto* Settings = GetDefault<UHALMatchBallSettings>();
		const UHALSpecialBallDefinition* ExpectedType = bBlackHole
			? static_cast<UHALSpecialBallDefinition*>(Settings->BlackHoleBallType.LoadSynchronous())
			: static_cast<UHALSpecialBallDefinition*>(Settings->ExplosiveBallType.LoadSynchronous());
		if (bMeteor) { ExpectedType = Settings->MeteorBallType.LoadSynchronous(); }
		if (!ExpectedType) { Test.AddError(TEXT("Missing production special ball type")); return true; }
		const auto& Room = Match.GetRoom()->GetData();
		APawn* Target = Room.Host ? Room.Host->GetPawn() : nullptr;
		auto* Health = Target ? Target->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
		if (!Health || !Health->IsHealthReady()) { return false; }
		UWorld* World = Match.GetWorld();
		if (Round != Id)
		{
			Round = Id;
			bSpawned = bArrival = bSawBody = bShot = bTerminalCue = bDone = false;
			bReportedDuplicateImpact = bReportedFieldEnd = false;
			MeteorVictim.Reset();
			LockedGround.Reset();
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
			UClass* BallClass = bBlackHole ? AHALBlackHoleBall::StaticClass() : AHALExplosiveBall::StaticClass();
			if (bMeteor) { BallClass = AHALMeteorBall::StaticClass(); }
			World->SpawnActor<AHALSpecialBall>(BallClass,
				Arena->FloorCenter + FVector(1650, -1850, 175), FRotator::ZeroRotator);
			bSpawned = true;
		}
		AHALSpecialBall* Ball = nullptr;
		int32 Primaries = 0;
		for (TActorIterator<ABasicBallActor> It(World); It; ++It)
		{
			Primaries += It->IsPrimaryMatchBall();
			if (auto* Typed = Cast<AHALSpecialBall>(*It); Typed && Typed->GetTypeDefinition() == ExpectedType) { Ball = Typed; }
		}
		int32 VisibleImpactActors = 0;
		for (TActorIterator<AHALSpecialBallCue> It(World); It; ++It)
		{
			const auto& Cue = It->GetCueState();
			if (Cue.Type != ExpectedType || Cue.Radius <= 0) { continue; }
			bArrival |= Cue.Kind == EHALBallCue::Arrival;
			VisibleImpactActors += Cue.Kind == EHALBallCue::Impact;
			const bool bHadTerminalCue = bTerminalCue;
			if (!bBlackHole && !bMeteor && Id == 1 && Cue.Kind == EHALBallCue::Impact && !bTerminalCue)
			{
				Test.TestEqual(TEXT("Replicated impact uses actual type radius"), Cue.Radius, CastChecked<UHALExplosiveBallDefinition>(ExpectedType)->ExplosionRadius);
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
		bool bFieldActive = false;
		if (bMeteor)
		{
			int32 Count = 0;
			for (TActorIterator<AHALMeteorStrike> It(World); It; ++It)
			{
				const auto& Strike = It->GetStrikeState();
				if (Strike.Type != ExpectedType) { continue; }
				++Count;
				if (Strike.Phase == EHALMeteorPhase::Tracking && Strike.Target && !MeteorVictim.IsValid())
				{
					MeteorVictim = Strike.Target->FindComponentByClass<UVehicleHealthComponent>();
					if (MeteorVictim.IsValid()) { InitialHP = MeteorVictim->GetCurrentHP(); }
				}
				if (Strike.Phase == EHALMeteorPhase::Falling && !LockedGround.IsSet())
				{
					Test.TestNull(TEXT("Falling releases tracking target on this peer"), Strike.Target.Get());
					LockedGround = FVector(Strike.Ground);
				}
				if (Strike.Phase == EHALMeteorPhase::Impact && !bTerminalCue)
				{
					Test.TestTrue(TEXT("Meteor tracking, fixed descent and impact seen in order"),
						MeteorVictim.IsValid() && LockedGround.IsSet() && LockedGround->Equals(Strike.Ground, .15));
					UE_LOG(LogTemp, Display, TEXT("Meteor observed Host=%d Ground=%s Victim=%s Radius=%.1f"),
						bHost, *Strike.Ground.ToCompactString(), *GetNameSafe(MeteorVictim.IsValid() ? MeteorVictim->GetOwner() : nullptr), Strike.Type->DamageRadius);
					bTerminalCue = true;
				}
			}
			if (Count > (Id == 1 ? 1 : 0) && !bReportedDuplicateImpact)
			{ Test.AddError(TEXT("Extra meteor strike or meteor created by free expiry")); bReportedDuplicateImpact = true; }
		}
		if (bBlackHole)
		{
			const auto* Hole = CastChecked<UHALBlackHoleBallDefinition>(ExpectedType);
			FHALBlackHoleConfig Tuned;
			if (!Hole->ResolveField(Tuned)) { Test.AddError(TEXT("Invalid field tuning")); return true; }
			int32 Count = 0;
			for (TActorIterator<AHALBlackHole> It(World, Hole->FieldClass); It; ++It)
			{
				const auto& Config = It->GetBlackHoleConfig();
				if (Config.MaximumRadius != Tuned.MaximumRadius || Config.DurationSeconds != Tuned.DurationSeconds) { continue; }
				++Count;
				if (!bTerminalCue && Id == 1 && It->GetStage() == EHALBlackHoleStage::PreparingPulse)
				{
					Test.TestEqual(TEXT("Replicated pull matches ball DA"), Config.ContinuousAcceleration, Tuned.ContinuousAcceleration);
					Test.TestTrue(TEXT("Full arena FX field radius on peer"), FMath::IsNearlyEqual(It->GetCurrentRadius(), Tuned.MaximumRadius, 1.f));
					UE_LOG(LogTemp, Display, TEXT("Black hole ball field observed Host=%d Round=%u Radius=%.1f Pull=%.1f Center=%s Target=%s HP=%.1f Primary=%d"),
						bHost, Id, Config.MaximumRadius, Config.ContinuousAcceleration, *It->GetActorLocation().ToCompactString(),
						*Target->GetActorLocation().ToCompactString(), Health->GetCurrentHP(), Primaries);
					bTerminalCue = true;
				}
			}
			bFieldActive = Count > 0;
			if (Id == 1 && bTerminalCue && !bFieldActive && !bReportedFieldEnd)
			{
				bReportedFieldEnd = true;
				FieldEndTime = World->GetTimeSeconds();
				UE_LOG(LogTemp, Display, TEXT("Black hole ball field retired Host=%d HP=%.1f Primary=%d Ball=%s"),
					bHost, Health->GetCurrentHP(), Primaries, *GetNameSafe(Ball));
			}
			if (Count > (Id == 1 ? 1 : 0) && !bReportedDuplicateImpact)
			{ Test.AddError(TEXT("Extra ball field or field left by natural expiry")); bReportedDuplicateImpact = true; }
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
			FVector Forward = Target->GetActorForwardVector().GetSafeNormal2D();
			FVector From = Target->GetActorLocation() - Forward * 380.f + FVector::UpVector * 18.f;
			if (bMeteor)
			{
				const auto* Arena = Settings->Arenas.FindByPredicate([&](const FHALMatchBallArena& Entry)
				{ return Entry.MatchMap.ToSoftObjectPath().GetLongPackageName() == UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()); });
				if (!Arena) { Test.AddError(TEXT("Missing meteor test floor")); return true; }
				// Four separated patches of deck keep the random target clear of moving arena hazards.
				for (int32 I = 0; I < Room.Members.Num(); ++I)
				{
					if (auto* Car = Room.Members[I] ? Room.Members[I]->GetPawn() : nullptr)
					{
						const FVector Position = Arena->FloorCenter + FVector(I & 1 ? 1480 : -1480, I & 2 ? 1600 : -1600, 130);
						Car->SetActorLocation(Position,false,nullptr,ETeleportType::ResetPhysics);
					}
				}
				From = Shooter->GetActorLocation() + Shooter->GetActorForwardVector() * 250.f + FVector::UpVector * 100.f;
			}
			if (bBlackHole)
			{
				// Spawn pads may border a punch or a wall. A radial explosion could still score there;
				// this fixture specifically exercises direct damage, so prove the shot lane is clear.
				auto* Sphere = Ball->GetPhysicsRoot();
				const auto* Rules = Settings->Arenas.FindByPredicate([&](const FHALMatchBallArena& Entry)
				{ return Entry.MatchMap.ToSoftObjectPath().GetLongPackageName() == UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()); });
				if (!Rules) { Test.AddError(TEXT("Missing ball arena for shot lane")); return true; }
				// Isolate the physical straight shot from moving spring punches beside the spawn pads.
				// Keep the real pawn, health, movement body, attack attribution and collision callbacks.
				const FVector Mark = Rules->FloorCenter + FVector(1480, 1600, 135);
				Target->SetActorLocation(Mark, false, nullptr, ETeleportType::ResetPhysics);
				FVector Aim = Target->GetActorLocation();
				Aim.Z = FMath::Max(Aim.Z + 18., Rules->FloorCenter.Z + Sphere->GetScaledSphereRadius() + 12.);
				FCollisionQueryParams Query(SCENE_QUERY_STAT(BlackHoleBallTestLane), false, Ball);
				Query.AddIgnoredActor(Shooter);
				bool bClear = false;
				for (int32 Lane = 0; Lane < 8 && !bClear; ++Lane)
				{
					Forward = FVector::ForwardVector.RotateAngleAxis(45.f * Lane, FVector::UpVector);
					From = Aim - Forward * 360.f;
					FHitResult First;
					FHitResult Curtain;
					bClear = !CastChecked<AHALContactSpecialBall>(Ball)->FindFieldHit(From, Aim, Curtain)
						&& World->SweepSingleByChannel(First, From, Aim, FQuat::Identity, Sphere->GetCollisionObjectType(),
							FCollisionShape::MakeSphere(Sphere->GetScaledSphereRadius()), Query) && First.GetActor() == Target && !First.bStartPenetrating;
				}
				if (!bClear) { Test.AddError(TEXT("Could not find a direct unobstructed shot at the host")); return true; }
			}
			Ball->SetActorLocation(From, false, nullptr, ETeleportType::TeleportPhysics);
			bShot = Ball->BeginControl(Shooter) && Ball->LaunchFromControl(Shooter, Shooter, Forward * 3800.f);
			if (!bShot) { Test.AddError(TEXT("Formal special shot could not start in match")); return true; }
		}
		if (!bSawBody || !bTerminalCue || Ball || Primaries != 1 || bFieldActive) { return false; }
		if (bMeteor && Id == 1)
		{
			Health = MeteorVictim.Get();
			if (!Health) { return false; }
		}
		const float Expected = InitialHP - (Id == 1 ? ExpectedType->Gameplay.Damage.VehicleHitDamage : 0.f);
		if (!FMath::IsNearlyEqual(Health->GetCurrentHP(), Expected))
		{
			if (bBlackHole && bReportedFieldEnd && World->GetTimeSeconds() - FieldEndTime > 2.)
			{
				Test.AddError(FString::Printf(TEXT("Direct shot: expected %.1f HP after field, observed %.1f"), Expected, Health->GetCurrentHP()));
				bDone = true;
				return true;
			}
			return false;
		}
		UE_LOG(LogTemp, Display, TEXT("Special ball verified Host=%d Round=%u Arrival=1 %s HP=%.1f Primary=%d"),
			bHost, Id, Id == 1 ? TEXT("Impact=1") : TEXT("Expiration=1"), Expected, Primaries);
		bDone = true;
		return true;
	}
};
#endif
