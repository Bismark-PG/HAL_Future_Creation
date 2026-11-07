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
	HAL_FUTURE_CREATION_API bool UsesShockDriver(FGameplayTag Tag);
	HAL_FUTURE_CREATION_API float ResidueWeight(const FHALStatusFXRenderRequest& Request);
}

USTRUCT()
struct FHALStatusFXSurfaceOwner
{
	GENERATED_BODY()
	TWeakObjectPtr<UMeshComponent> Mesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Original;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Legacy;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Shock;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Generic;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Applied;
	TWeakObjectPtr<UMaterialInterface> PreviousGeneric;
	TArray<TWeakObjectPtr<const UHALVehicleStatusFXDefinition>> Contributors;
	FGameplayTag GenericTag;
	int32 ShockPriority = 0;
	int32 GenericPriority = 0;
	bool bShockResidue = false;
	bool bGenericResidue = false;
};

/** Sole overlay writer. Specialised shock and generic channels share priority and restoration. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALVehicleStatusFXRenderer : public UObject
{
	GENERATED_BODY()
public:
	void SetLegacy(UMeshComponent* Mesh, UMaterialInterface* Material);
	void SetShockSurface(UMeshComponent* Mesh, UMaterialInterface* Material, int32 Priority = 0, bool bResidue = false);
	void ClearGeneric();
	void ClearAll();
	void RenderSurfaces(const TArray<FHALStatusFXRenderRequest>& Requests);
	int32 GetSurfaceCount() const { return Owners.Num(); }
	int32 GetGenericChannelCount() const;
	int32 RejectedSurfaces = 0;
	FString LastError;
private:
	FHALStatusFXSurfaceOwner& FindOrAdd(UMeshComponent* Mesh);
	static bool ShouldUseShock(const FHALStatusFXSurfaceOwner& Owner);
	void ApplyAndPrune();
	UPROPERTY(Transient) TArray<FHALStatusFXSurfaceOwner> Owners;
	TMap<TWeakObjectPtr<const UHALVehicleStatusFXDefinition>, FString> Contracts;
};
