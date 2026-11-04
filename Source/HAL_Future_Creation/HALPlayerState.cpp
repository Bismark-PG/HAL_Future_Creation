#include "HALPlayerState.h"

#include "HALAbilitySystemComponent.h"
#include "HALHealthAttributeSet.h"
#include "HALCombatAttributeSet.h"
#include "HALImpactAttributeSet.h"
#include "HALMovementAttributeSet.h"
#include "HALImpactImmunityAbility.h"
#include "HALGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "TestVehiclePawn.h"
#include "VehicleDefinition.h"

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
	MovementAttributes = CreateDefaultSubobject<UHALMovementAttributeSet>(TEXT("MovementAttributes"));
	AbilitySystem->AddAttributeSetSubobject(MovementAttributes.Get());
	FHALAbilityGrant& Primary = DefaultAbilities.AddDefaulted_GetRef();
	Primary.Ability = UHALImpactImmunityAbility::StaticClass();
	Primary.InputTag = HALGameplayTags::Input_Ability_Primary;
}

const TArray<FHALAbilityGrant>* AHALPlayerState::ResolveVehicleGrants(const ATestVehiclePawn* Vehicle,FString& Error) const
{
	if (!Vehicle || Vehicle->GetWorld()!=GetWorld() || !Vehicle->IsConfigurationValid())
	{ Error=TEXT("Vehicle ability source is not a valid current-world vehicle."); return nullptr; }
	const auto& Config=Vehicle->GetVehicleAbilityConfig();
	if (!Config.Validate(Error)) { return nullptr; }
	const auto& Grants=Config.Version==EHALVehicleAbilityVersion::VehicleDefinitionV1 ? Config.Grants : DefaultAbilities;
	return HALAbilities::ValidateGrants(Grants,Error)?&Grants:nullptr;
}
bool AHALPlayerState::ValidateVehicleAbilities(const ATestVehiclePawn* Vehicle,FString& Error,const UObject* OutgoingSource) const
{
	const auto* Grants=ResolveVehicleGrants(Vehicle,Error);
	return Grants && AbilitySystem->CanGrantAbilities(*Grants,Vehicle,Error,OutgoingSource);
}
bool AHALPlayerState::GrantVehicleAbilities()
{
	if (!HasAuthority()) { return false; }
	auto* Vehicle=Cast<ATestVehiclePawn>(AbilitySystem->GetAvatarActor()); FString Error;
	const auto* Grants=ResolveVehicleGrants(Vehicle,Error);
	if (!Vehicle || Vehicle->GetPlayerState()!=this || !Grants)
	{ UE_LOG(LogHALAbility,Error,TEXT("Vehicle ability configuration rejected Player=%d Avatar=%s: %s"),GetPlayerId(),*GetNameSafe(Vehicle),*Error); return false; }
	const bool Granted=AbilitySystem->GrantAbilities(*Grants,Vehicle);
	if (HALAbilities::ShouldLog()) { UE_LOG(LogHALAbility,Log,TEXT("Vehicle abilities Player=%d Avatar=%s Definition=%s Mode=%d Count=%d Granted=%d"),
		GetPlayerId(),*GetNameSafe(Vehicle),*GetNameSafe(Vehicle->GetVehicleDefinition()),int32(Vehicle->GetVehicleAbilityConfig().Version),Grants->Num(),Granted); }
	return Granted;
}
bool AHALPlayerState::AreVehicleAbilitiesReady(const ATestVehiclePawn* Vehicle) const
{
	if (!Vehicle || Vehicle->GetPlayerState()!=this || AbilitySystem->GetAvatarActor()!=Vehicle
		|| (!HasAuthority() && !Vehicle->IsLocallyControlled())) { return false; }
	FString Error; const auto* Grants=ResolveVehicleGrants(Vehicle,Error);
	return Grants && AbilitySystem->AreAbilitiesGranted(*Grants,Vehicle);
}

UAbilitySystemComponent* AHALPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AHALPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AHALPlayerState,RoomData); DOREPLIFETIME(AHALPlayerState,Lifecycle); }
void AHALPlayerState::SetRoomData(const FHALRoomPlayerData& Data)
{ if (HasAuthority()) { RoomData = Data; OnRep_RoomData(); ForceNetUpdate(); } }
void AHALPlayerState::OnRep_RoomData() { OnRoomDataChanged.Broadcast(); }
