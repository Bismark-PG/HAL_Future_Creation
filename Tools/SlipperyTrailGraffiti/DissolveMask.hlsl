// UV0 is fixed in ground units: holes grow without swimming with the oil pattern or WPO.
// The two output channels are coverage and a restrained colored rim on surviving pixels.
float progress = saturate(Progress);
if (progress <= 0) { return float2(1, 0); }
if (progress >= 1) { return float2(0, 0); }
float2 p = UV / (max(InvTile, 0.0005) * max(CellSize, 20));
float noise = 0;
float octaveWeight = 0.75;
[unroll]
for (int octave = 0; octave < 2; ++octave)
{
    float2 cell = floor(p);
    float2 f = frac(p);
    f = f * f * f * (f * (f * 6 - 15) + 10);
    float4 x = float4(cell.x, cell.x + 1, cell.x, cell.x + 1);
    float4 y = float4(cell.y, cell.y, cell.y + 1, cell.y + 1);
    float4 hash = frac(x * 0.1031 + y * 0.11369);
    hash = frac((hash * (hash + 33.33)) * (hash + 17.17));
    noise += octaveWeight * lerp(lerp(hash.x, hash.y, f.x), lerp(hash.z, hash.w, f.x), f.y);
    p = float2(p.x * 0.8 - p.y * 0.6, p.x * 0.6 + p.y * 0.8) * 2.03 + 17.7;
    octaveWeight = 0.25;
}
noise = saturate((noise - 0.1) / 0.8);
float distance = noise - progress;
float softness = max(Softness, 0.002);
float coverage = smoothstep(-softness, softness, distance);
float rim = 1 - smoothstep(0, max(EdgeWidth, 0.002), max(distance, 0));
return float2(coverage, rim);
