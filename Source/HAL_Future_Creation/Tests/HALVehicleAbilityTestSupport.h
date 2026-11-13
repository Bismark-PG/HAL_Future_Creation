#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "../TestVehiclePawn.h"
#include "../VehicleDefinition.h"
#include "../HALGameplayTags.h"
#include "../HALMovementBoostAbility.h"
#include "../HALImpactImmunityAbility.h"
#include "UObject/UnrealType.h"

struct FHALVehicleAbilityTestAccess
{
	static constexpr float BattleTopNetworkSpinRate = 540.f;
	static FHALVehicleAbilityConfig Config(TSubclassOf<UGameplayAbility> Ability)
	{
		FHALVehicleAbilityConfig C; C.Version=EHALVehicleAbilityVersion::VehicleDefinitionV1;
		auto& G=C.Grants.AddDefaulted_GetRef(); G.Ability=Ability; G.InputTag=HALGameplayTags::Input_Ability_Primary; return C;
	}
	static void UseDefinition(ATestVehiclePawn* Pawn,UVehicleDefinition* Definition)
	{ FindFProperty<FObjectProperty>(Pawn->GetClass(),TEXT("Definition"))->SetObjectPropertyValue_InContainer(Pawn,Definition); }
	static UVehicleDefinition* Configure(ATestVehiclePawn* Pawn,TSubclassOf<UGameplayAbility> Ability,FName Name=NAME_None)
	{
		// Only a spawned instance receives a transient copy. Never modifies a BP/CDO or saved DA.
		auto* D=DuplicateObject<UVehicleDefinition>(Pawn->GetVehicleDefinition(),GetTransientPackage(),Name);
		D->SetFlags(RF_Transient); D->ClearFlags(RF_Public|RF_Standalone); D->Abilities=Config(Ability); UseDefinition(Pawn,D); return D;
	}
	static void RuntimeConfig(ATestVehiclePawn* Pawn,const FHALVehicleAbilityConfig& C) { Pawn->RuntimeAbilities=C; }
};
#endif
