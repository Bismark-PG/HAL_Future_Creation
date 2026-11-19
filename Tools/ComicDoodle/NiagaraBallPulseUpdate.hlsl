// Niagara module NM_BallPulseUpdate (Particle Update, after ParticleState), emitter PulseRings of NS_BallInkTrail.
// Pasted into /Game/Assets/FX/Modules/NM_BallPulseUpdate through the Monolith editor connection
// (create_module_from_hlsl); this file is the source of record for that text and its input list.
// Draws "on twos": the ring's size, line widths and the material's boil seed only change when Age crosses a 1/Fps
// boundary. The ring grows (ease out) while its colour band and ink thin out; once the ink is thinner than MinInk
// the whole ring stops being drawn - it ends by thinning, it never fades.
// All lengths are multiples of Width (bound to User.TrailWidth, cm); the material receives them in cm.
// Inputs:
//   Width       float  User.TrailWidth (cm)
//   SizeStart   float  ring diameter across the travel at the first drawing, x Width
//   SizeEnd     float  ring diameter across the travel at the end of life, x Width
//   GrowPower   float  ease out of the growth (1 linear, > 1 grows fast first)
//   Band        float  colour band full width at birth, x Width
//   Ink         float  ink width on each side of the band at birth, x Width
//   ThinPower   float  shape of the thinning (1 linear, < 1 thins fast late, > 1 thins fast early)
//   MinInk      float  ink width below which the ring is no longer drawn, x Width
//   Boil        float  radial outline wobble, x Width (new shape on every drawing)
//   HoleMin     float  smallest clear gap inside the ring along the travel, x Width. A young, squashed ring with
//                      thick ink would otherwise close up into a solid pinched lozenge; this widens the short axis
//                      just enough to keep the hole open (squash is only loosened while the line is thick)
//   Fps         float  drawings per second
// Reads DynamicMaterialParameter3 written by NM_BallPulseSpawn (x squash, y size multiplier, z start pulse flag).
// Writes: SpriteSize (ring plus room for line and boil),
//         DynamicMaterialParameter (x stepped age at the start of the drawing, y drawing seed, z band half width cm,
//         w ink cm), DynamicMaterialParameter1 (x semi axis across cm, y semi axis along cm, z boil cm, w start pulse).

float fps = max(Fps, 1.0);
float life = max(Particles.Lifetime, 1e-3);
float drawing = floor(Particles.Age * fps);
// Stepped normalised age at the start of the drawing: the first drawing is exactly SizeStart, Band and Ink. The
// ring disappears on the drawing where the thinned ink falls under MinInk (with the default values the last one or
// two drawings), not by fading.
float t = saturate(drawing / (fps * life));
float3 stored = Particles.DynamicMaterialParameter3.xyz;
float grow = 1.0 - pow(1.0 - t, max(GrowPower, 0.1));
float across = 0.5 * Width * lerp(SizeStart, SizeEnd, grow) * stored.y;
float thin = pow(1.0 - t, max(ThinPower, 0.1));
float inkCm = Ink * Width * thin;
// A discarded ring (Lifetime 0, see NM_BallPulseSpawn) is never drawn, not even on its spawn frame.
float shown = (inkCm >= MinInk * Width ? 1.0 : 0.0) * (Particles.Lifetime > 1e-3 ? 1.0 : 0.0);
float bandHalf = 0.5 * Band * Width * thin * shown;
float along = max(across * stored.x, min(bandHalf + inkCm + max(HoleMin, 0.0) * Width, across));
float boilCm = Boil * Width;
// Sprite room around the band centre line: line, ink, boil, plus 3% of Width for antialiasing.
float pad = bandHalf + inkCm + boilCm + 0.03 * Width;
Particles.SpriteSize = 2.0 * float2(across + pad, along + pad) * shown;
Particles.DynamicMaterialParameter = float4(t, drawing + Particles.MaterialRandom * 17.0, bandHalf, inkCm * shown);
Particles.DynamicMaterialParameter1 = float4(across, along, boilCm, stored.z);
