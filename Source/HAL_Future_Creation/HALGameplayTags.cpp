#include "HALGameplayTags.h"

namespace HALGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stun, "State.Stun", "Blocks controls, abilities, ball acquisition and launch; does not freeze physics.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Stun, "Effect.Vehicle.Stun", "Shared timed Avatar-scoped stun.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combat_Source_Bomb, "Combat.Source.Bomb", "Bomb explosion: configured damage and authored impact tier.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combat_Source_SpringPunch, "Combat.Source.SpringPunch", "Environment-owned spring punch; fixed damage and authored impact tier.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Ability_Primary, "Input.Ability.Primary", "Local primary skill edge; never replayed by Network Physics.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ImpactImmunity, "Ability.Vehicle.ImpactImmunity", "Timed self impact immunity, with separate owner cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_MovementBoost, "Ability.Vehicle.MovementBoost", "Instant server action applies the configured timed movement boost; repeated presses refresh its GE.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Fail_VehicleState, "Ability.Fail.VehicleState", "Vehicle, match or effect configuration is not ready for this activation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_ImpactImmunity, "Cooldown.Ability.ImpactImmunity", "Owner-scoped cooldown, retained across Avatar replacement.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Scope_Avatar, "Effect.Scope.Avatar", "Remove this active effect when the current Avatar changes or is lost.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_ImpactImmunity, "Effect.Vehicle.ImpactImmunity", "Identifies the native timed immunity effect, separate from other immunity sources.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Movement_Boost, "Effect.Vehicle.MovementBoost", "Identifies the first movement boost family; debug removal preserves other movement GEs.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_Boost, "State.Movement.Boost", "Movement boost is active; actual scales are consumed from per-physics-frame history.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_ImpactImmune, "State.Combat.ImpactImmune", "Blocks rule impact/drop from every source, not natural Chaos contacts or damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combat_Source_Collision, "Combat.Source.Collision", "Server-adapted natural contact episode; never deals ordinary collision damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_DamageImmune, "State.Combat.DamageImmune", "Blocks only the damage branch, never impact or hit acceptance.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combat_Source_Ball, "Combat.Source.Ball", "Ordinary ball launch source metadata.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Combat_Damage, "Data.Combat.Damage",
		"Signed HP delta passed by the authority Combat Resolver; negative values deal damage.");
}
