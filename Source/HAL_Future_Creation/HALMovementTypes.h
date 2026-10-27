#pragma once

#include "CoreMinimal.h"
#include "HALMovementTypes.generated.h"

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
	bool IsValidValues() const;
	bool IsValid() const { return IsValidValues() && EffectiveServerFrame >= 0; }
	bool IsUnset() const;
	bool Equals(const FHALMovementSnapshot& Other) const;
	void Serialize(FArchive& Ar);
	static bool CalculateScales(float AccelerationDelta, float SpeedDelta, float& Acceleration, float& Speed);
};

/** Inclusive interval of server frames with piecewise-constant, authoritative values. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALMovementTimeline
{
	GENERATED_BODY()
	static constexpr int32 MaximumEntries = 256;
	UPROPERTY() uint8 FormatVersion = 2;
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
	int32 ServerFrame = INDEX_NONE;
	int32 LocalFrame = INDEX_NONE;
	int32 HistoryFrames = 0;
	int32 RewindRequests = 0;
	int32 LastRewindFrame = INDEX_NONE;
	bool bReady = false;
	bool bAuthoritative = false;
	bool bReplay = false;
	bool bRecovery = false;
};
