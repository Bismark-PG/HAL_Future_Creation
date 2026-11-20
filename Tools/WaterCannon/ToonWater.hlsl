// U is distance from the nozzle in metres. Upper skin, lower skin and foam each
// have their own V band, so small airborne droplets stay bright at playing distance.
if (UV.y > 2.01)
{
    float fold = saturate((UV.y-2.02)/.96);
    float3 film = lerp(lerp(Shadow,Middle,.34),Middle,smoothstep(.04,.38,fold));
    float3 foam = lerp(film,lerp(Middle,Crest,.52),smoothstep(.39,.54,fold));
    return lerp(foam,Crest,smoothstep(.62,.80,fold));
}
float u = UV.x - Travel;
float v = UV.y > 1.0 ? UV.y-1.0 : UV.y;
float roll = .055*sin(1.75*u+5.2*v)+.028*sin(3.4*u-7.0*v);
float side = v+roll;
float face = 1.0-abs(2.0*side-.86)-.25*step(1.001,UV.y);
float3 core = lerp(Shadow,Middle,smoothstep(.22-Soft,.22+Soft,face));
float3 litWater = lerp(Middle,Crest,.38);
core = lerp(core,litWater,smoothstep(.73-Soft,.73+Soft,face));
float filament = abs(sin(14.0*side+.8*sin(1.55*u+5.0*v)+.28*sin(3.2*u)));
float broken = .5+.5*sin(2.9*u+1.1*sin(1.3*u-10.0*v)+11.0*v);
float foam = (1.0-smoothstep(.045,.19+Soft,filament))*smoothstep(.19,.68,broken);
float lip = (1.0-smoothstep(.012,.052,min(v,1.0-v)))*smoothstep(.34,.85,broken);
return lerp(core,Crest,saturate(foam+.73*lip));
