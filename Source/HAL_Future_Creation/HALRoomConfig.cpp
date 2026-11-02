#include "HALRoomConfig.h"
#include "HALVehicleCatalog.h"
#include "Misc/DataValidation.h"

bool UHALRoomConfig::Validate(FString& Error) const
{
	if (!Rules.IsValid() || !Catalog) { Error = TEXT("Room requires an explicit Exhibition/Development profile and a vehicle catalog."); return false; }
	return Catalog->Validate(Error);
}
#if WITH_EDITOR
EDataValidationResult UHALRoomConfig::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context); FString Error;
	if (!Validate(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return EDataValidationResult::Valid;
}
#endif
