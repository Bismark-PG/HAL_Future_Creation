#include "HALRoomConfig.h"
#include "HALVehicleCatalog.h"
#include "Misc/DataValidation.h"

bool UHALRoomConfig::Validate(FString& Error) const
{
	if (!Rules.IsValid() || !Catalog) { Error = TEXT("Room requires an explicit Exhibition/Development profile and a vehicle catalog."); return false; }
	if (bEnableRoundTravel && (MatchMap.IsNull() || !FMath::IsFinite(CountdownSeconds) || CountdownSeconds<.5f || CountdownSeconds>10
		|| !FMath::IsFinite(LoadTimeoutSeconds) || LoadTimeoutSeconds<1 || LoadTimeoutSeconds>180))
	{ Error=TEXT("Round travel requires a match map, countdown 0.5-10s and load timeout 1-180s."); return false; }
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
