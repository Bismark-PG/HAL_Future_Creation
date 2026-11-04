// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "HALAbilityTypes.h"
#include "HALRoomTypes.h"
#include "HALVehicleLifecycleTypes.h"
#include "UObject/ObjectKey.h"
#include "HALPlayerState.generated.h"

class UHALAbilitySystemComponent;
class UHALHealthAttributeSet;
class UHALCombatAttributeSet;
class UHALImpactAttributeSet;
class UHALMovementAttributeSet;
class ATestVehiclePawn;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHALRoomPlayerChanged);

/** Stable player identity and GAS Owner; the current vehicle is its Avatar. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	AHALPlayerState();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void CopyProperties(APlayerState* Other) override;
	void PrepareForWorldExit();
	void RestoreTravelCounters(uint32 Serial, uint32 Revision, uint32 Round);
	UHALAbilitySystemComponent* GetHALAbilitySystemComponent() const { return AbilitySystem; }
	/** Called after Avatar binding, authority only. Existing specs survive a repeated bind. */
	bool GrantVehicleAbilities();
	bool ValidateVehicleAbilities(const ATestVehiclePawn* Vehicle,FString& Error,const UObject* OutgoingSource=nullptr) const;
	/** Server or owning client only; simulated proxies do not receive owner-only ability specs. */
	bool AreVehicleAbilitiesReady(const ATestVehiclePawn* Vehicle) const;
	/** Explicit authority transitions, fresh Pawn only. Serials are monotonic within this PlayerState. */
	bool ChangeVehicleAvatar(ATestVehiclePawn* Next, uint32 ChangeSerial);
	bool BeginNewRoundAvatar(ATestVehiclePawn* Next, uint32 RoundId, uint32 ChangeSerial);
	bool CanBindVehicleAvatar(const ATestVehiclePawn* Pawn) const;
	const FHALVehicleLifecycleState& GetVehicleLifecycle() const { return Lifecycle; }
	UFUNCTION(BlueprintPure, Category="Room") const FHALRoomPlayerData& GetRoomData() const { return RoomData; }
	UPROPERTY(BlueprintAssignable, Category="Room") FHALRoomPlayerChanged OnRoomDataChanged;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

protected:
	/** Explicit Legacy Player State mode only. Definition V1 ignores this list entirely. */
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Legacy Abilities", meta = (TitleProperty = "InputTag"))
	TArray<FHALAbilityGrant> DefaultAbilities;

private:
	const TArray<FHALAbilityGrant>* ResolveVehicleGrants(const ATestVehiclePawn* Vehicle,FString& Error) const;
	bool TransitionVehicleAvatar(ATestVehiclePawn* Next, uint32 ChangeSerial, uint32 RoundId);
	UFUNCTION() void OnRep_Lifecycle();
	UPROPERTY(ReplicatedUsing=OnRep_Lifecycle) FHALVehicleLifecycleState Lifecycle;
	FObjectKey LastVehicleTransitionKey;
	uint32 LastVehicleTransitionRound = 0;
	bool bVehicleTransitionInProgress = false, bLastVehicleTransitionSucceeded = false;
	friend class UHALRoomComponent;
	void SetRoomData(const FHALRoomPlayerData& Data);
	UFUNCTION() void OnRep_RoomData();
	UPROPERTY(ReplicatedUsing=OnRep_RoomData) FHALRoomPlayerData RoomData;
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UHALAbilitySystemComponent> AbilitySystem;
	UPROPERTY()
	TObjectPtr<UHALHealthAttributeSet> HealthAttributes;
	UPROPERTY()
	TObjectPtr<UHALCombatAttributeSet> CombatAttributes;
	UPROPERTY()
	TObjectPtr<UHALImpactAttributeSet> ImpactAttributes;
	UPROPERTY()
	TObjectPtr<UHALMovementAttributeSet> MovementAttributes;
};
