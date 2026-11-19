// Vehicle doodle status marks, drawn on a camera facing sprite from its UV. No texture.
// Kind 0: three four-point stars circling over a stunned car (a squashed orbit reads as a ring seen from above).
// Kind 1: sweat drops flicking off a car that is nearly out of HP.
// Same hand-inked language as the impact burst: flat fill, black contour, heavier ink side, drawn "on twos".
// Inputs are Custom node pins supplied by Tools/ComicDoodle/CreateComicDoodleAssets.py.
//   UV      sprite coordinates, 0..1
//   Time    seconds

float2 q0 = UV * 2.0 - 1.0;
float fps = max(Fps, 1.0);
// Stepped clock: the pose only changes a few times per second.
float clock = floor(Time * fps) / fps;
float aa = max(fwidth(q0.x) + fwidth(q0.y), 1e-4) * 0.75;
float isDrops = step(0.5, Kind);

float silhouette = -1.0;
float body = -1.0;
for (int pass = 0; pass < 2; ++pass)
{
	float2 q = q0 - float2(ShadowOffset, ShadowOffset) * pass;
	float field = -1.0;
	for (int i = 0; i < 3; ++i)
	{
		float f;
		if (isDrops > 0.5)
		{
			// Each drop repeats its own short flight: out from the middle, up and to the side, shrinking at the end.
			float t = frac(clock * max(DropsPerSecond, 0.1) + i * 0.37);
			float angle = radians(-90.0 + (i - 1.0) * 48.0);
			float2 dir = float2(cos(angle), sin(angle));
			float2 centre = dir * lerp(0.22, saturate(DropReach), sqrt(t));
			float size = max(DropSize, 0.01) * (1.0 - smoothstep(0.6, 1.0, t));
			float2 d = q - centre;
			// Teardrop: a round head with a tail pointing back at where it came from.
			float along = dot(d, -dir);
			float across = abs(dot(d, float2(-dir.y, dir.x)));
			float tail = saturate(along / (size * 1.9));
			float head = size * 0.62 - length(d);
			// The bias and the far cap retire the tail before it degrades into a hairline along its own axis.
			float cone = min(min(size * 0.6 * (1.0 - tail) - across - 0.006, along + size * 0.1), size * 1.9 - along);
			f = size > 0.012 ? max(head, cone) : -1.0;
		}
		else
		{
			float theta = 6.2831853 * (clock * TurnsPerSecond + i / 3.0);
			float2 centre = float2(cos(theta) * OrbitWidth, sin(theta) * OrbitHeight);
			// Stars on the near side of the ring are a little bigger: cheap depth without any sorting.
			float size = max(StarSize, 0.01) * lerp(0.78, 1.0, 0.5 + 0.5 * sin(theta));
			// Two-pose twinkle: every other step the star turns 45 degrees.
			float turn = fmod(floor(Time * fps) + i, 2.0) * 0.7853982;
			float cs = cos(turn);
			float sn = sin(turn);
			float2 d = (q - centre) / size;
			float2 p = abs(float2(d.x * cs - d.y * sn, d.x * sn + d.y * cs));
			float k = clamp(StarPinch, 0.2, 1.0);
			float radius = pow(pow(p.x, k) + pow(p.y, k), 1.0 / k);
			f = max(1.0 - radius - 0.03, 0.46 - length(d)) * size;
		}
		field = max(field, f);
	}
	if (pass == 0) { body = field; }
	silhouette = max(silhouette, field);
}

float cover = smoothstep(-aa, aa, silhouette);
float fill = smoothstep(-aa, aa, body - max(InkWidth, 0.0));
float heart = smoothstep(-aa, aa, body - max(InkWidth, 0.0) * 2.6);
float3 base = lerp(StarColor, DropColor, isDrops);
float3 paint = lerp(base, float3(1.0, 1.0, 1.0), heart * saturate(HeartMix)) * max(FillGain, 0.0);
float3 rgb = lerp(InkColor, paint, fill);
return float4(rgb, saturate(cover * saturate(Opacity)));
