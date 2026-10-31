#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class ATestVehiclePawn;
class UVehicleLocalConfig;

namespace HALVehicleInputPreview
{
	/** Read the current source without copying defaults into the Pawn or dirtying an asset. */
	UVehicleLocalConfig* LocalConfig(const ATestVehiclePawn* Pawn);
	UObject* Input(const ATestVehiclePawn* Pawn, FName PropertyName);
}

/** Definition-mode Blueprint input rows project the shared Local Config. */
class FHALVehicleInputDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
