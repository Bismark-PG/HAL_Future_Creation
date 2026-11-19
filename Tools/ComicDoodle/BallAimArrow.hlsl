// Held-ball aim arrow, drawn on a flat ground quad from its UV. No texture.
// Look: a short fat hand-drawn arrow in the holder's colour with a black contour and a heavier ink side.
// The outline "boils": it is redrawn a few times per second with a slightly different wobble, like a
// looping pencil animation. Only the line wobbles - the arrow's direction is the quad's own +X and never moves.
// Inputs are Custom node pins supplied by Tools/ComicDoodle/CreateComicDoodleAssets.py.
//   UV      quad coordinates, 0..1; U runs along the arrow when FlipAxes is 0, V when it is 1
//   Time    seconds
//   Color   holder identity colour, linear (written once per capture by C++)

float2 uv = FlipAxes > 0.5 ? UV.yx : UV;
// x: -1 tail .. +1 tip. y: across.
float2 p0 = float2(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0);
if (Reverse > 0.5) { p0.x = -p0.x; }
float pose = floor(Time * max(BoilFps, 0.0));
float amp = max(BoilAmount, 0.0);
// Three poses cycle; each one bends the outline along a different pair of waves.
float phase = fmod(pose, 3.0) * 2.1;
float2 wobble = float2(sin(p0.y * 7.0 + phase * 3.7) + sin(p0.x * 5.0 + phase), sin(p0.x * 9.0 + phase * 2.3) + sin(p0.y * 4.0 + phase * 1.7)) * 0.5 * amp;
float aa = max(fwidth(p0.x) + fwidth(p0.y), 1e-4) * 0.75;
float headBase = clamp(HeadBase, -0.8, 0.6);
float shaftHalf = max(ShaftWidth, 0.02);
float headHalf = max(HeadWidth, shaftHalf);

float silhouette = -1.0;
float body = -1.0;
for (int pass = 0; pass < 2; ++pass)
{
	float2 p = p0 + wobble - float2(ShadowOffset, ShadowOffset) * pass;
	// Head: a triangle from headBase to the tip at x = 0.86.
	float headLength = 0.86 - headBase;
	float head = min(headHalf * (0.86 - p.x) / headLength - abs(p.y), p.x - headBase);
	// Shaft: a slab from the tail to just inside the head, narrowing slightly toward the tail.
	float taper = lerp(0.8, 1.0, saturate((p.x + 0.86) / (headBase + 0.86)));
	float shaft = min(shaftHalf * taper - abs(p.y), min(p.x + 0.86, headBase + 0.05 - p.x));
	float field = max(head, shaft);
	if (pass == 0) { body = field; }
	silhouette = max(silhouette, field);
}

float cover = smoothstep(-aa, aa, silhouette);
float fill = smoothstep(-aa, aa, body - max(InkWidth, 0.0));
// Identity colour lifted toward white so dark colours stay readable on a dark floor.
float3 paint = lerp(Color, float3(1.0, 1.0, 1.0), saturate(ColorLift)) * max(FillGain, 0.0);
float3 rgb = lerp(InkColor, paint, fill);
return float4(rgb, saturate(cover * saturate(Opacity)));
