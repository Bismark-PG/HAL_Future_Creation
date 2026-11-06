#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/** Cosmetic seconds copied from the validated FX-2 recipe. No asset or gameplay duration ownership. */
struct HAL_FUTURE_CREATION_API FHALStatusFXTiming
{
	float FadeInSeconds=0;
	float FadeOutSeconds=0;
	float ResidueSeconds=0;
	bool IsValid() const;
	bool operator==(const FHALStatusFXTiming& Other) const;
};

/** An allocated source lease is usable only in its original runtime/binding and until explicitly removed. */
struct HAL_FUTURE_CREATION_API FHALStatusFXSourceToken
{
	bool IsValid() const { return Binding.IsValid() && Id!=0; }
	bool operator==(const FHALStatusFXSourceToken& Other) const { return Binding==Other.Binding && Id==Other.Id; }
private:
	friend class FHALStatusFXRuntime;
	FGuid Binding;
	uint64 Id=0;
};

enum class EHALStatusFXPhase : uint8 { Waiting, Active, Fading, Residue };

/** Expected target for an asynchronous availability result; protects rebind and same-tag replacement. */
struct HAL_FUTURE_CREATION_API FHALStatusFXInstanceHandle
{
	FGameplayTag Tag;
	FGuid Binding;
	uint64 Id=0;
};

/** Value-only observation at the last accepted timestamp; instance identity is Binding + InstanceId. */
struct HAL_FUTURE_CREATION_API FHALStatusFXInstanceSnapshot
{
	FGameplayTag Tag;
	FGuid Binding;
	uint64 InstanceId=0;
	EHALStatusFXPhase Phase=EHALStatusFXPhase::Waiting;
	int32 SourceCount=0;
	bool bAvailable=false;
	double ClockSeconds=0;
	double PhaseSeconds=0;
	/** Active/fade envelope only. Residue is a separate phase with intensity zero, not a brightness restart. */
	float Intensity=0;
	FHALStatusFXInstanceHandle GetHandle() const { return {Tag,Binding,InstanceId}; }
};

/** One vehicle's local ledger. Explicit monotonic timestamps, no Tick/UObject/resource/GAS ownership.
 * FX-3B owns this value and drives availability; FX-3C adapts real observations into source leases.
 * Never copy a ledger between vehicles: that would alias its source/binding identity.
 */
class HAL_FUTURE_CREATION_API FHALStatusFXRuntime
{
public:
	FHALStatusFXRuntime();
	FHALStatusFXRuntime(const FHALStatusFXRuntime&)=delete;
	FHALStatusFXRuntime& operator=(const FHALStatusFXRuntime&)=delete;
	FHALStatusFXRuntime(FHALStatusFXRuntime&&)=delete;
	FHALStatusFXRuntime& operator=(FHALStatusFXRuntime&&)=delete;
	FHALStatusFXSourceToken AcquireSource();
	bool HasSource(const FHALStatusFXSourceToken& Source) const;
	/** First submit binds one exact tag. InitialClockSeconds seeds a newly created instance after a late recipe;
	 * refresh/re-entry never replaces the existing clock. The driver retains pending observation times.
	 */
	bool Submit(const FHALStatusFXSourceToken& Source,FGameplayTag Tag,const FHALStatusFXTiming& Timing,double Now,double InitialClockSeconds=0);
	/** Revokes the lease permanently; re-entry uses a new lease and may reuse a fading/residual instance. */
	bool RemoveSource(const FHALStatusFXSourceToken& Source,double Now);
	/** New instances wait by default. Existing tails keep aging while targets are unavailable. */
	bool SetAvailable(const FHALStatusFXInstanceHandle& Expected,bool Available,double Now);
	bool Advance(double Now);
	/** Owner/ASC/Avatar lifecycle boundary. Old leases and instance identities become unusable. */
	void ResetBinding();
	FGuid GetBinding() const { return Binding; }
	int32 GetInstanceCount() const { return Instances.Num(); }
	int32 GetSourceLeaseCount() const { return Sources.Num(); }
	int32 GetActiveSourceCount() const;
	bool GetSnapshot(FGameplayTag Tag,FHALStatusFXInstanceSnapshot& Out) const;
	TArray<FHALStatusFXInstanceSnapshot> GetSnapshots() const;
	/** Phase/envelope work only. Future curve/spatial rendering may independently need updates. */
	bool NeedsPhaseUpdate() const;
private:
	struct FInstance
	{
		FHALStatusFXTiming Timing;
		uint64 Id=0;
		int32 Sources=0;
		bool Available=false;
		EHALStatusFXPhase Phase=EHALStatusFXPhase::Waiting;
		double StartedAt=0,PhaseStartedAt=0,EnvelopeAt=0,ReleasedAt=0;
		float EnvelopeFrom=0,ReleaseIntensity=0;
	};
	bool AcceptsTime(double Now) const;
	static bool IsAliveAt(const FInstance& Instance,double Now);
	static float EnvelopeAt(const FInstance& Instance,double Now);
	FGuid Binding;
	uint64 NextSourceId=1,NextInstanceId=1;
	double Time=0;
	TMap<uint64,FGameplayTag> Sources;
	TMap<FGameplayTag,FInstance> Instances;
};
