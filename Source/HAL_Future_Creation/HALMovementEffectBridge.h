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
	/** Binding identity, independent of temporary physics Ready/Recovery. GT-only. */
	FGuid GetBindingEpoch() const { return Epoch; }
	/** GT-only. Preserve all changed-channel recovery requests until one paired snapshot is queued. */
	void SetAreaRecoverySeconds(const FVector& Seconds);
	void SetAreaSway(const FHALMovementAreaSway& Sway);
	void SetOverdriveMotion(const FHALOverdriveMotion& Motion);
	void EndOverdriveMotion();
	/** Receiver-selected Drag flow; published only while GAS grants State.Movement.Drag. */
	void SetDragDrive(const FHALGravityDrive& Field);
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALRoomTestAccess;
	friend struct FHALCameraTestAccess;
#endif
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
	FDelegateHandle StunHandle, BattleTopHandle, DragHandle, WorldHandle;
	FVector AreaRecoverySeconds = FVector::ZeroVector;
	FHALMovementAreaSway AreaSway;
	FHALOverdriveMotion Overdrive;
	FHALGravityDrive DragDrive;
	FHALMovementSnapshot LastPublished;
	int32 BoundPlayerId = INDEX_NONE;
	bool bDirty = false;
	bool bLastFlowBlocked = false;
};
