#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "../HALRoomComponent.h"
#include "../HALRoomConfig.h"
#include "../HALVehicleCatalog.h"
#include "../HALMovementEffectBridge.h"
#include "../TestVehiclePawn.h"

struct FHALRoomTestAccess
{
	static UHALRoomConfig* Config(UObject* Outer, EHALRoomProfile Profile)
	{
		auto* C = NewObject<UHALRoomConfig>(Outer); C->Rules.Profile = Profile;
		C->Catalog = NewObject<UHALVehicleCatalog>(C);
		FHALVehicleCatalogEntry E; E.VehicleId = TEXT("Car_01");
		E.VehicleClass = TSoftClassPtr<ATestVehiclePawn>(FSoftObjectPath(TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C")));
		C->Catalog->Vehicles.Add(E); return C;
	}
	static const FHALMovementSnapshot& Flush(UHALMovementEffectBridge* Bridge)
	{ Bridge->Flush(Bridge->GetWorld(),LEVELTICK_All,.02f); return Bridge->LastPublished; }
	static bool HasPendingPublication(const UHALMovementEffectBridge* Bridge) { return Bridge->bDirty; }
};
#endif
