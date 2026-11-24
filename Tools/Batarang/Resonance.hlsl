// UV opens along +X (the published push direction). Positive signed fields
// cover ink. Negative space inside the crests leaves the car visible.
float2 p = UV*2.-1.;
float aa = clamp(max(fwidth(p.x),fwidth(p.y))*.65,.006,.08);
float t = saturate(Phase);
float field = -2., hot = -2., seat = -2., opacity = 1.;
if (Burst < .5)
{
    float radius = length(p);
    float angle = atan2(p.y,p.x);
    float front = .25+.61*(1.-(1.-t)*(1.-t));
    float notch = cos(angle*3.+.65);
    float snap = min(.053-abs(radius-front), min(notch+.35,(1.-t)*.9-.025));
    float jaw = min(.055-abs(radius-(.54+.055*Signal)),cos(angle*2.-.5)-.30);
    float tick = min(.066-abs(abs(p.y)-.10), min(p.x-.49,.79-p.x));
    field = max(snap,max(jaw,tick));
    hot = max(snap,tick);
    seat = jaw;
    opacity = max(1.-t,.46+.48*Signal);
}
else
{
    float move = 1.-pow(1.-t,2.1);
    // These open pressure fronts translate away from the socket, with an empty wake behind.
    float wing = .18+.56*move;
    float lateral = p.y/wing;
    float nose = .13+.75*move - (.20+.20*move)*lateral*lateral;
    nose += .009*sin(10.*lateral+.6);
    float lead = min(.032*(1.-.35*t)-abs(p.x-nose),(1.-abs(lateral))*wing);
    float echoX = nose-.12-.10*move+.048*lateral;
    float trailing = min(.023-abs(p.x-echoX),(.83-abs(lateral))*wing);
    trailing = min(trailing,abs(lateral+.25)-.12);
    float shards = -2.;
    [unroll] for (int i=0;i<7;++i)
    {
        float side = (float)i-3.;
        float start = -.04+move*(.16+.13*sin(i*2.25));
        float end = .15+move*(.49+.13*cos(i*2.17));
        float u = saturate((p.x-start)/(end-start));
        float center = side*.033 + (side*.096+.029*sin(i*1.7))*(.25+move)*u;
        float dart = min((.014+.021*(1.-t))*(1.-u)-abs(p.y-center),min(p.x-start,end-p.x));
        shards = max(shards,dart);
    }
    // A short contact flash lies behind the two moving pressure fronts.
    float flash = .25*(1.-smoothstep(.0,.45,t))-(abs(p.x)*.66+abs(p.y)*1.24);
    field = max(max(lead,trailing),max(shards,flash));
    hot = max(flash,lead);
    seat = shards;
    opacity = smoothstep(0.,.045,t)*(1.-smoothstep(.62,1.,t));
}
float outer = smoothstep(-aa,aa,field+(Burst < .5 ? .023 : .014));
float inner = smoothstep(-aa,aa,field);
float3 paint = lerp(Livery,lerp(SeatColor,Livery,.20),smoothstep(-aa,aa,seat));
paint = lerp(paint,float3(1.15,1.28,1.04),smoothstep(-aa,aa,hot-.005));
return float4(lerp(Ink,paint,inner),outer*opacity);
