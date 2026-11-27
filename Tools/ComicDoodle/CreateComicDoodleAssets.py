"""Author the ball and vehicle readability materials (held aim arrow and hold rings, launched-ball trails, vehicle status marks) through official Unreal Editor APIs.

Creates or rebuilds, in this order (every asset below is fully owned by this script):
  1. /Game/Assets/FX/Materials/M_FX_BallAimArrow    parent
  2. /Game/Assets/FX/Materials/MI_FX_BallAimArrow   instance referenced by UHALBallComicFXSettings (0 overrides)
  3. /Game/Assets/FX/Materials/M_FX_BallPulseRing   parent of the launched-ball pulse rings
  4. /Game/Assets/FX/Materials/MI_FX_BallPulseRing  the only sprite renderer of NS_BallInkTrail (0 overrides)
  4a. /Game/Assets/FX/Materials/M_FX_BallTrailWedge   parent of the launched-ball triangle trail
  4b. /Game/Assets/FX/Materials/MI_FX_BallTrailWedge  triangle trail instance (alternative style, 0 overrides)
  4c. /Game/Assets/FX/Materials/M_FX_BallTrailComet   parent of the launched-ball comet trail
  4d. /Game/Assets/FX/Materials/MI_FX_BallTrailComet  instance referenced by UHALBallComicFXSettings (0 overrides)
  4e. /Game/Assets/FX/Materials/M_FX_BallFlameShell   parent of the 3D flame wrap round the launched ball
  4f. /Game/Assets/FX/Materials/MI_FX_BallFlameShell  instance referenced by UHALBallComicFXSettings (0 overrides)
  5. /Game/Assets/FX/Materials/M_FX_DoodleStatus    parent
  6. /Game/Assets/FX/Materials/MI_FX_DoodleStatus_Stun       circling stars, referenced by UHALVehicleDoodleStatusSettings
  7. /Game/Assets/FX/Materials/MI_FX_DoodleStatus_LowHealth  sweat drops (only override: FX_Kind = 1)
  8. /Game/Assets/FX/Materials/M_FX_BallHoldRing    parent of the held-ball ownership marks
  9. /Game/Assets/FX/Materials/MI_FX_BallHoldRing_Ball  orbit rings on the held ball (0 overrides)
 10. /Game/Assets/FX/Materials/MI_FX_BallHoldRing_Roof  holder marker over the car (only override: FX_Kind = 1)

Idempotent: parent graphs are rebuilt on every run, instances keep their identity so the settings and the
Niagara renderer never lose their reference. The Niagara system NS_BallInkTrail and its modules
(/Game/Assets/FX/Modules/NM_BallPulseRate, NM_BallPulseSpawn, NM_BallPulseUpdate) are authored through the editor
connection and recorded in Docs/Presentation/Comic_Doodle_FX_CN_v0.1.md (HLSL text of record in
Tools/ComicDoodle/NiagaraBallPulse*.hlsl); this script does not touch them.
Retired and no longer authored here: the ribbon material M_FX_BallInkTrail and the flame materials M_FX_BallFlame /
MI_FX_BallFlame_Ink / _Fill / _Heart (recoverable from commit 3f8041d).
Writes Saved/ComicDoodle/ComicDoodle_Authoring.json and fails loudly instead of half-applying.

Run headless with the editor closed:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Tools/ComicDoodle/CreateComicDoodleAssets.py" -AllowCommandletRendering
Without -AllowCommandletRendering (or with -nullrhi) no shader is compiled, and the script stops on its own
"No compiled pixel shader statistics" check before saving anything.
An open editor locks the packages, so run it inside that editor instead when one is running.
"""
import json
import pathlib
import sys
import traceback
import unreal

ROOT = pathlib.Path(unreal.Paths.project_dir()).resolve()
WORK = ROOT / 'Saved/ComicDoodle'
WORK.mkdir(parents=True, exist_ok=True)
MAT = unreal.MaterialEditingLibrary
ASSET = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

BLACK = unreal.LinearColor(0.0, 0.0, 0.0, 1.0)

# Parameter name, HLSL pin, default, slider minimum, slider maximum, parameter group.
ARROW = {
    'marker': 'HAL Ball Aim Arrow v2 (comet flame arrow)',
    'material': '/Game/Assets/FX/Materials/M_FX_BallAimArrow',
    'instance': '/Game/Assets/FX/Materials/MI_FX_BallAimArrow',
    'hlsl': 'Tools/ComicDoodle/BallAimArrow.hlsl',
    'usage': None,
    # The ground quad must be cut by cars standing on it, so depth testing stays on.
    'disable_depth_test': False,
    'sources': ('UV', 'Time'),
    'scalars': (
        ('FX_FlipAxes', 'FlipAxes', 0.0, 0.0, 1.0, '01 Mesh Mapping'),     # 1 when the quad's V runs along its +X
        ('FX_Reverse', 'Reverse', 0.0, 0.0, 1.0, '01 Mesh Mapping'),       # 1 when the mapped axis runs tip to tail
        ('FX_HeadBase', 'HeadBase', -0.05, -0.8, 0.6, '02 Shape'),         # where the head starts, -1 tail .. +1 tip
        ('FX_HeadWidth', 'HeadWidth', 0.8, 0.1, 1.0, '02 Shape'),          # half width of the head base
        ('FX_ShaftWidth', 'ShaftWidth', 0.34, 0.02, 1.0, '02 Shape'),      # half width of the shaft
        ('FX_FlameDensity', 'FlameDensity', 5.0, 0.5, 16.0, '03 Flame'),   # flame tongues across the arrow
        ('FX_FlameAmount', 'FlameAmount', 0.22, 0.0, 0.6, '03 Flame'),     # how torn the tail end's edge is
        ('FX_ScrollSpeed', 'ScrollSpeed', 3.0, 0.0, 20.0, '03 Flame'),     # tongues flowing toward the tip
        ('FX_PulseSpeed', 'PulseSpeed', 6.0, 0.0, 20.0, '03 Flame'),       # heat pulse running to the tip
        ('FX_PulseDepth', 'PulseDepth', 0.08, 0.0, 0.3, '03 Flame'),       # how much the white heart shrinks between pulses
        ('FX_MidDepth', 'MidDepth', 0.11, 0.0, 0.5, '04 Bands'),           # distance in from the edge where the pale band starts
        ('FX_CoreDepth', 'CoreDepth', 0.22, 0.0, 0.5, '04 Bands'),         # ... and where the white heart starts
        ('FX_MidLift', 'MidLift', 0.35, 0.0, 1.0, '04 Bands'),             # pale band toward white
        ('FX_AuraWidth', 'AuraWidth', 0.08, 0.01, 0.4, '05 Aura'),
        ('FX_AuraStrength', 'AuraStrength', 0.2, 0.0, 1.0, '05 Aura'),
        ('FX_FillGain', 'FillGain', 1.05, 0.0, 8.0, '06 Color'),
        ('FX_CoreGain', 'CoreGain', 1.15, 0.0, 8.0, '06 Color'),
        ('FX_Opacity', 'Opacity', 0.88, 0.0, 1.0, '06 Color'),
    ),
    'vectors': (
        ('FX_Color', 'Color', unreal.LinearColor(0.98, 0.62, 0.12, 1.0), '00 Driven By Runtime'),
    ),
    'driven': ('FX_Color',),
}
# Pulse rings of the launched-ball trail ("force field" layers the ball leaves behind). NS_BallInkTrail draws each
# ring as one flat sprite (custom facing +Z, aligned to the ball's travel when the ring is born) with this single
# instance: colour band plus black ink on both sides in one pass. Niagara decides every length in centimetres from
# User.TrailWidth and hands it over per particle (Particle Color, Particle Size, Dynamic Parameters 0 and 1, see
# BallPulseRing.hlsl), so nothing is driven by C++ at the material level and the scalars only shape the drawing.
PULSE = {
    'marker': 'HAL Ball Pulse Ring v1 (comic force-field ring, ink both sides)',
    'material': '/Game/Assets/FX/Materials/M_FX_BallPulseRing',
    'instance': '/Game/Assets/FX/Materials/MI_FX_BallPulseRing',
    'hlsl': 'Tools/ComicDoodle/BallPulseRing.hlsl',
    'usage': 'used_with_niagara_sprites',
    'disable_depth_test': False,
    'sources': ('UV', 'Color', 'Alpha', 'Size', 'Dynamic', 'Dynamic1'),
    'scalars': (
        ('FX_BoilWaves', 'BoilWaves', 3.0, 1.0, 8.0, '01 Line'),          # lobes of the outline wobble (whole numbers)
        ('FX_Pressure', 'Pressure', 0.2, 0.0, 0.6, '01 Line'),            # band swell around the ring, share of its width
        ('FX_CoreWidth', 'CoreWidth', 0.3, 0.0, 1.0, '02 Core'),          # pale centre line, share of the band half width
        ('FX_CoreLife', 'CoreLife', 0.45, 0.05, 1.0, '02 Core'),          # normalised age at which the centre line is gone
        ('FX_StartCoreLife', 'StartCoreLife', 1.5, 0.5, 3.0, '02 Core'),  # CoreLife multiplier on the start pulse
        # R9 PIE: 0.55 / 1.6 washed pale identity colours (e.g. lavender) almost white; 0.3 / 1.3 keeps the hue.
        ('FX_CoreLift', 'CoreLift', 0.3, 0.0, 1.0, '02 Core'),            # centre line toward white
        ('FX_CoreGain', 'CoreGain', 1.3, 0.0, 8.0, '02 Core'),            # centre line brightness, so it stays bright, not grey
        ('FX_ColorLift', 'ColorLift', 0.0, 0.0, 1.0, '03 Color'),
        ('FX_FillGain', 'FillGain', 1.0, 0.0, 8.0, '03 Color'),
        ('FX_Opacity', 'Opacity', 1.0, 0.0, 1.0, '03 Color'),
    ),
    'vectors': (
        ('FX_InkColor', 'InkColor', BLACK, '03 Color'),
    ),
    'driven': (),
}

# Triangle trail of the launched ball: one level quad behind the ball, stretched along its travel by C++.
# The default trail from 2026-11-24 to 2026-11-27 (replaced by COMET); stays authored as an alternative style.
WEDGE = {
    'marker': 'HAL Ball Trail Wedge v1 (flat inked triangle)',
    'material': '/Game/Assets/FX/Materials/M_FX_BallTrailWedge',
    'instance': '/Game/Assets/FX/Materials/MI_FX_BallTrailWedge',
    'hlsl': 'Tools/ComicDoodle/BallTrailWedge.hlsl',
    'usage': None,
    # A level quad at the ball's height: the ball and the cars cut it like any other object.
    'disable_depth_test': False,
    'sources': ('UV', 'Time'),
    'scalars': (
        ('FX_Aspect', 'Aspect', 2.0, 0.05, 20.0, '00 Driven By Runtime'),  # quad length over width
        ('FX_Fps', 'Fps', 8.0, 1.0, 24.0, '01 Motion'),                   # drawings per second
        ('FX_BoilAmount', 'BoilAmount', 0.03, 0.0, 0.2, '01 Motion'),     # line wobble, half widths
        ('FX_TipFlicker', 'TipFlicker', 0.88, 0.5, 1.0, '01 Motion'),     # triangle length on every other drawing
        ('FX_BaseWidth', 'BaseWidth', 0.8, 0.05, 1.0, '02 Shape'),        # half width of the base, quad half widths
        ('FX_InkWidth', 'InkWidth', 0.1, 0.0, 0.4, '03 Ink'),
        ('FX_ShadowOffset', 'ShadowOffset', 0.05, 0.0, 0.3, '03 Ink'),
        ('FX_ShadowAngle', 'ShadowAngle', 45.0, -180.0, 180.0, '03 Ink'),  # side of the heavy ink, degrees in UV space
        ('FX_ColorLift', 'ColorLift', 0.0, 0.0, 1.0, '04 Color'),
        ('FX_FillGain', 'FillGain', 1.05, 0.0, 8.0, '04 Color'),
        ('FX_Opacity', 'Opacity', 0.7, 0.0, 1.0, '04 Color'),                 # team asked for a see-through trail
    ),
    'vectors': (
        ('FX_Color', 'Color', unreal.LinearColor(0.98, 0.62, 0.12, 1.0), '00 Driven By Runtime'),
        ('FX_InkColor', 'InkColor', BLACK, '04 Color'),
    ),
    'driven': ('FX_Color', 'FX_Aspect'),
}

# Comet trail of the launched ball: same quad and C++ contract as WEDGE (FX_Color, FX_Aspect, V = 0 at the ball),
# drawn as an anime fireball streak: white-hot core, launcher colour flame wrapping the front of the ball, broken speed
# wisps, lightning sparks. Also needs FX_Ball (UHALBallComicFXSettings TrailWedgeBallParameter).
# The default trail since 2026-11-27; WEDGE stays authored as an alternative style.
COMET = {
    'marker': 'HAL Ball Trail Comet v1 (anime fireball streak)',
    'material': '/Game/Assets/FX/Materials/M_FX_BallTrailComet',
    'instance': '/Game/Assets/FX/Materials/MI_FX_BallTrailComet',
    'hlsl': 'Tools/ComicDoodle/BallTrailComet.hlsl',
    'usage': None,
    'disable_depth_test': False,
    'sources': ('UV', 'Time'),
    'scalars': (
        ('FX_Aspect', 'Aspect', 4.0, 0.05, 20.0, '00 Driven By Runtime'),  # quad length over width
        ('FX_Fps', 'Fps', 12.0, 1.0, 30.0, '01 Motion'),                  # spark / wisp redraws per second
        ('FX_ScrollSpeed', 'ScrollSpeed', 6.0, 0.0, 30.0, '01 Motion'),    # flame streaks sliding to the tail
        ('FX_BendSpeed', 'BendSpeed', 9.0, 0.0, 30.0, '01 Motion'),
        ('FX_BendAmount', 'BendAmount', 0.06, 0.0, 0.3, '01 Motion'),      # tail sway, half widths
        ('FX_BodyLength', 'BodyLength', 0.72, 0.1, 1.0, '02 Body'),       # share of the quad the flame body covers
        ('FX_HeadWidth', 'HeadWidth', 0.85, 0.1, 0.9, '02 Body'),         # half width at the ball, quad half widths
        ('FX_CapLength', 'CapLength', 0.5, 0.01, 2.0, '02 Body'),         # rounding of the front without a ball, half widths
        ('FX_WrapFront', 'WrapFront', 0.25, 0.0, 1.0, '02 Body'),         # flame shell ahead of the ball, ball radii
        ('FX_WrapRim', 'WrapRim', 0.08, 0.0, 0.5, '02 Body'),             # white rim against the ball's front, ball radii
        ('FX_TaperPower', 'TaperPower', 0.75, 0.1, 3.0, '02 Body'),       # <1 fat comet, >1 thin spike
        ('FX_StreakDensity', 'StreakDensity', 4.5, 0.5, 12.0, '02 Body'), # flame tongues across the width
        ('FX_NoiseAmount', 'NoiseAmount', 1.2, 0.0, 2.0, '02 Body'),      # how torn the flame edge is
        ('FX_Cooling', 'Cooling', 0.6, 0.0, 1.5, '02 Body'),             # heat lost toward the tail
        ('FX_EdgeLevel', 'EdgeLevel', 0.04, -0.5, 0.9, '03 Bands'),       # heat at the flame silhouette
        ('FX_MidLevel', 'MidLevel', 0.42, 0.0, 0.95, '03 Bands'),          # heat of the pale colour band
        ('FX_CoreLevel', 'CoreLevel', 0.74, 0.0, 1.0, '03 Bands'),        # heat of the white core
        ('FX_MidLift', 'MidLift', 0.35, 0.0, 1.0, '03 Bands'),            # pale band toward white
        ('FX_AuraWidth', 'AuraWidth', 0.5, 0.01, 2.0, '04 Aura'),         # glow outside the body, body half widths
        ('FX_AuraStrength', 'AuraStrength', 0.18, 0.0, 1.0, '04 Aura'),
        ('FX_WispCount', 'WispCount', 4.0, 0.0, 6.0, '05 Wisps'),
        ('FX_WispWidth', 'WispWidth', 0.04, 0.0, 0.2, '05 Wisps'),       # half thickness at the root, half widths
        ('FX_SparkReach', 'SparkReach', 0.35, 0.0, 1.0, '06 Sparks'),      # share of the body the lightning runs along
        ('FX_SparkSegment', 'SparkSegment', 0.35, 0.05, 2.0, '06 Sparks'),  # zig-zag step, half widths
        ('FX_SparkJitter', 'SparkJitter', 0.12, 0.0, 0.4, '06 Sparks'),
        ('FX_SparkWidth', 'SparkWidth', 0.016, 0.0, 0.1, '06 Sparks'),
        ('FX_SparkChance', 'SparkChance', 0.4, 0.0, 1.0, '06 Sparks'),    # share of drawings on which a side shows its bolt
        ('FX_FillGain', 'FillGain', 1.05, 0.0, 8.0, '07 Color'),
        ('FX_CoreGain', 'CoreGain', 1.15, 0.0, 8.0, '07 Color'),            # >1 lets bloom pick the core up
        ('FX_Opacity', 'Opacity', 0.75, 0.0, 1.0, '07 Color'),
        ('FX_Fade', 'Fade', 1.0, 0.0, 1.0, '00 Driven By Runtime'),       # 1 live .. 0 dissolved (left-behind trails)
        ('FX_FadeErode', 'FadeErode', 0.9, 0.0, 2.0, '08 Dissolve'),      # heat taken away by a full dissolve
    ),
    'vectors': (
        ('FX_Color', 'Color', unreal.LinearColor(1.0, 0.18, 0.55, 1.0), '00 Driven By Runtime'),
        # R ball centre from the front edge, G drawn ball radius, quad half widths; 0 radius = no ball on the quad.
        ('FX_Ball', 'Ball', unreal.LinearColor(0.0, 0.0, 0.0, 0.0), '00 Driven By Runtime'),
        # R path distance behind the ball at the quad's front edge, G whole trail length, quad half widths; G = 0: no path.
        ('FX_Path', 'Path', unreal.LinearColor(0.0, 0.0, 0.0, 0.0), '00 Driven By Runtime'),
    ),
    'driven': ('FX_Color', 'FX_Aspect', 'FX_Ball', 'FX_Path', 'FX_Fade'),
}

# Flame shell of the launched ball: the comet's wrap round the ball itself, drawn on an ellipsoid (engine Sphere placed by
# C++, HALBallComicFX::TrailShellTransform) so the flame also covers the ball in height. Referenced by
# UHALBallComicFXSettings.TrailShellMaterial; shares FX_Color with the trail quad.
SHELL = {
    'marker': 'HAL Ball Flame Shell v1 (3D flame wrap round the launched ball)',
    'material': '/Game/Assets/FX/Materials/M_FX_BallFlameShell',
    'instance': '/Game/Assets/FX/Materials/MI_FX_BallFlameShell',
    'hlsl': 'Tools/ComicDoodle/BallFlameShell.hlsl',
    'usage': None,
    'disable_depth_test': False,
    # Front faces only: the far half of the shell is behind the ball and would fill the ring solid.
    'two_sided': False,
    'sources': ('UV', 'Time', 'Local', 'Fresnel'),
    'scalars': (
        # Ball centre on the shell's axis, -1 rear tip .. 1 front tip: (Back - Front) / (Back + Front) of the
        # TrailShell*Radii settings (2.4 and 1.2 give 0.333).
        ('FX_BallX', 'BallX', 0.333, -0.9, 0.9, '01 Shape'),
        ('FX_FrontLevel', 'FrontLevel', 0.5, 0.0, 1.0, '01 Shape'),      # silhouette share left clear at the nose (higher = thinner flame)
        ('FX_SideLevel', 'SideLevel', 0.4, 0.0, 1.0, '01 Shape'),         # same beside the ball
        ('FX_BackOpen', 'BackOpen', 0.18, 0.0, 1.0, '01 Shape'),          # how much fuller the flame gets toward the back
        ('FX_RearFade', 'RearFade', 0.6, 0.01, 2.0, '01 Shape'),          # length of the dissolve at the rear tip, axis units
        ('FX_NoiseAmount', 'NoiseAmount', 0.35, 0.0, 1.5, '02 Motion'),   # depth of the flame tongues
        ('FX_TongueStretch', 'TongueStretch', 5.0, 0.0, 20.0, '02 Motion'),  # twist of the tongues along the travel
        ('FX_ScrollSpeed', 'ScrollSpeed', 14.0, 0.0, 40.0, '02 Motion'),
        ('FX_CoreBand', 'CoreBand', 0.05, 0.0, 1.0, '03 Bands'),           # width of the white band against the ball
        ('FX_MidBand', 'MidBand', 0.14, 0.0, 1.0, '03 Bands'),            # where the pale band gives way to the colour
        ('FX_MidLift', 'MidLift', 0.35, 0.0, 1.0, '03 Bands'),
        ('FX_FillGain', 'FillGain', 1.05, 0.0, 8.0, '04 Color'),
        ('FX_CoreGain', 'CoreGain', 1.15, 0.0, 8.0, '04 Color'),
        ('FX_Opacity', 'Opacity', 0.7, 0.0, 1.0, '04 Color'),
        ('FX_Fade', 'Fade', 1.0, 0.0, 1.0, '00 Driven By Runtime'),       # 1 live .. 0 dissolved (shell left behind)
        ('FX_FadeErode', 'FadeErode', 0.7, 0.0, 2.0, '05 Dissolve'),
    ),
    'vectors': (
        ('FX_Color', 'Color', unreal.LinearColor(1.0, 0.18, 0.55, 1.0), '00 Driven By Runtime'),
    ),
    'driven': ('FX_Color', 'FX_Fade'),
}

STATUS = {
    'marker': 'HAL Vehicle Doodle Status v1 (stun stars / sweat drops)',
    'material': '/Game/Assets/FX/Materials/M_FX_DoodleStatus',
    'instance': '/Game/Assets/FX/Materials/MI_FX_DoodleStatus_Stun',
    # Extra instances: path, {parameter: override}.
    'variants': (('/Game/Assets/FX/Materials/MI_FX_DoodleStatus_LowHealth', {'FX_Kind': 1.0}),),
    'hlsl': 'Tools/ComicDoodle/DoodleStatus.hlsl',
    'usage': None,
    # Status marks float over the car and must read even when another car drives in front of them.
    'disable_depth_test': True,
    'sources': ('UV', 'Time'),
    'scalars': (
        ('FX_Kind', 'Kind', 0.0, 0.0, 1.0, '01 Kind'),                    # 0 stun stars, 1 sweat drops
        ('FX_Fps', 'Fps', 8.0, 1.0, 24.0, '02 Motion'),                   # poses per second (drawn on twos)
        ('FX_TurnsPerSecond', 'TurnsPerSecond', 0.6, 0.0, 4.0, '02 Motion'),  # star orbit speed
        ('FX_OrbitWidth', 'OrbitWidth', 0.66, 0.1, 0.9, '03 Stars'),      # orbit half width, sprite half extents
        ('FX_OrbitHeight', 'OrbitHeight', 0.26, 0.0, 0.9, '03 Stars'),    # orbit half height: the squash that reads as a ring
        ('FX_StarSize', 'StarSize', 0.27, 0.05, 0.6, '03 Stars'),
        ('FX_StarPinch', 'StarPinch', 0.62, 0.2, 1.0, '03 Stars'),
        ('FX_DropSize', 'DropSize', 0.3, 0.05, 0.6, '04 Drops'),
        ('FX_DropReach', 'DropReach', 0.72, 0.2, 1.0, '04 Drops'),        # how far the drops fly, sprite half extents
        ('FX_DropsPerSecond', 'DropsPerSecond', 1.6, 0.1, 6.0, '04 Drops'),
        ('FX_InkWidth', 'InkWidth', 0.055, 0.0, 0.2, '05 Ink'),
        ('FX_ShadowOffset', 'ShadowOffset', 0.025, 0.0, 0.2, '05 Ink'),
        ('FX_FillGain', 'FillGain', 1.0, 0.0, 8.0, '06 Color'),
        ('FX_HeartMix', 'HeartMix', 0.4, 0.0, 1.0, '06 Color'),            # 0 flat fill, 1 paper white heart,
        ('FX_Opacity', 'Opacity', 1.0, 0.0, 1.0, '06 Color'),
    ),
    'vectors': (
        ('FX_StarColor', 'StarColor', unreal.LinearColor(1.0, 0.75, 0.07, 1.0), '06 Color'),
        ('FX_DropColor', 'DropColor', unreal.LinearColor(0.12, 0.6, 1.0, 1.0), '06 Color'),
        ('FX_InkColor', 'InkColor', BLACK, '06 Color'),
    ),
    'driven': (),
}

# Held-ball ownership marks: orbit rings on the ball and the marker floating over the holder. They replace the
# magnet beam arc while UHALBallComicFXSettings.bHoldReplacesBeam is set.
HOLD = {
    'marker': 'HAL Ball Hold Ring v3 (orbiting comets / flame holder marker)',
    'material': '/Game/Assets/FX/Materials/M_FX_BallHoldRing',
    'instance': '/Game/Assets/FX/Materials/MI_FX_BallHoldRing_Ball',
    'variants': (('/Game/Assets/FX/Materials/MI_FX_BallHoldRing_Roof', {'FX_Kind': 1.0}),),
    'hlsl': 'Tools/ComicDoodle/BallHoldRing.hlsl',
    'usage': None,
    # Ownership must read even when another car drives in front of the ball or the roof.
    'disable_depth_test': True,
    'sources': ('UV', 'Time'),
    'scalars': (
        ('FX_Kind', 'Kind', 0.0, 0.0, 1.0, '01 Kind'),                    # 0 ball orbit rings, 1 holder marker
        ('FX_FlipV', 'FlipV', 0.0, 0.0, 1.0, '01 Kind'),                  # 1 when the far side of the ball is at V = 1
        ('FX_BallRadius', 'BallRadius', 0.5, 0.0, 1.0, '00 Driven By Runtime'),  # drawn ball radius, sprite half extents
        ('FX_OrbitWidth', 'OrbitWidth', 0.88, 0.1, 0.95, '02 Ball Rings'),  # ring half length, sprite half extents
        ('FX_OrbitHeight', 'OrbitHeight', 0.27, 0.02, 0.9, '02 Ball Rings'),  # ring half height: the squash that reads as an orbit
        ('FX_OrbitLine', 'OrbitLine', 0.075, 0.005, 0.2, '02 Ball Rings'),  # half thickness at the comet's head
        ('FX_OrbitTilt', 'OrbitTilt', 18.0, 0.0, 60.0, '02 Ball Rings'),  # degrees; the second ring leans the other way
        ('FX_OrbitWobble', 'OrbitWobble', 3.0, 0.0, 15.0, '02 Ball Rings'),  # degrees of slow rocking
        ('FX_OrbitSpeed', 'OrbitSpeed', 1.1, 0.0, 6.0, '02 Ball Rings'),  # turns per second of each comet
        ('FX_HeadSwell', 'HeadSwell', 0.9, 0.0, 3.0, '02 Ball Rings'),    # extra thickness of the bead at the head
        ('FX_TailThin', 'TailThin', 0.5, 0.0, 1.0, '02 Ball Rings'),      # thickness left at the end of the tail
        ('FX_TailAlpha', 'TailAlpha', 0.75, 0.0, 1.0, '02 Ball Rings'),   # opacity left at the end of the tail
        ('FX_CoreHeat', 'CoreHeat', 0.86, 0.0, 1.0, '02 Ball Rings'),      # how much of the head is white-hot (lower = more)
        ('FX_MidHeat', 'MidHeat', 0.66, 0.0, 1.0, '02 Ball Rings'),       # where the pale band gives way to the colour
        ('FX_MarkerArrowTop', 'MarkerArrowTop', -0.31, -0.9, 0.8, '03 Holder Marker'),  # V of the arrow head's base; centred by default
        ('FX_MarkerArrowLength', 'MarkerArrowLength', 0.62, 0.05, 1.5, '03 Holder Marker'),  # base to tip
        ('FX_MarkerArrowWidth', 'MarkerArrowWidth', 0.4, 0.05, 0.9, '03 Holder Marker'),  # half width of the base
        ('FX_MarkerBob', 'MarkerBob', 0.07, 0.0, 0.3, '03 Holder Marker'),  # travel of the bob along the arrow
        ('FX_MarkerBobSpeed', 'MarkerBobSpeed', 7.0, 0.0, 30.0, '03 Holder Marker'),
        ('FX_MarkerMidDepth', 'MarkerMidDepth', 0.09, 0.0, 0.4, '03 Holder Marker'),   # distance in from the edge of the pale band
        ('FX_MarkerCoreDepth', 'MarkerCoreDepth', 0.17, 0.0, 0.4, '03 Holder Marker'),  # ... and of the white heart
        ('FX_FlameDensity', 'FlameDensity', 6.0, 0.5, 16.0, '04 Flame'),  # marker: flame tongues across the arrow
        ('FX_FlameAmount', 'FlameAmount', 0.16, 0.0, 0.6, '04 Flame'),    # marker: torn base edge; rings: tail flicker
        ('FX_ScrollSpeed', 'ScrollSpeed', 4.0, 0.0, 20.0, '04 Flame'),    # marker: tongues streaming back from the tip
        ('FX_AuraWidth', 'AuraWidth', 0.06, 0.01, 0.4, '05 Aura'),
        ('FX_AuraStrength', 'AuraStrength', 0.22, 0.0, 1.0, '05 Aura'),
        ('FX_MidLift', 'MidLift', 0.35, 0.0, 1.0, '06 Color'),
        ('FX_FillGain', 'FillGain', 1.05, 0.0, 8.0, '06 Color'),
        ('FX_CoreGain', 'CoreGain', 1.15, 0.0, 8.0, '06 Color'),
        ('FX_Opacity', 'Opacity', 0.9, 0.0, 1.0, '06 Color'),
    ),
    'vectors': (
        ('FX_Color', 'Color', unreal.LinearColor(0.98, 0.62, 0.12, 1.0), '00 Driven By Runtime'),
    ),
    'driven': ('FX_Color', 'FX_BallRadius'),
}

report = {}


def node(material, cls, x, y, **properties):
    value = MAT.create_material_expression(material, cls, x, y)
    if not value:
        raise RuntimeError('Could not create ' + str(cls))
    for key, item in properties.items():
        value.set_editor_property(key, item)
    return value


def wire(source, target, input_name='', output=''):
    if not MAT.connect_material_expressions(source, output, target, input_name):
        raise RuntimeError('Failed connection: ' + (input_name or 'default'))


def ensure_asset(path, cls, factory):
    existing = unreal.load_asset(path)
    if existing is not None:
        if not isinstance(existing, cls):
            raise RuntimeError('Existing asset has the wrong class: ' + path)
        return existing, False
    package, name = path.rsplit('/', 1)
    created = TOOLS.create_asset(name, package, cls, factory)
    if created is None:
        raise RuntimeError('Could not create ' + path)
    return created, True


def clear_graph(material):
    """delete_all_material_expressions leaves every second node behind; delete from fresh lists instead."""
    for _ in range(16):
        expressions = list(MAT.get_material_expressions(material))
        if not expressions:
            return
        for item in expressions:
            MAT.delete_material_expression(material, item)
    raise RuntimeError('Could not empty the material graph before rebuilding')


def build(spec):
    out = report.setdefault(spec['material'], {})
    material, created = ensure_asset(spec['material'], unreal.Material, unreal.MaterialFactoryNew())
    out['material_created'] = created
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided', spec.get('two_sided', True))
    material.set_editor_property('disable_depth_test', spec['disable_depth_test'])
    if spec['usage']:
        material.set_editor_property(spec['usage'], True)
    clear_graph(material)

    code = (ROOT / spec['hlsl']).read_text(encoding='utf-8')
    custom = node(material, unreal.MaterialExpressionCustom, 400, 0, description=spec['marker'], desc=spec['marker'],
                  code=code, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    names = spec['sources'] + tuple(pin for _, pin, _, _, _, _ in spec['scalars']) \
        + tuple(pin for _, pin, _, _ in spec['vectors'])
    pins = []
    for pin in names:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', pin)
        pins.append(item)
    custom.set_editor_property('inputs', pins)

    wire(node(material, unreal.MaterialExpressionTextureCoordinate, -700, -460), custom, 'UV')
    if 'Time' in spec['sources']:
        wire(node(material, unreal.MaterialExpressionTime, -700, -360), custom, 'Time')
    if 'Local' in spec['sources']:
        # Position on the unit sphere in the mesh's own space (the engine Sphere has a 50 cm radius).
        world = node(material, unreal.MaterialExpressionWorldPosition, -1100, -560)
        local = node(material, unreal.MaterialExpressionTransformPosition, -900, -560,
                     transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                     transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
        wire(world, local)
        wire(local, custom, 'Local')
    if 'Fresnel' in spec['sources']:
        # 1 - N.V: 0 facing the camera, 1 on the silhouette.
        wire(node(material, unreal.MaterialExpressionFresnel, -700, -620, exponent=1.0, base_reflect_fraction=0.0), custom,
             'Fresnel')
    if 'Color' in spec['sources']:
        particle_color = node(material, unreal.MaterialExpressionParticleColor, -700, -260)
        wire(particle_color, custom, 'Color', 'RGB')
        wire(particle_color, custom, 'Alpha', 'A')
    if 'Size' in spec['sources']:
        wire(node(material, unreal.MaterialExpressionParticleSize, -700, -160), custom, 'Size')
    if 'Dynamic' in spec['sources']:
        # Dynamic Parameter 0 (Niagara: Particles.DynamicMaterialParameter). Its four scalar outputs have no
        # connectable names from Python, so the whole RGBA output feeds one float4 pin and the HLSL splits it.
        wire(node(material, unreal.MaterialExpressionDynamicParameter, -700, -60), custom, 'Dynamic', 'RGBA')
    if 'Dynamic1' in spec['sources']:
        # Dynamic Parameter 1 (Niagara: Particles.DynamicMaterialParameter1), split in the HLSL the same way.
        wire(node(material, unreal.MaterialExpressionDynamicParameter, -700, 0, parameter_index=1), custom, 'Dynamic1',
             'RGBA')
    for index, (name, pin, default, low, high, group) in enumerate(spec['scalars']):
        scalar = node(material, unreal.MaterialExpressionScalarParameter, -700, 20 + index * 90,
                      parameter_name=name, default_value=default, slider_min=low, slider_max=high, group=group)
        wire(scalar, custom, pin)
    for index, (name, pin, default, group) in enumerate(spec['vectors']):
        vector = node(material, unreal.MaterialExpressionVectorParameter, -700,
                      20 + len(spec['scalars']) * 90 + index * 170,
                      parameter_name=name, default_value=default, group=group)
        wire(vector, custom, pin, 'RGB')

    emissive = node(material, unreal.MaterialExpressionComponentMask, 820, -60, r=True, g=True, b=True, a=False)
    coverage = node(material, unreal.MaterialExpressionComponentMask, 820, 140, r=False, g=False, b=False, a=True)
    wire(custom, emissive)
    wire(custom, coverage)
    if not MAT.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Could not connect the emissive output of ' + spec['material'])
    if not MAT.connect_material_property(coverage, '', unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError('Could not connect the coverage output of ' + spec['material'])

    messages = MAT.recompile_material(material)
    out['compile_messages'] = [str(item) for item in messages] if messages else []
    if out['compile_messages']:
        raise RuntimeError(spec['material'] + ' compilation reported: ' + ' | '.join(out['compile_messages']))
    stats = MAT.get_statistics(material)
    out['pixel_instructions'] = stats.get_editor_property('num_pixel_shader_instructions')
    if out['pixel_instructions'] <= 0:
        raise RuntimeError('No compiled pixel shader statistics for ' + spec['material'])
    out['nodes'] = MAT.get_num_material_expressions(material)

    instance, created = ensure_asset(spec['instance'], unreal.MaterialInstanceConstant,
                                     unreal.MaterialInstanceConstantFactoryNew())
    out['instance_created'] = created
    MAT.set_material_instance_parent(instance, material)
    scalars = [str(name) for name in MAT.get_scalar_parameter_names(instance)]
    vectors = [str(name) for name in MAT.get_vector_parameter_names(instance)]
    missing = [name for name, _, _, _, _, _ in spec['scalars'] if name not in scalars]
    missing += [name for name, _, _, _ in spec['vectors'] if name not in vectors]
    if missing:
        raise RuntimeError(spec['instance'] + ' does not expose its contract: ' + ', '.join(missing))
    stripped = [name for name in spec['driven'] if name not in scalars and name not in vectors]
    if stripped:
        raise RuntimeError('Runtime driven parameters were stripped: ' + ', '.join(stripped))
    out['instance_parameters'] = len(scalars) + len(vectors)
    built = [material, instance]
    for path, overrides in spec.get('variants', ()):
        variant, created = ensure_asset(path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MAT.set_material_instance_parent(variant, material)
        for name, value in overrides.items():
            # The setter's return value is unreliable when the value equals the parent default; read it back instead.
            MAT.set_material_instance_scalar_parameter_value(variant, name, value)
            MAT.update_material_instance(variant)
            if abs(MAT.get_material_instance_scalar_parameter_value(variant, name) - value) > 1e-4:
                raise RuntimeError('Could not set ' + name + ' on ' + path)
        out.setdefault('variants_created', {})[path] = created
        built.append(variant)
    return built


# Optional section filter: pass section names (ARROW PULSE WEDGE COMET SHELL STATUS HOLD) after the script path to rebuild only
# those, e.g. "CreateComicDoodleAssets.py PULSE". No arguments rebuilds everything. A filtered run writes its
# report to ComicDoodle_Authoring_<SECTIONS>.json so it never overwrites the full run's report.
SECTIONS = {'ARROW': ARROW, 'PULSE': PULSE, 'WEDGE': WEDGE, 'COMET': COMET, 'SHELL': SHELL, 'STATUS': STATUS, 'HOLD': HOLD}
SELECTED = [name.upper() for name in sys.argv[1:]]
UNKNOWN = [name for name in SELECTED if name not in SECTIONS]
if UNKNOWN:
    raise RuntimeError('Unknown section(s): ' + ', '.join(UNKNOWN) + '; expected ' + ', '.join(SECTIONS))
REPORT_NAME = 'ComicDoodle_Authoring.json' if not SELECTED else 'ComicDoodle_Authoring_' + '_'.join(SELECTED) + '.json'
report['sections'] = SELECTED or list(SECTIONS)

try:
    assets = []
    for item in (SECTIONS[name] for name in (SELECTED or list(SECTIONS))):
        assets.extend(build(item))
    for asset in assets:
        if not ASSET.save_loaded_asset(asset):
            raise RuntimeError('Could not save ' + asset.get_path_name())
    report['status'] = 'ok'
    unreal.log('COMIC_DOODLE_FX ' + json.dumps(report))
except Exception:
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
    unreal.log_error(report['error'])
    raise
finally:
    (WORK / REPORT_NAME).write_text(json.dumps(report, indent=2), encoding='utf-8')
