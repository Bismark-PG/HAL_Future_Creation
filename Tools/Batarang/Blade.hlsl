// UV0 selects the authored facets. RGB supplies the metal base and A is roughness.
int panel = (int)floor(UV.x * 10.0);
float3 coat = Ink;
float3 steel = float3(.55, .57, .58);
float3 base = coat * .42;
float roughness = .39;
if (panel == 1) { base = coat; roughness = .32; }
if (panel == 2) { base = coat * 2.3; roughness = .25; }
if (panel == 3) { base = steel; roughness = .20; } // narrow polished leading bevel
if (panel == 4) { base = lerp(coat, steel, .11); roughness = .28; }
if (panel == 5) { base = lerp(coat, steel, .18 + .22*saturate(Signal)); roughness = .21; }
if (panel == 6) { base = steel * (.46 + .50*saturate(Signal)); roughness = .17; }
return float4(base, roughness);
