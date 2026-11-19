// Niagara module NM_BallPulseSpawn (Particle Spawn), emitter PulseRings of NS_BallInkTrail.
// Pasted into /Game/Assets/FX/Modules/NM_BallPulseSpawn through the Monolith editor connection
// (create_module_from_hlsl); this file is the source of record for that text and its input list.
// A ring stays where it was born (world space, never follows the ball), lies flat (sprite facing +Z) and is aligned
// with the ball's travel. At every speed it is the same squashed ellipse aligned with the ball's current velocity.
// The direction is used down to HeadingMinSpeed (a few cm/s), so a ball that is slowing down keeps its rings behind
// it. A fully still ball (rare while Launched: below 250 cm/s for 0.5 s the ball returns to Free) has no direction,
// so rings born then are discarded (Lifetime 0, NM_BallPulseUpdate draws nothing) instead of being put on an
// arbitrary side - that arbitrary side read as "rings under the ball". A held "last direction" would need an emitter
// attribute that the particle script can read; attempts with custom HLSL and bound inputs did not compile ("hasn't
// been encountered yet" / "read before being set"), see Docs/Presentation/Comic_Doodle_FX_CN_v0.1.md section 3.
// The ring centre sits behind the ball along that direction: BackOffset when fast, growing smoothly to
// BackOffsetSlow when slow, so a slow ring shows behind the drawn ball (radius about 0.67 Width) instead of under it.
// NM_BallPulseUpdate owns every size, so this module leaves SpriteSize at 0.
// Inputs (module inputs, set in the stack):
//   Width          float   User.TrailWidth (cm)
//   Color          float4  User.TrailColor (launcher identity colour, used unchanged)
//   LifeMin/Max    float   seconds
//   Squash         float   ring length along the travel / across it (every speed)
//   SlowSpeed      float   ball speed (x Width/s) at and below which the centre sits at BackOffsetSlow
//   FastSpeed      float   ball speed (x Width/s) at and above which the centre sits at BackOffset; smoothstep between
//   BackOffset     float   ring centre behind the ball centre when fast, x Width
//   BackOffsetSlow float   ring centre behind the ball centre when slow or still, x Width
//   DropHeight     float   ring height below the ball centre, x Width (0 = ball centre height)
//   StartWindow    float   seconds after activation; rings born inside it are start pulses (the burst)
//   StartScale     float   size multiplier of a start pulse
//   HeadingMinSpeed float  ball speed (x Width/s) above which the velocity direction is used; slower: no ring
// Writes: Position, Velocity, Lifetime, MaterialRandom, SpriteSize, SpriteFacing, SpriteAlignment, Color,
//         DynamicMaterialParameter3 (storage only, never read by the material: x squash, y size multiplier,
//         z start pulse flag).

float lifeRandom = rand(1.0f);
float seedRandom = rand(1.0f);
float planarSpeed = length(Engine.Owner.Velocity.xy);
float slowCm = max(SlowSpeed, 0.0) * Width;
// Hand written smoothstep: the CPU vector VM has no smoothstep operation (the spawn script fails to compile).
float blend = saturate((planarSpeed - slowCm) / max(FastSpeed * Width - slowCm, 1.0));
float fast = blend * blend * (3.0 - 2.0 * blend);
float2 ownerPlanar = Engine.Owner.Velocity.xy;
float headingSpeed = max(HeadingMinSpeed * Width, 0.5);
float moving = planarSpeed > headingSpeed ? 1.0 : 0.0;
float3 heading = moving > 0.5 ? float3(ownerPlanar / max(planarSpeed, 1e-3), 0.0) : float3(1.0, 0.0, 0.0);
float back = lerp(BackOffsetSlow, BackOffset, fast) * Width;
float startPulse = Emitter.Age < StartWindow ? 1.0 : 0.0;
Particles.Position = Engine.Owner.Position - heading * back - float3(0.0, 0.0, DropHeight * Width);
Particles.Velocity = float3(0.0, 0.0, 0.0);
Particles.Lifetime = moving * lerp(LifeMin, LifeMax, lifeRandom);
Particles.MaterialRandom = seedRandom;
Particles.SpriteSize = float2(0.0, 0.0);
Particles.SpriteFacing = float3(0.0, 0.0, 1.0);
Particles.SpriteAlignment = heading;
Particles.Color = Color;
Particles.DynamicMaterialParameter3 = float4(Squash, lerp(1.0, StartScale, startPulse), startPulse, 0.0);
