#pragma once
#include "CoreMinimal.h"
#include "HALSideShiftMotion.generated.h"

/** Immutable authority-authored side-step. Direction never follows subsequent chassis yaw. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALSideShiftMotion
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Translation", meta=(Units="cm", ClampMin="10", ClampMax="2000")) float Distance = 300;
	UPROPERTY(EditAnywhere, Category="Translation", meta=(Units="s", ClampMin="0.1", ClampMax="5")) float DurationSeconds = .24f;
	UPROPERTY(EditAnywhere, Category="Translation", meta=(Units="cm/s^2", ClampMin="100", ClampMax="100000")) float MaximumAcceleration = 100000;
	/** Net lateral span of the enhanced drift, not distance travelled along its curve. */
	UPROPERTY(EditAnywhere, Category="Drift Arc", meta=(Units="cm", ClampMin="10", ClampMax="2000")) float DriftLateralSpan = 450;
	/** Forward wrapping depth / lateral span. 0=flat, 1=quarter circle, 2=deeper ellipse. */
	UPROPERTY(EditAnywhere, Category="Drift Arc", meta=(ClampMin="0", ClampMax="2")) float DriftArcBend = 1;
	UPROPERTY(EditAnywhere, Category="Drift", meta=(Units="s", ClampMin="0.05", ClampMax="5", ToolTip="Start window; an arc must begin before the translation finishes.")) float DriftSeconds = .3f;
	UPROPERTY(EditAnywhere, Category="Drift", meta=(Units="deg", ClampMin="1", ClampMax="150", ToolTip="One steering press during the drift window captures this relative heading once.")) float DriftTurnDegrees = 90;
	UPROPERTY(EditAnywhere, Category="Drift", meta=(ClampMin="0.05", ClampMax="1")) float DriftInputThreshold = .2f;
	UPROPERTY(EditAnywhere, Category="Drift", meta=(Units="deg/s", ClampMin="30", ClampMax="1440", ToolTip="Maximum turn speed; the angle controller brakes before reaching the captured heading.")) float DriftYawRate = 600;
	UPROPERTY(EditAnywhere, Category="Drift", meta=(ClampMin="1", ClampMax="100", ToolTip="Critically damped heading response in inverse seconds; larger values settle faster.")) float DriftResponse = 33.333333f;
	UPROPERTY(EditAnywhere, Category="Drift", meta=(ClampMin="1", ClampMax="1000", ToolTip="Maximum skill steering angular acceleration, radians per second squared.")) float MaximumYawAcceleration = 444.44445f;
	UPROPERTY() FGuid Activation;
	UPROPERTY() bool bActive = false;
	/** Cast-time modifier travels with the skill request; its release is a reliable edge. */
	UPROPERTY() bool bDriftAtStart = false;
	UPROPERTY() bool bDriftReleased = false;
	UPROPERTY() FVector Direction = FVector::ZeroVector;
	UPROPERTY() FVector InitialForward = FVector::ForwardVector;
	UPROPERTY() FVector Origin = FVector::ZeroVector;
	UPROPERTY() FVector InitialVelocity = FVector::ZeroVector;
	UPROPERTY() float InitialLateralSpeed = 0;
	UPROPERTY() int32 StartFrame = INDEX_NONE;
	UPROPERTY() int32 EndFrame = INDEX_NONE;
	UPROPERTY() int32 ShiftFrames = 0;
	UPROPERTY() int32 DriftFrames = 0;
	bool IsValidTuning() const;
	bool IsValid(bool bStamped) const;
	bool IsSliding(int32 Frame) const;
	bool IsDrifting(int32 Frame) const;
	/** Average velocity over this exact physics interval; smooth position profile integrates to Distance. */
	float StepSpeed(int32 Frame, float FixedDelta) const;
	FVector ArcOffset(float Progress,float Span) const;
	/** Tangent yaw as a fraction of a quarter turn; uses the same ellipse as translation. */
	float ArcHeadingProgress(float Progress) const;
	float HeadingAcceleration(float ErrorRadians, float CurrentYawRate, float FixedDelta,float ReferenceRate=0,float ReferenceAcceleration=0) const;
	void Stamp(const FHALSideShiftMotion* Previous, int32 Frame, float FixedDelta);
	void Serialize(FArchive& Ar);
	bool operator==(const FHALSideShiftMotion& Other) const = default;
};

/** Small physics-owned latch, restored with Chaos history; no GAS/UObject state. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALSideShiftTurnState
{
	GENERATED_BODY()
	UPROPERTY() FGuid Activation;
	UPROPERTY() float OriginYaw = 0;
	UPROPERTY() float TargetYaw = 0;
	UPROPERTY() bool bConsumed = false;
	UPROPERTY() bool bHasTarget = false;
	UPROPERTY() bool bReleased = false;
	UPROPERTY() int32 ArcStartFrame = INDEX_NONE;
	UPROPERTY() int32 ArcFrames = 0;
	UPROPERTY() float ArcSpan = 0;
	UPROPERTY() FVector ArcOrigin = FVector::ZeroVector;
	UPROPERTY() FVector ArcBaseVelocity = FVector::ZeroVector;
	UPROPERTY() float ReleaseProgress = 1;
	bool Update(const FHALSideShiftMotion& Motion,int32 Frame,float YawDegrees,float YawRate,float Steering,bool bHandbrake,float Delta,float& Acceleration,
		const FVector& Position=FVector::ZeroVector,const FVector& GameplayImpulse=FVector::ZeroVector);
	void TrackGameplayImpulse(const FHALSideShiftMotion& Motion,int32 Frame,const FVector& Impulse);
	float ArcProgress(int32 Frame) const;
	bool IsArcDriving(const FHALSideShiftMotion& Motion,int32 Frame) const;
	FVector ArcStepVelocity(const FHALSideShiftMotion& Motion,int32 Frame,float Delta) const;
	/** Turn initial driving momentum with the arc; explicit gameplay impulses stay in world space. */
	FVector ArcBackgroundVelocity(const FHALSideShiftMotion& Motion,int32 Frame) const;
	void Cancel(FGuid Source) { *this={}; Activation=Source; bConsumed=Source.IsValid(); bReleased=bConsumed; }
	bool IsValid() const;
	bool Equals(const FHALSideShiftTurnState& Other) const;
	void Serialize(FArchive& Ar);
};
