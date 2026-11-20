// UV0 is the wall-clipped footprint. Puff carries a stable phase and the remaining
// dissolve weight in UV1. The same charcoal palette also colours the exhaust.
float2 p = UV * 2.0 - 1.0;
float phase = Puff.x * 6.2831853;
float t = Clock * FlowSpeed;
float cs = cos(phase), sn = sin(phase);
float2 q = float2(cs*p.x-sn*p.y, sn*p.x+cs*p.y);
// Turn the folds into each other as the cloud rises. The outside of each eddy
// moves further than its centre and the two eddies turn in opposite directions.
float2 eddy = q-float2(.31,-.26);
q += float2(-eddy.y,eddy.x) * (.75+.21*sin(t*.53+phase)) * (1-smoothstep(.0,.54,dot(eddy,eddy)));
eddy = q-float2(-.29,.23);
q -= float2(-eddy.y,eddy.x) * (.67+.19*cos(t*.49+phase)) * (1-smoothstep(.0,.51,dot(eddy,eddy)));
q += .072 * sin(q.yx*4.3 + float2(t*.48, -t*.36) + phase);
q += .027 * sin(float2(dot(q,float2(10,6)),dot(q,float2(-6,12))) - t*.51);

// Softly join five differently proportioned folds. Their height slopes give a
// common light direction and broad, curved cel shadows across the whole bank.
const float2 centers[5] = {float2(-.32,-.23),float2(.28,-.34),float2(.39,.19),float2(-.03,.39),float2(-.41,.15)};
const float2 widths[5] = {float2(.57,.48),float2(.48,.57),float2(.51,.43),float2(.56,.53),float2(.45,.49)};
float height = -12.0;
float2 slope = 0.0;
[unroll] for (int i=0; i<5; ++i)
{
    float2 center = centers[i] + .035 * sin(float2(2.7,4.1)*i + t*.43 + phase);
    float2 d = q-center;
    float2 inv = rcp(widths[i]*widths[i]);
    float cap = 1.0-dot(d*d,inv);
    float blend = saturate(.5+.5*(cap-height)/.55);
    height = lerp(height,cap,blend)+.55*blend*(1.0-blend);
    slope = lerp(slope,-2.0*d*inv,blend);
}
float2 worldSlope = float2(cs*slope.x+sn*slope.y,-sn*slope.x+cs*slope.y);
float lighting = dot(normalize(float3(-worldSlope*.52,1.0)),normalize(float3(-.45,-.42,.78)));
float band = .12+.42*smoothstep(.25,.33,lighting)+.37*smoothstep(.64,.73,lighting);
float remaining = saturate(Puff.y);
float breakup = .14*sin(dot(q,float2(8,4))+t*.7)+.09*sin(dot(q,float2(-7,12))-t*.53);
float silhouette = height-(1.0-remaining)*(.98+breakup);
float body = smoothstep(-.075,.18,silhouette);

// Unequal, open curls escape the dense body. Their thin outer wisps reveal the
// scene underneath while the central bank keeps its three opaque cel tones.
const float2 curls[3] = {float2(-.52,-.39),float2(.53,-.21),float2(.12,.59)};
float wisps=0;
[unroll] for(int j=0;j<3;++j)
{
    float a=j*2.39+phase*.23+t*.29;
    float2 direction=float2(cos(a),sin(a));
    float2 v=q-curls[j]-.049*float2(sin(a*1.3),cos(a*.9));
    float radius=max(.002,length(v));
    float winding=dot(v/radius,direction);
    float curlRadius=.17+.035*j+.037*winding+.019*sin(a);
    float tip=smoothstep(-.78,.62,winding);
    float arc=(1-smoothstep(.01,.062+.018*tip,abs(radius-curlRadius)))*tip;
    wisps=max(wisps,arc);
}
float fringe=wisps*smoothstep(0,.48,remaining);
float edge=(1-(1-body)*(1-.65*fringe))*(1-smoothstep(.955,1.,length(p)));
float tone=saturate(band+.21*fringe*(1-smoothstep(.30,.85,silhouette)));
return float4(lerp(DarkColor.rgb,LightColor.rgb,tone),edge*Opacity);
