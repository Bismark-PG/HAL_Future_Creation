// Niagara module NM_BallPulseRate (Emitter Update, before SpawnRate), emitter PulseRings of NS_BallInkTrail.
// Pasted into /Game/Assets/FX/Modules/NM_BallPulseRate through the Monolith editor connection
// (create_module_from_hlsl); this file is the source of record for that text and its input list.
// Turns the three spawning rules into one rate that the stock SpawnRate module reads (its SpawnRate input is bound
// to Emitter.PulseRate):
//   distance  one ring every Spacing x Width of travel      -> rate = ball speed / (Spacing x Width)
//   ceiling   never more than MaxRate rings per second       (a fast ball would otherwise stack rings densely)
//   floor     never fewer than MinRate rings per second      (a slow or nearly still dangerous ball keeps pulsing)
// SpawnRate carries its own fractional remainder, so a rate that changes every frame still spaces rings evenly.
// Inputs:
//   Width    float  User.TrailWidth (cm)
//   Spacing  float  travel between rings, x Width
//   MinRate  float  rings per second, lower bound
//   MaxRate  float  rings per second, upper bound
// Writes: Emitter.PulseRate (rings per second).

float planarSpeed = length(Engine.Owner.Velocity.xy);
float lowRate = max(MinRate, 0.0);
float highRate = max(MaxRate, lowRate);
Emitter.PulseRate = clamp(planarSpeed / max(Spacing * Width, 1.0), lowRate, highRate);
