// Comic impact burst, drawn entirely from the sprite UV. No texture, no flipbook, no Niagara.
// Inputs are supplied as Custom node pins by Tools/ImpactBurst/CreateImpactBurstAssets.py.
//   UV            sprite coordinates, 0..1
//   Age01         0 at the contact frame, 1 at the end of the burst lifetime (written per frame by C++)
//   Seed          0..1, derived from the authority event so every client draws the same star
//   Spikes        lobe count; the authority maps impact strength onto this
//   Strength      0..1 impact strength; only trims brightness, the real size is the sprite size
//   FillColor     flat comic fill, normally white
//   EdgeColor     contour colour, normally the impact tier colour
// The remaining pins are authored look constants exposed on the material instance.

float2 p = UV * 2.0 - 1.0;
float age = saturate(Age01);

// Pop: snap outward almost immediately, then drift a little wider while fading.
float pop = 1.0 - pow(1.0 - saturate(age / max(PopFraction, 0.01)), 3.0);
float scale = lerp(max(StartScale, 0.01), 1.0, pop) + age * TailGrowth;
p /= max(scale, 0.01);

float r = length(p);
float ang = atan2(p.y, p.x) + Seed * 6.2831853;
float lobes = max(floor(Spikes), 3.0);

// Two sharpened harmonics combined with max(): long and short spikes with hard points between them.
// Averaging them instead, or using an exponent below one, rounds the lobes into a flower.
float k = max(Sharpness, 0.05);
float s1 = pow(abs(sin(ang * lobes * 0.5)), k);
float s2 = pow(abs(sin(ang * (lobes * 0.5 + 1.0) + 2.3)), k) * saturate(Irregular);
float shape = max(s1, s2);
float rad = lerp(saturate(InnerRadius), 1.0, shape) * 0.98;

// One pixel of antialiasing: hard comic edges, no soft glow halo.
float aa = max(fwidth(r), 1e-4) * 1.2;
float fill = 1.0 - smoothstep(rad - aa, rad + aa, r);
float inner = 1.0 - smoothstep(rad * (1.0 - saturate(EdgeWidth)) - aa, rad * (1.0 - saturate(EdgeWidth)) + aa, r);
float edge = saturate(fill - inner);

// Hold at full, then fade over the tail.
float fade = 1.0 - smoothstep(saturate(FadeStart), 1.0, age);
float gain = lerp(1.0, max(StrengthGain, 0.0), saturate(Strength));
float3 rgb = lerp(FillColor * max(FillGain, 0.0), EdgeColor * max(EdgeGain, 0.0), edge) * gain;

return float4(rgb * fade, saturate(fill * fade * saturate(Opacity)));
