// U=1 touches the leading blade. Two tapered cuts let the actual deck show
// between the brush tips; the cuts merge into a single stroke at its head.
float u = saturate(UV.x);
float v = UV.y * 2.0 - 1.0;
float bow = sin(3.14159 * u);
float width = .70 * pow(u, .70);
float center = .13 * bow;
float field = width - abs(v - center);
float cutA = (.085 * (1.0 - smoothstep(.20, .79, u))) - abs(v - center + .19*u + .09*bow);
float cutB = (.055 * (1.0 - smoothstep(.10, .52, u))) - abs(v - center - .29*u - .06*bow);
field = min(field, -min(cutA, (.79-u)*1.5));
field = min(field, -min(cutB, (.52-u)*1.5));
float aa = clamp(fwidth(v)*.65, .009, .11);
float coverage = smoothstep(-aa, aa, field) * smoothstep(.0, .08, u);
return float4(0., 0., 0., coverage * .98);
