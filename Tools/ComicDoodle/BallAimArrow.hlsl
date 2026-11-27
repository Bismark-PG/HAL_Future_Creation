// Held-ball aim arrow, drawn on a flat ground quad from its UV. No texture.
// Look: a short fat arrow made of the same flame as the launched ball's comet (BallTrailComet.hlsl): holder colour at
// the edge, a pale band, a white-hot heart, flame tongues streaming from the tail toward the tip. It is the shot that
// has not been fired yet: the comet will leave along it.
// Only the flame moves - the arrow's direction is the quad's own +X and never moves; the head stays crisp so it points.
// Inputs are Custom node pins supplied by Tools/ComicDoodle/CreateComicDoodleAssets.py.
//   UV      quad coordinates, 0..1; U runs along the arrow when FlipAxes is 0, V when it is 1
//   Time    seconds
//   Color   holder identity colour, linear (written once per capture by C++)

float2 uv = FlipAxes > 0.5 ? UV.yx : UV;
// x: -1 tail .. +1 tip. y: across.
float2 p = float2(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0);
if (Reverse > 0.5) { p.x = -p.x; }
float aa = max(fwidth(p.x) + fwidth(p.y), 1e-4) * 0.75;
float headBase = clamp(HeadBase, -0.8, 0.6);
float shaftHalf = max(ShaftWidth, 0.02);
float headHalf = max(HeadWidth, shaftHalf);

// Head: a triangle from headBase to the tip at x = 0.86.
float headLength = 0.86 - headBase;
float head = min(headHalf * (0.86 - p.x) / headLength - abs(p.y), p.x - headBase);
// Shaft: a slab from the tail to just inside the head, narrowing slightly toward the tail.
float taper = lerp(0.8, 1.0, saturate((p.x + 0.86) / (headBase + 0.86)));
float shaft = min(shaftHalf * taper - abs(p.y), min(p.x + 0.86, headBase + 0.05 - p.x));
float field = max(head, shaft);

// Flame tongues flowing toward the tip; strongest at the tail, gone at the head.
float2 n = float2(p.y * FlameDensity, p.x * FlameDensity * 0.6 - Time * ScrollSpeed);
float2 i0 = floor(n);
float2 f0 = frac(n);
f0 = f0 * f0 * (3.0 - 2.0 * f0);
float a = frac(sin(dot(i0, float2(127.1, 311.7))) * 43758.5453);
float b = frac(sin(dot(i0 + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
float c = frac(sin(dot(i0 + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
float e = frac(sin(dot(i0 + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
float noise = lerp(lerp(a, b, f0.x), lerp(c, e, f0.x), f0.y);
float tail = saturate((headBase - p.x) / (headBase + 0.86));          // 0 from the head on .. 1 at the tail end
field += (noise - 0.5) * max(FlameAmount, 0.0) * tail;
// A slow pulse of heat running from tail to tip: the shot gathering.
float pulse = sin(p.x * 3.0 - Time * PulseSpeed) * 0.5 + 0.5;

float body = smoothstep(-aa, aa, field);
float midMask = smoothstep(MidDepth - aa, MidDepth + aa, field);
float coreMask = smoothstep(CoreDepth - aa, CoreDepth + aa, field - (1.0 - pulse) * max(PulseDepth, 0.0));
float3 paint = Color * max(FillGain, 0.0);
float3 mid = lerp(Color, float3(1.0, 1.0, 1.0), saturate(MidLift)) * max(FillGain, 0.0);
float3 hot = float3(1.0, 1.0, 1.0) * max(CoreGain, 0.0);
float3 rgb = lerp(lerp(paint, mid, midMask), hot, coreMask);
float aura = saturate(1.0 + field / max(AuraWidth, 0.01));
aura = aura * aura * saturate(AuraStrength) * (1.0 - body);
return float4(rgb, saturate((body + aura) * saturate(Opacity)));
