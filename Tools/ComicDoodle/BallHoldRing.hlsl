// Held-ball ownership marks, drawn from UV. No texture.
// Kind 0: two orbit rings around the held ball (camera facing sprite centred on the ball). Each ring is a small comet
//         circling the ball: a bright head, a tail that thins and cools round the orbit behind it. It is the launched
//         ball's comet (BallTrailComet.hlsl, BallFlameShell.hlsl) held on a leash: on launch the flame is let go.
// Kind 1: the holder marker over the rear half of the holder (camera facing quad, rolled by C++ so that +V points
//         at the car's centre): one arrow head made of the same flame, burning back from its tip.
// Same bands as the comet, from the hottest part outward: white-hot, pale, holder colour. No ink line.
// Inputs are Custom node pins supplied by Tools/ComicDoodle/CreateComicDoodleAssets.py.
//   UV      0..1
//   Time    seconds
//   Color   holder identity colour, linear (written once per capture by C++)
//   BallRadius  drawn ball radius in sprite half extents (written by C++); hides the far half of each orbit ring

float2 q = UV * 2.0 - 1.0;
if (FlipV > 0.5) { q.y = -q.y; }
float aa = max(fwidth(q.x) + fwidth(q.y), 1e-4) * 0.75;
float isMarker = step(0.5, Kind);

float3 paint = Color * max(FillGain, 0.0);
float3 mid = lerp(Color, float3(1.0, 1.0, 1.0), saturate(MidLift)) * max(FillGain, 0.0);
float3 hot = float3(1.0, 1.0, 1.0) * max(CoreGain, 0.0);
float3 rgb = paint;
float alpha = 0.0;

if (isMarker > 0.5)
{
	// Gentle bob along the pointing direction.
	float2 m = q - float2(0.0, (sin(Time * MarkerBobSpeed) * 0.5 + 0.5) * MarkerBob);
	// Arrow head: tip toward +V.
	float halfWidth = max(MarkerArrowWidth, 0.02);
	float arrowLength = max(MarkerArrowLength, 0.02);
	float top = MarkerArrowTop;
	float arrow = min(m.y - top, ((halfWidth - abs(m.x)) * arrowLength - (m.y - top) * halfWidth) / length(float2(arrowLength, halfWidth)));
	// Flame tongues streaming back from the tip (toward -V); the tip itself stays calm so it keeps pointing.
	float2 p = float2(m.x * FlameDensity, m.y * FlameDensity * 0.6 + Time * ScrollSpeed);
	float2 i0 = floor(p);
	float2 f0 = frac(p);
	f0 = f0 * f0 * (3.0 - 2.0 * f0);
	float a = frac(sin(dot(i0, float2(127.1, 311.7))) * 43758.5453);
	float b = frac(sin(dot(i0 + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
	float c = frac(sin(dot(i0 + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
	float e = frac(sin(dot(i0 + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
	float noise = lerp(lerp(a, b, f0.x), lerp(c, e, f0.x), f0.y);
	float back = saturate((top + arrowLength - m.y) / arrowLength);     // 0 at the tip .. 1 at the base
	float field = arrow + (noise - 0.5) * max(FlameAmount, 0.0) * back;
	float body = smoothstep(-aa, aa, field);
	float midMask = smoothstep(MarkerMidDepth - aa, MarkerMidDepth + aa, field);
	float coreMask = smoothstep(MarkerCoreDepth - aa, MarkerCoreDepth + aa, field);
	rgb = lerp(lerp(paint, mid, midMask), hot, coreMask);
	float aura = saturate(1.0 + field / max(AuraWidth, 0.01));
	aura = aura * aura * saturate(AuraStrength) * (1.0 - body);
	alpha = body + aura;
}
else
{
	for (int i = 0; i < 2; ++i)
	{
		// The two rings lean opposite ways and rock slowly.
		float tilt = radians(OrbitTilt * (i == 0 ? -1.0 : 1.3) + sin(Time * 1.7 + i * 2.1) * OrbitWobble);
		float cs = cos(tilt);
		float sn = sin(tilt);
		float2 p = float2(q.x * cs + q.y * sn, -q.x * sn + q.y * cs);
		float major = max(OrbitWidth, 0.05) * (i == 0 ? 1.0 : 0.94);
		float minor = max(OrbitHeight, 0.02) * (i == 0 ? 1.0 : 1.12);
		float e = max(length(float2(p.x / major, p.y / minor)), 1e-4);
		// First order distance to the ellipse, so the line keeps its thickness all the way round.
		float slope = max(length(float2(p.x / (major * major), p.y / (minor * minor))) / e, 1e-4);
		// Where on the orbit this pixel is, and how far behind the comet's head (0 at the head .. 1 a full turn behind).
		float turn = atan2(p.y / minor, p.x / major) / 6.2831853;
		float u = i == 0 ? frac(Time * OrbitSpeed - turn) : frac(Time * OrbitSpeed + 0.5 + turn);
		float flicker = sin(turn * 31.0 + Time * 9.0 * (i == 0 ? 1.0 : -1.0)) * 0.5 + sin(turn * 57.0 - Time * 13.0) * 0.5;
		// Fat at the head, thinning along the tail; the head itself swells into a bead.
		float width = max(OrbitLine, 0.005) * (lerp(1.0, saturate(TailThin), pow(u, 0.6)) + max(HeadSwell, 0.0) * exp(-u * 28.0))
			* (1.0 + flicker * max(FlameAmount, 0.0) * u);
		float f = width - abs(e - 1.0) / slope;
		// The far half of a ring passes behind the ball.
		if (p.y < 0.0) { f = min(f, length(q) - BallRadius); }
		float body = smoothstep(-aa, aa, f);
		// Hot at the head and along the middle of the line, cooling round the orbit.
		float heat = (1.0 - u) * 0.75 + saturate(f / max(width, 1e-4)) * 0.25;
		float3 ring = lerp(lerp(paint, mid, smoothstep(MidHeat - 0.03, MidHeat + 0.03, heat)), hot, smoothstep(CoreHeat - 0.03, CoreHeat + 0.03, heat));
		float ringAlpha = body * lerp(1.0, saturate(TailAlpha), u);
		float aura = saturate(1.0 + f / max(AuraWidth, 0.01));
		aura = aura * aura * saturate(AuraStrength) * (1.0 - u) * (1.0 - body) * step(BallRadius, length(q) + step(0.0, p.y) * 10.0);
		float layer = saturate(ringAlpha + aura);
		float3 layerRgb = (ring * ringAlpha + paint * aura) / max(ringAlpha + aura, 1e-3);
		rgb = lerp(rgb, layerRgb, layer / max(layer + alpha * (1.0 - layer), 1e-3));
		alpha = layer + alpha * (1.0 - layer);
	}
}
return float4(rgb, saturate(alpha * saturate(Opacity)));
