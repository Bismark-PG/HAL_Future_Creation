// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "HALPlayerStart.generated.h"

/** A stable, editor-authored spawn slot for up to four players. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Match|Spawn", meta = (ClampMin = "0", ClampMax = "3"))
	int32 SpawnSlot = 0;
};
