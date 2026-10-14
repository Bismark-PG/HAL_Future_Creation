#include "HALPlayerState.h"

#include "HALAbilitySystemComponent.h"
#include "HALHealthAttributeSet.h"
#include "HALCombatAttributeSet.h"

AHALPlayerState::AHALPlayerState()
{
	AbilitySystem = CreateDefaultSubobject<UHALAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	HealthAttributes = CreateDefaultSubobject<UHALHealthAttributeSet>(TEXT("HealthAttributes"));
	AbilitySystem->AddAttributeSetSubobject(HealthAttributes.Get());
	CombatAttributes = CreateDefaultSubobject<UHALCombatAttributeSet>(TEXT("CombatAttributes"));
	AbilitySystem->AddAttributeSetSubobject(CombatAttributes.Get());
}

UAbilitySystemComponent* AHALPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}
