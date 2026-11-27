// Launched-ball flame shell: the part of the comet that wraps the ball itself, in all three dimensions.
// Drawn on an ellipsoid (engine Sphere, scaled and placed by C++, HALBallComicFX::TrailShellTransform) that surrounds
// the ball: its +X is the travel, its front tip sits just ahead of the ball, its rear tip well behind it. The shell is
// see-through where the camera looks straight at it, so the ball stays visible, and burns along its silhouette, so from
// any camera angle the ball sits inside a ring of flame: above and below it as well as left and right.
// The flat comet trail (BallTrailComet.hlsl) carries on behind it.
// Bands from the ball outward: white-hot, pale, launcher colour.
// Inputs are Custom node pins supplied by Tools/ComicDoodle/CreateComicDoodleAssets.py.
//   UV       unused
//   Time     seconds
//   Local    position on the unit sphere in the shell's own space (+X = travel)
//   Fresnel  0 where the surface faces the camera .. 1 on the silhouette
//   Color    launcher identity colour, linear (written once per launch by C++)
//   Fade     1 while live; a shell left behind runs it down to 0 and burns away from the inside out

float3 L = normalize(Local);
float f = L.x;                                   // 1 front tip .. -1 rear tip
float ang = atan2(L.z, L.y);                     // round the travel axis
float ballX = clamp(BallX, -0.9, 0.9);           // where the ball's centre sits on that axis
float ahead = saturate((f - ballX) / (1.0 - ballX));      // 0 beside the ball .. 1 at the front tip
float behind = saturate((ballX - f) / (1.0 + ballX));     // 0 beside the ball .. 1 at the rear tip

// Flame tongues running back along the shell; whole numbers round the axis, so there is no seam.
float tongues = 0.5
	+ 0.25 * sin(ang * 3.0 + f * TongueStretch + Time * ScrollSpeed)
	+ 0.25 * sin(ang * 7.0 - f * TongueStretch * 1.8 + Time * ScrollSpeed * 1.6 + 1.7);

// Thin and calm round the nose, fuller and more torn toward the back.
float level = lerp(SideLevel, FrontLevel, ahead) - behind * BackOpen;
float fade = saturate(Fade);
float heat = Fresnel - level + (tongues - 0.5) * NoiseAmount * (0.25 + behind + (1.0 - fade)) - (1.0 - fade) * max(FadeErode, 0.0);
float hw = max(fwidth(heat), 1e-4);
float bodyMask = smoothstep(-hw, hw, heat);
float midMask = smoothstep(CoreBand - hw, CoreBand + hw, heat);
float edgeMask = smoothstep(MidBand - hw, MidBand + hw, heat);

float3 paint = Color * max(FillGain, 0.0);
float3 mid = lerp(Color, float3(1.0, 1.0, 1.0), saturate(MidLift)) * max(FillGain, 0.0);
float3 hot = float3(1.0, 1.0, 1.0) * max(CoreGain, 0.0);
float3 rgb = lerp(lerp(hot, mid, midMask), paint, edgeMask);

// The rear tip dissolves into the flat trail instead of closing like an egg.
float rear = smoothstep(-1.0, -1.0 + max(RearFade, 0.01), f);
return float4(rgb, saturate(bodyMask * rear * saturate(Opacity) * saturate(fade * 4.0)));
