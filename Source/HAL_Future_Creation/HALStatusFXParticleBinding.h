#pragma once
#include "CoreMinimal.h"
class UNiagaraComponent;
struct FHALVehicleFXTarget;
struct FHALStatusFXNiagaraLayer;
/** CPU/local-space template adapter. No gameplay/physics changes or asset mutation. */
namespace HALStatusFXParticle
{
	HAL_FUTURE_CREATION_API bool Configure(UNiagaraComponent* Component, const FHALVehicleFXTarget& Target,
		const FHALStatusFXNiagaraLayer& Layer, FString& Error);
}
