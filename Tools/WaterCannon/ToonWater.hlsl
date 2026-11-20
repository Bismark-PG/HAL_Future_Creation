// U measures metres from the nozzle; V spans the cross section. Positive travel runs downstream.
float u = UV.x - Travel;
float v = saturate(UV.y);
float drift = sin(u * 4.7 + v * 11.0) * .018 + sin(u * 2.2 - v * 8.0) * .025;
float face = 1.0 - abs(2.0 * (v + drift) - .78);
face -= step(1.0, UV.y) * .4;
float3 body = lerp(Shadow, Middle, smoothstep(.31 - Soft, .31 + Soft, face));
float ridge = smoothstep(.87 - Soft, .87 + Soft, face);
float lane = .5 + .5 * cos((v + drift) * 39.0 + sin(2.2 * u) * .6);
float dash = .5 + .5 * sin(4.9 * u + 3.0 * sin(1.7 * u + v * 8.0) + v * 7.0);
float foam = smoothstep(.76 - Soft, .76 + Soft, lane * dash);
return lerp(body, Crest, saturate(ridge * .72 + foam * .91));
