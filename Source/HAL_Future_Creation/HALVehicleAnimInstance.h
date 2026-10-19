#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HALVehiclePresentationTypes.h"
#include "HALVehicleAnimInstance.generated.h"

class UHALVehiclePresentationComponent;

/** No gameplay/physics access from AnimGraph. Only this game-thread update pulls the frame. */
UCLASS(Transient, Blueprintable)
class HAL_FUTURE_CREATION_API UHALVehicleAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	FHALVehiclePresentationFrame Presentation;
private:
	TWeakObjectPtr<UHALVehiclePresentationComponent> Source;
};
