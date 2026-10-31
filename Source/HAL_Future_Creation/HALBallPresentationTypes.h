#pragma once

#include "CoreMinimal.h"
#include "HALBallPresentationTypes.generated.h"

class AActor;

/** Visual only. Does not change acquisition, joints, physics replication or launch permission. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALBallPresentationConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Controlled") bool bEnabled = true;
	UPROPERTY(EditAnywhere, Category="Controlled", meta=(ClampMin="5",ClampMax="60")) float SendRateHz = 20;
	UPROPERTY(EditAnywhere, Category="Controlled", meta=(ClampMin="0",ClampMax="0.25",Units="s")) float RelativeBlendSeconds = .05f;
	UPROPERTY(EditAnywhere, Category="Controlled", meta=(ClampMin="0.01",ClampMax="0.5",Units="s")) float HandoffSeconds = .12f;
	UPROPERTY(EditAnywhere, Category="Controlled", meta=(ClampMin="0.1",ClampMax="2",Units="s")) float StaleSeconds = .5f;
	UPROPERTY(EditAnywhere, Category="Controlled", meta=(ClampMin="0",ClampMax="0.2",Units="s")) float MaximumExtrapolationSeconds = .1f;
	UPROPERTY(EditAnywhere, Category="Controlled", meta=(ClampMin="100",ClampMax="10000",Units="cm")) float MaximumRelativeDistance = 2000;
	bool Validate(FString& Error) const;
};

/** Presentation sample identity must match the ball's existing gameplay state, never authorizes possession. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALBallRelativePose
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<AActor> Holder;
	UPROPERTY() uint32 StateSequence = 0;
	UPROPERTY() int32 PhysicsFrame = INDEX_NONE;
	/** World game time when GT published this PostSolve sample, not a claimed client clock. */
	UPROPERTY() float ServerPublishSeconds = 0;
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() FQuat Rotation = FQuat::Identity;
	UPROPERTY() FVector LinearVelocity = FVector::ZeroVector;
	UPROPERTY() FVector AngularVelocity = FVector::ZeroVector;
	bool IsValid(float MaximumDistance) const;
	bool IsNewerThan(const FHALBallRelativePose& Previous) const;
};

enum class EHALBallVisualMode : uint8 { Physics, Relative, Handoff };

namespace HALBallPresentation
{
	/** All coordinates are cm in the holder's unscaled rigid-body frame. */
	HAL_FUTURE_CREATION_API FTransform Compose(const FTransform& HolderPose, const FTransform& RelativePose);
	HAL_FUTURE_CREATION_API FTransform Extrapolate(const FHALBallRelativePose& Sample, float Age, float Limit);
	HAL_FUTURE_CREATION_API FTransform Blend(const FTransform& A, const FTransform& B, float Alpha);
}
