#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HALDamageFeedbackConfig.h"
#include "HALDamageFeedbackComponent.generated.h"

/** Authority combat-result adapter. No HP observer, collision callback, GA Tick or client hit request. */
UCLASS(ClassGroup=(Vehicle))
class HAL_FUTURE_CREATION_API UHALDamageFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALDamageFeedbackComponent();
	void NotifyResolvedDamage(const FGuid& Attack,uint32 Pulse,float ActualDamage);
	void ClearPresentation();
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	uint32 GetReceivedPulseCount() const { return ReceivedCount; }
private:
	UFUNCTION(NetMulticast,Unreliable) void MulticastPulse(FHALDamageFeedbackPulse Pulse);
	uint32 NextSequence=0,LastSequence=0,ReceivedCount=0;
	bool bReceived=false;
};
