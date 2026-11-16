"""Author the comic impact burst sprite material through official Unreal Editor APIs.

Creates or rebuilds, in this order:
  1. /Game/Assets/FX/Materials/M_FX_ImpactBurst    parent material, fully owned by this script
  2. /Game/Assets/FX/Materials/MI_FX_ImpactBurst   instance referenced by UHALImpactBurstSettings

Idempotent: the parent graph is rebuilt on every run, the instance keeps its identity so the
settings reference never breaks. Nothing else in the project is touched - no recipe, no catalog,
no vehicle asset. Writes Saved/ImpactBurst/ImpactBurst_Authoring.json and fails loudly instead
of half-applying.

Run headless with the editor closed:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Tools/ImpactBurst/CreateImpactBurstAssets.py"
An open editor locks the packages, so run it inside that editor instead when one is running.
"""
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(unreal.Paths.project_dir()).resolve()
WORK = ROOT / 'Saved/ImpactBurst'
WORK.mkdir(parents=True, exist_ok=True)
MAT = unreal.MaterialEditingLibrary
ASSET = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

MARKER = 'HAL Comic Impact Burst v1'
MATERIAL_PATH = '/Game/Assets/FX/Materials/M_FX_ImpactBurst'
INSTANCE_PATH = '/Game/Assets/FX/Materials/MI_FX_ImpactBurst'

# Parameter name, HLSL pin, default, slider minimum, slider maximum, parameter group.
# The first group is written per burst by UHALVehiclePresentationComponent; everything else is
# an authored look constant the team can tune on the instance without touching code.
SCALARS = (
    ('FX_Age01', 'Age01', 0.0, 0.0, 1.0, '01 Driven By Runtime'),
    ('FX_Strength', 'Strength', 0.5, 0.0, 1.0, '01 Driven By Runtime'),
    ('FX_Seed', 'Seed', 0.0, 0.0, 1.0, '01 Driven By Runtime'),
    ('FX_Spikes', 'Spikes', 9.0, 3.0, 24.0, '01 Driven By Runtime'),
    ('FX_InnerRadius', 'InnerRadius', 0.34, 0.05, 0.95, '02 Shape'),
    ('FX_Sharpness', 'Sharpness', 4.0, 0.5, 12.0, '02 Shape'),
    ('FX_Irregular', 'Irregular', 0.78, 0.0, 1.0, '02 Shape'),
    ('FX_EdgeWidth', 'EdgeWidth', 0.2, 0.0, 0.6, '02 Shape'),
    ('FX_StartScale', 'StartScale', 0.5, 0.05, 1.0, '03 Envelope'),
    ('FX_PopFraction', 'PopFraction', 0.18, 0.01, 1.0, '03 Envelope'),
    ('FX_TailGrowth', 'TailGrowth', 0.22, 0.0, 1.0, '03 Envelope'),
    ('FX_FadeStart', 'FadeStart', 0.5, 0.0, 1.0, '03 Envelope'),
    ('FX_FillGain', 'FillGain', 1.4, 0.0, 8.0, '04 Brightness'),
    ('FX_EdgeGain', 'EdgeGain', 3.0, 0.0, 12.0, '04 Brightness'),
    ('FX_StrengthGain', 'StrengthGain', 1.6, 0.0, 4.0, '04 Brightness'),
    ('FX_Opacity', 'Opacity', 1.0, 0.0, 1.0, '04 Brightness'),
)
VECTORS = (
    ('FX_FillColor', 'FillColor', unreal.LinearColor(1.0, 1.0, 1.0, 1.0), '05 Color'),
    ('FX_EdgeColor', 'EdgeColor', unreal.LinearColor(1.0, 0.72, 0.18, 1.0), '05 Color'),
)
# Written per burst by C++; the contract check below fails if the compiler strips one of them.
DRIVEN_NAMES = ('FX_Age01', 'FX_Strength', 'FX_Seed', 'FX_Spikes', 'FX_FillColor', 'FX_EdgeColor')
PINS = ('UV',) + tuple(pin for _, pin, _, _, _, _ in SCALARS) + tuple(pin for _, pin, _, _ in VECTORS)

report = {'material': MATERIAL_PATH, 'instance': INSTANCE_PATH}


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
    """Empty the graph before rebuilding.

    delete_all_material_expressions leaves every second node behind (it removes while iterating
    by index), so re-running the script would otherwise pile up orphan parameter nodes. Delete one
    at a time from a fresh list, repeat until the graph is empty, and refuse to rebuild on leftovers.
    """
    for _ in range(16):
        expressions = list(MAT.get_material_expressions(material))
        if not expressions:
            return
        for item in expressions:
            MAT.delete_material_expression(material, item)
    raise RuntimeError('Could not empty the material graph before rebuilding')


def build_material():
    material, created = ensure_asset(MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())
    report['material_created'] = created
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided', True)
    # A comic stamp always reads: the car it is drawn on must never cut the sprite in half.
    material.set_editor_property('disable_depth_test', True)
    clear_graph(material)

    code = (ROOT / 'Tools/ImpactBurst/ImpactBurst.hlsl').read_text(encoding='utf-8')
    custom = node(material, unreal.MaterialExpressionCustom, 400, 0, description=MARKER, desc=MARKER,
                  code=code, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    pins = []
    for pin in PINS:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', pin)
        pins.append(item)
    custom.set_editor_property('inputs', pins)
    report['pins'] = list(PINS)

    wire(node(material, unreal.MaterialExpressionTextureCoordinate, -700, -140), custom, 'UV')
    for index, (name, pin, default, low, high, group) in enumerate(SCALARS):
        scalar = node(material, unreal.MaterialExpressionScalarParameter, -700, 20 + index * 90,
                      parameter_name=name, default_value=default,
                      slider_min=low, slider_max=high, group=group)
        wire(scalar, custom, pin)
    for index, (name, pin, default, group) in enumerate(VECTORS):
        vector = node(material, unreal.MaterialExpressionVectorParameter, -700, 1420 + index * 170,
                      parameter_name=name, default_value=default, group=group)
        wire(vector, custom, pin, 'RGB')

    emissive = node(material, unreal.MaterialExpressionComponentMask, 820, -60,
                    r=True, g=True, b=True, a=False, desc='Impact burst emissive')
    coverage = node(material, unreal.MaterialExpressionComponentMask, 820, 140,
                    r=False, g=False, b=False, a=True, desc='Impact burst coverage')
    wire(custom, emissive)
    wire(custom, coverage)
    if not MAT.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Could not connect the impact burst emissive output')
    if not MAT.connect_material_property(coverage, '', unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError('Could not connect the impact burst coverage output')

    messages = MAT.recompile_material(material)
    report['compile_messages'] = [str(item) for item in messages] if messages else []
    if report['compile_messages']:
        raise RuntimeError('Material compilation reported: ' + ' | '.join(report['compile_messages']))
    stats = MAT.get_statistics(material)
    report['statistics'] = {name: stats.get_editor_property(name) for name in (
        'num_vertex_shader_instructions', 'num_pixel_shader_instructions', 'num_samplers')}
    if report['statistics']['num_pixel_shader_instructions'] <= 0:
        raise RuntimeError('No compiled pixel shader statistics')
    report['nodes'] = MAT.get_num_material_expressions(material)
    return material


def build_instance(material):
    instance, created = ensure_asset(INSTANCE_PATH, unreal.MaterialInstanceConstant,
                                     unreal.MaterialInstanceConstantFactoryNew())
    report['instance_created'] = created
    MAT.set_material_instance_parent(instance, material)
    scalars = [str(name) for name in MAT.get_scalar_parameter_names(instance)]
    vectors = [str(name) for name in MAT.get_vector_parameter_names(instance)]
    missing = [name for name, _, _, _, _, _ in SCALARS if name not in scalars]
    missing += [name for name, _, _, _ in VECTORS if name not in vectors]
    if missing:
        raise RuntimeError('The instance does not expose the burst contract: ' + ', '.join(missing))
    stripped = [name for name in DRIVEN_NAMES if name not in scalars and name not in vectors]
    if stripped:
        raise RuntimeError('Runtime driven parameters were stripped: ' + ', '.join(stripped))
    report['instance_scalars'] = scalars
    report['instance_vectors'] = vectors
    return instance


def check_settings():
    """Report whether the project settings already point at this instance; never changes them."""
    try:
        settings = unreal.get_default_object(unreal.load_class(None, '/Script/HAL_Future_Creation.HALImpactBurstSettings'))
        burst = settings.get_editor_property('burst')
        report['settings_enabled'] = bool(burst.get_editor_property('enabled'))
        report['settings_material'] = str(burst.get_editor_property('material'))
    except Exception as error:
        report['settings_error'] = str(error)


try:
    burst_material = build_material()
    burst_instance = build_instance(burst_material)
    for asset in (burst_material, burst_instance):
        if not ASSET.save_loaded_asset(asset):
            raise RuntimeError('Could not save ' + asset.get_path_name())
    check_settings()
    report['status'] = 'ok'
    unreal.log('VEHICLE_IMPACT_BURST ' + json.dumps(report))
except Exception:
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
    unreal.log_error(report['error'])
    raise
finally:
    (WORK / 'ImpactBurst_Authoring.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
