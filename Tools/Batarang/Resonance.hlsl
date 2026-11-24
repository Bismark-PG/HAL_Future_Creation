// A small dark latch on the socket. Burst uses this sheet only for the impact flash and flying chips;
// the expanding fire and cooling smoke are three separate, depth-tested volumes.
float2 p = UV * 2.0 - 1.0;
float aa = clamp(max(fwidth(p.x), fwidth(p.y)) * .7, .005, .07);
float t = saturate(Phase);
float radius = length(p);
float angle = atan2(p.y,p.x);
if (Burst < .5)
{
    float front = .28 + .53 * (1.-(1.-t)*(1.-t));
    float snap = min(.050-abs(radius-front), min(cos(angle*3.+.65)+.32,(1.-t)*.85-.025));
    float jaws = min(.043-abs(radius-(.48+.035*Signal)), cos(angle*2.-.5)-.45);
    return float4(0., 0., 0., smoothstep(-aa,aa,max(snap,jaws)) * max(1.-t,.32+.36*Signal));
}

// The asymmetric ivory flash peaks as the cloud buds are opening, then is gone.
float flashTime = smoothstep(.0,.055,t) * (1.-smoothstep(.14,.49,t));
float lobes = .38 + .62*pow(abs(cos(angle*3.5+.24)),3.3);
float flash = flashTime*(.15+.43*lobes) - radius;
float fragments = -2.;
float travel = 1.-pow(1.-t,2.3);
[unroll] for (int i=0; i<8; ++i)
{
    float a = (float)i*.785398 + .20*sin(i*5.3+.7);
    float2 dir = float2(cos(a),sin(a));
    float reach = .57+.28*(.5+.5*cos(i*2.91))+.10*dir.x;
    float2 d = p-dir*(.08+reach*travel);
    float lengthwise = dot(d,dir);
    float across = dot(d,float2(-dir.y,dir.x));
    float halfLen = (.06+.08*abs(dir.x))*(1.-.68*t);
    float halfWidth = (.020+.019*(.5+.5*sin(i*1.7)))*(1.-.57*t);
    float field = halfWidth*(1.-abs(lengthwise)/halfLen)-abs(across);
    fragments = max(fragments,field);
}
float chipTime = smoothstep(.015,.10,t)*(1.-smoothstep(.46,.91,t));
float chipAlpha = smoothstep(-aa,aa,fragments+.013)*chipTime;
float flashAlpha = smoothstep(-aa,aa,flash+.018)*flashTime;
float3 carbon = float3(.014,.013,.020);
float3 chips = lerp(carbon,float3(1.8,1.12,.44),smoothstep(-aa,aa,fragments));
float3 flame = lerp(float3(1.5,.32,.045),float3(2.7,2.1,1.35),smoothstep(.012,.09,flash));
float alpha = flashAlpha + chipAlpha*(1.-flashAlpha);
return float4((flame*flashAlpha + chips*chipAlpha*(1.-flashAlpha))/max(.001,alpha),alpha);
