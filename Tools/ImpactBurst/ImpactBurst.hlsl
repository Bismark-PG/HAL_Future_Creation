// Comic impact burst, drawn entirely from the sprite UV. No texture, no flipbook, no Niagara.
// Look: hand-inked shards - flat white blades with a black contour and a heavier ink side, thrown
// outward from a hollow centre. They start joined by a spiked ring, then break apart, fly out and
// thin to slivers instead of fading. Inputs are Custom node pins supplied by
// Tools/ImpactBurst/CreateImpactBurstAssets.py.
//   UV            sprite coordinates, 0..1
//   Age01         0 at the contact frame, 1 at the end of the burst lifetime (written per frame by C++)
//   Seed          0..1, derived from the authority event so every client draws the same burst
//   Spikes        shard count; the authority maps impact strength onto this
//   Strength      0..1 impact strength; only trims brightness, the real size is the sprite size
//   Tier          impact tier as the authority enum value: 1 light, 2 medium, 3 heavy
//                   light   a few loose short shards, no ring - a glancing scrape
//                   medium  the spiked ring that breaks into flying shards
//                   heavy   medium plus a toothed shock ring racing out past the shards
//   ColorMode     0 plain fill, 1 every shard takes SourceColor, 2 shards alternate SourceColor / TargetColor
//   SourceColor   identity colour of the car that hit (or launched the ball), linear
//   TargetColor   identity colour of the car that was hit, linear; only read in mode 2
//   FillColor     flat comic fill, normally white
//   EdgeColor     impact tier colour; tints the root of each shard by TierTint
// The remaining pins are authored look constants exposed on the material instance.

float2 q0 = UV * 2.0 - 1.0;
float age = saturate(Age01);

// Pop: snap outward almost immediately, then keep drifting to the sprite edge over the tail.
float pop = 1.0 - pow(1.0 - saturate(age / max(PopFraction, 0.01)), 3.0);
float tail = saturate(TailGrowth);
float extent = max(lerp(saturate(StartScale) * (1.0 - tail), 1.0 - tail, pop) + tail * age, 0.01);

// Comic timing: the shards retract and thin to nothing, the opacity stays hard.
float thin = smoothstep(saturate(FadeStart), 1.0, age);
float lobes = max(floor(Spikes), 3.0);
float cell = 6.2831853 / lobes;
float irregular = saturate(Irregular);
float hollow = lerp(saturate(HollowStart), saturate(InnerRadius), smoothstep(0.0, 0.7, age));
float light = 1.0 - step(1.5, Tier);
float heavy = step(2.5, Tier);
float ringHalf = max(RingWidth, 0.0) * (1.0 + heavy * 0.5) * (1.0 - light) * (1.0 - smoothstep(0.0, max(RingEnd, 0.01), age));
// Heavy only: a thin ring with a toothed outer edge that outruns the shards, then thins away.
float waveRadius = lerp(saturate(HollowStart), 0.9, pow(age, 0.55));
float waveLife = heavy * (1.0 - smoothstep(0.45, 0.95, age));
float waveHalf = max(WaveWidth, 0.0) * waveLife;
float aa = max(fwidth(length(q0)) / extent, 1e-4) * 1.2;

// Pass 0 is the shape itself, pass 1 the same shape shifted down-right: the heavy ink side.
float silhouette = -1.0;
float body = -1.0;
float rootBand = 0.0;
float secondCar = 0.0;
for (int pass = 0; pass < 2; ++pass)
{
	float2 q = (q0 - float2(ShadowOffset, ShadowOffset) * pass) / extent;
	float r = length(q);
	float u = frac(atan2(q.y, q.x) / 6.2831853 + Seed) * lobes;
	float field = ringHalf > 0.0 ? ringHalf - abs(r - hollow) : -1.0;
	if (waveHalf > 0.0)
	{
		float tooth = abs(frac(u * 2.0 + Seed * 7.0) - 0.5) * 2.0;
		float outer = waveRadius + waveHalf + max(WaveTeeth, 0.0) * waveLife * tooth * tooth;
		// One antialiasing width of bias retires the ring before it degrades into a hairline circle.
		field = max(field, min(r - (waveRadius - waveHalf), outer - r) - aa);
	}
	float root = 0.0;
	float odd = 0.0;
	// Neighbouring cells too: a bent shard may lean across its own cell border.
	for (int k = -1; k <= 1; ++k)
	{
		float id = floor(u) + k;
		float a = u - id - 0.5;
		float wrapped = fmod(id + lobes, lobes) + Seed * 31.0;
		float h1 = frac(sin(wrapped * 12.9898 + 1.3) * 43758.5453);
		float h2 = frac(sin(wrapped * 78.233 + 4.1) * 24634.6345);
		float h3 = frac(sin(wrapped * 39.346 + 7.7) * 35734.7331);
		float h4 = frac(sin(wrapped * 93.989 + 2.9) * 17431.3117);

		float h5 = frac(sin(wrapped * 57.531 + 5.3) * 28461.9173);
		// Light keeps only some of the shards and shortens them.
		float keep = 1.0 - light * step(h5, saturate(LightDropout));
		float r1 = (0.97 - 0.45 * irregular * h1) * lerp(1.0, saturate(LightLength), light);
		float r0 = lerp(hollow, r1, thin * 0.55);
		float span = max(r1 - r0, 1e-3);
		float t = (r - r0) / span;
		float tc = saturate(t);
		float side = h3 < 0.5 ? -1.0 : 1.0;
		// Centre line: a random lean plus a sweep that grows toward the tip.
		float c = (h2 - 0.5) * 0.3 * irregular + side * Bend * tc * tc * 0.3;
		float s = (a - c) * cell * r;
		float wmax = max(ShardWidth * 0.5 * cell * lerp(r0, r1, 0.5) * (0.65 + 0.35 * h4) * (1.0 - thin) * keep, 1e-4);
		// Blade: full width on the outside of the sweep, half on the inside, pointed at the tip.
		float w = wmax * (s * side > 0.0 ? 1.0 : 0.5) * pow(1.0 - tc, max(TipPower, 0.05));
		// Torn root: a sawtooth eats into the inner end.
		float notch = abs(frac(s / wmax * 1.1 + h4) - 0.5) * 2.0;
		float f = min(min(w - abs(s), (t - saturate(Notch) * notch * 0.35) * span), (1.0 - t) * span);
		// A left-out shard must not leave a hairline behind.
		if (keep > 0.5 && f > field) { field = f; root = 1.0 - step(saturate(TierBand), t); odd = step(1.0, fmod(fmod(id + lobes, lobes), 2.0)); }
	}
	if (pass == 0) { body = field; rootBand = root; secondCar = odd; }
	silhouette = max(silhouette, field);
}

float cover = smoothstep(-aa, aa, silhouette);
float fill = smoothstep(-aa, aa, body - max(InkWidth, 0.0));
float gain = lerp(1.0, max(StrengthGain, 0.0), saturate(Strength)) * max(FillGain, 0.0);
// Car identity: washed out and lifted toward white so the ink still reads and dark cars stay visible.
// The ring and the shock ring belong to the car that hit; in mode 2 every other shard is the car that was hit.
float3 car = lerp(SourceColor, TargetColor, step(1.5, ColorMode) * secondCar);
car = lerp(car, dot(car, float3(0.2126, 0.7152, 0.0722)).xxx, saturate(Desaturate));
car = lerp(car, float3(1.0, 1.0, 1.0), saturate(ColorLift));
float3 fillBase = lerp(FillColor, FillColor * car, step(0.5, ColorMode));
float3 paint = lerp(fillBase, EdgeColor, saturate(TierTint) * rootBand) * gain;
float3 rgb = lerp(InkColor, paint, fill);

// Only the very last frames soften, so a dropped frame never leaves a hard pop-off.
float fade = 1.0 - smoothstep(0.92, 1.0, age);
return float4(rgb, saturate(cover * fade * saturate(Opacity)));
