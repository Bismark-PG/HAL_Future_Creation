#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "HALHealingAreaEffect.h"
#include "HALHealingAreaReceiver.generated.h"

class UHALAbilitySystemComponent;
struct FOnAttributeChangeData;

/** Authority-only area source aggregation. Strongest HP/second wins; no Tick or independent HP store. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALHealingAreaReceiver : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALHealingAreaReceiver();
	void Bind(UHALAbilitySystemComponent* InASC);
	void Unbind();
	void ClearForOwnerExit();
	bool RegisterArea(AActor* Source, const FHALHealingAreaRecipe& Recipe);
	void UnregisterArea(AActor* Source);
	int32 GetAreaCount() const { return Sources.Num(); }
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	bool CanHeal() const;
	void Rebuild();
	void HealthChanged(const FOnAttributeChangeData& Data);
	UFUNCTION() void SourceEnded(AActor* Actor, EEndPlayReason::Type Reason);
	UPROPERTY(Transient) TObjectPtr<UHALAbilitySystemComponent> ASC;
	TMap<TWeakObjectPtr<AActor>, FHALHealingAreaRecipe> Sources;
	FHALHealingAreaRecipe AppliedRecipe;
	FActiveGameplayEffectHandle EffectHandle;
	FDelegateHandle HealthChangedHandle;
};
