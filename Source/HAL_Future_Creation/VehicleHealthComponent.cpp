// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleHealthComponent.h"
#include "VehicleConfigurationTypes.h"
#include "VehicleConfigurationApplication.h"
#include "UObject/UnrealType.h"

#include "Net/UnrealNetwork.h"

UVehicleHealthComponent::UVehicleHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UVehicleHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		CurrentHP = FMath::Max(1.0f, MaxHP);
		GetOwner()->ForceNetUpdate();
	}
	OnHealthChanged.Broadcast(CurrentHP, MaxHP);
}

void UVehicleHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVehicleHealthComponent, CurrentHP);
}

bool UVehicleHealthComponent::ApplyResolvedDamage(const float Damage)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !FMath::IsFinite(Damage)
		|| Damage <= 0.0f || CurrentHP <= 0.0f)
	{
		return false;
	}

	const float PreviousHP = CurrentHP;
	CurrentHP = FMath::Clamp(CurrentHP - Damage, 0.0f, FMath::Max(1.0f, MaxHP));
	GetOwner()->ForceNetUpdate();
	OnHealthChanged.Broadcast(CurrentHP, MaxHP);

	if (bLogHealthChanges)
	{
		UE_LOG(LogTemp, Log, TEXT("Vehicle %s HP: %.1f -> %.1f"),
			*GetNameSafe(GetOwner()), PreviousHP, CurrentHP);
	}
	return true;
}

void UVehicleHealthComponent::OnRep_CurrentHP()
{
	OnHealthChanged.Broadcast(CurrentHP, MaxHP);
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
