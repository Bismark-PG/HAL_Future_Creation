// Niagara module NM_BallFlameUpdate (Particle Update, after ParticleState), used by both emitters of NS_BallInkTrail.
// Pasted into /Game/Assets/FX/Modules/NM_BallFlameUpdate through the Monolith editor connection
// (create_module_from_hlsl); this file is the source of record for that text and its input list.
// Replaces SolveForcesAndVelocity: integrates drag and velocity itself, then draws "on twos" - size, sideways
// sway and the material's drawing counter only change when Age crosses a 1/Fps boundary.
// The head direction (SpriteAlignment) is set once at spawn and left alone here.
// Inputs:
//   Width          float  User.TrailWidth (cm)
//   SizeMin/Max    float  as in NM_BallFlameSpawn (x Width)
//   Aspect         float  sprite length / width; must match NM_BallFlameSpawn
//   Fps            float  drawings per second (12 = drawn on twos at 24 fps)
//   StartScale     float  size at the first drawing
//   GrowPeak       float  stepped normalised age at which the puff is fullest
//   GrowScale      float  size at GrowPeak (the swell)
//   ShrinkStart    float  stepped normalised age at which the shrink begins
//   ShrinkPower    float  shape of the shrink (1 linear, > 1 holds longer then collapses)
//   MinScale       float  below this share of its full size a puff is not drawn at all (no lingering dots)
//   SwayAmp        float  sideways sway amplitude at the end of life, x Width (grows with age)
//   SwayHz         float  sway cycles per second
//   Drag           float  1/s, exponential velocity decay
//   Core           float  1 inner flame allowed, 0 never
//   StartWindow    float  seconds after activation; must match NM_BallFlameSpawn
//   StartSizeScale float  size of puffs born at activation, growing to 1 over StartWindow (by birth time
//                         Emitter.Age - Particles.Age); must match NM_BallFlameSpawn
// The last drawing always has size 0: n is measured at the END of the current drawing, so the puff ends by
// shrinking to nothing instead of popping out at a visible size.

float dt = Engine.DeltaTime;
Particles.Velocity = Particles.Velocity * exp(-max(Drag, 0.0) * dt);
Particles.Position = Particles.Position + Particles.Velocity * dt;
float3 heading = Particles.SpriteAlignment;
float3 side = float3(-heading.y, heading.x, 0.0);

float fps = max(Fps, 1.0);
float life = max(Particles.Lifetime, 1e-3);
float drawing = floor(Particles.Age * fps);
float previousDrawing = floor(max(Particles.Age - dt, 0.0) * fps);
float n = saturate((drawing + 1.0) / (fps * life));
float previousN = saturate((previousDrawing + 1.0) / (fps * life));
float phase = Particles.MaterialRandom * 6.2831853;
float swayNow = SwayAmp * Width * n * sin(6.2831853 * SwayHz * drawing / fps + phase);
float swayBefore = SwayAmp * Width * previousN * sin(6.2831853 * SwayHz * previousDrawing / fps + phase);
// Sideways sway is applied as a position jump only when the drawing changes (held between drawings).
Particles.Position = Particles.Position + side * (swayNow - swayBefore);

float grow = n < GrowPeak ? lerp(StartScale, GrowScale, n / max(GrowPeak, 1e-3)) : GrowScale;
float shrink = 1.0 - pow(saturate((n - ShrinkStart) / max(1.0 - ShrinkStart, 1e-3)), max(ShrinkPower, 0.1));
float scale = grow * shrink;
scale = scale < MinScale ? 0.0 : scale;
float baseSize = Width * lerp(SizeMin, SizeMax, Particles.MaterialRandom);
float birthAge = max(Emitter.Age - Particles.Age, 0.0);
scale = scale * lerp(StartSizeScale, 1.0, saturate(birthAge / max(StartWindow, 1e-3)));
Particles.SpriteSize = float2(baseSize, baseSize * Aspect) * scale;
Particles.DynamicMaterialParameter = float4(drawing, n, Core, Particles.MaterialRandom);
