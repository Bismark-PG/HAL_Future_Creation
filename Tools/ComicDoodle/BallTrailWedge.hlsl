// Launched-ball trail: one flat triangle behind the ball, drawn from UV. No texture.
// The quad lies level behind the ball, stretched by C++ along the ball's travel: its base (V = 0) sits at the ball,
// its tip (V = 1) points back the way the ball came. Same hand-inked language as the other doodle layers: flat
// fill, black contour, heavier ink on one side, redrawn a few times per second, never faded.
// Inputs are Custom node pins supplied by Tools/ComicDoodle/CreateComicDoodleAssets.py.
//   UV      0..1
//   Time    seconds
//   Color   launcher identity colour, linear (written once per launch by C++)
//   Aspect  quad length over quad width (written every frame by C++), so the ink keeps one thickness however
//           long the triangle is stretched

float aspect = max(Aspect, 0.05);
// One unit is half the quad's width on both axes.
float2 q0 = (UV * 2.0 - 1.0) * float2(1.0, aspect);
float fps = max(Fps, 1.0);
float pose = floor(Time * fps);
float aa = max(fwidth(q0.x) + fwidth(q0.y), 1e-4) * 0.75;
// Three poses cycle; each one bends the line along a different pair of waves.
float phase = fmod(pose, 3.0) * 2.1;
float2 wobble = float2(sin(q0.y * 7.0 + phase * 3.7) + sin(q0.x * 5.0 + phase), sin(q0.x * 9.0 + phase * 2.3) + sin(q0.y * 4.0 + phase * 1.7)) * 0.5 * max(BoilAmount, 0.0);
float shadowAngle = radians(ShadowAngle);
float2 shadow = float2(cos(shadowAngle), sin(shadowAngle)) * max(ShadowOffset, 0.0);
// Everything drawn stays inside the quad.
float margin = max(InkWidth, 0.0) + max(ShadowOffset, 0.0) + max(BoilAmount, 0.0) + 0.02;
float halfWidth = clamp(BaseWidth, 0.05, 1.0 - margin);
float top = -aspect + margin;
// Two-pose flicker: every other drawing the tip is a little shorter.
float reach = max((2.0 * aspect - 2.0 * margin) * lerp(1.0, saturate(TipFlicker), fmod(pose, 2.0)), 0.05);

float silhouette = -1.0;
float body = -1.0;
for (int pass = 0; pass < 2; ++pass)
{
	float2 q = q0 + wobble - shadow * pass;
	float field = min(q.y - top, ((halfWidth - abs(q.x)) * reach - (q.y - top) * halfWidth) / length(float2(reach, halfWidth)));
	if (pass == 0) { body = field; }
	silhouette = max(silhouette, field);
}

float cover = smoothstep(-aa, aa, silhouette);
float fill = smoothstep(-aa, aa, body - max(InkWidth, 0.0));
float3 paint = lerp(Color, float3(1.0, 1.0, 1.0), saturate(ColorLift)) * max(FillGain, 0.0);
float3 rgb = lerp(InkColor, paint, fill);
return float4(rgb, saturate(cover * saturate(Opacity)));
