// Launched-ball comic flame puff, drawn on a Niagara sprite from its UV. No texture.
// Look: a cartoon fire drop - round head toward the ball, a bent pointed tail away from it - in the launcher's
// colour, a paler heart while the puff is young, and a black ink contour. Drawn "on twos": the outline only
// changes when the Niagara step counter changes (about 12 drawings per second).
// Sprite renderers draw every puff in layers: Layer 0 paints a slightly larger black silhouette underneath,
// Layer 1 paints the flat colour fill on top, Layer 2 (body puffs only) paints the pale heart above all fills.
// Overlapping puffs therefore merge into one contour and one heart instead of showing a black line or a pale
// crescent inside every overlap.
// Inputs are Custom node pins supplied by Tools/ComicDoodle/CreateComicDoodleAssets.py.
//   UV      sprite coordinates, 0..1; V runs along the sprite's alignment axis (the flame's travel direction)
//   Color   particle colour (linear RGB): the launcher's identity colour
//   Alpha   particle alpha; multiplies the coverage (Niagara keeps it at 1: puffs shrink, they never fade)
//   Size    particle sprite size in cm: x across, y along. Only the ratio is used (shape stays round)
//   Dynamic Dynamic Parameter 0 (Niagara Particles.DynamicMaterialParameter):
//           x drawing counter (whole numbers, changes about 12 times per second)
//           y stepped normalised age, 0 at birth .. 1 at the end of the last drawing
//           z 1 draws the pale heart (body puffs), 0 never (flung tongues)
//           w per-particle random 0..1

float Step = Dynamic.x;
float AgeN = Dynamic.y;
float Core = Dynamic.z;
float Rand = Dynamic.w;
// p: x across (-1..1), y along with +1 at the head end.
float2 p = float2(UV.x * 2.0 - 1.0, 1.0 - UV.y * 2.0);
if (Reverse > 0.5) { p.y = -p.y; }
float aspect = max(Size.y, 1e-3) / max(Size.x, 1e-3);
// q: same scale on both axes (units of the sprite half width), so the head stays a circle.
float2 q = float2(p.x, p.y * aspect);
float drawing = floor(Step + 0.5);
// Tail bend flips side every drawing; the starting side is random per puff.
float flip = (fmod(drawing, 2.0) < 0.5 ? 1.0 : -1.0) * (Rand < 0.5 ? 1.0 : -1.0);
float seed = drawing * 1.618 + Rand * 13.0;
q += float2(sin(q.y * 5.3 + seed * 6.1), sin(q.x * 4.7 + seed * 3.9)) * max(Boil, 0.0);

float aa = max(fwidth(q.x) + fwidth(q.y), 1e-4) * 0.75;
float r = max(HeadRadius, 0.05);
float headY = aspect - max(Margin, 0.0) - r;
float tipY = -aspect + max(Margin, 0.0);
float ink = max(InkWidth, 0.0);
// Heart: the same flame drop scaled toward the head centre (an inner flame, not an eroded circle). Its scale
// starts at CoreScale and reaches 0 at normalised age CoreLife, stepping with the drawings.
float heartScale = max(CoreScale, 0.0) * (1.0 - saturate(AgeN / max(CoreLife, 1e-3)));
float silhouette = 0.0;
float fillCover = 0.0;
float heart = 0.0;
// Pass 0 draws the puff, pass 1 the heart (the same drop evaluated in scaled coordinates).
for (int pass = 0; pass < 2; ++pass)
{
	float s = pass == 0 ? 1.0 : max(heartScale, 1e-3);
	float2 c = (q - float2(0.0, headY)) / s + float2(0.0, headY);
	float paa = aa / s;
	float head = r - length(c - float2(0.0, headY));
	// Tail: half width r * t^TailPower (TailPower > 1 keeps the point sharp instead of a blunt petal).
	float t = saturate((c.y - tipY) / max(headY - tipY, 1e-3));
	float bend = TailBend * flip * (1.0 - t) * (1.0 - t);
	float halfWidth = r * pow(max(t, 1e-4), max(TailPower, 1.0));
	// The bias cuts the last sub-pixel sliver off the point; without it the tip renders as a grey hairline.
	float bias = max(TipBias, paa);
	float tailRaw = halfWidth - abs(c.x - bend) - bias;
	float field = max(head, min(tailRaw, headY - c.y));
	// The stop is a width, not a distance: its gradient along the tail is shallow, so it is antialiased by its
	// own screen derivative (a shared aa would smear the point into a soft grey tip).
	float stopField = halfWidth - bias;
	float stop = saturate(stopField / max(fwidth(stopField), 1e-5) + 0.5);
	float cover = min(smoothstep(-paa, paa, field), stop);
	if (pass == 0)
	{
		fillCover = cover;
		// Ink silhouette: grown sideways by InkWidth, but the tail ink stops where the fill's point stops,
		// otherwise the ink alone runs on past the colour as a thin black hairline.
		silhouette = max(smoothstep(-aa, aa, head + ink),
		                 min(smoothstep(-aa, aa, min(tailRaw + ink, headY - c.y + ink)), stop));
	}
	else
	{
		heart = heartScale > 1e-3 ? cover * saturate(Core) : 0.0;
	}
}
float3 paint = lerp(Color, float3(1.0, 1.0, 1.0), saturate(ColorLift)) * max(FillGain, 0.0);
// Heart: the launcher colour lifted toward white and pushed brighter (CoreGain), so it reads as a hot inner
// flame after tonemapping instead of a flat grey.
float3 heartPaint = lerp(Color, float3(1.0, 1.0, 1.0), saturate(CoreLift)) * max(FillGain, 0.0) * max(CoreGain, 0.0);
// Layer 0 ink silhouette, 1 flat colour fill, 2 heart only. The heart has its own renderer above every fill:
// drawn inside the fill layer, younger puffs cover older hearts and leave pale crescents instead of one hot core.
float3 rgb = Layer < 0.5 ? InkColor : (Layer < 1.5 ? paint : heartPaint);
float cover = Layer < 0.5 ? silhouette : (Layer < 1.5 ? fillCover : heart);
return float4(rgb, saturate(cover * saturate(Alpha) * saturate(Opacity)));
