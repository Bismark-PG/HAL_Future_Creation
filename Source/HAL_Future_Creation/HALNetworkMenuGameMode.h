// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HALNetworkMenuGameMode.generated.h"

/** Empty local entry map: no vehicle or match state exists before hosting/joining. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALNetworkMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHALNetworkMenuGameMode();
};
