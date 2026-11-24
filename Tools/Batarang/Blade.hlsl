// Each face of the authored mesh occupies one palette column in UV0.
// Keep the ivory leading bevel readable regardless of a seat's identity colour.
int panel = (int)floor(UV.x * 10.0);
float3 graphite = float3(.04,.062,.082);
float3 pearl = float3(.84,.94,.88);
float3 paint = Ink;
if (panel == 1) paint = graphite;
if (panel == 2) paint = float3(.15,.22,.25);
if (panel == 3) paint = pearl;
if (panel == 4) paint = Livery;
if (panel == 5) paint = lerp(SeatColor,pearl,.14) * (.85 + .68*Signal);
if (panel == 6) paint = lerp(pearl,float3(1.3,1.45,1.28),saturate(Signal));
return float4(paint,1);
