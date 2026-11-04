#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "HALCombatTypes.h"
#include "HALAbilityTypes.h"
#include "UObject/ObjectKey.h"
#include "HALAbilitySystemComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FHALHealthInitialized);

/** Actor-owned GAS runtime; initialization survives re-binding a PlayerState's Avatar. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UHALAbilitySystemComponent();
	/** Authority-only. Positive finite game seconds; shorter repeats never shorten stun.
	 * True includes an accepted repeat covered by the existing duration. No client RPC. */
	bool ApplyStun(float DurationSeconds, AActor* SourceActor);
	/** Authority presentation marker; requires an active Stun, follows all subsequent extensions. */
	bool MarkShockedForCurrentStun(AActor* SourceActor);
	bool IsStunned() const;
	/** Shared query for vehicle actions and future item-use execution. */
	static bool IsActorStunned(const AActor* Actor);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	/** Authority-only, idempotent for the same source/class/input. Does not reset active cooldowns. */
	bool GrantAbilities(const TArray<FHALAbilityGrant>& Grants, UObject* Source);
	/** IgnoreSource is only the outgoing Avatar during an explicit transition. */
	bool CanGrantAbilities(const TArray<FHALAbilityGrant>& Grants, const UObject* Source, FString& Error, const UObject* IgnoreSource=nullptr) const;
	bool AreAbilitiesGranted(const TArray<FHALAbilityGrant>& Grants, const UObject* Source) const;
	bool HasAbilityForInput(FGameplayTag InputTag) const;
	/** Uses GAS's own activation RPC; true on a client means a request, not authority acceptance. */
	bool TryActivateAbilityInput(FGameplayTag InputTag);
	virtual bool AreAbilityTagsBlocked(const FGameplayTagContainer& Tags) const override;
	/** Read-only GAS query. -1 means an infinite/unknown source, not zero remaining time. */
	float GetEffectTimeRemaining(FGameplayTag GrantedTag) const;

	/** Authority-only, once per ASC lifetime. Repeated Avatar binding never heals or resets HP. */
	bool InitializeHealth(float SavedMaxHP);
	bool IsHealthInitialized() const { return bHealthInitialized; }
	bool InitializeCombat(const FVehicleCombatConfig& Config);
	bool IsCombatInitialized() const { return bCombatInitialized; }
	bool InitializeImpact(const FHALVehicleImpactConfig& Config);
	bool IsImpactInitialized() const { return bImpactInitialized; }
	EHALCombatConfigVersion GetCombatConfigVersion() const { return CombatConfigVersion; }
	/** An old Pawn cannot clear a newer Pawn's Avatar during delayed cleanup. */
	void ClearAvatarIfCurrent(const AActor* ExpectedAvatar);
	/** Disabled by default; logs on replicated initialization/HP notifications when hal.GASLog=1. */
	void LogHealthStatus() const;
	FHALHealthInitialized OnHealthInitialized;

private:
	friend class AHALPlayerState;
	bool ResetVehicleAttributes(const struct FHALVehicleLifecycleSeed& Seed, bool bNewRound);
public:
	/** Remove only grants owned by this exact source; player-owned shared grants survive. */
	void RemoveAbilitiesFromSource(UObject* Source);
private:
	void StunTagChanged(FGameplayTag Tag, int32 Count);
	FDelegateHandle StunTagHandle;
	void RemoveAvatarEffects();
	void LogAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& Reasons);
	void LogEffectAdded(UAbilitySystemComponent* Target, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
	void LogEffectRemoved(const FActiveGameplayEffect& Effect);
	/** Identity still compares correctly during Pawn destruction when a weak Avatar getter is already null. */
	FObjectKey BoundAvatarKey;
	UFUNCTION()
	void OnRep_HealthInitialized();
	UPROPERTY(ReplicatedUsing = OnRep_HealthInitialized)
	bool bHealthInitialized = false;
	UPROPERTY(Replicated)
	bool bCombatInitialized = false;
	UPROPERTY(Replicated)
	bool bImpactInitialized = false;
	UPROPERTY(Replicated)
	EHALCombatConfigVersion CombatConfigVersion = EHALCombatConfigVersion::LegacyFixedDamage;
};
