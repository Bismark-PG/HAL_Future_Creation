#pragma once
#include "CoreMinimal.h"
#include "HALBattleTopFXTypes.generated.h"

class UNiagaraSystem;
class UMaterialInterface;

/** Skill-specific cosmetic recipe. Missing systems never block gameplay. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALBattleTopFXConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Systems") TObjectPtr<UNiagaraSystem> Wind;
	UPROPERTY(EditAnywhere, Category = "Systems") TObjectPtr<UNiagaraSystem> ContactSparks;
	UPROPERTY(EditAnywhere, Category = "Systems") TObjectPtr<UNiagaraSystem> GroundSparks;
	/** Optional material override for the wind system's User.WindMaterial binding. */
	UPROPERTY(EditAnywhere, Category = "Systems") TObjectPtr<UMaterialInterface> WindMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") FLinearColor MainColor = FLinearColor(1.f, .25f, .025f);
	UPROPERTY(EditAnywhere, Category = "Look") FLinearColor AccentColor = FLinearColor(1.f, .055f, .025f);
	UPROPERTY(EditAnywhere, Category = "Look") FLinearColor HighlightColor = FLinearColor(1.f, .72f, .22f);
	UPROPERTY(EditAnywhere, Category = "Look", meta = (ClampMin = "0", ClampMax = "1")) float Opacity = .55f;
	UPROPERTY(EditAnywhere, Category = "Look", meta = (ClampMin = "1", ClampMax = "2")) float RadiusScale = 1.15f;
	/** Independent cosmetic airflow speed; never changes vehicle spin or contact motion. */
	UPROPERTY(EditAnywhere, Category = "Cartoon Wind", meta = (Units="deg/s", ClampMin="-720", ClampMax="720"))
	float WindFlowDegreesPerSecond = 240.f;
	/** Approximate steady number of independently living arc particles. */
	UPROPERTY(EditAnywhere, Category = "Cartoon Wind", meta = (ClampMin="1", ClampMax="8"))
	int32 WindBandCount = 5;
	UPROPERTY(EditAnywhere, Category = "Cartoon Wind", meta = (Units="s", ClampMin="0.4", ClampMax="3"))
	float WindBandLifetime = 1.2f;
	UPROPERTY(EditAnywhere, Category = "Cartoon Wind", meta = (Units="cm", ClampMin="0", ClampMax="150"))
	float WindLayerHeight = 45.f;
	UPROPERTY(EditAnywhere, Category = "Cartoon Wind", meta = (Units="deg", ClampMin="0", ClampMax="30"))
	float WindTiltDegrees = 12.f;
	UPROPERTY(EditAnywhere, Category = "Cartoon Wind", meta = (ClampMin="0.35", ClampMax="2.5"))
	float WindBandWidthScale = 1.f;
	UPROPERTY(EditAnywhere, Category = "Cartoon Wind", meta = (Units="cm/s", ClampMin="0", ClampMax="100"))
	float WindRiseSpeed = 18.f;
	UPROPERTY(EditAnywhere, Category = "Cartoon Wind", meta = (ClampMin="0", ClampMax="3"))
	float WindGlowStrength = .8f;
	UPROPERTY(EditAnywhere, Category = "Contact Style", meta = (ClampMin="0", ClampMax="3"))
	float ContactGlowStrength = 1.f;
	UPROPERTY(EditAnywhere, Category = "Contact", meta = (Units = "s", ClampMin = "0.03", ClampMax = "1"))
	float ContactInterval = .08f;
	UPROPERTY(EditAnywhere, Category = "Contact", meta = (Units = "cm/s", ClampMin = "0", ClampMax = "2000"))
	float MinimumSlipSpeed = 80;
	UPROPERTY(EditAnywhere, Category = "Contact", meta = (ClampMin = "1", ClampMax = "64")) int32 ContactCount = 7;
	/** Width/length in cm. Each new contact particle samples between these bounds. */
	UPROPERTY(EditAnywhere, Category = "Contact Spray", meta = (Units = "cm", ClampMin = "1", ClampMax = "200"))
	FVector2D ContactSizeMin = FVector2D(12, 40);
	UPROPERTY(EditAnywhere, Category = "Contact Spray", meta = (Units = "cm", ClampMin = "1", ClampMax = "200"))
	FVector2D ContactSizeMax = FVector2D(22, 85);
	UPROPERTY(EditAnywhere, Category = "Contact Spray", meta = (Units = "s", ClampMin = "0.08", ClampMax = "1.2"))
	float ContactLifetimeMin = .3f;
	UPROPERTY(EditAnywhere, Category = "Contact Spray", meta = (Units = "s", ClampMin = "0.08", ClampMax = "1.2"))
	float ContactLifetimeMax = .55f;
	UPROPERTY(EditAnywhere, Category = "Contact Spray", meta = (Units = "cm/s", ClampMin = "100", ClampMax = "4000"))
	float ContactSpeedMin = 650;
	UPROPERTY(EditAnywhere, Category = "Contact Spray", meta = (Units = "cm/s", ClampMin = "100", ClampMax = "4000"))
	float ContactSpeedMax = 1700;
	/** Bounds the angle from the opposite contact tangent; scatter stays outside the hit surface. */
	UPROPERTY(EditAnywhere, Category = "Contact Spray", meta = (Units = "deg", ClampMin = "5", ClampMax = "80"))
	float ContactSpreadDegrees = 65;
	/** Cosmetic Niagara acceleration only, independent of vehicle/ball gravity. */
	UPROPERTY(EditAnywhere, Category = "Contact Spray", meta = (ClampMin = "0", ClampMax = "3"))
	float ContactGravityScale = 1;
	/** Niagara linear drag in inverse seconds. */
	UPROPERTY(EditAnywhere, Category = "Contact Spray", meta = (DisplayName = "Contact Drag (per second)", ClampMin = "0", ClampMax = "15"))
	float ContactDrag = 1.8f;
	UPROPERTY(EditAnywhere, Category = "Ground", meta = (Units = "s", ClampMin = "0.05", ClampMax = "1"))
	float GroundInterval = .16f;
	UPROPERTY(EditAnywhere, Category = "Ground", meta = (Units = "cm/s", ClampMin = "1", ClampMax = "2000"))
	float GroundMinimumSpeed = 150;
	UPROPERTY(EditAnywhere, Category = "Ground", meta = (Units = "cm", ClampMin = "1", ClampMax = "100"))
	float GroundDistance = 30;
	bool IsValid() const;
};

namespace HALBattleTopFX
{
	struct HAL_FUTURE_CREATION_API FContactSprayMotion
	{
		FQuat Rotation = FQuat::Identity;
		FVector VelocityMin = FVector::ZeroVector;
		FVector VelocityMax = FVector::ZeroVector;
		FVector Gravity = FVector::ZeroVector;
	};
	/** Cosmetic spin is deliberately included; it is not represented by Chaos angular velocity. */
	HAL_FUTURE_CREATION_API FVector SparkDirection(const FVector& RelativePointVelocity, const FVector& Normal);
	HAL_FUTURE_CREATION_API FContactSprayMotion ContactSprayMotion(const FHALBattleTopFXConfig& Config,
																   const FVector& TangentDirection,
																   const FVector& SurfaceNormal);
} // namespace HALBattleTopFX
