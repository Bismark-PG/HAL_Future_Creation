#include "HALPlayerState.h"

#include "HALAbilitySystemComponent.h"
#include "HALHealthAttributeSet.h"

AHALPlayerState::AHALPlayerState()
{
	AbilitySystem = CreateDefaultSubobject<UHALAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	HealthAttributes = CreateDefaultSubobject<UHALHealthAttributeSet>(TEXT("HealthAttributes"));
	AbilitySystem->AddAttributeSetSubobject(HealthAttributes.Get());
}

UAbilitySystemComponent* AHALPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}
