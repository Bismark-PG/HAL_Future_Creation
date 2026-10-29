#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Scene.h"
#include "PhysicsEngine/BodyInstance.h"
#include "VehicleConfigurationTypes.h"
#include "VehicleKnockbackTypes.h"
#include "HALCombatTypes.h"
#include "VehicleDefinition.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UInputMappingContext;
class UInputAction;
class UHALVehiclePresentationConfig;
class UHALCameraConfig;

UENUM(BlueprintType)
enum class EVehicleCollisionShape : uint8 { ConvexMesh, Box };

/** Defaults only. Never copy a live body's instance into this shared template. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FVehicleBodyConfig
{
	GENERATED_BODY()
	FVehicleBodyConfig();
	UPROPERTY(EditAnywhere, Category = "Physics")
	FBodyInstance Body;
	UPROPERTY(EditAnywhere, Category = "Physics")
	FVector Scale = FVector::OneVector;
	UPROPERTY(EditAnywhere, Category = "Physics")
	bool bGenerateOverlapEvents = true;
	UPROPERTY(EditAnywhere, Category = "Presentation")
	bool bHiddenInGame = true;
	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FVehicleVisualConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Visual")
	TObjectPtr<UStaticMesh> Mesh;
	/** Override slots only; empty means use the mesh's own materials. */
	UPROPERTY(EditAnywhere, Category = "Visual")
	TArray<TObjectPtr<UMaterialInterface>> Materials;
	UPROPERTY(EditAnywhere, Category = "Visual")
	TObjectPtr<UMaterialInterface> OverlayMaterial;
	UPROPERTY(EditAnywhere, Category = "Visual")
	FTransform RelativeTransform = FTransform::Identity;
	UPROPERTY(EditAnywhere, Category = "Visual")
	bool bVisible = true;
	UPROPERTY(EditAnywhere, Category = "Visual")
	bool bHiddenInGame = false;
	UPROPERTY(EditAnywhere, Category = "Visual")
	bool bCastShadow = true;
	bool Validate(FString& Error) const;
};

/** Fixed local camera configuration. No dynamic multiplayer camera policy here. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FVehicleCameraConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Boom")
	FTransform BoomTransform = FTransform(FRotator(-60, 0, 0));
	UPROPERTY(EditAnywhere, Category = "Boom")
	bool bAbsoluteRotation = true;
	UPROPERTY(EditAnywhere, Category = "Boom")
	bool bAbsoluteLocation = false;
	UPROPERTY(EditAnywhere, Category = "Boom")
	bool bAbsoluteScale = false;
	UPROPERTY(EditAnywhere, Category = "Boom", meta = (ClampMin = "0", Units = "cm"))
	float TargetArmLength = 1200;
	UPROPERTY(EditAnywhere, Category = "Boom")
	FVector SocketOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Boom")
	FVector TargetOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Boom")
	bool bDoCollisionTest = false;
	UPROPERTY(EditAnywhere, Category = "Boom", meta = (ClampMin = "0", Units = "cm"))
	float ProbeSize = 12;
	UPROPERTY(EditAnywhere, Category = "Boom")
	TEnumAsByte<ECollisionChannel> ProbeChannel = ECC_Camera;
	UPROPERTY(EditAnywhere, Category = "Boom")
	bool bUsePawnControlRotation = false;
	UPROPERTY(EditAnywhere, Category = "Boom")
	bool bInheritPitch = true;
	UPROPERTY(EditAnywhere, Category = "Boom")
	bool bInheritYaw = true;
	UPROPERTY(EditAnywhere, Category = "Boom")
	bool bInheritRoll = true;
	UPROPERTY(EditAnywhere, Category = "Lag")
	bool bEnableCameraLag = true;
	UPROPERTY(EditAnywhere, Category = "Lag")
	bool bEnableCameraRotationLag = false;
	UPROPERTY(EditAnywhere, Category = "Lag", meta = (ClampMin = "0"))
	float CameraLagSpeed = 8;
	UPROPERTY(EditAnywhere, Category = "Lag", meta = (ClampMin = "0"))
	float CameraRotationLagSpeed = 10;
	UPROPERTY(EditAnywhere, Category = "Lag", meta = (ClampMin = "0", Units = "cm"))
	float CameraLagMaxDistance = 0;
	UPROPERTY(EditAnywhere, Category = "Lag")
	bool bUseCameraLagSubstepping = true;
	UPROPERTY(EditAnywhere, Category = "Lag", meta = (ClampMin = "0.001", Units = "s"))
	float CameraLagMaxTimeStep = 1.0f / 60.0f;
	UPROPERTY(EditAnywhere, Category = "Lag")
	bool bClampToMaxPhysicsDeltaTime = false;
	UPROPERTY(EditAnywhere, Category = "Lag")
	bool bDrawDebugLagMarkers = false;
	UPROPERTY(EditAnywhere, Category = "Camera")
	FTransform CameraTransform = FTransform::Identity;
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1", ClampMax = "179", Units = "deg"))
	float FieldOfView = 90;
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.01"))
	float AspectRatio = 1.777778f;
	UPROPERTY(EditAnywhere, Category = "Camera")
	bool bConstrainAspectRatio = false;
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "1"))
	float PostProcessBlendWeight = 1;
	UPROPERTY(EditAnywhere, Category = "Camera")
	FPostProcessSettings PostProcess;
	bool Validate(FString& Error) const;
};

/** Native Data Assets are created and populated by the team in the Editor. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UVehicleLocalConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	/** Enable only after filling and comparing against the saved baseline. */
	UPROPERTY(EditAnywhere, Category = "Status")
	bool bReadyForUse = false;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SteeringAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ThrottleAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> BrakeAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> HandbrakeAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LaunchAction;
	/** Optional skill edge. Existing local assets stay valid until the team wires this action. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> PrimarySkillAction;
	/** Optional independent held recovery action. Leave empty only when recovery input is not wired yet. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SelfRightAction;
	/** Optional Axis2D actions; camera input never enters vehicle physics commands. */
	UPROPERTY(EditAnywhere, Category = "Input|Camera") TObjectPtr<UInputAction> CameraMouseAction;
	UPROPERTY(EditAnywhere, Category = "Input|Camera") TObjectPtr<UInputAction> CameraStickAction;
	/** When assigned and valid, this profile is the sole active camera source. */
	UPROPERTY(EditAnywhere, Category = "Camera") TObjectPtr<UHALCameraConfig> CameraProfile;
	UPROPERTY(EditAnywhere, Category = "Camera", meta=(EditCondition="CameraProfile == nullptr"))
	FVehicleCameraConfig Camera;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UVehicleDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Status")
	bool bReadyForUse = false;
	UPROPERTY(EditAnywhere, Category = "Physics")
	EVehicleCollisionShape CollisionShape = EVehicleCollisionShape::ConvexMesh;
	UPROPERTY(EditAnywhere, Category = "Physics", meta = (EditCondition = "CollisionShape == EVehicleCollisionShape::ConvexMesh"))
	TObjectPtr<UStaticMesh> CollisionMesh;
	UPROPERTY(EditAnywhere, Category = "Physics", meta = (EditCondition = "CollisionShape == EVehicleCollisionShape::Box", Units = "cm"))
	FVector BoxExtent = FVector(100, 70, 35);
	UPROPERTY(EditAnywhere, Category = "Physics")
	FVehicleBodyConfig Physics;
	UPROPERTY(EditAnywhere, Category = "Movement")
	FArcadeVehicleConfig Movement;
	UPROPERTY(EditAnywhere, Category = "Ball Control")
	FBallControlConfig BallControl;
	UPROPERTY(EditAnywhere, Category = "Ball Control")
	FTransform BallControlPoint = FTransform(FVector(170, 0, 20));
	UPROPERTY(EditAnywhere, Category = "Health")
	FVehicleHealthConfig Health;
	/** Explicit migration and combat initialization seed; never a live runtime HP store. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FVehicleCombatConfig Combat;
	/** Explicitly migrated tier/drop/collision configuration. Physical body tuning is unchanged. */
	UPROPERTY(EditAnywhere, Category = "Impact")
	FHALVehicleImpactConfig Impact;
	/** Static appearance/fallback. Optional for driving vehicles with a valid ready skeletal Presentation. */
	UPROPERTY(EditAnywhere, Category = "Visual")
	FVehicleVisualConfig Visual;
	/** Visual-only rig. An invalid/unready rig requires a valid static Visual fallback. */
	UPROPERTY(EditAnywhere, Category = "Visual")
	TObjectPtr<UHALVehiclePresentationConfig> Presentation;
	UPROPERTY(EditAnywhere, Category = "Local")
	TObjectPtr<UVehicleLocalConfig> LocalConfig;
	UPROPERTY(EditAnywhere, Category = "Marker")
	FTransform ForwardArrowTransform = FTransform(FVector(110, 0, 0));
	UPROPERTY(EditAnywhere, Category = "Marker")
	FColor ForwardArrowColor = FColor::Cyan;
	UPROPERTY(EditAnywhere, Category = "Marker", meta = (ClampMin = "0"))
	float ForwardArrowSize = 1.5f;
	UPROPERTY(EditAnywhere, Category = "Marker", meta = (ClampMin = "0", Units = "cm"))
	float ForwardArrowLength = 80;
	UPROPERTY(EditAnywhere, Category = "Marker")
	bool bForwardArrowVisible = true;
	UPROPERTY(EditAnywhere, Category = "Marker")
	bool bForwardArrowHiddenInGame = false;
	/** Box definitions are passive fixtures; movement/local configuration is not consumed. */
	bool Validate(FString& Error) const;
	/** At least one supported visual path must be valid; passive Box fixtures require static Visual. */
	bool ValidateVisual(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UBallDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UBallDefinition();
	UPROPERTY(EditAnywhere, Category = "Status")
	bool bReadyForUse = false;
	UPROPERTY(EditAnywhere, Category = "Physics", meta = (ClampMin = "0.01", Units = "cm"))
	float Radius = 50;
	UPROPERTY(EditAnywhere, Category = "Physics")
	FVehicleBodyConfig Physics;
	UPROPERTY(EditAnywhere, Category = "Gameplay")
	FBallGameplayConfig Gameplay;
	UPROPERTY(EditAnywhere, Category = "Visual")
	FVehicleVisualConfig Visual;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FVehicleKnockbackConfig
{
	GENERATED_BODY()
	FVehicleKnockbackConfig();
	UPROPERTY(EditAnywhere, Category = "Thresholds", meta = (ClampMin = "0", Units = "cm/s"))
	float MediumImpactSpeedThreshold = 1000;
	UPROPERTY(EditAnywhere, Category = "Thresholds", meta = (ClampMin = "0", Units = "cm/s"))
	float HeavyImpactSpeedThreshold = 2600;
	UPROPERTY(EditAnywhere, Category = "Outcomes")
	FVehicleKnockbackTierDefinition LightTier;
	UPROPERTY(EditAnywhere, Category = "Outcomes")
	FVehicleKnockbackTierDefinition MediumTier;
	UPROPERTY(EditAnywhere, Category = "Outcomes")
	FVehicleKnockbackTierDefinition HeavyTier;
	UPROPERTY(EditAnywhere, Category = "Stability", meta = (ClampMin = "0", Units = "rad/s"))
	float MaxAirborneAngularSpeed = 8;
	UPROPERTY(EditAnywhere, Category = "Stability", meta = (ClampMin = "0", Units = "cm"))
	float GroundProbeExtraDistance = 20;
	bool Validate(FString& Error) const;
	EVehicleKnockbackTier SelectTier(float ImpactSpeed) const;
	const FVehicleKnockbackTierDefinition& GetTierDefinition(EVehicleKnockbackTier Tier) const;
};

UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UCombatRulesDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Status")
	bool bReadyForUse = false;
	/** Shared response table reused by authored attacks and calibrated physical contacts. */
	UPROPERTY(EditAnywhere, Category = "Ball Impacts")
	FVehicleKnockbackConfig BallImpacts;
	/** Enable TieredV1 only after migrating vehicle and ball Impact sections. */
	UPROPERTY(EditAnywhere, Category = "Collision Impacts")
	FHALCollisionImpactRules CollisionImpacts;
	/** Authored attacks only. Existing content defaults to Standard (fixed x1). */
	UPROPERTY(EditAnywhere, Category = "Impact Push Presets")
	FHALImpactPushRules ImpactPushPresets;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
