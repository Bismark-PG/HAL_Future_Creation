#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HALArenaEffectGenerator.generated.h"

class AHALExpandingSlipperyArea;
class AHALBlackHole;
class AHALElectricSphereGimmick;
class UBoxComponent;
class UDynamicMeshComponent;
class UTextRenderComponent;
class UMaterialBillboardComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;
class UStaticMeshComponent;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EHALArenaEffect : uint8 { None, SlipperyArea, BlackHole, ElectricField };

UENUM(BlueprintType)
enum class EHALArenaGeneratorStage : uint8 { Waiting, Rolling, Revealing, Hint, Delay, Active, Finished, Disabled };

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALArenaGeneratorConfig
{
	GENERATED_BODY()
	FHALArenaGeneratorConfig();
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0", ClampMax="60", Units="s")) float RollSeconds = 1.5f;
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0", ClampMax="60", Units="s")) float RevealSeconds = .6f;
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0", ClampMax="60", Units="s")) float HintSeconds = .5f;
	/** The warning stays visible during this reaction window. No effect Actor exists yet. */
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0", ClampMax="60", Units="s")) float SpawnDelaySeconds = 2.f;
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0", ClampMax="60", Units="s")) float FinishSeconds = .5f;
	/** Uses each selected native/BP class's own settings, including expansion and lifetime. */
	UPROPERTY(EditAnywhere, Category="Effects") TSubclassOf<AHALExpandingSlipperyArea> SlipperyAreaClass;
	UPROPERTY(EditAnywhere, Category="Effects") TSubclassOf<AHALBlackHole> BlackHoleClass;
	UPROPERTY(EditAnywhere, Category="Effects") TSubclassOf<AHALElectricSphereGimmick> ElectricFieldClass;
	/** Shared world-space offset. Keeps the original serialized EffectOffset; per-effect offsets are added to it. */
	UPROPERTY(EditAnywhere, Category="Placement", meta=(Units="cm", DisplayName="Base Effect Offset")) FVector EffectOffset = FVector::ZeroVector;
	/** Added to Base Effect Offset. Initial Z aligns MVPB_Match's -80 cm area with its -190 cm pillar. */
	UPROPERTY(EditAnywhere, Category="Placement", meta=(Units="cm")) FVector SlipperyAreaOffset = FVector(0,0,110);
	/** Added to Base Effect Offset. Initial Z aligns MVPB_Match's 0 cm black hole with its -190 cm pillar. */
	UPROPERTY(EditAnywhere, Category="Placement", meta=(Units="cm")) FVector BlackHoleOffset = FVector(0,0,190);
	/** Added to Base Effect Offset. Initial Z aligns MVPB_Match's 0 cm electric field with its -190 cm pillar. */
	UPROPERTY(EditAnywhere, Category="Placement", meta=(Units="cm")) FVector ElectricFieldOffset = FVector(0,0,190);
	UPROPERTY(EditAnywhere, Category="Pillar", meta=(ClampMin="1", ClampMax="2000", Units="cm")) FVector PillarHalfExtent = FVector(40,40,100);
	/** The Box is the pillar's only collider; visual meshes never block vehicles or balls. */
	UPROPERTY(EditAnywhere, Category="Pillar") bool bSolidPillar = true;
	/** Local Z offset of the text / forecast panel above the Box. Positive raises it; Actor scale applies. */
	UPROPERTY(EditAnywhere, Category="Presentation", meta=(ClampMin="-10000", ClampMax="10000", Units="cm")) float TextHeightOffset = 0.f;
	/** Camera-facing hologram row above the pillar. Empty keeps the plain debug text. Presentation only. */
	UPROPERTY(EditAnywhere, Category="Presentation|Forecast") TObjectPtr<UMaterialInterface> ForecastMaterial;
	/** Optional per-effect icons; empty slots keep the material's own default textures. */
	UPROPERTY(EditAnywhere, Category="Presentation|Forecast") TObjectPtr<UTexture2D> SlipperyAreaIcon;
	UPROPERTY(EditAnywhere, Category="Presentation|Forecast") TObjectPtr<UTexture2D> BlackHoleIcon;
	UPROPERTY(EditAnywhere, Category="Presentation|Forecast") TObjectPtr<UTexture2D> ElectricFieldIcon;
	/** World-space edge of the selected (centre) icon. Actor scale does not apply. */
	UPROPERTY(EditAnywhere, Category="Presentation|Forecast", meta=(ClampMin="10", ClampMax="2000", Units="cm")) float ForecastIconSize = 220.f;
	/** Size of the two unselected side icons relative to the centre icon. */
	UPROPERTY(EditAnywhere, Category="Presentation|Forecast", meta=(ClampMin="0.1", ClampMax="1")) float ForecastSideScale = .65f;
	/** World-space distance between neighbouring icon centres. */
	UPROPERTY(EditAnywhere, Category="Presentation|Forecast", meta=(ClampMin="10", ClampMax="4000", Units="cm")) float ForecastSpacing = 230.f;
	/** Icons passing the centre per second while rolling; the reveal decelerates from this speed. */
	UPROPERTY(EditAnywhere, Category="Presentation|Forecast", meta=(ClampMin="0.5", ClampMax="30")) float ForecastRollSpeed = 8.f;
	/** Animate two visual-only meshes; the Box switches off underground and returns when fully raised. */
	UPROPERTY(EditAnywhere, Category="Mesh Animation") bool bAnimateMeshes = true;
	/** World-space downward travel, independent of the pillar's authored scale. */
	UPROPERTY(EditAnywhere, Category="Mesh Animation", meta=(ClampMin="0", ClampMax="10000", Units="cm", EditCondition="bAnimateMeshes")) float MeshSinkDepth = 600.f;
	/** Total rotation while sinking. Negative values reverse the direction; rising retraces it. */
	UPROPERTY(EditAnywhere, Category="Mesh Animation", meta=(ClampMin="-7200", ClampMax="7200", Units="deg", EditCondition="bAnimateMeshes")) float MeshRotationDegrees = 360.f;
	/** Match/room maps wait for Playing; worlds without HAL match state start immediately. */
	UPROPERTY(EditAnywhere, Category="Lifecycle") bool bWaitForMatchStart = true;
	/** 0 chooses a fresh authority seed. Nonzero makes the sequence reproducible; repeats are allowed. */
	UPROPERTY(EditAnywhere, Category="Random") int32 RandomSeed = 0;
	bool Validate(FString& Error) const;
	float PreviewRadius(EHALArenaEffect Effect) const;
	FVector PlacementOffset(EHALArenaEffect Effect) const;
	UClass* EffectClass(EHALArenaEffect Effect) const;
	static EHALArenaEffect Draw(FRandomStream& Random);
};

/** One atomic replicated phase. Clients never author selections, transitions or effects. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALArenaGeneratorState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EHALArenaGeneratorStage Stage = EHALArenaGeneratorStage::Waiting;
	UPROPERTY(BlueprintReadOnly) EHALArenaEffect Effect = EHALArenaEffect::None;
	UPROPERTY(BlueprintReadOnly) int32 Cycle = 0;
	UPROPERTY(BlueprintReadOnly) double StartedAt = 0;
	UPROPERTY(BlueprintReadOnly) float Duration = 0;
	/** Measured length of this cycle's Rolling stage, so every peer decelerates the row from the same position. */
	UPROPERTY(BlueprintReadOnly) float RolledSeconds = 0;
	UPROPERTY(BlueprintReadOnly) FVector EffectCenter = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float WarningRadius = 0;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> ActiveEffect;
};

/** Server-owned looping arena pillar. A BP child only supplies effect classes and presentation. */
UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API AHALArenaEffectGenerator : public AActor
{
	GENERATED_BODY()
public:
	AHALArenaEffectGenerator();
	bool ConfigureGenerator(const FHALArenaGeneratorConfig& Config);
	UFUNCTION(BlueprintPure, Category="Arena Generator") const FHALArenaGeneratorState& GetGeneratorState() const { return State; }
	UFUNCTION(BlueprintPure, Category="Arena Generator") EHALArenaEffect GetDisplayedEffect() const;
	/** -1 for waiting, active effect or disabled; otherwise synchronized remaining phase time. */
	UFUNCTION(BlueprintPure, Category="Arena Generator") float GetStageRemainingSeconds() const;
	/** Continuous row position in icons; integer k shows effect 1+(k mod 3) at the centre. Never draws a result. */
	static double ForecastPosition(const FHALArenaGeneratorConfig& Config, const FHALArenaGeneratorState& InState, double Age);
	UFUNCTION(BlueprintPure, Category="Arena Generator") float GetForecastPosition() const;
	/** 1 to 0 across Hint + Delay; -1 outside the countdown. */
	UFUNCTION(BlueprintPure, Category="Arena Generator") float GetForecastCountdown() const;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Settings, Category="Arena Generator") FHALArenaGeneratorConfig Settings;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arena Generator|Components") TObjectPtr<UBoxComponent> PillarBody;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arena Generator|Components") TObjectPtr<UDynamicMeshComponent> PillarVisual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arena Generator|Components") TObjectPtr<UDynamicMeshComponent> HintRing;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arena Generator|Components") TObjectPtr<UTextRenderComponent> StageText;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arena Generator|Components") TObjectPtr<UMaterialBillboardComponent> ForecastPanel;
	/** Cosmetic only. Late relevance presents the current phase rather than replaying previous phases. */
	UFUNCTION(BlueprintImplementableEvent, Category="Arena Generator|Presentation")
	void OnGeneratorStageChanged(EHALArenaGeneratorStage NewStage, EHALArenaEffect SelectedEffect, int32 Cycle);
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALArenaGeneratorTestAccess;
	friend struct FHALFeverNetworkTestAccess;
#endif
	UFUNCTION() void OnRep_Settings();
	UFUNCTION() void OnRep_State();
	UFUNCTION() void EffectEnded(AActor* Actor, EEndPlayReason::Type Reason);
	void UpdateGenerator();
	void ApplyFever(const class UHALMatchFeverDefinition* Fever);
	float WaitMultiplier = 1.f;
	void EnterStage(EHALArenaGeneratorStage Stage, float Duration = 0);
	void BeginCycle();
	void SpawnEffect();
	void RemoveEffect();
	bool CanRun() const;
	double ServerNow() const;
	void RefreshGeometry();
	void RefreshPresentation();
	void InitializeMeshMotion();
	void RefreshMeshMotion();
	void UpdateMeshMotion();
	void ClearMeshMotion();
	float MeshMotionAlpha() const;
	void UpdatePillarCollision();
	FVector2D ForecastPanelSize() const;
	void RefreshForecast();
	void UpdateForecast();
	void ClearPillarCollision();
	/** Render-only copies leave authored attachments/transforms fixed; collision is managed separately. */
	struct FMeshMotionSource
	{
		TWeakObjectPtr<UStaticMeshComponent> Source;
		bool bVisible = true, bHiddenInGame = false, bCastShadow = true;
	};
	TArray<FMeshMotionSource> MeshMotionSources;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> MeshMotionRoot;
	FTimerHandle MeshMotionTimer;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ForecastMID;
	FTimerHandle ForecastTimer;
	UPROPERTY(ReplicatedUsing=OnRep_State) FHALArenaGeneratorState State;
	UPROPERTY(Transient) TObjectPtr<UClass> PendingClass;
	FRandomStream Random;
	FTimerHandle UpdateTimer;
	bool bReady = false, bEnding = false, bPresented = false;
	EHALArenaGeneratorStage PresentedStage = EHALArenaGeneratorStage::Waiting;
	int32 PresentedCycle = 0;
	float BuiltRingRadius = -1;
};
