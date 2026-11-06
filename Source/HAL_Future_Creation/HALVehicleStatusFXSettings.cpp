#include "HALVehicleStatusFXSettings.h"
#include "HALVehicleStatusFXCatalog.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
bool UHALVehicleStatusFXSettings::CollectCookPackages(TArray<FName>& Packages, FString& Error) const
{
	Error.Reset();
	if (DefaultCatalog.IsNull()) { return true; }
	const FString Package = DefaultCatalog.ToSoftObjectPath().GetLongPackageName();
	if (!FPackageName::IsValidLongPackageName(Package) || Package.StartsWith(TEXT("/Engine/Transient")))
	{ Error = TEXT("Default Catalog must point to a saved asset package."); return false; }
	const auto* Catalog = DefaultCatalog.LoadSynchronous();
	if (!Catalog || !Catalog->Validate(Error))
	{
		Error = FString::Printf(TEXT("Default Catalog %s: %s"), *DefaultCatalog.ToString(), Catalog ? *Error : TEXT("missing package or wrong asset class"));
		return false;
	}
	Packages.AddUnique(FName(*Package));
	return true;
}
#endif
