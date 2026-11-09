#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "HALVehicleAbilityTestSupport.h"
#include "../HALBattleTopAbility.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

namespace
{
	// Opt-in test fixture: replaces only spawned instances, never saved assets or BP CDOs.
	struct FVehicleAbilityNetworkFixture
	{
		FVehicleAbilityNetworkFixture()
		{
			FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* W,const UWorld::InitializationValues&)
			{
				if (!W->IsGameWorld()) { return; }
				const bool bBattleTop = FParse::Param(FCommandLine::Get(), TEXT("HALBattleTopFixture"));
				if (!bBattleTop && !FParse::Param(FCommandLine::Get(),TEXT("HALVehicleAbilitiesFixture"))) { return; }
				if (bBattleTop)
				{
					auto* Ability = GetMutableDefault<UHALBattleTopAbility>();
					// Process-scoped native test defaults. No Blueprint defaults or assets are saved.
					auto* Property = FindFProperty<FObjectProperty>(Ability->GetClass(), TEXT("SkillConfig"));
					if (!Property->GetObjectPropertyValue_InContainer(Ability))
					{
						auto* Config = NewObject<UHALBattleTopConfig>(Ability);
						Config->SpinRate = FHALVehicleAbilityTestAccess::BattleTopNetworkSpinRate;
						Property->SetObjectPropertyValue_InContainer(Ability, Config);
					}
				}
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([bBattleTop](AActor* Actor)
				{
					auto* Pawn=Cast<ATestVehiclePawn>(Actor); if (!Pawn || !Pawn->GetVehicleDefinition()) { return; }
					const auto Class=Pawn->GetClass()->GetPathName();
					const bool Boost=Class==TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C");
					const bool bCar03=Class==TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_03.BP_VehiclePawn_Car_03_C");
					if (!Boost && Class!=TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_02.BP_VehiclePawn_Car_02_C")
						&& !(bBattleTop && bCar03)) { return; }
					static UVehicleDefinition* Definitions[3]={nullptr,nullptr,nullptr}; const int32 I=bCar03?2:Boost?0:1;
					if (!Definitions[I])
					{
						auto* GA=bBattleTop ? UHALBattleTopAbility::StaticClass() : LoadClass<UGameplayAbility>(nullptr,Boost?TEXT("/Game/Blueprints/GA/GA_MovementBoost.GA_MovementBoost_C")
							:TEXT("/Game/Blueprints/GA/GA_ImpactImmunity.GA_ImpactImmunity_C"));
						const FName Name = bCar03 ? TEXT("HALCar03AbilityFixture") : Boost ? TEXT("HALCar01AbilityFixture") : TEXT("HALCar02AbilityFixture");
						Definitions[I]=FHALVehicleAbilityTestAccess::Configure(Pawn,GA,Name);
						Definitions[I]->AddToRoot();
					}
					else { FHALVehicleAbilityTestAccess::UseDefinition(Pawn,Definitions[I]); }
				}));
			});
		}
	} VehicleAbilityNetworkFixture;
}
#endif
