// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HALDebugHUDWidget.generated.h"

class STextBlock;
class SBox;

/** Read-only local diagnostics. Native layout needs no Blueprint asset or gameplay Tick. */
UCLASS(NotBlueprintable)
class HAL_FUTURE_CREATION_API UHALDebugHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void RefreshStatus();
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SBox> StatusPanel;
	float CachedViewportScale = 0.0f;
	FTimerHandle RefreshTimer;
};
