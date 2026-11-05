#pragma once
#include "../HALPlayerController.h"
#include "HALRejoinTestController.generated.h"

/** Native regression fixture: observe rejection without disconnecting the transient test World. */
UCLASS(Transient,NotBlueprintable,HideDropdown,NotPlaceable)
class AHALRejoinTestController : public AHALPlayerController
{
	GENERATED_BODY()
public:
	FString ReturnReason;
	virtual void ClientReturnToMainMenuWithTextReason_Implementation(const FText& Reason) override
	{ ReturnReason=Reason.ToString(); }
};
