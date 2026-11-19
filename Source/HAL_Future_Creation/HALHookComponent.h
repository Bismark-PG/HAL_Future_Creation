#pragma once
#include "Components/ActorComponent.h"
#include "HALHookComponent.generated.h"

class AHALHookThrow;
/** Narrow per-Avatar claim and input bridge. Receivers need no controller or ability grant. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALHookComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALHookComponent();
	bool Claim(AHALHookThrow& Hook);
	void ReleaseClaim(AHALHookThrow& Hook);
	void SetOwnedHook(AHALHookThrow* Hook);
	AHALHookThrow* GetOwnedHook() const { return OwnedHook; }
	AHALHookThrow* GetCapturingHook() const { return CapturingHook; }
	bool BlocksActions() const;
	static bool BlocksActionsOn(const AActor* Actor);
	void ObserveSteering(float Value);
	void NotifyExternalLaunch();
	void ClearForOwnerExit();
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
	UFUNCTION(Server,Reliable) void ServerDirection(FGuid Activation,uint32 Sequence,float Value);
	UPROPERTY(Replicated) TObjectPtr<AHALHookThrow> OwnedHook;
	UPROPERTY(Replicated) TObjectPtr<AHALHookThrow> CapturingHook;
	uint32 NextDirectionSequence=0;
	FGuid LocalChoice;
	float LastSentDirection=2;
};
