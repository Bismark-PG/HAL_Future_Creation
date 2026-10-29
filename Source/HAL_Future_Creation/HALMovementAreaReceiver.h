#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "HALMovementAreaTypes.h"
#include "HALMovementAreaReceiver.generated.h"

class UHALAbilitySystemComponent;

/** Authority-side regional source bookkeeping. No Tick, no physical forces and no player skill grants. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALMovementAreaReceiver : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALMovementAreaReceiver();
	void Bind(UHALAbilitySystemComponent* InASC);
	void Unbind();
	bool RegisterArea(AActor* Source, const FHALMovementAreaConfig& Config);
	void UnregisterArea(AActor* Source);
	int32 GetAreaCount() const { return Sources.Num(); }
	const FHALMovementAreaSelection& GetSelection() const { return Selection; }
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void Rebuild();
	void UpdateEffects();
	UFUNCTION() void SourceEnded(AActor* Actor, EEndPlayReason::Type Reason);
	UPROPERTY(Transient) TObjectPtr<UHALAbilitySystemComponent> ASC;
	TMap<TWeakObjectPtr<AActor>, FHALMovementAreaConfig> Sources;
	FHALMovementAreaSelection Selection;
	FVector AppliedScales = FVector::OneVector;
	FActiveGameplayEffectHandle SlowHandle, SlipperyHandle;
};
