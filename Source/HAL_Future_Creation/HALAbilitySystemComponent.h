#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "HALCombatTypes.h"
#include "HALAbilitySystemComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FHALHealthInitialized);

/** Actor-owned GAS runtime; initialization survives re-binding a PlayerState's Avatar. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UHALAbilitySystemComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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
