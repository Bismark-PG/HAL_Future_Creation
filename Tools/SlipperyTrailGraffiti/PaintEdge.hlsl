// UV2: signed distance to the original edge, allowed outward margin (both cm).
// RGB remains coverage / ink alpha / mode. A adds depth inside this same contour, cm.
// Coverage never cuts into the original core. Mode=0 preserves the original ink alpha.
if (Enabled < 0.5) { return float4(1, 0, 0, -EdgeMetadata.x); }
float distance = EdgeMetadata.x;
float padding = max(EdgeMetadata.y, 0);
float strength = saturate(Strength);
if (padding < 0.0001 || strength < 0.0001)
{
    return float4(distance <= 0 ? 1 : 0, 0, 0, -distance);
}
float width = max(OutlineWidth, 0);
float outlineSoftness = max(OutlineSoftness, 0.1);
// Evaluate the same contour noise inside the core too: a color buffer needs its
// actual outward offset even where coverage and ink alpha already equal one.

float2 p = UV / (max(InvTile, 0.0005) * max(CellSize, 20));
float detail = clamp(Detail, 0, 0.5);
float noise = 0;
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
    noise += (octave == 0 ? 1 - detail : detail)
        * lerp(lerp(hash.x, hash.y, f.x), lerp(hash.z, hash.w, f.x), f.y);
    p = float2(p.x * 0.8 - p.y * 0.6, p.x * 0.6 + p.y * 0.8) * 2.03 + 17.7;
}
noise = smoothstep(0.1, 0.9, noise);
float outset = padding * strength * lerp(0.05, 1, noise);
float depth = outset - distance;
float softness = max(Softness, 0.1);
float coverage = distance <= 0 ? 1 : smoothstep(-softness, softness, depth);
float inkAlpha = width <= 0 ? 1 : smoothstep(width - outlineSoftness * 0.5, width + outlineSoftness * 0.5, depth);
return float4(coverage, inkAlpha, 1, depth);
