#include "HALVehicleStatusFXSettings.h"
#include "HALVehicleStatusFXCatalog.h"
#include "HALDamageFeedbackConfig.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
bool UHALVehicleStatusFXSettings::CollectCookPackages(TArray<FName>& Packages, FString& Error) const
{
	Error.Reset();
	TArray<FName> Collected;
	if(!DamageFeedbackConfig.IsNull())
	{
		const FString DamagePackage=DamageFeedbackConfig.ToSoftObjectPath().GetLongPackageName();
		const auto* Damage=FPackageName::IsValidLongPackageName(DamagePackage)?DamageFeedbackConfig.LoadSynchronous():nullptr;
		if(!Damage||!Damage->Validate(Error)) { Error=TEXT("Damage Feedback Config: ")+(Damage?Error:TEXT("missing saved asset or wrong class")); return false; }
		Collected.AddUnique(FName(*DamagePackage));
	}
	if (DefaultCatalog.IsNull()) { for(FName P:Collected) { Packages.AddUnique(P); } return true; }
	const FString Package = DefaultCatalog.ToSoftObjectPath().GetLongPackageName();
	if (!FPackageName::IsValidLongPackageName(Package) || Package.StartsWith(TEXT("/Engine/Transient")))
	{ Error = TEXT("Default Catalog must point to a saved asset package."); return false; }
	const auto* Catalog = DefaultCatalog.LoadSynchronous();
	if (!Catalog || !Catalog->Validate(Error))
	{
		Error = FString::Printf(TEXT("Default Catalog %s: %s"), *DefaultCatalog.ToString(), Catalog ? *Error : TEXT("missing package or wrong asset class"));
		return false;
	}
	Collected.AddUnique(FName(*Package));
	for(FName P:Collected) { Packages.AddUnique(P); }
	return true;
}
#endif
