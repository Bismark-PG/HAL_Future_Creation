// UV.x measures past flight, with 1 beside the blade. A pair of pen strokes
// and a detached sliver leave negative space between the two wing tracks.
float x = saturate(UV.x);
float y = UV.y*2-1;
float aa = clamp(fwidth(y)*.65,.012,.115);
float bow = sin(3.14159*x);
float mainCenter = -.06 + .22*bow;
float mainWidth = .41*pow(x,.78)*(.85+.15*bow);
float main = min(mainWidth-abs(y-mainCenter), min(x-.075,1.-x)*4.);
float slash = min((.040+.055*x)-abs(y+.26+.21*bow), min(x-.29,.95-x)*3.);
float feather = min(.05-abs(y-.28-.23*bow), min(x,.49-x)*2.);
float silhouette = max(main,max(slash,feather));
float outer = smoothstep(-aa,aa,silhouette+.042);
float interior = smoothstep(-aa,aa,silhouette);
float3 colour = lerp(SeatColor,Livery,.60);
float innerLine = smoothstep(-aa,aa,.034+.055*x-abs(y-mainCenter-.11*x));
colour = lerp(colour,float3(.95,1.10,.87),innerLine);
colour = lerp(colour,Livery,step(main,max(slash,feather)));
return float4(lerp(Ink,colour,interior),outer*smoothstep(0.,.12,x)*(.52+.47*x));
