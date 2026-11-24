#pragma once

#include "HALSpecialBall.h"
#include "Blueprint/UserWidget.h"
#include "HALMeteorBall.generated.h"

class ATestVehiclePawn;
class UHALAbilitySystemComponent;
struct FOnAttributeChangeData;
class UInstancedStaticMeshComponent;
class UTexture2D;
class UWidgetComponent;
class UHALMeteorMarkerWidget;

/** The ground warning and the server hit read the same radius and phase times. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALMeteorBallDefinition : public UHALSpecialBallDefinition
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Meteor", meta=(ClampMin=".15", ClampMax="15", Units="s")) float TrackingSeconds = 1.6f;
	UPROPERTY(EditAnywhere, Category="Meteor", meta=(ClampMin=".2", ClampMax="8", Units="s")) float FallSeconds = 1.f;
	UPROPERTY(EditAnywhere, Category="Meteor", meta=(ClampMin="50", ClampMax="5000", Units="cm")) float DamageRadius = 500.f;
	/** Preferred distance ahead of the launcher for a fixed ground strike when no opponent can be tracked. */
	UPROPERTY(EditAnywhere, Category="Meteor", meta=(ClampMin="100", ClampMax="10000", Units="cm")) float UntargetedDistance = 1200.f;
	/** Vertical travel shown on takeoff and descent, measured above the local surface. */
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX", meta=(ClampMin="500", ClampMax="20000", Units="cm")) float SkyHeight = 6000.f;
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX", meta=(ClampMin=".08", ClampMax="2", Units="s")) float RiseSeconds = .38f;
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX", meta=(ClampMin="15", ClampMax="600", Units="cm")) float FallingRockRadius = 110.f;
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX", meta=(ClampMin="0", ClampMax="8")) float TrailLength = 3.2f;
	/** Seconds from emission on the descent path until a smoke billow disperses. */
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX", meta=(ClampMin=".08", ClampMax="1", Units="s")) float SmokeLifetime = .46f;
	/** Sideways travel and curl of the released smoke, in falling rock radii. */
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX", meta=(ClampMin="0", ClampMax="4")) float SmokeSpread = 1.15f;
	/** Angular speed of the falling rock around its tilted axis, in degrees per second. */
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX", meta=(ClampMin="0", ClampMax="1440", Units="deg/s")) float RockSpinRate = 320.f;
	/** Opacity of the slipstreams that peel back from the leading face during descent. */
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX", meta=(ClampMin="0", ClampMax="1")) float AirflowStrength = .72f;
	/** Cycles per second of the crests moving up the slipstreams. */
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX", meta=(ClampMin="0", ClampMax="8")) float AirflowSpeed = 2.4f;
	/** Optional tapered flow mesh. Its nose points along local +X, and UV U increases toward the tail. */
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX") TObjectPtr<UStaticMesh> AirflowMesh;
	/** Optional material override for the falling streaks. */
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX") TObjectPtr<UMaterialInterface> AirflowMaterial;
	/** Optional rolling three-tone smoke. Falls back to Plume Material. */
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX") TObjectPtr<UMaterialInterface> DescentSmokeMaterial;
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX") FLinearColor SmokeColor = FLinearColor(.012f,.014f,.016f);
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX") TObjectPtr<UStaticMesh> FlameMesh;
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX") TObjectPtr<UStaticMesh> GroundPlane;
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX") TObjectPtr<UMaterialInterface> WarningMaterial;
	UPROPERTY(EditAnywhere, Category="Meteor|Flight FX") TObjectPtr<UMaterialInterface> PlumeMaterial;
	/** Maximum reach of the rolling fire clouds as a fraction of the damage radius. */
	UPROPERTY(EditAnywhere, Category="Meteor|Impact FX", meta=(ClampMin=".35", ClampMax=".94")) float CloudCoverage = .88f;
	/** Crest height as a fraction of the damage radius. Zero keeps the clouds on the ground. */
	UPROPERTY(EditAnywhere, Category="Meteor|Impact FX", meta=(ClampMin="0", ClampMax=".65")) float CloudLift = .29f;
	/** Forward roll of the expanding puffs. Zero lets the volumes expand without rolling. */
	UPROPERTY(EditAnywhere, Category="Meteor|Impact FX", meta=(ClampMin="0", ClampMax="2")) float CloudCurl = 1.f;
	/** Fraction of Impact Seconds spent inflating the main fire clouds. */
	UPROPERTY(EditAnywhere, Category="Meteor|Impact FX", meta=(ClampMin=".2", ClampMax=".95")) float CloudExpansionFraction = .62f;
	/** Extra seconds for cooling smoke to rise and disperse after the ground effect ends. */
	UPROPERTY(EditAnywhere, Category="Meteor|Impact FX", meta=(ClampMin="0", ClampMax="1.5", Units="s")) float CloudTailSeconds = .44f;
	UPROPERTY(EditAnywhere, Category="Meteor|Impact FX") FLinearColor CloudTint = FLinearColor::White;
	/** Deep smoke revealed as the flame front cools. */
	UPROPERTY(EditAnywhere, Category="Meteor|Impact FX") FLinearColor CooledSmokeColor = FLinearColor(.094f,.052f,.082f);
	/** Band thickness measured inward from the travelling edge. The outside stops at Damage Radius. */
	UPROPERTY(EditAnywhere, Category="Meteor|Impact FX", meta=(ClampMin="2", ClampMax="100", Units="cm")) float ShockRingWidth = 23.f;
	/** Brightness of the single ground pressure ring; zero hides it. */
	UPROPERTY(EditAnywhere, Category="Meteor|Impact FX", meta=(ClampMin="0", ClampMax="4")) float ShockRingIntensity = 1.25f;
	UPROPERTY(EditAnywhere, Category="Meteor|Impact FX") FLinearColor ShockAccentColor = FLinearColor(.24f,.87f,1.f);
	/** Closed cloud puff centered on its pivot. Slot 0 supplies its fire-to-ash material. */
	UPROPERTY(EditAnywhere, Category="Meteor|Impact FX") TObjectPtr<UStaticMesh> ImpactVolumeMesh;
	/** Optional sheet for types authored with the original crest effect. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Meteor|Impact FX") TObjectPtr<UStaticMesh> ImpactCloudMesh;
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Meteor|Impact FX") TObjectPtr<UMaterialInterface> ImpactCloudMaterial;
	UPROPERTY(EditAnywhere, Category="Meteor|Impact FX") TObjectPtr<UMaterialInterface> ImpactGroundMaterial;
	float GetCloudDuration() const { return ImpactSeconds + (ImpactVolumeMesh ? CloudTailSeconds : 0.f); }
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meteor|Tracking Marker") TObjectPtr<UTexture2D> MarkerCore;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meteor|Tracking Marker") TObjectPtr<UTexture2D> MarkerJaws;
	/** An authored Widget Blueprint may replace the native two-layer marker. */
	UPROPERTY(EditAnywhere, Category="Meteor|Tracking Marker") TSubclassOf<UHALMeteorMarkerWidget> MarkerWidgetClass;
	UPROPERTY(EditAnywhere, Category="Meteor|Tracking Marker", meta=(ClampMin="16", ClampMax="160")) float MarkerSize = 48.f;
	/** Width of the jaws at full lock relative to their initial size. The comet stays still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meteor|Tracking Marker", meta=(ClampMin=".55", ClampMax="1")) float MarkerClosedScale = .78f;
	/** Logical screen units above the vehicle label and health anchor. */
	UPROPERTY(EditAnywhere, Category="Meteor|Tracking Marker", meta=(ClampMin="0", ClampMax="200")) float MarkerLift = 38.f;
	virtual bool Validate(FString& Error) const override;
};

UCLASS()
class HAL_FUTURE_CREATION_API AHALMeteorBall : public AHALSpecialBall
{
	GENERATED_BODY()
protected:
	virtual UHALSpecialBallDefinition* DefaultType() const override;
	virtual bool DeliverLaunch(const FHALAttackSourceSnapshot& Attack, AActor* Launcher, const FVector& Velocity, const AActor* FrameSource) override;
};

UENUM()
enum class EHALMeteorPhase : uint8 { Tracking, Falling, Impact };

USTRUCT()
struct FHALMeteorStrikeState
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UHALMeteorBallDefinition> Type;
	UPROPERTY() TObjectPtr<ATestVehiclePawn> Target;
	UPROPERTY() FVector_NetQuantize10 LaunchPoint = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantize10 Ground = FVector::ZeroVector;
	/** Announces a fixed aim point at launch, independently of target replication or later target loss. */
	UPROPERTY() bool bGroundTargeted = false;
	UPROPERTY() double Started = 0.;
	UPROPERTY() EHALMeteorPhase Phase = EHALMeteorPhase::Tracking;
};

/** Collisionless attack spanning several seconds. Server timers own target selection, lock and damage. */
UCLASS(NotPlaceable)
class HAL_FUTURE_CREATION_API AHALMeteorStrike : public AActor
{
	GENERATED_BODY()
public:
	AHALMeteorStrike();
	static bool Start(AHALMeteorBall& Ball, const FHALAttackSourceSnapshot& Attack, UHALMeteorBallDefinition& Type,
		ATestVehiclePawn* Target, const FVector& Ground);
	const FHALMeteorStrikeState& GetStrikeState() const { return State; }
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
	void SampleTarget();
	void LockGround();
	void Land();
	void ReleaseTarget();
	void OnTargetHealth(const FOnAttributeChangeData& Change);
	UFUNCTION() void OnMatchPhase(EHALMatchPhase Phase);
	UFUNCTION() void RefreshPresentation();
	void DrawFlight(double Age);
	void DrawDescentSmoke(double SecondsSinceLock);
	void PrepareImpactFX();
	void DrawCrestSheets(float Progress);
	void DrawVolumeClouds(float SecondsAfterImpact);
	double ServerNow() const;
	UPROPERTY(ReplicatedUsing=RefreshPresentation) FHALMeteorStrikeState State;
	FVector LastGround = FVector::ZeroVector;
	FHALCombatAction Action;
	TWeakObjectPtr<UHALAbilitySystemComponent> TargetAbilitySystem;
	FDelegateHandle TargetHealthHandle;
	FTimerHandle SampleTimer, PhaseTimer;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Rock;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GroundFX;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Flames;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Smoke;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> DescentSmoke;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Airflow;
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> ImpactClouds;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> Marker;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GroundMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> AirflowMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DescentSmokeMID;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> CloudMIDs;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ImpactGroundMID;
	bool bUsesVolumeClouds = false;
	TArray<FTransform> PuffTransforms;
	TArray<FTransform> PreviousPuffTransforms;
};

UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALMeteorMarkerWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetLock(UHALMeteorBallDefinition* Type, float Progress);
	UPROPERTY(BlueprintReadOnly, Category="Meteor") TObjectPtr<UHALMeteorBallDefinition> BallType;
	UPROPERTY(BlueprintReadOnly, Category="Meteor") float TrackingProgress = 0;
	UFUNCTION(BlueprintImplementableEvent, Category="Meteor") void ReceiveTrackingProgress(float Progress);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Elements,
		int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
	bool bNative = true;
};
