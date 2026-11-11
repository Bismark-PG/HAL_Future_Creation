#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "HALVehicleAbilityTestSupport.h"
#include "../HALBattleTopAbility.h"
#include "../HALSlipperyTrailAbility.h"
#include "Engine/World.h"
#include "Materials/Material.h"
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
				const bool bTrail = FParse::Param(FCommandLine::Get(), TEXT("HALSlipperyTrailFixture"));
				if (!bBattleTop && !bTrail && !FParse::Param(FCommandLine::Get(),TEXT("HALVehicleAbilitiesFixture"))) { return; }
				if (bTrail)
				{
					auto* Ability = GetMutableDefault<UHALSlipperyTrailAbility>();
					auto* Property = FindFProperty<FObjectProperty>(Ability->GetClass(), TEXT("SkillConfig"));
					auto* Config = NewObject<UHALSlipperyTrailConfig>(Ability);
					Config->Shape.InitialHalfWidth = 750;
					Config->Shape.MaximumHalfWidth = 900;
					Config->Shape.LifetimeSeconds = 4;
					Config->Emission.EmissionSeconds = 1.5f;
					if (FParse::Param(FCommandLine::Get(), TEXT("HALSlipperyTrailGroundFixture")))
					{
						Config->Presentation.GroundMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
						Config->Shape.bDrawOutline = false;
						Config->Presentation.Deformation.bEnabled = FParse::Param(FCommandLine::Get(), TEXT("HALGroundSurfaceFixture"));
					}
					if (FParse::Param(FCommandLine::Get(), TEXT("HALSlipperyTrailDissolveFixture")))
					{
						Config->Presentation.DissolveSeconds = 1.2f;
						Config->Presentation.GroundMaterial = LoadObject<UMaterialInterface>(nullptr,
							TEXT("/Game/Assets/FX/Materials/MI_SlipperyTrail_Graffiti.MI_SlipperyTrail_Graffiti"));
					}
					Property->SetObjectPropertyValue_InContainer(Ability, Config);
				}
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
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([bBattleTop, bTrail](AActor* Actor)
				{
					auto* Pawn=Cast<ATestVehiclePawn>(Actor); if (!Pawn || !Pawn->GetVehicleDefinition()) { return; }
					const auto Class=Pawn->GetClass()->GetPathName();
					const bool Boost=Class==TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C");
					const bool bCar03=Class==TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_03.BP_VehiclePawn_Car_03_C");
					const bool bCar04=Class==TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_04.BP_VehiclePawn_Car_04_C");
					if (!Boost && Class!=TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_02.BP_VehiclePawn_Car_02_C")
						&& !((bBattleTop || bTrail) && bCar03) && !(bTrail && bCar04)) { return; }
					static UVehicleDefinition* Definitions[4]={nullptr,nullptr,nullptr,nullptr}; const int32 I=bCar04?3:bCar03?2:Boost?0:1;
					if (!Definitions[I])
					{
						auto* GA=bTrail ? UHALSlipperyTrailAbility::StaticClass() : bBattleTop ? UHALBattleTopAbility::StaticClass() : LoadClass<UGameplayAbility>(nullptr,Boost?TEXT("/Game/Blueprints/GA/GA_MovementBoost.GA_MovementBoost_C")
							:TEXT("/Game/Blueprints/GA/GA_ImpactImmunity.GA_ImpactImmunity_C"));
						const FName Name = bCar04 ? TEXT("HALCar04AbilityFixture") : bCar03 ? TEXT("HALCar03AbilityFixture") : Boost ? TEXT("HALCar01AbilityFixture") : TEXT("HALCar02AbilityFixture");
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
