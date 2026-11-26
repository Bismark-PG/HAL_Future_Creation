#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HALHealingAreaEffect.h"
#include "HALExpandingAreaGeometry.h"
#include "HALExpandingHealingArea.generated.h"

class UHALHealingAreaReceiver;
class UHALHealingAreaGizmoComponent;

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALExpandingHealingAreaConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0", ClampMax="60", Units="s")) float ExpansionSeconds = 2.f;
	/** Hold after expansion; total lifetime is expansion + hold. */
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0.01", ClampMax="120", Units="s")) float DurationSeconds = 5.f;
	/** World cm, independent of actor rotation and scale. */
	UPROPERTY(EditAnywhere, Category="Shape", meta=(ClampMin="1", ClampMax="10000", Units="cm")) float MaximumRadius = 500.f;
	UPROPERTY(EditAnywhere, Category="Shape", meta=(ClampMin="1", ClampMax="1000", Units="cm")) float HalfHeight = 200.f;
	UPROPERTY(EditAnywhere, Category="Healing") FHALHealingAreaRecipe Healing;
	bool IsValid() const;
	float RadiusAt(double Age) const { return HALExpandingArea::RadiusAt(Age, ExpansionSeconds, MaximumRadius); }
	bool IsExpired(double Age) const { return Age >= double(ExpansionSeconds) + DurationSeconds; }
};

/** Server-authoritative expanding healing cylinder. All living vehicles, including the creator, qualify. */
UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API AHALExpandingHealingArea : public AActor
{
	GENERATED_BODY()
public:
	AHALExpandingHealingArea();
	bool ConfigureArea(const FHALExpandingHealingAreaConfig& Config);
	const FHALExpandingHealingAreaConfig& GetAreaConfig() const { return Settings; }
	UFUNCTION(BlueprintPure, Category="Healing Area") float GetCurrentRadius() const;
#if WITH_EDITOR
	bool ShouldShowEditorGizmo() const { return bShowEditorGizmo; }
#endif
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FHALHealingAreaLifecycleTest;
#endif
	double Age() const;
	void UpdateArea();
	UPROPERTY(EditAnywhere, Replicated, Category="Healing Area|Debug", meta=(DisplayName="Debug Draw")) bool bDebugDraw = false;
#if WITH_EDITORONLY_DATA
	/** Selected-actor maximum cylinder preview, independent of the in-game Debug Draw checkbox. */
	UPROPERTY(EditAnywhere, Category="Healing Area|Debug", meta=(DisplayName="Show Editor Gizmo")) bool bShowEditorGizmo = true;
	UPROPERTY(VisibleAnywhere, Category="Healing Area|Debug") TObjectPtr<UHALHealingAreaGizmoComponent> EditorGizmo;
#endif
	UPROPERTY(EditAnywhere, Replicated, Category="Healing Area") FHALExpandingHealingAreaConfig Settings;
	UPROPERTY(Replicated) double StartedAt = -1;
	FTimerHandle UpdateTimer;
	TSet<TWeakObjectPtr<UHALHealingAreaReceiver>> Occupants;
};
