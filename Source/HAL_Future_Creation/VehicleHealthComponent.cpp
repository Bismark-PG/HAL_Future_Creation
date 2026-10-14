// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleHealthComponent.h"

#include "AbilitySystemGlobals.h"
#include "HALAbilitySystemComponent.h"
#include "HALHealthAttributeSet.h"
#include "VehicleConfigurationTypes.h"
#include "VehicleConfigurationApplication.h"
#include "UObject/UnrealType.h"

UVehicleHealthComponent::UVehicleHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVehicleHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	BindToAbilitySystem(Cast<UHALAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner())));
}

void UVehicleHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

bool UVehicleHealthComponent::BindToAbilitySystem(UHALAbilitySystemComponent* InAbilitySystem)
{
	if (AbilitySystem.Get() != InAbilitySystem)
	{
		UnbindFromAbilitySystem();
		AbilitySystem = InAbilitySystem;
	}
	if (!InAbilitySystem || InAbilitySystem->GetAvatarActor() != GetOwner()
		|| !InAbilitySystem->GetSet<UHALHealthAttributeSet>() || !VehicleConfiguration::IsReadyForGameplay(GetOwner()))
	{
		return false;
	}
	if (!HasBegunPlay()) { return true; }
	if (GetOwner()->HasAuthority() && !InAbilitySystem->InitializeHealth(MaxHP)) { return false; }
	if (!CurrentHPChangedHandle.IsValid())
	{
		CurrentHPChangedHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(
			UHALHealthAttributeSet::GetCurrentHPAttribute()).AddUObject(this, &ThisClass::HandleAttributeChanged);
		MaxHPChangedHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(
			UHALHealthAttributeSet::GetMaxHPAttribute()).AddUObject(this, &ThisClass::HandleAttributeChanged);
		HealthInitializedHandle = InAbilitySystem->OnHealthInitialized.AddUObject(this, &ThisClass::PublishHealth);
	}
	PublishHealth();
	return true;
}

void UVehicleHealthComponent::UnbindFromAbilitySystem()
{
	if (UHALAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute()).Remove(CurrentHPChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetMaxHPAttribute()).Remove(MaxHPChangedHandle);
		ASC->OnHealthInitialized.Remove(HealthInitializedHandle);
	}
	CurrentHPChangedHandle.Reset();
	MaxHPChangedHandle.Reset();
	HealthInitializedHandle.Reset();
	AbilitySystem.Reset();
}

bool UVehicleHealthComponent::IsHealthReady() const
{
	const UHALAbilitySystemComponent* ASC = AbilitySystem.Get();
	const UHALHealthAttributeSet* Attributes = ASC ? ASC->GetSet<UHALHealthAttributeSet>() : nullptr;
	return ASC && ASC->IsHealthInitialized() && ASC->GetAvatarActor() == GetOwner()
		&& Attributes && Attributes->GetMaxHP() >= 1.0f;
}

float UVehicleHealthComponent::GetCurrentHP() const
{
	if (IsHealthReady())
	{
		const UHALHealthAttributeSet* Attributes = AbilitySystem->GetSet<UHALHealthAttributeSet>();
		return FMath::Clamp(Attributes->GetCurrentHP(), 0.0f, Attributes->GetMaxHP());
	}
	// Editor/pre-game preview only; no independent runtime health or fallback replication.
	return HasBegunPlay() ? 0.0f : MaxHP;
}

float UVehicleHealthComponent::GetMaxHP() const
{
	return IsHealthReady() ? AbilitySystem->GetSet<UHALHealthAttributeSet>()->GetMaxHP() : MaxHP;
}

void UVehicleHealthComponent::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (bLogHealthChanges && Data.Attribute == UHALHealthAttributeSet::GetCurrentHPAttribute() && IsHealthReady())
	{
		UE_LOG(LogTemp, Log, TEXT("Vehicle %s GAS HP: %.1f -> %.1f"),
			*GetNameSafe(GetOwner()), Data.OldValue, GetCurrentHP());
	}
	PublishHealth();
}

void UVehicleHealthComponent::PublishHealth()
{
	if (IsHealthReady())
	{
		OnHealthChanged.Broadcast(GetCurrentHP(), GetMaxHP());
		AbilitySystem->LogHealthStatus();
	}
}

bool UVehicleHealthComponent::ApplyConfiguration(const FVehicleHealthConfig& Config)
{
	FString Error;
	if (HasBegunPlay() || !Config.Validate(Error)) { return false; }
	MaxHP = Config.Health.MaxHP;
	bLogHealthChanges = Config.Debug.bLogHealthChanges;
	return true;
}

#if WITH_EDITOR
bool UVehicleHealthComponent::CanEditChange(const FProperty* Property) const
{
	if (Property && Property->GetOwnerClass() == StaticClass() && VehicleConfiguration::UsesDefinition(GetOwner())) { return false; }
	return Super::CanEditChange(Property);
}
#endif
