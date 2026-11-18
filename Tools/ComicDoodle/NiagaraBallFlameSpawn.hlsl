// Niagara module NM_BallFlameSpawn (Particle Spawn), used by both emitters of NS_BallInkTrail.
// Pasted into /Game/Assets/FX/Modules/NM_BallFlameSpawn through the Monolith editor connection
// (create_module_from_hlsl); this file is the source of record for that text and its input list.
// All lengths are multiples of Width (bound to User.TrailWidth, cm), never absolute centimetres; speeds are
// multiples of Width per second.
// The system sits at the ball's centre. The sprite renderers pivot on the flame's head, so Position is where the
// head is drawn: BackOffset puts it at the ball's rear edge and the flame grows backward from there.
// Inputs (module inputs, set per emitter in the stack):
//   Width          float   User.TrailWidth
//   Color          float4  User.TrailColor (launcher identity colour)
//   SizeMin/Max    float   sprite width at scale 1, x Width (random per particle)
//   Aspect         float   sprite length / width; must match NM_BallFlameUpdate
//   LifeMin/Max    float   seconds
//   Inherit        float   share of the ball velocity a puff keeps at speed (the dragged-along feel)
//   MinBackSpeed   float   slowest a puff may fall back relative to the ball, x Width/s. Backward speed relative to
//                          the ball = max(ball speed x (1 - Inherit), MinBackSpeed x Width): a slow ball blows its
//                          flame back like a jet, so the trail never collapses onto the ball
//   MinValidSpeed  float   below this ball speed (x Width/s) the travel direction is not trusted (spawn frame,
//                          wall contact); such puffs are discarded (Lifetime 0) instead of pointing anywhere
//   BackOffset     float   birth point behind the ball centre along the travel direction, x Width (the drawn
//                          ball radius is about 0.67 Width)
//   DropHeight     float   birth point below the ball centre, x Width (0 = ball centre height)
//   SideSpeedMin/Max float  outward fling speed, x Width/s (0 for body puffs)
//   SideOffset     float   outward birth offset on a random side, x Width (tongues start on the ball's flanks)
//   SideJitter     float   extra symmetric random sideways birth offset, +- x Width (ragged body outline)
//   TailLean       float   how far the tail leans outward (away from the side the puff sits on), share of heading
//   LeanJitter     float   extra symmetric random lean
//   StartWindow    float   seconds after activation during which puffs get an extra random backward kick
//   StartBoost     float   that kick: backward speed x random 1..StartBoost, so the first puffs spread along the
//                          whole trail at once (1 = no kick; a kick throws the first puffs back over the launching car)
//   StartSizeScale float   size of puffs born at activation; grows linearly to 1 over StartWindow, so the flame
//                          right after launch stays short and does not lie on the launching car. Must match
//                          NM_BallFlameUpdate, which re-applies the same factor every frame from the birth time
//   Core           float   1 draws the inner flame, 0 never (stored for the material in DynamicMaterialParameter.z)
// Writes: Position, Velocity, Lifetime, MaterialRandom, SpriteSize, SpriteAlignment, Color, DynamicMaterialParameter.
// SpriteAlignment is the head direction and is fixed for the particle's life (NM_BallFlameUpdate never changes it).

float sizeRandom = rand(1.0f);
float lifeRandom = rand(1.0f);
float sideRandom = rand(1.0f);
float speedRandom = rand(1.0f);
float jitterRandom = rand(1.0f);
float leanRandom = rand(1.0f);
float boostRandom = rand(1.0f);
float3 ownerVelocity = Engine.Owner.Velocity;
float planarSpeed = length(ownerVelocity.xy);
// 1 when the travel direction can be trusted; 0 discards the puff (Lifetime 0, size 0) instead of pointing it anywhere.
float valid = (planarSpeed > max(MinValidSpeed, 0.0) * Width && planarSpeed > 1.0) ? 1.0 : 0.0;
float3 heading = valid > 0.5 ? float3(ownerVelocity.xy / max(planarSpeed, 1e-3), 0.0) : float3(1.0, 0.0, 0.0);
float3 side = float3(-heading.y, heading.x, 0.0);
float sideSign = sideRandom < 0.5 ? -1.0 : 1.0;
float offset = sideSign * SideOffset + (jitterRandom * 2.0 - 1.0) * SideJitter;
// The side the puff sits on decides where its tail leans (outward); a centred puff leans by its random sign.
float leanSign = abs(offset) > 1e-4 ? (offset > 0.0 ? 1.0 : -1.0) : sideSign;
float sideSpeed = lerp(SideSpeedMin, SideSpeedMax, speedRandom) * Width;
float backSpeed = max(planarSpeed * (1.0 - Inherit), max(MinBackSpeed, 0.0) * Width);
backSpeed = backSpeed * (Emitter.Age < StartWindow ? lerp(1.0, max(StartBoost, 1.0), boostRandom) : 1.0);
Particles.Position = Engine.Owner.Position + side * (offset * Width) - heading * (BackOffset * Width) - float3(0.0, 0.0, DropHeight * Width);
Particles.Velocity = heading * (planarSpeed - backSpeed) + side * (sideSign * sideSpeed);
Particles.Lifetime = valid * lerp(LifeMin, LifeMax, lifeRandom);
Particles.MaterialRandom = sizeRandom;
float baseSize = Width * lerp(SizeMin, SizeMax, sizeRandom);
float startSize = lerp(StartSizeScale, 1.0, saturate(Emitter.Age / max(StartWindow, 1e-3)));
Particles.SpriteSize = float2(baseSize, baseSize * Aspect) * (valid * startSize);
// Head direction: forward, leaning inward so the tail streams backward and outward.
float lean = leanSign * TailLean + (leanRandom * 2.0 - 1.0) * LeanJitter;
float3 headDirection = heading - side * lean;
Particles.SpriteAlignment = headDirection / max(length(headDirection), 1e-3);
Particles.Color = Color;
Particles.DynamicMaterialParameter = float4(0.0, 0.0, Core, sizeRandom);
