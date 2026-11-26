// A projection from the six ports, below the moving ball. All distances are in UV half-widths.
float2 p = 2.0 * UV - 1.0;
float r = length(p);
float angle = atan2(p.y, p.x + .00001);
float t = saturate(Progress);
float aa = clamp(fwidth(r), .002, .023);
float on = smoothstep(0.0, .09, t);
float lock = smoothstep(.07, .57, t);
float click = smoothstep(.48, .57, t) * (1.0 - smoothstep(.57, .82, t));

// Each port sends a short tapered stroke in toward the core, then goes dark.
float portAngle = abs(frac(angle * (6.0 / 6.2831853) + .5) - .5) * 1.0471976;
float head = lerp(.87, .22, lock);
float tail = saturate((r - head) / .18);
float feedDistance = max(abs(r - head - .09) - .09, portAngle * r - lerp(.023, .004, tail));
float feed = (1.0 - smoothstep(-aa, aa, feedDistance)) * (1.0 - smoothstep(.44, .62, t));

// Stamped opposing shoes slide in from either side. Three stepped faces echo the steel cage.
float2 q = float2(abs(p.x), abs(p.y));
float jawR = lerp(.80, .46, lock);
float jawDistance = max(abs(r - jawR) - .065, (q.y - 1.43 * q.x) * .57);
float jaw = 1.0 - smoothstep(-aa, aa, jawDistance);
float bevel = smoothstep(-.025 - aa, -.015 + aa, jawDistance);
float face = 1.0 - smoothstep(.010, .017 + aa, abs(r - jawR + .007));
float padDistance = max(abs(r - jawR - .007) - .033, q.y - .16);
float pad = 1.0 - smoothstep(-aa, aa, padDistance);
float3 steelPaint = Steel * (.58 + 1.55 * bevel) + Tip * (.27 * bevel + .15 * face);
float3 jawPaint = lerp(steelPaint, Pole * (1.02 + .64 * click), pad);

// The segments light clockwise and stay full at lock. The fuse starts later on contact.
float u = frac(angle / 6.2831853 + .25);
float sectors = max(4.0, round(Segments));
float cell = u * sectors;
float radius = lerp(.68, .61, lock);
float ringDistance = max(abs(r - radius) - .035, (abs(frac(cell) - .5) - .37) * (6.2831853 * radius / sectors));
float dial = 1.0 - smoothstep(-aa, aa, ringDistance);
float lit = smoothstep(0.0, .8, saturate(t / .53) * sectors - floor(cell));
float3 dialPaint = lerp(Steel * 1.20, Lamp * (1.03 + click * .45), lit);

// A small warm contact flare survives the closing jaws and narrows as it fades.
float flashRadius = lerp(.33, .23, smoothstep(.56, .94, t));
float contact = (1.0 - smoothstep(.009, .020 + aa, abs(r - flashRadius))) * click;
float jawAlpha = jaw * (1.0 - smoothstep(.62, .89, t));
float dialAlpha = dial * (1.0 - smoothstep(.63, .96, t));
float alpha = max(max(feed, jawAlpha), max(dialAlpha, contact)) * on;
float3 ink = lerp(Pole, Lamp, 1.0 - tail);
ink = lerp(ink, dialPaint, dial);
ink = lerp(ink, jawPaint, jaw);
ink = lerp(ink, Tip, contact);
return float4(ink * (.64 + .46 * Glow), saturate(alpha));
