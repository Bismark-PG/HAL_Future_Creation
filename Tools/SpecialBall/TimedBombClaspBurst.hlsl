// Authored into M_TimedBomb_ClaspBurst by the Unreal asset script. P is measured
// in cue radii, with X along the opening axis of the two physical cage halves.
struct FClaspPaint
{
    // The accumulator holds premultiplied RGB internally. Output is straight alpha.
    void PaintOver(inout float4 layer, float3 color, float opacity)
    {
        float a=saturate(opacity);
        layer=float4(color*a+layer.rgb*(1.-a),a+layer.a*(1.-a));
    }
};
FClaspPaint brush;
float2 p=2.*UV-1.;
float t=saturate(Progress), r=length(p);
float aa=clamp(max(fwidth(p.x),fwidth(p.y)),.0009,.025);
float4 layer=0.;

// The pressure wave is exclusive to a dropped bomb. It reaches exactly one
// gameplay radius and then thins in place. Variations stay behind the wavefront.
if (GroundPulse>.5)
{
    float angle=atan2(p.y,p.x);
    float front=lerp(.09,1.,1.-pow(1.-saturate(t/.53),2.2));
    float weather=.5+.5*sin(5.*angle+.9*sin(3.*angle)-.3);
    float behind=front-r;
    float width=(.018+.034*weather)*(1.-.58*smoothstep(.35,.73,t));
    float band=smoothstep(0.,aa,behind)*(1.-smoothstep(width,width+aa,behind));
    float pressure=band*(.34+.53*weather)*smoothstep(0.,.06,t)*(1.-smoothstep(.47,.80,t));
    float leading=1.-smoothstep(.002,width*.66,behind);
    brush.PaintOver(layer,lerp(float3(.13,.032,.047),float3(.91,.28,.118),leading*(.45+.55*weather)),pressure);
}

float kick=1.-pow(1.-saturate(t/.68),2.55);
float lean=.10+.18*kick;
float s,c; sincos(lean,s,c);
float2 aligned=float2(c*p.x+s*p.y,-s*p.x+c*p.y);

// A single painted smoke mass inflates from a compressed ignition point. Interior
// folds open at different rates; the still-expanding outer lobes cool and erode.
float inflate=1.-pow(1.-saturate(t/.65),2.25);
float size=SmokeSize*(.24+.76*inflate+.11*smoothstep(.48,.96,t));
float2 smokeP=(p-float2(-.016*t,.073*t))/max(size,.001);
smokeP.y*=lerp(1.29,.90,inflate);
float2 smokeUV=.5+.5*float2(smokeP.x,-smokeP.y);
float2 fold=smokeUV-float2(.51,.61);
float turn=(inflate-.44)*.36*(1.-smoothstep(.09,.73,length(fold)));
sincos(turn,s,c);
smokeUV=float2(.51,.61)+float2(c*fold.x-s*fold.y,s*fold.x+c*fold.y);
float2 inset=min(smokeUV,1.-smokeUV);
float inside=smoothstep(0.,.022,min(inset.x,inset.y));
float4 smoke=Texture2DSample(Paint,PaintSampler,saturate(smokeUV));
float lum=dot(smoke.rgb,float3(.30,.55,.15));
float eddies=.50+.22*sin(smokeUV.x*11.+smokeUV.y*8.+1.7*sin(smokeUV.y*13.))+
                    .17*cos(smokeUV.y*16.-smokeUV.x*4.)+.10*sin(28.*smokeUV.x+3.*smokeUV.y);
float cool=smoothstep(.29,.86,t+.07*(eddies-.5));
float3 soot=lerp(float3(.038,.029,.047),float3(.26,.092,.105),sqrt(saturate(lum)));
float3 smokeColor=lerp(smoke.rgb*float3(1.13,.84,.81),soot,cool);
float erode=smoothstep(.55,1.,t);
float breakup=1.-smoothstep(.09+.55*eddies,.38+.59*eddies,erode);
float smokeAlpha=inside*smoke.a*smoothstep(0.,.06,t)*breakup*(1.-smoothstep(.76,1.,t));
brush.PaintOver(layer,smokeColor,smokeAlpha);

// Two bright creases at ignition. The cloud remains after they go out.
float flash=(1.-smoothstep(.08,.43,t))*smoothstep(0.,.035,t);
float2 hot=float2(aligned.x,aligned.y+.025);
float core=abs(hot.x)/(.08+.21*kick)+abs(hot.y)/(.027+.024*kick);
float spark=1.-smoothstep(.86,1.05,core);
brush.PaintOver(layer,float3(1.55,.43,.105)*(.69+.26*Glow),flash*(1.-smoothstep(.70,1.21,core*.74)));
brush.PaintOver(layer,float3(2.2,1.48,.84)*(.70+.26*Glow),spark*flash);
return float4(layer.rgb/max(.0001,layer.a),layer.a);
