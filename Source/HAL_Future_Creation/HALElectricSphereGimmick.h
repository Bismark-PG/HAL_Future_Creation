#pragma once
#include "CoreMinimal.h"
#include "HALElectricWallGimmick.h"
#include "HALElectricSphereGimmick.generated.h"

class USphereComponent;

UENUM(BlueprintType)
enum class EHALElectricSphereStage : uint8 { Begin, Loop, Over };

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALElectricSphereConfig
{
	GENERATED_BODY()
	/** World radius with unit actor scale. Actor origin is the sphere center. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="1", Units="cm")) float Radius = 300;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s")) float BeginSeconds = 1;
	/** Effective electrical lifetime, counted after Begin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.01", Units="s")) float LoopSeconds = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s")) float OverSeconds = 1;
	bool Validate() const;
	EHALElectricSphereStage StageAt(double Age) const;
};

/** Spawnable query-only electrical volume. Shares wall contact/prediction rules; no generator. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALElectricSphereGimmick : public AHALElectricWallGimmick
{
	GENERATED_BODY()
public:
	AHALElectricSphereGimmick();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	/** Authority-only deferred spawn, before FinishSpawning. Contact rules use inherited Configure. */
	bool ConfigureSphere(const FHALElectricSphereConfig& InConfig);
	UFUNCTION(BlueprintPure, Category="Electric Sphere") EHALElectricSphereStage GetStage() const { return Stage; }
protected:
	UPROPERTY(EditDefaultsOnly, ReplicatedUsing=OnRep_Lifecycle, BlueprintReadOnly, Category="Electric Sphere") FHALElectricSphereConfig SphereConfig;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electric Sphere") TObjectPtr<USphereComponent> SphereField;
	/** Presentation only. Stop the previous stage's effects, then start this stage.
	 * Age supports late relevance; a client can first observe Loop or Over. No gameplay here. */
	UFUNCTION(BlueprintImplementableEvent, Category="Electric Sphere|Presentation")
	void OnStageChanged(EHALElectricSphereStage NewStage, float StageAgeSeconds, float StageDurationSeconds, float RadiusCm);
	virtual UPrimitiveComponent* GetContactSurface() const override;
	virtual bool IsContactActive() const override;
	virtual void UpdateGeometry() override;
	virtual FVector GetPushDirection(UPrimitiveComponent* Surface,const FVector& Current,const FVector& Previous,const FVector& Velocity) const override;
private:
	UFUNCTION() void OnRep_Lifecycle();
	void RefreshLifecycle();
	UPROPERTY(ReplicatedUsing=OnRep_Lifecycle) double StartedAt = -1;
	EHALElectricSphereStage Stage = EHALElectricSphereStage::Begin;
	bool bPresented = false;
	bool bLifecycleStarted = false;
	FTimerHandle LifecycleTimer;
};
