// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HALNetworkMenuWidget.generated.h"

/** Blueprint children only arrange the entry form and bind its buttons. */
UCLASS(Abstract, Blueprintable)
class HAL_FUTURE_CREATION_API UHALNetworkMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Network Menu")
	bool HostMatch();

	UFUNCTION(BlueprintCallable, Category = "Network Menu")
	bool JoinMatch(const FString& HostIPv4);

	UFUNCTION(BlueprintPure, Category = "Network Menu")
	FText GetConnectionMessage() const;
};
