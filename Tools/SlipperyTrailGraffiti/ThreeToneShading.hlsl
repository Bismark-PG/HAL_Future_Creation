// Ground-relative three-tone ramp. UV1 differentiates the existing WPO surface;
// no vertex displacement, mask coverage, or gameplay state is changed here.
#if PIXELSHADER
float strength = saturate(ToneStrength);
if (strength <= 0) { return OldPalette; }
float3 baseNormal = normalize(GroundNormal);
baseNormal = baseNormal.z < 0 ? -baseNormal : baseNormal;
float angle = 6.28318530718 * (dot(UV, WaveUV.xy) + Phase);
float height = Height + Amplitude * sin(angle);
float2 heightGradient = Gradient * height
    + Weight * Amplitude * 6.28318530718 * cos(angle) * WaveUV.xy * InvTileSize;
float3 surfaceNormal = normalize(baseNormal - float3(heightGradient * baseNormal.z, 0));
float3 exaggerated = baseNormal + (surfaceNormal - baseNormal) * clamp(NormalStrength, 0, 8);
float3 normal = exaggerated * rsqrt(max(dot(exaggerated, exaggerated), 1e-12));
float lightLength = dot(LightDirection, LightDirection);
float3 light = lightLength > 1e-8 ? LightDirection * rsqrt(lightLength) : float3(0, 0, 1);
// Subtract the ground-facing reference so a flat patch remains the middle color.
float facing = dot(normal, light) - dot(baseNormal, light);
float low = clamp(min(ShadowThreshold, LitThreshold), -1, 1);
float high = max(low + 0.001, clamp(max(ShadowThreshold, LitThreshold), -1, 1));
float softness = min(max(ToneSoftness, 0), (high - low) * 0.5);
float toMid = softness > 0.00001 ? smoothstep(low - softness * 0.5, low + softness * 0.5, facing) : step(low, facing);
float toLit = softness > 0.00001 ? smoothstep(high - softness * 0.5, high + softness * 0.5, facing) : step(high, facing);
float3 ramp = lerp(lerp(ShadowColor, MidColor, toMid), LitColor, toLit);
float bufferWidth = clamp(EdgeBufferWidth, 0, 200);
if (bufferWidth > 0)
{
    float innerDepth = EdgeDepth - max(EdgeOutlineWidth, 0);
    float bufferSoftness = clamp(EdgeBufferSoftness, 0, 100);
    float interior = bufferSoftness > 0.00001
        ? smoothstep(bufferWidth, bufferWidth + bufferSoftness, innerDepth)
        : step(bufferWidth, innerDepth);
    ramp = lerp(MidColor, ramp, interior);
}
// Reuse the current contour alpha, including its optional paint-edge replacement.
float3 colored = lerp(EdgeColor, ramp, saturate(InkAlpha));
return lerp(OldPalette, colored, strength);
#else
return OldPalette;
#endif
