"""Add editable volume shading to the saved graffiti material using Editor APIs.

Run in UnrealEditor-Cmd with -ExecutePythonScript=<this file> and
-EnablePlugins=PythonScriptPlugin. This edits only M_SlipperyTrail_Graffiti.
The existing MI, palette, WPO graph, textures, DA, and Blueprint assets are preserved.
"""
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(unreal.Paths.project_dir()).resolve()
WORK = ROOT / 'Saved/SlipperyTrail/Volume_20261111'
WORK.mkdir(parents=True, exist_ok=True)
MAT = unreal.MaterialEditingLibrary
MARKER = 'HAL Ground Volume Shading v1'
report = {}


def palette(instance):
    result = {}
    for name in MAT.get_vector_parameter_names(instance):
        value = MAT.get_material_instance_vector_parameter_value(instance, name)
        result[str(name)] = [value.r, value.g, value.b, value.a]
    return result


def node(material, cls, x, y, **properties):
    value = MAT.create_material_expression(material, cls, x, y)
    for key, item in properties.items():
        value.set_editor_property(key, item)
    return value


def connect(source, output, target, input_name):
    if not MAT.connect_material_expressions(source, output, target, input_name):
        raise RuntimeError('Failed connection: ' + input_name)


try:
    material = unreal.load_asset('/Game/Assets/FX/Materials/M_SlipperyTrail_Graffiti')
    instance = unreal.load_asset('/Game/Assets/FX/Materials/MI_SlipperyTrail_Graffiti')
    if not material or not instance:
        raise RuntimeError('Existing graffiti material and instance are required')
    report['palette_before'] = palette(instance)
    old_nodes = MAT.get_material_expressions(material)
    old_root = MAT.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    old_output = MAT.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if not old_root:
        raise RuntimeError('The current color output must be preserved')
    source = (ROOT / 'Tools/SlipperyTrailGraffiti/VolumeShading.hlsl').read_text(encoding='utf-8')
    existing = [item for item in old_nodes if isinstance(item, unreal.MaterialExpressionCustom)
                and item.get_editor_property('description') == MARKER]
    if existing:
        if len(existing) != 1 or (old_root != existing[0]
                and existing[0] not in MAT.get_inputs_for_material_expression(material, old_root)):
            raise RuntimeError('Unexpected volume graph; refusing to rewire existing work')
        custom = existing[0]
        custom.set_editor_property('code', source)
    else:
        custom = node(material, unreal.MaterialExpressionCustom, 1200, -100,
                      description=MARKER, desc=MARKER, code=source,
                      output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        parameters = [
            ('VolumeStrength', 'Strength', 1.0, 0, 1),
            ('VolumeNormalStrength', 'NormalStrength', 4.0, 0, 8),
            ('VolumeBandThreshold', 'BandThreshold', 0.08, 0.002, 1),
            ('VolumeSoftness', 'Softness', 0.12, 0.002, 1),
            ('VolumeShadowStrength', 'ShadowStrength', 0.45, 0, 1),
            ('VolumeHighlightStrength', 'HighlightStrength', 0.15, 0, 1),
        ]
        names = ['BaseColor', 'Weight', 'LightDirection']
        names += [item[1] for item in parameters]
        inputs = []
        for name in names:
            item = unreal.CustomInput()
            item.set_editor_property('input_name', name)
            inputs.append(item)
        custom.set_editor_property('inputs', inputs)
        connect(old_root, old_output, custom, 'BaseColor')
        color = node(material, unreal.MaterialExpressionVertexColor, 650, 900)
        connect(color, 'R', custom, 'Weight')
        direction = node(material, unreal.MaterialExpressionVectorParameter, 650, 1100,
                         parameter_name='VolumeLightDirection', group='04 Volume shading',
                         default_value=unreal.LinearColor(-0.5, -0.6, 0.65, 0))
        connect(direction, 'RGB', custom, 'LightDirection')
        for index, (name, input_name, default, low, high) in enumerate(parameters):
            scalar = node(material, unreal.MaterialExpressionScalarParameter, 900, 450 + index * 180,
                          parameter_name=name, group='04 Volume shading', default_value=default,
                          slider_min=low, slider_max=high)
            connect(scalar, '', custom, input_name)
        if not MAT.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError('Could not connect the volume color output')

    # Upgrade the prototype without replacing its color input or editable parameters.
    inputs = [item for item in custom.get_editor_property('inputs')
              if str(item.get_editor_property('input_name')) not in ('Position', 'BasePosition')]
    added = ['GroundNormal', 'Gradient', 'UV', 'WaveUV', 'Height', 'Amplitude', 'Phase', 'InvTileSize']
    names = {str(item.get_editor_property('input_name')) for item in inputs}
    for name in added:
        if name not in names:
            item = unreal.CustomInput()
            item.set_editor_property('input_name', name)
            inputs.append(item)
    custom.set_editor_property('inputs', inputs)
    expressions = MAT.get_material_expressions(material)
    for item in expressions:
        if item.get_editor_property('desc') in ('Deformed position including WPO', 'Sampled ground position excluding WPO'):
            MAT.delete_material_expression(material, item)
    expressions = MAT.get_material_expressions(material)
    gradient_nodes = [item for item in expressions if isinstance(item, unreal.MaterialExpressionTextureCoordinate)
                      and item.get_editor_property('coordinate_index') == 1]
    gradient = gradient_nodes[0] if gradient_nodes else node(material, unreal.MaterialExpressionTextureCoordinate,
        650, 1300, coordinate_index=1, desc='Smooth ground weight gradient, inverse cm')
    uv = next(item for item in expressions if isinstance(item, unreal.MaterialExpressionTextureCoordinate)
              and item.get_editor_property('coordinate_index') == 0)
    connect(gradient, '', custom, 'Gradient')
    connect(uv, '', custom, 'UV')
    normal_nodes = [item for item in expressions if isinstance(item, unreal.MaterialExpressionPixelNormalWS)]
    normal = normal_nodes[0] if normal_nodes else node(material, unreal.MaterialExpressionPixelNormalWS,
        650, 1500, desc='Original ground orientation, independent of stylized WPO normals')
    connect(normal, '', custom, 'GroundNormal')
    for parameter_name, input_name, vector_parameter in (
        ('HAL_GroundBulgeHeight', 'Height', False), ('HAL_GroundWaveAmplitude', 'Amplitude', False),
        ('HAL_GroundWavePhase', 'Phase', False), ('HAL_GroundInvTileSize', 'InvTileSize', False),
        ('HAL_GroundWaveUV', 'WaveUV', True)):
        cls = unreal.MaterialExpressionVectorParameter if vector_parameter else unreal.MaterialExpressionScalarParameter
        found = [item for item in expressions if isinstance(item, cls)
                 and str(item.get_editor_property('parameter_name')) == parameter_name]
        parameter = found[0] if found else node(material, cls, 900, 1900,
            parameter_name=parameter_name, group='Runtime Ground', default_value=0.)
        connect(parameter, 'RGB' if vector_parameter else '', custom, input_name)

    MAT.recompile_material(material)
    statistics = MAT.get_statistics(material)
    report['statistics'] = {name: statistics.get_editor_property(name) for name in (
        'num_vertex_shader_instructions', 'num_pixel_shader_instructions', 'num_samplers')}
    if report['statistics']['num_pixel_shader_instructions'] <= 0:
        raise RuntimeError('No compiled shader statistics')
    report['palette_after'] = palette(instance)
    if any(report['palette_after'].get(name) != value for name, value in report['palette_before'].items()):
        raise RuntimeError('Existing palette was altered')
    report['nodes'] = MAT.get_num_material_expressions(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Could not save the volume material')
    report['status'] = 'updated'
    unreal.log('GROUND_VOLUME_MATERIAL ' + json.dumps(report))
except Exception:
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
    unreal.log_error(report['error'])
    raise
finally:
    (WORK / 'Material_Update.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
