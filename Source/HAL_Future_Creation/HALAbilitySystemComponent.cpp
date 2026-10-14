#include "HALAbilitySystemComponent.h"

#include "HAL/IConsoleManager.h"
#include "HALHealthAttributeSet.h"
#include "HALCombatAttributeSet.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogHALGAS, Log, All);
static TAutoConsoleVariable<int32> CVarHALGASLog(TEXT("hal.GASLog"), 0,
	TEXT("Log authority health initialization and Avatar cleanup. 0=off, 1=on."));

UHALAbilitySystemComponent::UHALAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
}

void UHALAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHALAbilitySystemComponent, bHealthInitialized);
	DOREPLIFETIME(UHALAbilitySystemComponent, bCombatInitialized);
	DOREPLIFETIME(UHALAbilitySystemComponent, CombatConfigVersion);
}

bool UHALAbilitySystemComponent::InitializeCombat(const FVehicleCombatConfig& Config)
{
	FString Error;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Config.Validate(Error)
		|| !GetSet<UHALCombatAttributeSet>()) { return false; }
	if (bCombatInitialized) { return true; }
	CombatConfigVersion = Config.Version;
	const bool bMigrated = Config.Version == EHALCombatConfigVersion::AttackSnapshotV1;
	// Legacy assets never silently become scaled-attack configurations.
	SetNumericAttributeBase(UHALCombatAttributeSet::GetAttackPowerAttribute(), bMigrated ? Config.AttackPower : 0.0f);
	SetNumericAttributeBase(UHALCombatAttributeSet::GetOutputMultiplierDeltaAttribute(), bMigrated ? Config.OutputMultiplierDelta : 0.0f);
	SetNumericAttributeBase(UHALCombatAttributeSet::GetDamageTakenMultiplierAttribute(), bMigrated ? Config.DamageTakenMultiplier : 1.0f);
	bCombatInitialized = true;
	GetOwner()->ForceNetUpdate();
	return true;
}

bool UHALAbilitySystemComponent::InitializeHealth(float SavedMaxHP)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !FMath::IsFinite(SavedMaxHP) || SavedMaxHP < 1.0f
		|| !GetSet<UHALHealthAttributeSet>())
	{
		return false;
	}
	if (bHealthInitialized) { return true; }
	SetNumericAttributeBase(UHALHealthAttributeSet::GetMaxHPAttribute(), SavedMaxHP);
	SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(), SavedMaxHP);
	bHealthInitialized = true;
	GetOwner()->ForceNetUpdate();
	OnHealthInitialized.Broadcast();
	if (CVarHALGASLog.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogHALGAS, Log, TEXT("Health initialized Owner=%s Avatar=%s MaxHP=%.1f"),
			*GetNameSafe(GetOwner()), *GetNameSafe(GetAvatarActor()), SavedMaxHP);
	}
	return true;
}

void UHALAbilitySystemComponent::ClearAvatarIfCurrent(const AActor* ExpectedAvatar)
{
	if (!ExpectedAvatar || GetAvatarActor() != ExpectedAvatar) { return; }
	CancelAllAbilities();
	InitAbilityActorInfo(GetOwner(), nullptr);
	if (CVarHALGASLog.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogHALGAS, Log, TEXT("Avatar cleared Owner=%s FormerAvatar=%s"),
			*GetNameSafe(GetOwner()), *GetNameSafe(ExpectedAvatar));
	}
}

void UHALAbilitySystemComponent::OnRep_HealthInitialized()
{
	OnHealthInitialized.Broadcast();
}

void UHALAbilitySystemComponent::LogHealthStatus() const
{
	const UHALHealthAttributeSet* Attributes = GetSet<UHALHealthAttributeSet>();
	if (Attributes && CVarHALGASLog.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogHALGAS, Log, TEXT("Health state Owner=%s Avatar=%s Authority=%d HP=%.1f MaxHP=%.1f"),
			*GetNameSafe(GetOwnerActor()), *GetNameSafe(GetAvatarActor()), GetOwner()->HasAuthority() ? 1 : 0,
			Attributes->GetCurrentHP(), Attributes->GetMaxHP());
	}
}
