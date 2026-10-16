#include "HALPlayerState.h"

#include "HALAbilitySystemComponent.h"
#include "HALHealthAttributeSet.h"
#include "HALCombatAttributeSet.h"
#include "HALImpactAttributeSet.h"
#include "HALImpactImmunityAbility.h"
#include "HALGameplayTags.h"

AHALPlayerState::AHALPlayerState()
{
	AbilitySystem = CreateDefaultSubobject<UHALAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	HealthAttributes = CreateDefaultSubobject<UHALHealthAttributeSet>(TEXT("HealthAttributes"));
	AbilitySystem->AddAttributeSetSubobject(HealthAttributes.Get());
	CombatAttributes = CreateDefaultSubobject<UHALCombatAttributeSet>(TEXT("CombatAttributes"));
	AbilitySystem->AddAttributeSetSubobject(CombatAttributes.Get());
	ImpactAttributes = CreateDefaultSubobject<UHALImpactAttributeSet>(TEXT("ImpactAttributes"));
	AbilitySystem->AddAttributeSetSubobject(ImpactAttributes.Get());
	FHALAbilityGrant& Primary = DefaultAbilities.AddDefaulted_GetRef();
	Primary.Ability = UHALImpactImmunityAbility::StaticClass();
	Primary.InputTag = HALGameplayTags::Input_Ability_Primary;
}

void AHALPlayerState::GrantVehicleAbilities()
{
	if (HasAuthority()) { AbilitySystem->GrantAbilities(DefaultAbilities, this); }
}

UAbilitySystemComponent* AHALPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}
