#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "HALVehicleAbilityTestSupport.h"
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
				if (!W->IsGameWorld() || !FParse::Param(FCommandLine::Get(),TEXT("HALVehicleAbilitiesFixture"))) { return; }
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* Actor)
				{
					auto* Pawn=Cast<ATestVehiclePawn>(Actor); if (!Pawn || !Pawn->GetVehicleDefinition()) { return; }
					const auto Class=Pawn->GetClass()->GetPathName();
					const bool Boost=Class==TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C");
					if (!Boost && Class!=TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_02.BP_VehiclePawn_Car_02_C")) { return; }
					static UVehicleDefinition* Definitions[2]={nullptr,nullptr}; const int32 I=Boost?0:1;
					if (!Definitions[I])
					{
						auto* GA=LoadClass<UGameplayAbility>(nullptr,Boost?TEXT("/Game/Blueprints/GA/GA_MovementBoost.GA_MovementBoost_C")
							:TEXT("/Game/Blueprints/GA/GA_ImpactImmunity.GA_ImpactImmunity_C"));
						Definitions[I]=FHALVehicleAbilityTestAccess::Configure(Pawn,GA,Boost?TEXT("HALCar01AbilityFixture"):TEXT("HALCar02AbilityFixture"));
						Definitions[I]->AddToRoot();
					}
					else { FHALVehicleAbilityTestAccess::UseDefinition(Pawn,Definitions[I]); }
				}));
			});
		}
	} VehicleAbilityNetworkFixture;
}
#endif
