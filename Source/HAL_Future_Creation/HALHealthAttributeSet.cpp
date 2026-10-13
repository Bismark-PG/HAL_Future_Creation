#include "HALHealthAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

void UHALHealthAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALHealthAttributeSet, MaxHP, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALHealthAttributeSet, CurrentHP, COND_None, REPNOTIFY_Always);
}

void UHALHealthAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetMaxHPAttribute())
	{
		NewValue = FMath::IsFinite(NewValue) ? FMath::Max(1.0f, NewValue) : FMath::Max(1.0f, GetMaxHP());
	}
	else if (Attribute == GetCurrentHPAttribute())
	{
		NewValue = FMath::IsFinite(NewValue) ? FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxHP())) : GetCurrentHP();
	}
}

void UHALHealthAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UHALHealthAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UHALHealthAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (Attribute == GetMaxHPAttribute() && ASC && ASC->GetOwner() && ASC->GetOwner()->HasAuthority()
		&& GetCurrentHP() > NewValue)
	{
		ASC->SetNumericAttributeBase(GetCurrentHPAttribute(), NewValue);
	}
}

void UHALHealthAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	if (Data.EvaluatedData.Attribute == GetCurrentHPAttribute())
	{
		SetCurrentHP(FMath::Clamp(GetCurrentHP(), 0.0f, FMath::Max(0.0f, GetMaxHP())));
	}
}

void UHALHealthAttributeSet::OnRep_MaxHP(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHALHealthAttributeSet, MaxHP, OldValue);
}

void UHALHealthAttributeSet::OnRep_CurrentHP(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHALHealthAttributeSet, CurrentHP, OldValue);
}
