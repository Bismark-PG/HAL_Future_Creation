// Object-space great circles meet at two antipodal compass badges.
float3 p = normalize((float3)LocalPoint);
float3 pole = normalize(float3(-.46, -.32, .83));
float3 east = normalize(cross(float3(0,1,0), pole));
float3 north = cross(pole, east);
float2 tangent = float2(dot(p,east), dot(p,north));
float2 uv = float2(dot(tangent,float2(.819,-.574)),dot(tangent,float2(.574,.819)));
float2 first = normalize(float2(.76,.65));
float2 second = float2(first.x,-first.y);
float2 tracks = float2(dot(uv,first),dot(uv,second));
float seam = min(abs(tracks.x),abs(tracks.y));
float along = dot(uv, float2(-first.y,first.x));
float badgeRadius = length(uv);
float aa = clamp(max(length(ddx(uv)),length(ddy(uv))) * .63, .0008, .035);

float groove = 1.-smoothstep(.168-aa,.168+aa,seam);
float rail = 1.-smoothstep(.158-aa,.158+aa,seam);
float railBead = 1.-smoothstep(.004,.011+aa,abs(seam-.149));
float enamel = 1.-smoothstep(.127-aa,.127+aa,seam);
float shoulder = smoothstep(.112-aa,.125+aa,seam);
float3 ink = float3(.045,.035,.044);
float3 alloy = Brass.rgb*float3(.95,.81,.55);
float3 base = lerp(Pearl.rgb, ink, groove);
base = lerp(base, alloy*(.64+.70*railBead), rail);
base = lerp(base, Cobalt.rgb*float3(.42,.42,.63)*(1.-.42*shoulder), enamel);
Metal = lerp(lerp(.10,.78,rail),.38,enamel);
Rough = lerp(lerp(.32,.25,rail),.26,enamel);

// A dark seat for the dial, a bevelled brass bezel, then the teal face.
float seat = 1.-smoothstep(.257-aa,.257+aa,badgeRadius);
float bezel = 1.-smoothstep(.247-aa,.247+aa,badgeRadius);
float dial = 1.-smoothstep(.212-aa,.212+aa,badgeRadius);
float lip = 1.-smoothstep(.004,.014+aa,abs(badgeRadius-.235));
float bevel = dot(uv/max(.001,badgeRadius), normalize(float2(-.40,.92)));
base = lerp(base,ink,seat);
base = lerp(base,alloy*(.76+.35*bevel+.31*lip),bezel);
base = lerp(base,DialColor.rgb*(.43+.28*smoothstep(-.15,.17,uv.y)),dial);
Metal = lerp(lerp(Metal,.81,bezel),.36,dial);
Rough = lerp(lerp(Rough,.23,bezel),.30,dial);

float2 compass = abs(uv);
float starDist = max(compass.x,compass.y) + 2.95*min(compass.x,compass.y);
float starEdge = 1.-smoothstep(.181-2.*aa,.181+2.*aa,starDist);
float starFace = 1.-smoothstep(.170-2.*aa,.170+2.*aa,starDist);
float faceA = step(0.,uv.x*uv.y);
float faceB = step(compass.y,compass.x);
float facet = abs(faceA-faceB);
base = lerp(base, Brass.rgb*.52,starEdge);
base = lerp(base,lerp(float3(.69,.46,.23),float3(1.,.91,.70),facet),starFace);
Metal = lerp(Metal,.50,starFace);
Rough = lerp(Rough,.27,starFace);

// Inset signal lights sit along one edge of the cobalt, inside the silhouette.
float2 dash = float2((abs(along)-.68)/.115,(tracks.x-.085)/.016);
float dashShape = length(float2(max(abs(dash.x)-.78,0.)*4.5,dash.y));
float glow = (1.-smoothstep(.70,1.0+aa*42.,dashShape))*enamel*(1.-seat)*step(0.,along*dot(p,pole));
base = lerp(base, Signal.rgb,glow);
// Three softly edged lacquer tones remain legible at the arena's overhead exposure.
float key = dot(normalize(SurfaceNormal),normalize(float3(-.78,.24,.56)));
float3 tone = lerp(float3(.22,.21,.24),float3(.61,.55,.50),smoothstep(-.12,.05,key));
tone = lerp(tone,float3(1.,.94,.83),smoothstep(.52,.66,key));
Glow = base*tone*.022 + Signal.rgb*(glow*SignalStrength);
return base*tone;
