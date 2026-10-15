// Copyright Epic Games, Inc. All Rights Reserved.

#include "HALPlayerController.h"
#include "HALImpactAttributeSet.h"
#include "HALImpactReceiverComponent.h"

#include "ArcadeVehicleMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HALMatchGameMode.h"
#include "HALMatchGameState.h"
#include "HALNetworkGameInstance.h"
#include "HALDebugHUDWidget.h"
#include "TestVehiclePawn.h"
#include "EngineUtils.h"
#include "HALAbilitySystemComponent.h"
#include "HALCombatAttributeSet.h"
#include "HALGameplayTags.h"
#include "VehicleHealthComponent.h"

AHALPlayerController::AHALPlayerController()
{
	bAutoManageActiveCameraTarget = true;
}

void AHALPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		if (UHALNetworkGameInstance* Network = GetGameInstance<UHALNetworkGameInstance>())
		{
			Network->ShowMatchLobby(this);
		}
#if !UE_BUILD_SHIPPING
		DebugHUD = CreateWidget<UHALDebugHUDWidget>(this, UHALDebugHUDWidget::StaticClass());
		if (DebugHUD)
		{
			DebugHUD->SetPositionInViewport(FVector2D(12.0f, 12.0f));
			DebugHUD->AddToPlayerScreen(100);
		}
#endif
	}
}

void AHALPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !UE_BUILD_SHIPPING
	for (const auto& WeakASC : DebugImpactImmuneTargets)
	{
		if (auto* ASC = WeakASC.Get())
		{
			ASC->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune, 1, EGameplayTagReplicationState::TagOnly);
		}
	}
	DebugImpactImmuneTargets.Empty();
#endif
	if (DebugHUD)
	{
		DebugHUD->RemoveFromParent();
		DebugHUD = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AHALPlayerController::RequestStartMatch()
{
	if (!IsLocalController())
	{
		return;
	}

	if (HasAuthority())
	{
		if (AHALMatchGameMode* MatchMode = GetWorld()->GetAuthGameMode<AHALMatchGameMode>())
		{
			MatchMode->TryStartMatch(this);
		}
	}
	else
	{
		ServerRequestStartMatch();
	}
}

void AHALPlayerController::HALStartMatch()
{
	RequestStartMatch();
}

void AHALPlayerController::HALMatchStatus()
{
	const AHALMatchGameState* MatchState = GetWorld()->GetGameState<AHALMatchGameState>();
	const TCHAR* PhaseName = !MatchState ? TEXT("Unavailable")
		: MatchState->GetMatchPhase() == EHALMatchPhase::Playing ? TEXT("Playing") : TEXT("WaitingForPlayers");
	const FString Status = FString::Printf(
		TEXT("HAL Match: Local=%d Authority=%d Pawn=%s PlayerId=%d Phase=%s"),
		IsLocalController() ? 1 : 0,
		HasAuthority() ? 1 : 0,
		*GetNameSafe(GetPawn()),
		PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE,
		PhaseName);
	UE_LOG(LogTemp, Display, TEXT("%s"), *Status);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, Status);
	}
}

void AHALPlayerController::HALNetMetrics()
{
	const ATestVehiclePawn* Vehicle = Cast<ATestVehiclePawn>(GetPawn());
	const UArcadeVehicleMovementComponent* Movement = Vehicle
		? Vehicle->FindComponentByClass<UArcadeVehicleMovementComponent>() : nullptr;
	const FString Status = FString::Printf(TEXT("HAL Net: Pawn=%s Local=%d PhysicsFrame=%d ResimRuns=%d"),
		*GetNameSafe(Vehicle), IsLocalController() ? 1 : 0,
		Movement ? Movement->GetLastPhysicsFrame() : INDEX_NONE,
		Movement ? Movement->GetResimulationRunCount() : 0);
	UE_LOG(LogTemp, Display, TEXT("%s"), *Status);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, Status);
	}
}

void AHALPlayerController::ServerRequestStartMatch_Implementation()
{
	if (AHALMatchGameMode* MatchMode = GetWorld()->GetAuthGameMode<AHALMatchGameMode>())
	{
		MatchMode->TryStartMatch(this);
	}
}

void AHALPlayerController::HALGASStatus()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const UVehicleHealthComponent* Health = It->FindComponentByClass<UVehicleHealthComponent>();
		if (!Health) { continue; }
		const UHALAbilitySystemComponent* ASC = Health->GetBoundAbilitySystem();
		const UHALCombatAttributeSet* Combat = ASC ? ASC->GetSet<UHALCombatAttributeSet>() : nullptr;
		const APawn* VehiclePawn = Cast<APawn>(*It);
		const APlayerState* State = VehiclePawn ? VehiclePawn->GetPlayerState() : nullptr;
		FString Status = FString::Printf(
			TEXT("HAL GAS: Vehicle=%s PlayerId=%d Owner=%s Avatar=%s Authority=%d Ready=%d HP=%.1f MaxHP=%.1f Mode=%s"),
			*GetNameSafe(*It), State ? State->GetPlayerId() : INDEX_NONE, *GetNameSafe(ASC ? ASC->GetOwnerActor() : nullptr),
			*GetNameSafe(ASC ? ASC->GetAvatarActor() : nullptr), It->HasAuthority() ? 1 : 0,
			Health->IsHealthReady() ? 1 : 0, Health->GetCurrentHP(), Health->GetMaxHP(),
			!ASC ? TEXT("Unavailable") : ASC->ReplicationMode == EGameplayEffectReplicationMode::Mixed
				? TEXT("Mixed") : ASC->ReplicationMode == EGameplayEffectReplicationMode::Minimal ? TEXT("Minimal") : TEXT("Full"));
		Status += FString::Printf(TEXT(" CombatReady=%d CombatVersion=%d Attack=%.2f OutDelta=%.2f Taken=%.2f DamageImmune=%d"),
			ASC && ASC->IsCombatInitialized() ? 1 : 0, ASC ? int32(ASC->GetCombatConfigVersion()) : -1,
			Combat ? Combat->GetAttackPower() : 0.0f, Combat ? Combat->GetOutputMultiplierDelta() : 0.0f,
			Combat ? Combat->GetDamageTakenMultiplier() : 0.0f,
			ASC && ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_DamageImmune) ? 1 : 0);
		const UHALImpactAttributeSet* Impact = ASC ? ASC->GetSet<UHALImpactAttributeSet>() : nullptr;
		const UHALImpactReceiverComponent* Receiver = It->FindComponentByClass<UHALImpactReceiverComponent>();
		const FHALImpactSummary Last = Receiver ? Receiver->GetLastImpact() : FHALImpactSummary();
		Status += FString::Printf(TEXT(" ImpactReady=%d ImpactVersion=%d Reduction=%.0f ImpactImmune=%d LastTier=%d->%d Drop=%d ImpactFrame=%d"),
			ASC && ASC->IsImpactInitialized(), Receiver ? int32(Receiver->GetConfiguration().Version) : -1,
			Impact ? Impact->GetTierReduction() : 0, ASC && ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune),
			int32(Last.SourceTier), int32(Last.EffectiveTier), Last.bBallDropped, Last.ServerPhysicsFrame);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Status);
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Green, Status); }
	}
}

void AHALPlayerController::HALImpactDebug(int32 PlayerId, int32 Reduction, bool bImmune)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() || !IsLocalController() || Reduction < 0 || Reduction > 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("HALImpactDebug: use the host console; args: PlayerId Reduction(0..3) Immune(0/1)."));
		return;
	}
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APlayerState* State = It->GetPlayerState();
		const UVehicleHealthComponent* Health = It->FindComponentByClass<UVehicleHealthComponent>();
		UHALAbilitySystemComponent* ASC = Health ? Health->GetBoundAbilitySystem() : nullptr;
		if (!State || State->GetPlayerId() != PlayerId || !ASC || !ASC->IsImpactInitialized()) { continue; }
		ASC->SetNumericAttributeBase(UHALImpactAttributeSet::GetTierReductionAttribute(), Reduction);
		const bool bAdded = DebugImpactImmuneTargets.Contains(ASC);
		if (bImmune && !bAdded)
		{
			ASC->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune, 1, EGameplayTagReplicationState::TagOnly);
			DebugImpactImmuneTargets.Add(ASC);
		}
		else if (!bImmune && bAdded)
		{
			ASC->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune, 1, EGameplayTagReplicationState::TagOnly);
			DebugImpactImmuneTargets.Remove(ASC);
		}
		ASC->GetOwner()->ForceNetUpdate();
		HALGASStatus();
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("HALImpactDebug: no ready Pawn with PlayerId=%d."), PlayerId);
#endif
}
