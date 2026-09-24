// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "HALMatchGameState.generated.h"

UENUM(BlueprintType)
enum class EHALMatchPhase : uint8
{
	WaitingForPlayers,
	Playing
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHALMatchPhaseChanged, EHALMatchPhase, NewPhase);

/** Replicated public match phase. Only the authoritative GameMode changes it. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALMatchGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	EHALMatchPhase GetMatchPhase() const { return MatchPhase; }
	void SetMatchPhase(EHALMatchPhase NewPhase);

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FHALMatchPhaseChanged OnMatchPhaseChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase, BlueprintReadOnly, Category = "Match")
	EHALMatchPhase MatchPhase = EHALMatchPhase::WaitingForPlayers;

	UFUNCTION()
	void OnRep_MatchPhase();
};
