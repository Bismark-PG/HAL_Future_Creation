#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HALExpandingSlipperyArea.generated.h"

class UHALMovementAreaReceiver;

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALExpandingSlipperyAreaConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0", ClampMax="60", Units="s")) float ExpansionSeconds = 2.f;
	/** Hold time after expansion has finished. */
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0.01", ClampMax="120", Units="s")) float DurationSeconds = 5.f;
	/** World-space radius; actor scale does not change gameplay size. */
	UPROPERTY(EditAnywhere, Category="Shape", meta=(ClampMin="1", ClampMax="10000", Units="cm")) float MaximumRadius = 500.f;
	UPROPERTY(EditAnywhere, Category="Shape", meta=(ClampMin="1", ClampMax="1000", Units="cm")) float HalfHeight = 200.f;
	UPROPERTY(EditAnywhere, Category="Handling", meta=(ClampMin="0", ClampMax="1")) float LateralGripMultiplier = .2f;
	UPROPERTY(EditAnywhere, Category="Handling", meta=(ClampMin="0", ClampMax="10", Units="s")) float RecoverySeconds = .4f;
	bool IsValid() const;
	float RadiusAt(double Age) const;
	bool IsExpired(double Age) const { return Age >= double(ExpansionSeconds) + DurationSeconds; }
};

/** Server-owned, query-only circular area. Expansion is already damaging to grip, with no sway. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALExpandingSlipperyArea : public AActor
{
	GENERATED_BODY()
public:
	AHALExpandingSlipperyArea();
	/** Configure on authority before FinishSpawning. Location is the center of the volume. */
	bool ConfigureArea(const FHALExpandingSlipperyAreaConfig& Config);
	const FHALExpandingSlipperyAreaConfig& GetAreaConfig() const { return Settings; }
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FHALExpandingAreaTest;
	friend class FHALSlipperyGroundDebugTest;
#endif
	double Age() const;
	void UpdateArea();
	void DrawGroundDebug(double Elapsed) const;
	/** Debug-only ground mesh and grid. Does not control the slippery effect. */
	UPROPERTY(EditAnywhere, Replicated, Category="Slippery Area|Debug", meta=(DisplayName="Debug Draw")) bool bDebugDraw = false;
	UPROPERTY(EditAnywhere, Replicated, Category="Slippery Area") FHALExpandingSlipperyAreaConfig Settings;
	UPROPERTY(Replicated) double StartedAt = -1;
	FTimerHandle UpdateTimer;
	TSet<TWeakObjectPtr<UHALMovementAreaReceiver>> Occupants;
};
