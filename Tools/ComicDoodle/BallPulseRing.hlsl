// Launched-ball pulse ring ("force field" layers left behind the ball), drawn on a flat Niagara sprite. No texture.
// Look: a hand inked ellipse band in the launcher's colour with black ink on BOTH sides (inner and outer), a thin
// pale line along the middle of the band while the ring is young, and an outline that boils a little on every
// drawing. The ring never fades: Niagara thins the band and its ink down to nothing, then stops drawing it.
// All geometry is decided by Niagara in centimetres (multiples of User.TrailWidth), so the band keeps one world
// width all the way round the ellipse whatever the sprite size; the scalars here only shape the drawing.
// Inputs are Custom node pins supplied by Tools/ComicDoodle/CreateComicDoodleAssets.py.
//   UV       sprite coordinates, 0..1. U runs across the ball's travel, V along it (custom alignment)
//   Color    particle colour (linear RGB): the launcher's identity colour, used as is
//   Alpha    particle alpha; multiplies the coverage (Niagara keeps it at 1)
//   Size     particle sprite size in cm: x across, y along. Includes the padding Niagara leaves for line and boil
//   Dynamic  Dynamic Parameter 0 (Niagara Particles.DynamicMaterialParameter):
//            x stepped normalised age, start of the current drawing (0..1)
//            y drawing seed (changes on every drawing, differs per ring)
//            z colour band half width, cm (0 = ring not drawn)
//            w ink width on each side of the band, cm
//   Dynamic1 Dynamic Parameter 1 (Niagara Particles.DynamicMaterialParameter1):
//            x ellipse semi axis across the travel, cm (centre line of the band)
//            y ellipse semi axis along the travel, cm
//            z boil amplitude, cm (radial wobble of the whole band, new each drawing)
//            w 1 on the start pulse fired when the trail starts, else 0

float AgeN = Dynamic.x;
float Seed = Dynamic.y;
float BandHalf = Dynamic.z;
float Ink = max(Dynamic.w, 0.0);
float2 Semi = max(Dynamic1.xy, float2(1.0, 1.0));
float BoilCm = max(Dynamic1.z, 0.0);
float StartFlag = saturate(Dynamic1.w);

// p: centimetres from the ring centre, x across the travel, y along it.
float2 p = float2((UV.x - 0.5) * Size.x, (0.5 - UV.y) * Size.y);
// First order signed distance to the ellipse (positive outside), so the band keeps one width all the way round.
float2 k = p / Semi;
float e = length(k);
float slope = length(p / (Semi * Semi)) / max(e, 1e-4);
float dist = (e - 1.0) / max(slope, 1e-6);
// Boil: the whole band wobbles radially, with a fresh shape on every drawing. Whole wave counts keep it seamless.
float theta = atan2(k.y, k.x);
float s1 = frac(Seed * 0.6180339) * 6.2831853;
float s2 = frac(Seed * 0.4142136) * 6.2831853;
float s3 = frac(Seed * 0.7320508) * 6.2831853;
float waves = max(floor(BoilWaves + 0.5), 1.0);
dist -= BoilCm * (0.6 * sin(theta * waves + s1) + 0.4 * sin(theta * (waves + 2.0) + s2));
// Hand drawn pressure: the band swells and thins a little around the ring.
float band = max(BandHalf, 0.0) * (1.0 + clamp(Pressure, 0.0, 0.9) * sin(theta * 2.0 + s3));
float d = abs(dist);
float aa = max(fwidth(dist), 1e-3) * 0.75;
float drawn = BandHalf > 0.0 ? 1.0 : 0.0;
float cover = drawn * smoothstep(band + Ink + aa, band + Ink - aa, d);
float fill = drawn * smoothstep(band + aa, band - aa, d);
// Pale centre line: a share of the band, gone by normalised age CoreLife (the start pulse keeps it longer).
float coreLife = max(CoreLife, 1e-3) * lerp(1.0, max(StartCoreLife, 0.0), StartFlag);
float coreHalf = band * saturate(CoreWidth) * (1.0 - saturate(AgeN / coreLife));
float core = coreHalf > aa * 0.25 ? fill * smoothstep(coreHalf + aa, coreHalf - aa, d) : 0.0;

float3 paint = lerp(Color, float3(1.0, 1.0, 1.0), saturate(ColorLift)) * max(FillGain, 0.0);
// The core is lifted toward white and pushed brighter, so it stays a bright line after tonemapping, not grey.
float3 corePaint = lerp(Color, float3(1.0, 1.0, 1.0), saturate(CoreLift)) * max(FillGain, 0.0) * max(CoreGain, 0.0);
float3 rgb = lerp(InkColor, lerp(paint, corePaint, core), fill);
return float4(rgb, saturate(cover * saturate(Alpha) * saturate(Opacity)));
