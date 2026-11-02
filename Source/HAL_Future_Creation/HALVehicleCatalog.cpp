#include "HALVehicleCatalog.h"
#include "TestVehiclePawn.h"
#include "VehicleDefinition.h"
#include "Misc/DataValidation.h"

UClass* UHALVehicleCatalog::Resolve(FName Id, FString& Error) const
{
	const auto* Entry = Vehicles.FindByPredicate([Id](const FHALVehicleCatalogEntry& E) { return E.VehicleId == Id; });
	if (Id.IsNone() || !Entry) { Error = TEXT("Vehicle ID is not in the server catalog."); return nullptr; }
	UClass* Class = Entry->VehicleClass.LoadSynchronous();
	const auto* Pawn = Class && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
		? Cast<ATestVehiclePawn>(Class->GetDefaultObject()) : nullptr;
	const auto* Definition = Pawn ? Pawn->GetVehicleDefinition() : nullptr;
	if (!Pawn || Pawn->GetConfigurationSource() != EVehicleConfigurationSource::Definition || !Definition
		|| Definition->CollisionShape != EVehicleCollisionShape::ConvexMesh || !Definition->Validate(Error)
		|| Definition->Impact.Version != EHALImpactVersion::TieredV1)
	{
		Error = FString::Printf(TEXT("Vehicle %s requires a concrete driving BP with a valid, migrated Definition. %s"),*Id.ToString(),*Error);
		return nullptr;
	}
	return Class;
}
bool UHALVehicleCatalog::Validate(FString& Error) const
{
	if (Vehicles.IsEmpty()) { Error = TEXT("Vehicle catalog must contain at least one entry."); return false; }
	TSet<FName> Ids;
	for (const auto& E : Vehicles)
	{
		if (E.VehicleId.IsNone() || Ids.Contains(E.VehicleId)) { Error = TEXT("Vehicle IDs must be nonempty and unique (case-insensitive)."); return false; }
		Ids.Add(E.VehicleId);
		if (!Resolve(E.VehicleId,Error)) { return false; }
	}
	return true;
}
#if WITH_EDITOR
EDataValidationResult UHALVehicleCatalog::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context); FString Error;
	if (!Validate(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return EDataValidationResult::Valid;
}
#endif
