#include "HALGameplayTags.h"

namespace HALGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_DamageImmune, "State.Combat.DamageImmune", "Blocks only the damage branch, never impact or hit acceptance.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combat_Source_Ball, "Combat.Source.Ball", "Ordinary ball launch source metadata.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Combat_Damage, "Data.Combat.Damage",
		"Signed HP delta passed by the authority Combat Resolver; negative values deal damage.");
}
