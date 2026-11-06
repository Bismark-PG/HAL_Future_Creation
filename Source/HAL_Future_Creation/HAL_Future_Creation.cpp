// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL_Future_Creation.h"
#include "Modules/ModuleManager.h"
#if WITH_EDITOR
#include "HALVehicleStatusFXSettings.h"
#include "GameDelegates.h"
#endif

class FHALFutureCreationModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
#if WITH_EDITOR
		CookHandle = FGameDelegates::Get().GetModifyCookDelegate().AddLambda(
			[](TConstArrayView<const ITargetPlatform*>, TArray<FName>& Packages, TArray<FName>& NeverCook)
			{
				TArray<FName> Roots; FString Error;
				if (!GetDefault<UHALVehicleStatusFXSettings>()->CollectCookPackages(Roots, Error))
				{ UE_LOG(LogTemp, Error, TEXT("Status FX cook configuration invalid: %s"), *Error); return; }
				for (const FName Root : Roots)
				{
					if (NeverCook.Contains(Root)) { UE_LOG(LogTemp, Error, TEXT("Status FX catalog %s is also marked NeverCook."), *Root.ToString()); }
					else { Packages.AddUnique(Root); }
				}
			});
#endif
	}
	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		FGameDelegates::Get().GetModifyCookDelegate().Remove(CookHandle);
#endif
		FDefaultGameModuleImpl::ShutdownModule();
	}
private:
#if WITH_EDITOR
	FDelegateHandle CookHandle;
#endif
};

IMPLEMENT_PRIMARY_GAME_MODULE(FHALFutureCreationModule, HAL_Future_Creation, "HAL_Future_Creation");
