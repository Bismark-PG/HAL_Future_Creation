#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HALShockedPresentationComponent.generated.h"

class UHALAbilitySystemComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHALShockedChanged, bool, bActive);

/** Event-only visual hook; no physics, ability restrictions, asset writes or tick. */
UCLASS(ClassGroup=(Presentation))
class HAL_FUTURE_CREATION_API UHALShockedPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALShockedPresentationComponent();
	void Bind(UHALAbilitySystemComponent* InASC);
	void Unbind();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UFUNCTION(BlueprintPure, Category="Shocked") bool IsShocked() const { return bShocked; }
	UPROPERTY(BlueprintAssignable, Category="Shocked") FHALShockedChanged OnShockedChanged;
private:
	void TagChanged(FGameplayTag Tag, int32 Count);
	void SetShocked(bool bActive);
	UPROPERTY(Transient) TObjectPtr<UHALAbilitySystemComponent> ASC;
	FDelegateHandle TagHandle;
	bool bShocked = false;
};
