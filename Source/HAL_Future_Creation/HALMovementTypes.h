#pragma once

#include "CoreMinimal.h"
#include "HALMovementTypes.generated.h"

/** A regional contribution, independent of ordinary additive movement buffs.
 * The authority stamps the recovery curve once; every physics frame evaluates it analytically. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALMovementAreaChannel
{
	GENERATED_BODY()
	UPROPERTY() float OrdinaryDelta = 0;
	UPROPERTY() float Target = 1;
	UPROPERTY() float RecoverySeconds = 0.4f;
	UPROPERTY() float From = 1;
	UPROPERTY() int32 StartFrame = INDEX_NONE;
	UPROPERTY() int32 DurationFrames = 0;
	bool IsValid(bool bStamped) const;
	bool Equals(const FHALMovementAreaChannel& Other) const;
	float RegionalValue(int32 Frame) const;
	float EffectiveValue(int32 Frame) const;
	void Stamp(const FHALMovementAreaChannel* Previous, int32 Frame, float FixedDeltaSeconds);
	void Serialize(FArchive& Ar);
};

/** Absolute parameters for one Avatar; no GAS/UObject data crosses into physics. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALMovementSnapshot
{
	GENERATED_BODY()
	UPROPERTY() FGuid Epoch;
	UPROPERTY() uint32 Revision = 0;
	UPROPERTY() int32 EffectiveServerFrame = INDEX_NONE;
	UPROPERTY() float AccelerationScale = 1;
	UPROPERTY() float SpeedScale = 1;
	UPROPERTY() bool bControlsBlocked = false;
	UPROPERTY() float GripScale = 1;
	/** Legacy/native callers may supply constant scales. The GAS bridge supplies all three channels. */
	UPROPERTY() bool bHasAreaState = false;
	UPROPERTY() FHALMovementAreaChannel AreaAcceleration;
	UPROPERTY() FHALMovementAreaChannel AreaSpeed;
	UPROPERTY() FHALMovementAreaChannel AreaGrip;
	bool IsValidValues() const;
	bool IsValid() const;
	bool IsUnset() const;
	bool Equals(const FHALMovementSnapshot& Other) const;
	void Serialize(FArchive& Ar);
	void Stamp(const FHALMovementSnapshot* Previous, int32 Frame, float FixedDeltaSeconds);
	FVector EvaluateScales(int32 Frame) const;
	static bool CalculateScales(float AccelerationDelta, float SpeedDelta, float& Acceleration, float& Speed);
};

/** Inclusive interval of server frames with immutable, authoritative curve definitions. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALMovementTimeline
{
	GENERATED_BODY()
	static constexpr int32 MaximumEntries = 256;
	/** Version 3 combines Stun permission and the regional recovery curves; neither independent v2 is compatible. */
	UPROPERTY() uint8 FormatVersion = 3;
	UPROPERTY() FGuid Epoch;
	UPROPERTY() int32 StartFrame = INDEX_NONE;
	UPROPERTY() int32 EndFrame = INDEX_NONE;
	UPROPERTY() FHALMovementSnapshot Anchor;
	UPROPERTY() TArray<FHALMovementSnapshot> Changes;
	bool IsEmpty() const { return StartFrame == INDEX_NONE; }
	bool Validate() const;
	bool Query(int32 Frame, FHALMovementSnapshot& Out) const;
	bool Append(const FHALMovementSnapshot& Value, int32 WindowFrames);
	bool ExtendTo(int32 Frame, int32 WindowFrames);
	bool Merge(const FHALMovementTimeline& Incoming, int32 WindowFrames);
	void Trim(int32 WindowFrames);
	bool Serialize(FArchive& Ar);
};

/** Thread-safe published diagnostics; never a source of authoritative rules. */
struct FHALMovementStatus
{
	FHALMovementSnapshot Value;
	/** Immutable definition used by the next authority action, distinct from evaluated HUD values. */
	FHALMovementSnapshot Definition;
	int32 ServerFrame = INDEX_NONE;
	int32 LocalFrame = INDEX_NONE;
	int32 HistoryFrames = 0;
	int32 RewindRequests = 0;
	int32 LastRewindFrame = INDEX_NONE;
	bool bReady = false;
	bool bAuthoritative = false;
	bool bReplay = false;
	bool bRecovery = false;
	bool bAreaRecovering = false;
};
