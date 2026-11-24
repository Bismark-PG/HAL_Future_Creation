"""Author the comic doodle materials (held aim arrow, launched-ball pulse ring trail, vehicle status marks) through official Unreal Editor APIs.

Creates or rebuilds, in this order (every asset below is fully owned by this script):
  1. /Game/Assets/FX/Materials/M_FX_BallAimArrow    parent
  2. /Game/Assets/FX/Materials/MI_FX_BallAimArrow   instance referenced by UHALBallComicFXSettings (0 overrides)
  3. /Game/Assets/FX/Materials/M_FX_BallPulseRing   parent of the launched-ball pulse rings
  4. /Game/Assets/FX/Materials/MI_FX_BallPulseRing  the only sprite renderer of NS_BallInkTrail (0 overrides)
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
    'marker': 'HAL Ball Aim Arrow v1 (boiling doodle arrow)',
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
        ('FX_InkWidth', 'InkWidth', 0.12, 0.0, 0.4, '03 Ink'),
        ('FX_ShadowOffset', 'ShadowOffset', 0.05, 0.0, 0.3, '03 Ink'),
        ('FX_BoilFps', 'BoilFps', 8.0, 0.0, 24.0, '04 Boil'),              # outline redraws per second
        ('FX_BoilAmount', 'BoilAmount', 0.02, 0.0, 0.1, '04 Boil'),        # wobble size; the direction never moves
        ('FX_ColorLift', 'ColorLift', 0.0, 0.0, 1.0, '05 Color'),
        ('FX_FillGain', 'FillGain', 1.05, 0.0, 8.0, '05 Color'),
        ('FX_Opacity', 'Opacity', 0.92, 0.0, 1.0, '05 Color'),
    ),
    'vectors': (
        ('FX_Color', 'Color', unreal.LinearColor(0.98, 0.62, 0.12, 1.0), '00 Driven By Runtime'),
        ('FX_InkColor', 'InkColor', BLACK, '05 Color'),
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
    'marker': 'HAL Ball Hold Ring v2 (ball orbit rings / holder marker)',
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
        ('FX_Fps', 'Fps', 8.0, 1.0, 24.0, '02 Motion'),                   # drawings per second
        ('FX_BoilAmount', 'BoilAmount', 0.012, 0.0, 0.1, '02 Motion'),    # line wobble
        ('FX_OrbitWidth', 'OrbitWidth', 0.88, 0.1, 0.95, '03 Ball Rings'),  # ring half length, sprite half extents
        ('FX_OrbitHeight', 'OrbitHeight', 0.27, 0.02, 0.9, '03 Ball Rings'),  # ring half height: the squash that reads as an orbit
        ('FX_OrbitLine', 'OrbitLine', 0.07, 0.005, 0.2, '03 Ball Rings'),  # half thickness including ink
        ('FX_OrbitTilt', 'OrbitTilt', 18.0, 0.0, 60.0, '03 Ball Rings'),  # degrees; the second ring leans the other way
        ('FX_OrbitWobble', 'OrbitWobble', 3.0, 0.0, 15.0, '03 Ball Rings'),  # degrees of rocking between drawings
        ('FX_MarkerArrowTop', 'MarkerArrowTop', -0.31, -0.9, 0.8, '04 Holder Marker'),  # V of the arrow head's base; centred by default
        ('FX_MarkerArrowLength', 'MarkerArrowLength', 0.62, 0.05, 1.5, '04 Holder Marker'),  # base to tip
        ('FX_MarkerArrowWidth', 'MarkerArrowWidth', 0.4, 0.05, 0.9, '04 Holder Marker'),  # half width of the base
        ('FX_MarkerBob', 'MarkerBob', 0.07, 0.0, 0.3, '04 Holder Marker'),  # shift along the arrow on every other drawing
        ('FX_InkWidth', 'InkWidth', 0.036, 0.0, 0.2, '05 Ink'),
        ('FX_ShadowOffset', 'ShadowOffset', 0.02, 0.0, 0.2, '05 Ink'),
        ('FX_ShadowAngle', 'ShadowAngle', 45.0, -180.0, 180.0, '05 Ink'),  # side of the heavy ink, degrees in UV space
        ('FX_ColorLift', 'ColorLift', 0.0, 0.0, 1.0, '06 Color'),
        ('FX_FillGain', 'FillGain', 1.05, 0.0, 8.0, '06 Color'),
        ('FX_Opacity', 'Opacity', 1.0, 0.0, 1.0, '06 Color'),
    ),
    'vectors': (
        ('FX_Color', 'Color', unreal.LinearColor(0.98, 0.62, 0.12, 1.0), '00 Driven By Runtime'),
        ('FX_InkColor', 'InkColor', BLACK, '06 Color'),
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
    material.set_editor_property('two_sided', True)
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


# Optional section filter: pass section names (ARROW PULSE STATUS HOLD) after the script path to rebuild only
# those, e.g. "CreateComicDoodleAssets.py PULSE". No arguments rebuilds everything. A filtered run writes its
# report to ComicDoodle_Authoring_<SECTIONS>.json so it never overwrites the full run's report.
SECTIONS = {'ARROW': ARROW, 'PULSE': PULSE, 'STATUS': STATUS, 'HOLD': HOLD}
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
