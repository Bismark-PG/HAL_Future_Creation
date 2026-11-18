// Held-ball ownership marks, drawn from UV. No texture.
// Kind 0: two hand drawn orbit rings around the held ball (camera facing sprite centred on the ball).
// Kind 1: the holder's roof crest, seen from above (flat quad over the roof): two concentric thin rings with a
//         four-point star on each of the four axes. Each star has a short tip pointing out and a long thin tip
//         pointing at the centre; the four long tips stop short of the centre, which stays empty.
// Same hand-inked language as the other doodle layers: flat fill, black contour, heavier ink on one side,
// redrawn a few times per second, never faded.
// Inputs are Custom node pins supplied by Tools/ComicDoodle/CreateComicDoodleAssets.py.
//   UV      0..1
//   Time    seconds
//   Color   holder identity colour, linear (written once per capture by C++)
//   BallRadius  drawn ball radius in sprite half extents (written by C++); hides the far half of each orbit ring

float2 q0 = UV * 2.0 - 1.0;
if (FlipV > 0.5) { q0.y = -q0.y; }
float fps = max(Fps, 1.0);
float pose = floor(Time * fps);
// Stepped clock: everything moves on the same few drawings per second.
float clock = pose / fps;
float aa = max(fwidth(q0.x) + fwidth(q0.y), 1e-4) * 0.75;
float isCrest = step(0.5, Kind);
// Three poses cycle; each one bends the line along a different pair of waves.
float phase = fmod(pose, 3.0) * 2.1;
float2 wobble = float2(sin(q0.y * 7.0 + phase * 3.7) + sin(q0.x * 5.0 + phase), sin(q0.x * 9.0 + phase * 2.3) + sin(q0.y * 4.0 + phase * 1.7)) * 0.5 * max(BoilAmount, 0.0);
float shadowAngle = radians(ShadowAngle);
float2 shadow = float2(cos(shadowAngle), sin(shadowAngle)) * max(ShadowOffset, 0.0);

float silhouette = -1.0;
float body = -1.0;
for (int pass = 0; pass < 2; ++pass)
{
	// The heavy ink side is offset before any rotation, so it stays on the same side while the crest turns.
	float2 q = q0 + wobble - shadow * pass;
	float field = -1.0;
	if (isCrest > 0.5)
	{
		float turn = 6.2831853 * TurnsPerSecond * clock;
		float cs = cos(turn);
		float sn = sin(turn);
		float2 r = float2(q.x * cs - q.y * sn, q.x * sn + q.y * cs);
		float radius = length(r);
		float ringHalf = max(RingLine, 0.005);
		field = max(ringHalf - abs(radius - RingOuter), ringHalf - abs(radius - RingInner));
		// Fold the four axes onto one: a.y runs along the nearest axis, a.x across it.
		float2 a = abs(r);
		if (a.x > a.y) { a = a.yx; }
		// Two-pose twinkle: every other drawing the stars are a little smaller.
		float twinkle = lerp(1.0, saturate(StarTwinkle), fmod(pose, 2.0));
		float along = a.y - 0.5 * (RingOuter + RingInner);
		float reach = max(along > 0.0 ? StarOut * twinkle : StarIn, 0.01);
		float halfWidth = max(StarWidth, 0.01) * twinkle;
		float k = clamp(StarPinch, 0.2, 1.0);
		// Superellipse with an exponent below one: concave sides, four sharp tips.
		// The bias retires each tip before it degrades into a hairline along its own axis.
		float star = (1.0 - pow(pow(abs(along) / reach, k) + pow(a.x / halfWidth, k), 1.0 / k)) * halfWidth - max(StarTipBias, 0.0);
		field = max(field, star);
	}
	else
	{
		for (int i = 0; i < 2; ++i)
		{
			// The two rings lean opposite ways and rock a little from drawing to drawing.
			float tilt = radians(OrbitTilt * (i == 0 ? -1.0 : 1.3) + (fmod(pose + i, 3.0) - 1.0) * OrbitWobble);
			float cs = cos(tilt);
			float sn = sin(tilt);
			float2 p = float2(q.x * cs + q.y * sn, -q.x * sn + q.y * cs);
			float major = max(OrbitWidth, 0.05) * (i == 0 ? 1.0 : 0.94);
			float minor = max(OrbitHeight, 0.02) * (i == 0 ? 1.0 : 1.12);
			float e = max(length(float2(p.x / major, p.y / minor)), 1e-4);
			// First order distance to the ellipse, so the line keeps one thickness all the way round.
			float slope = max(length(float2(p.x / (major * major), p.y / (minor * minor))) / e, 1e-4);
			float f = max(OrbitLine, 0.005) - abs(e - 1.0) / slope;
			// The far half of a ring passes behind the ball.
			if (p.y < 0.0) { f = min(f, length(q) - BallRadius); }
			field = max(field, f);
		}
	}
	if (pass == 0) { body = field; }
	silhouette = max(silhouette, field);
}

float cover = smoothstep(-aa, aa, silhouette);
float fill = smoothstep(-aa, aa, body - max(InkWidth, 0.0));
// Identity colour lifted toward white so dark colours stay readable on a dark roof or floor.
float3 paint = lerp(Color, float3(1.0, 1.0, 1.0), saturate(ColorLift)) * max(FillGain, 0.0);
float3 rgb = lerp(InkColor, paint, fill);
return float4(rgb, saturate(cover * saturate(Opacity)));
