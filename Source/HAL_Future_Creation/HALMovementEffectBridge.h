#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeSet.h"
#include "HALMovementTypes.h"
#include "GameplayTagContainer.h"
#include "HALMovementEffectBridge.generated.h"

class UHALAbilitySystemComponent;
class UArcadeVehicleMovementComponent;
class UNetworkPhysicsComponent;
struct FOnAttributeChangeData;

/** Event-driven GT adapter. GAS owns effects; physics consumes immutable pure values. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALMovementEffectBridge : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALMovementEffectBridge();
	void Bind(UHALAbilitySystemComponent* InASC, int32 PlayerId);
	void Unbind();
	/** GT-only. Preserve all changed-channel recovery requests until one paired snapshot is queued. */
	void SetAreaRecoverySeconds(const FVector& Seconds);
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
	void ConfigurePhysics();
	void AttributeChanged(const FOnAttributeChangeData& Data);
	void StunChanged(FGameplayTag Tag, int32 Count);
	void Flush(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	UFUNCTION() void OnRep_Epoch();
	UPROPERTY(ReplicatedUsing=OnRep_Epoch) FGuid Epoch;
	UPROPERTY(Transient) TObjectPtr<UHALAbilitySystemComponent> ASC;
	UPROPERTY(Transient) TObjectPtr<UArcadeVehicleMovementComponent> Movement;
	UPROPERTY(Transient) TObjectPtr<UNetworkPhysicsComponent> History;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeHandles;
	FDelegateHandle StunHandle, WorldHandle;
	FVector AreaRecoverySeconds = FVector::ZeroVector;
	FHALMovementSnapshot LastPublished;
	int32 BoundPlayerId = INDEX_NONE;
	bool bDirty = false;
};
