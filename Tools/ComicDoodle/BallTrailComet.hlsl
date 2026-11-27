// Launched-ball trail, comet style: an anime fireball streak behind the ball, drawn from UV. No texture.
// Same quad as BallTrailWedge.hlsl, stretched by C++ along the ball's travel, but it also reaches ahead of the ball
// (UHALBallComicFXSettings TrailWedgeLeadRadiusFraction): V = 0 is in front of the ball, V = 1 is the far end of the tail.
// Layers, front to back:
//   body      toon-banded flame: a bow of flame hugging the front of the ball that opens into the tail; white-hot core,
//             launcher colour band, a soft colour aura round it
//   wisps     thin broken speed streaks in the launcher colour that run on past the body
//   sparks    jagged white lightning lines hugging the front of the body, redrawn a few times per second
// Inputs are Custom node pins supplied by Tools/ComicDoodle/CreateComicDoodleAssets.py.
//   UV      0..1
//   Time    seconds
//   Color   launcher identity colour, linear (written once per launch by C++)
//   Aspect  quad length over quad width (written every frame by C++)
//   Ball    R = ball centre from the front edge, G = drawn ball radius, both in quad half widths (every frame by C++);
//           G = 0 falls back to a tail that starts at the front edge
//   Path    where this quad sits on the trail's path, in quad half widths (every frame by C++): R = path distance behind
//           the ball at the quad's front edge, G = whole length of the trail. A trail that bounced is one path with a bend:
//           the leg the ball is on has R = 0 and is cut at the bounce point, each old leg starts there with the R the
//           new leg stopped at, so heat, taper, streaks and wisps run straight through the bend. G = 0: the quad is the
//           whole trail (no path).
//   Fade    1 while the trail is live; a trail left behind (the shot ended, or the ball bounced away from this path)
//           runs it down to 0: the core burns out first, the flame thins to streaks, then those go too

float aspect = max(Aspect, 0.05);
float lengthHW = 2.0 * aspect;               // quad length in half widths
float x = UV.x * 2.0 - 1.0;                  // across the quad, -1..1 (half widths)
float d = saturate(UV.y) * lengthHW;         // distance from the front edge in half widths
float ballR = max(Ball.y, 0.0);
float ballC = ballR > 0.0 ? clamp(Ball.x, 0.0, lengthHW) : 0.0;
bool onPath = Path.y > 0.0;
float db = d - ballC + (onPath ? max(Path.x, 0.0) : 0.0);   // path distance behind the ball's centre (negative ahead of it)
float tailLen = onPath ? max(Path.y, 0.1) : max(lengthHW - ballC, 0.1);
float t = saturate(db / tailLen);            // 0 at the ball .. 1 at the tail
float fps = max(Fps, 1.0);
float pose = floor(Time * fps);
float aa = max(fwidth(x) + fwidth(d), 1e-4);

// ---- Body envelope -------------------------------------------------------------------------------------------
float bodyLen = clamp(BodyLength, 0.1, 1.0);
float s = saturate(t / bodyLen);             // 0 head .. 1 end of body
float headW = clamp(HeadWidth, 0.1, 0.9);
// Without a ball on the quad: rounded front at the front edge. With one: the tail starts as wide as the head at the
// ball's centre, and ahead of it the flame is a shell round the ball, thick at the sides and thin at the very front.
float cap = (ballR > 0.0 || onPath) ? 1.0 : sqrt(saturate(d / max(CapLength, 0.01)));
float halfW = headW * pow(saturate(1.0 - s), max(TaperPower, 0.1)) * cap;
// A slow S bend that grows toward the tail, so the flame reads as moving air, not a cut-out.
float bend = sin(db * 0.8 - Time * BendSpeed) * BendAmount * s;
float r = abs(x + bend) / max(halfW, 1e-3);
float ahead = 0.0;                           // 0 at the sides of the ball .. 1 straight ahead of it
float shellR = headW;
if (ballR > 0.0 && db < 0.0)
{
	float dist = length(float2(x, db));
	ahead = saturate(-db / max(dist, 1e-4));
	float front = ballR * (1.0 + max(WrapFront, 0.0));
	shellR = lerp(max(headW, front), front, ahead);
	r = dist / max(shellR, 1e-3);
}

// Streak noise: value noise stretched along the travel and scrolled back toward the tail.
float noise = 0.0;
float amp = 0.6;
float2 p = float2(x * StreakDensity, db * 0.35 - Time * ScrollSpeed);
for (int o = 0; o < 2; ++o)
{
	float2 i0 = floor(p);
	float2 f0 = frac(p);
	f0 = f0 * f0 * (3.0 - 2.0 * f0);
	float a = frac(sin(dot(i0, float2(127.1, 311.7))) * 43758.5453);
	float b = frac(sin(dot(i0 + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
	float c = frac(sin(dot(i0 + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
	float e = frac(sin(dot(i0 + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
	noise += amp * lerp(lerp(a, b, f0.x), lerp(c, e, f0.x), f0.y);
	p = p * float2(2.1, 1.7) + float2(3.7, 1.3);
	amp *= 0.4;
}
noise /= 0.84;

// Heat: hottest on the axis near the ball, cooling outward and backward, torn up by the streak noise.
float fade = saturate(Fade);
float heat = (1.0 - r) - (noise - 0.5) * NoiseAmount * (1.0 - 0.8 * ahead) - s * Cooling - (1.0 - fade) * max(FadeErode, 0.0);
// A thin white-hot rim pressed against the front of the ball, the bow wave of the shot.
if (ballR > 0.0 && db < 0.0)
{
	float rim = length(float2(x, db)) / ballR - 1.0;
	heat = max(heat, (CoreLevel + 0.1) * step(rim, max(WrapRim, 0.0)) * step(0.999, fade));
}
float hw = max(fwidth(heat), 1e-4);
float bodyMask = smoothstep(EdgeLevel - hw, EdgeLevel + hw, heat);
float midMask = smoothstep(MidLevel - hw, MidLevel + hw, heat);
float coreMask = smoothstep(CoreLevel - hw, CoreLevel + hw, heat);

float3 paint = Color * max(FillGain, 0.0);
float3 mid = lerp(Color, float3(1.0, 1.0, 1.0), saturate(MidLift)) * max(FillGain, 0.0);
float3 hot = float3(1.0, 1.0, 1.0) * max(CoreGain, 0.0);
float3 rgb = lerp(lerp(paint, mid, midMask), hot, coreMask);
float alpha = bodyMask;

// Soft aura just outside the body, fading toward the tail.
float aura = saturate(1.0 - (r - 1.0) / max(AuraWidth, 0.01));
aura = aura * aura * (1.0 - s) * saturate(AuraStrength) * step(1.0, r + 1e-3) * cap * fade;
float auraAlpha = alpha + aura * (1.0 - alpha);
rgb = (rgb * alpha + paint * aura * (1.0 - alpha)) / max(auraAlpha, 1e-3);
alpha = auraAlpha;

// ---- Wisps: thin broken speed streaks ------------------------------------------------------------------------
float wispCount = clamp(WispCount, 0.0, 6.0);
for (int w = 0; w < 6; ++w)
{
	if (w >= wispCount) { break; }
	float hA = frac(sin(w * 12.9898 + 4.1) * 43758.5453);
	float hB = frac(sin(w * 78.233 + 1.7) * 43758.5453);
	float hC = frac(sin((w + pose * 0.37) * 39.425) * 43758.5453);
	float lane = (hA * 2.0 - 1.0) * headW;
	float d0 = tailLen * bodyLen * (0.15 + 0.35 * hB);
	float d1 = tailLen * (0.7 + 0.3 * hC);
	float k = saturate((db - d0) / max(d1 - d0, 1e-3));
	float wave = sin(db * 0.9 + w * 2.3 - Time * 5.0) * 0.05 + bend;
	// Lanes draw together toward the tail, like streaks converging behind a fast object.
	float centre = lane * (1.0 - 0.4 * k);
	float width = max(WispWidth, 0.0) * (1.0 - k);
	float lineMask = smoothstep(width + aa, width - aa, abs(x + wave - centre));
	// Dashes slide back toward the tail.
	float dash = step(frac(db * 0.08 - Time * 1.8 + hB), 0.85);
	float wisp = lineMask * dash * step(d0, db) * step(db, d1);
	float3 wispRgb = lerp(Color, float3(1.0, 1.0, 1.0), 0.35 * (1.0 - k)) * max(FillGain, 0.0);
	rgb = lerp(rgb, wispRgb, wisp * (1.0 - alpha * 0.5));
	alpha = max(alpha, wisp * (1.0 - 0.4 * k) * sqrt(fade));
}

// ---- Sparks: jagged lightning running from the front of the ball down the sides of the body --------------------
float sparkLen = tailLen * bodyLen * clamp(SparkReach, 0.0, 1.0);
for (int side = 0; side < 2; ++side)
{
	float sgn = side == 0 ? -1.0 : 1.0;
	// Each side is drawn on some poses and missing on others: anime flicker.
	float on = step(1.0 - saturate(SparkChance), frac(sin((pose + side * 17.0) * 91.7) * 43758.5453)) * step(0.999, fade);
	float seg = max(SparkSegment, 0.05);
	// Along the body the bolt is measured down the travel; round the front of the ball, along the shell's arc.
	bool onShell = ballR > 0.0 && db < 0.0;
	float along = onShell ? -asin(ahead) * shellR : db;
	float kSeg = floor(along / seg);
	float fSeg = frac(along / seg);
	float j0 = frac(sin((kSeg + pose * 7.0 + side * 13.0) * 53.17) * 43758.5453) * 2.0 - 1.0;
	float j1 = frac(sin((kSeg + 1.0 + pose * 7.0 + side * 13.0) * 53.17) * 43758.5453) * 2.0 - 1.0;
	float jitter = lerp(j0, j1, fSeg) * SparkJitter;
	float envAt = headW * pow(saturate(1.0 - s), max(TaperPower, 0.1)) * cap;
	float miss = onShell
		? abs(length(float2(x, db)) - (shellR * 1.05 + 0.04 + jitter)) + step(x * sgn, 0.0) * 10.0
		: abs(x - (sgn * (envAt * 1.05 + 0.04) + jitter - bend));
	float fade = 1.0 - saturate(max(db, 0.0) / max(sparkLen, 1e-3));
	float spark = smoothstep(SparkWidth + aa, SparkWidth - aa, miss) * step(db, sparkLen) * on * fade;
	rgb = lerp(rgb, hot, spark);
	alpha = max(alpha, spark);
}

// Nothing drawn may touch the quad border, except where two legs of one path meet: there the flame must run through.
float v = saturate(UV.y);
float joinFront = onPath && Path.x > 0.0 ? 1.0 : 0.0;
float joinBack = onPath && (lengthHW - ballC + max(Path.x, 0.0)) < tailLen - 0.05 ? 1.0 : 0.0;
float border = smoothstep(1.0, 0.94, abs(x)) * max(smoothstep(1.0, 0.97, v), joinBack) * max(smoothstep(0.0, 0.03, v), joinFront);
return float4(rgb, saturate(alpha * border * saturate(Opacity) * saturate(fade * 4.0)));
