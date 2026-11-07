#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "HALVehicleStatusFXRenderer.generated.h"

class UMeshComponent;
class UMaterialInterface;
class UHALVehicleStatusFXDefinition;
struct FHALStatusFXRenderRequest;
namespace HALStatusFXRender
{
	HAL_FUTURE_CREATION_API bool IsLegacyTag(FGameplayTag Tag);
	HAL_FUTURE_CREATION_API float ResidueWeight(const FHALStatusFXRenderRequest& Request);
}

USTRUCT()
struct FHALStatusFXSurfaceOwner
{
	GENERATED_BODY()
	TWeakObjectPtr<UMeshComponent> Mesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Original;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Legacy;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Generic;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Applied;
	TWeakObjectPtr<UMaterialInterface> PreviousGeneric;
	TArray<TWeakObjectPtr<const UHALVehicleStatusFXDefinition>> Contributors;
};

/** Sole overlay writer. Legacy shock borrows the same ownership path until FX-5. No Tick or gameplay state. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALVehicleStatusFXRenderer : public UObject
{
	GENERATED_BODY()
public:
	void SetLegacy(UMeshComponent* Mesh, UMaterialInterface* Material);
	void ClearGeneric();
	void ClearAll();
	void RenderSurfaces(const TArray<FHALStatusFXRenderRequest>& Requests);
	int32 GetSurfaceCount() const { return Owners.Num(); }
	int32 GetGenericChannelCount() const;
	int32 RejectedSurfaces = 0;
	FString LastError;
private:
	FHALStatusFXSurfaceOwner& FindOrAdd(UMeshComponent* Mesh);
	void ApplyAndPrune();
	UPROPERTY(Transient) TArray<FHALStatusFXSurfaceOwner> Owners;
	TMap<TWeakObjectPtr<const UHALVehicleStatusFXDefinition>, FString> Contracts;
};
