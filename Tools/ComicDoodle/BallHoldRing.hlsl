// Held-ball ownership marks, drawn from UV. No texture.
// Kind 0: two hand drawn orbit rings around the held ball (camera facing sprite centred on the ball).
// Kind 1: the holder marker over the rear half of the holder (camera facing quad, rolled by C++ so that +V points
//         at the car's centre): one arrow head.
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
float isMarker = step(0.5, Kind);
// Three poses cycle; each one bends the line along a different pair of waves.
float phase = fmod(pose, 3.0) * 2.1;
float2 wobble = float2(sin(q0.y * 7.0 + phase * 3.7) + sin(q0.x * 5.0 + phase), sin(q0.x * 9.0 + phase * 2.3) + sin(q0.y * 4.0 + phase * 1.7)) * 0.5 * max(BoilAmount, 0.0);
float shadowAngle = radians(ShadowAngle);
float2 shadow = float2(cos(shadowAngle), sin(shadowAngle)) * max(ShadowOffset, 0.0);

float silhouette = -1.0;
float body = -1.0;
for (int pass = 0; pass < 2; ++pass)
{
	// The heavy ink side is offset in quad space.
	float2 q = q0 + wobble - shadow * pass;
	float field = -1.0;
	if (isMarker > 0.5)
	{
		// Two-pose bob along the pointing direction.
		float2 m = q - float2(0.0, fmod(pose, 2.0) * MarkerBob);
		// Arrow head: tip toward +V.
		float halfWidth = max(MarkerArrowWidth, 0.02);
		float arrowLength = max(MarkerArrowLength, 0.02);
		float top = MarkerArrowTop;
		float arrow = min(m.y - top, ((halfWidth - abs(m.x)) * arrowLength - (m.y - top) * halfWidth) / length(float2(arrowLength, halfWidth)));
		field = arrow;
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
