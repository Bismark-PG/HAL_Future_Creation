#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HALCameraMath.h"
#include "HALDynamicCameraComponent.generated.h"

class UVehicleLocalConfig;
class UHALCameraConfig;
class APlayerController;

/** Local view calculation, called by Pawn::CalcCamera. No Tick, RPC or physics-history state. */
UCLASS(ClassGroup=(Camera))
class HAL_FUTURE_CREATION_API UHALDynamicCameraComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALDynamicCameraComponent();
	void Configure(UVehicleLocalConfig* InLocal);
	void AddMouseDelta(const FVector2D& Delta);
	void SetStickInput(const FVector2D& Value);
	void ResetCamera();
	bool CalculateView(float DeltaSeconds, FMinimalViewInfo& Out);
	const FHALCameraFrame& GetDebugFrame() const { return Frame; }
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALCameraTestAccess;
#endif
	bool Compute(float DeltaSeconds, const FVector2D& Viewport, bool bAcceptInput, FMinimalViewInfo& Out);
	TArray<FHALCameraTarget> CollectTargets(const UHALCameraConfig& Settings, const FVector& Own, float Dt, int32& MainCount);
	UPROPERTY(Transient) TObjectPtr<UVehicleLocalConfig> Local;
	TWeakObjectPtr<UHALCameraConfig> PreviousProfile;
	TWeakObjectPtr<APlayerController> PreviousController;
	TMap<TWeakObjectPtr<AActor>,float> Weights;
	FHALCameraState State;
	FHALCameraFrame Frame;
	FVector2D MouseDelta = FVector2D::ZeroVector, StickInput = FVector2D::ZeroVector;
	FString LastError;
	uint64 CalculatedFrame = MAX_uint64;
	bool bPreviousDynamic = false;
};
