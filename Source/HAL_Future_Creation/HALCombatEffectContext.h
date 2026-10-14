#pragma once
#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "HALCombatEffectContext.generated.h"

/** Stable attribution survives optional source Actor/ASC destruction. No custom Globals needed. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALCombatEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()
	UPROPERTY()
	FGuid AttackId;
	UPROPERTY()
	uint32 PulseSequence = 0;
	UPROPERTY()
	FGameplayTag SourceTag;
	UPROPERTY()
	int32 InstigatorPlayerId = INDEX_NONE;
	UPROPERTY()
	FGameplayTagContainer EffectTags;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual FGameplayEffectContext* Duplicate() const override
	{
		auto* Copy = new FHALCombatEffectContext();
		*Copy = *this;
		if (GetHitResult()) { Copy->AddHitResult(*GetHitResult(), true); }
		return Copy;
	}
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
};
template<> struct TStructOpsTypeTraits<FHALCombatEffectContext> : TStructOpsTypeTraitsBase2<FHALCombatEffectContext>
{
	enum { WithNetSerializer = true, WithCopy = true };
};
