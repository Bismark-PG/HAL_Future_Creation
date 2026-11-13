"""Add masked, rounded chunk dissolution through official Unreal Editor APIs.

Only the existing graffiti parent material is saved. Existing color, volume shading,
vertex deformation, MI overrides, textures, gameplay DA and Blueprint assets are preserved.
"""
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(unreal.Paths.project_dir()).resolve()
WORK = ROOT / 'Saved/SlipperyTrail/Dissolve_20261111'
WORK.mkdir(parents=True, exist_ok=True)
MAT = unreal.MaterialEditingLibrary
MARKER = 'HAL Ground Dissolve v1'
report = {}


def parameters(instance):
    result = {}
    for name in MAT.get_scalar_parameter_names(instance):
        result[str(name)] = MAT.get_material_instance_scalar_parameter_value(instance, name)
    for name in MAT.get_vector_parameter_names(instance):
        value = MAT.get_material_instance_vector_parameter_value(instance, name)
        result[str(name)] = [value.r, value.g, value.b, value.a]
    return result


def node(material, cls, x, y, **properties):
    value = MAT.create_material_expression(material, cls, x, y)
    for key, item in properties.items():
        value.set_editor_property(key, item)
    return value


def wire(source, target, input_name='', output=''):
    if not MAT.connect_material_expressions(source, output, target, input_name):
        raise RuntimeError('Failed connection: ' + input_name)


try:
    material = unreal.load_asset('/Game/Assets/FX/Materials/M_SlipperyTrail_Graffiti')
    instance = unreal.load_asset('/Game/Assets/FX/Materials/MI_SlipperyTrail_Graffiti')
    if not material or not instance or material.get_editor_property('blend_mode') != unreal.BlendMode.BLEND_MASKED:
        raise RuntimeError('The existing masked graffiti material is required')
    report['parameters_before'] = parameters(instance)
    source = (ROOT / 'Tools/SlipperyTrailGraffiti/DissolveMask.hlsl').read_text(encoding='utf-8')
    expressions = MAT.get_material_expressions(material)
    existing = [item for item in expressions if isinstance(item, unreal.MaterialExpressionCustom)
                and item.get_editor_property('description') == MARKER]
    if existing:
        if len(existing) != 1:
            raise RuntimeError('Ambiguous existing dissolve graph')
        existing[0].set_editor_property('code', source)
    else:
        color_root = MAT.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        color_output = MAT.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mask_root = MAT.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY_MASK)
        mask_output = MAT.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_OPACITY_MASK)
        if not color_root:
            raise RuntimeError('The original color output must be preserved')
        if not mask_root:
            mask_root = node(material, unreal.MaterialExpressionConstant, 1600, 900, r=1.)
        custom = node(material, unreal.MaterialExpressionCustom, 1850, 600, description=MARKER,
                      desc=MARKER, code=source, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2)
        inputs = []
        for name in ('UV', 'InvTile', 'Progress', 'CellSize', 'Softness', 'EdgeWidth'):
            item = unreal.CustomInput()
            item.set_editor_property('input_name', name)
            inputs.append(item)
        custom.set_editor_property('inputs', inputs)
        uv = next(item for item in expressions if isinstance(item, unreal.MaterialExpressionTextureCoordinate)
                  and item.get_editor_property('coordinate_index') == 0)
        inv_tile = next(item for item in expressions if isinstance(item, unreal.MaterialExpressionScalarParameter)
                        and str(item.get_editor_property('parameter_name')) == 'HAL_GroundInvTileSize')
        wire(uv, custom, 'UV')
        wire(inv_tile, custom, 'InvTile')
        for index, (name, pin, default, low, high) in enumerate((
            ('HAL_GroundDissolveProgress', 'Progress', 0., 0, 1),
            ('DissolveCellSize', 'CellSize', 160., 20, 1000),
            ('DissolveSoftness', 'Softness', .04, .002, .25),
            ('DissolveEdgeWidth', 'EdgeWidth', .06, .002, .25))):
            scalar = node(material, unreal.MaterialExpressionScalarParameter, 1500, 400 + index * 180,
                parameter_name=name, default_value=default, slider_min=low, slider_max=high,
                group='Runtime Ground' if name.startswith('HAL_') else '05 Dissolve')
            wire(scalar, custom, pin)
        coverage = node(material, unreal.MaterialExpressionComponentMask, 2100, 600, r=True, g=False, b=False, a=False)
        rim = node(material, unreal.MaterialExpressionComponentMask, 2100, 800, r=False, g=True, b=False, a=False)
        wire(custom, coverage)
        wire(custom, rim)
        opacity = node(material, unreal.MaterialExpressionMultiply, 2400, 650, desc='HAL Ground Dissolve Opacity v1')
        wire(mask_root, opacity, 'A', mask_output)
        wire(coverage, opacity, 'B')
        edge_color = node(material, unreal.MaterialExpressionVectorParameter, 1850, 1150,
            parameter_name='DissolveEdgeColor', group='05 Dissolve',
            default_value=MAT.get_material_instance_vector_parameter_value(instance, 'AccentColor'))
        edge_strength = node(material, unreal.MaterialExpressionScalarParameter, 2100, 1150,
            parameter_name='DissolveEdgeStrength', group='05 Dissolve', default_value=.35, slider_min=0., slider_max=1.)
        edge_mask = node(material, unreal.MaterialExpressionMultiply, 2400, 950)
        wire(rim, edge_mask, 'A')
        wire(edge_strength, edge_mask, 'B')
        bounded_edge = node(material, unreal.MaterialExpressionSaturate, 2600, 950)
        wire(edge_mask, bounded_edge)
        color = node(material, unreal.MaterialExpressionLinearInterpolate, 2850, 300, desc='HAL Ground Dissolve Edge v1')
        wire(color_root, color, 'A', color_output)
        wire(edge_color, color, 'B', 'RGB')
        wire(bounded_edge, color, 'Alpha')
        if not MAT.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY_MASK):
            raise RuntimeError('Could not connect dissolve opacity')
        if not MAT.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError('Could not connect dissolve edge color')

    MAT.recompile_material(material)
    stats = MAT.get_statistics(material)
    report['statistics'] = {name: stats.get_editor_property(name) for name in (
        'num_vertex_shader_instructions', 'num_pixel_shader_instructions', 'num_samplers')}
    if report['statistics']['num_pixel_shader_instructions'] <= 0:
        raise RuntimeError('No compiled shader statistics')
    report['parameters_after'] = parameters(instance)
    if any(report['parameters_after'].get(name) != value for name, value in report['parameters_before'].items()):
        raise RuntimeError('Existing material parameters changed')
    report['nodes'] = MAT.get_num_material_expressions(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Could not save the dissolve material')
    report['status'] = 'updated'
    unreal.log('GROUND_DISSOLVE_MATERIAL ' + json.dumps(report))
except Exception:
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
    unreal.log_error(report['error'])
    raise
finally:
    (WORK / 'Material_Update.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
