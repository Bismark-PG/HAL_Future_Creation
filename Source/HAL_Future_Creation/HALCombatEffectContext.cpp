#include "HALCombatEffectContext.h"

bool FHALCombatEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bBaseSuccess = false;
	bool bTagsSuccess = false;
	bool bSourceTagSuccess = false;
	const bool bBaseMapped = FGameplayEffectContext::NetSerialize(Ar, Map, bBaseSuccess);
	Ar << AttackId << InstigatorPlayerId << PulseSequence;
	const bool bSourceTagMapped = SourceTag.NetSerialize(Ar, Map, bSourceTagSuccess);
	const bool bTagsMapped = EffectTags.NetSerialize(Ar, Map, bTagsSuccess);
	bOutSuccess = bBaseSuccess && bTagsSuccess && bSourceTagSuccess && !Ar.IsError();
	return bBaseMapped && bTagsMapped && bSourceTagMapped;
}
