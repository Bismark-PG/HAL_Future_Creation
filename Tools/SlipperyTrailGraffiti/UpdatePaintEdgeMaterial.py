"""Add the paint silhouette and contour-following ink through official Editor APIs.

Preserves all saved MI overrides, colors, WPO, volume and dissolve branches.
Only the existing graffiti parent material is saved.
"""
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(unreal.Paths.project_dir()).resolve()
WORK = ROOT / 'Saved/SlipperyTrail/PaintEdge_20261111'
WORK.mkdir(parents=True, exist_ok=True)
MAT = unreal.MaterialEditingLibrary
MARKER = 'HAL Ground Paint Edge v1'
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
    result = MAT.create_material_expression(material, cls, x, y)
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


def wire(source, target, input_name='', output=''):
    if not MAT.connect_material_expressions(source, output, target, input_name):
        raise RuntimeError('Failed paint connection: ' + input_name)


try:
    material = unreal.load_asset('/Game/Assets/FX/Materials/M_SlipperyTrail_Graffiti')
    instance = unreal.load_asset('/Game/Assets/FX/Materials/MI_SlipperyTrail_Graffiti')
    if not material or not instance or material.get_editor_property('blend_mode') != unreal.BlendMode.BLEND_MASKED:
        raise RuntimeError('The existing masked graffiti material is required')
    report['parameters_before'] = parameters(instance)
    expressions = MAT.get_material_expressions(material)
    source = (ROOT/'Tools/SlipperyTrailGraffiti/PaintEdge.hlsl').read_text(encoding='utf-8')
    existing = [item for item in expressions if isinstance(item, unreal.MaterialExpressionCustom)
                and item.get_editor_property('description') == MARKER]
    if existing:
        if len(existing) != 1:
            raise RuntimeError('Ambiguous paint-edge graph')
        existing[0].set_editor_property('code', source)
        existing[0].set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    else:
        border = next(item for item in expressions if item.get_editor_property('desc') == 'Ink border threshold')
        palette = next(item for item in expressions if item.get_editor_property('desc') == 'Outer ink and purple side')
        if border not in MAT.get_inputs_for_material_expression(material, palette):
            raise RuntimeError('The existing ink connection was changed; refusing to replace it')
        old_mask = MAT.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY_MASK)
        old_mask_output = MAT.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_OPACITY_MASK)
        if not old_mask:
            raise RuntimeError('The existing dissolve mask must be preserved')
        custom = node(material, unreal.MaterialExpressionCustom, 1900, -1500,
            code=source, description=MARKER, desc=MARKER, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
        names = ['UV', 'EdgeMetadata', 'InvTile', 'Enabled', 'Strength', 'CellSize', 'Detail',
                 'Softness', 'OutlineWidth', 'OutlineSoftness']
        inputs = []
        for name in names:
            value = unreal.CustomInput()
            value.set_editor_property('input_name', name)
            inputs.append(value)
        custom.set_editor_property('inputs', inputs)
        uv = next(item for item in expressions if isinstance(item, unreal.MaterialExpressionTextureCoordinate)
                  and item.get_editor_property('coordinate_index') == 0)
        inv_tile = next(item for item in expressions if isinstance(item, unreal.MaterialExpressionScalarParameter)
                        and str(item.get_editor_property('parameter_name')) == 'HAL_GroundInvTileSize')
        metadata = node(material, unreal.MaterialExpressionTextureCoordinate, 1300, -1900,
                        coordinate_index=2, desc='Core signed distance and bounded paint margin, cm')
        wire(uv, custom, 'UV')
        wire(inv_tile, custom, 'InvTile')
        wire(metadata, custom, 'EdgeMetadata')
        for index, (name, pin, default, low, high) in enumerate((
            ('HAL_GroundPaintEdgeEnabled', 'Enabled', 0., 0, 1),
            ('PaintEdgeStrength', 'Strength', 1., 0, 1),
            ('PaintEdgeCellSize', 'CellSize', 100., 20, 500),
            ('PaintEdgeDetail', 'Detail', .2, 0, .5),
            ('PaintEdgeSoftness', 'Softness', .6, .1, 5),
            ('PaintEdgeOutlineWidth', 'OutlineWidth', 4., 0, 20),
            ('PaintEdgeOutlineSoftness', 'OutlineSoftness', 1., .1, 10))):
            scalar = node(material, unreal.MaterialExpressionScalarParameter, 1100 + index % 2 * 280,
                -1500 + index * 160, parameter_name=name, default_value=default, slider_min=low, slider_max=high,
                group='Runtime Ground' if name.startswith('HAL_') else '06 Paint edge')
            wire(scalar, custom, pin)
        coverage = node(material, unreal.MaterialExpressionComponentMask, 2150, -1500, r=True, g=False, b=False, a=False)
        ink = node(material, unreal.MaterialExpressionComponentMask, 2150, -1300, r=False, g=True, b=False, a=False)
        mode = node(material, unreal.MaterialExpressionComponentMask, 2150, -1100, r=False, g=False, b=True, a=False)
        for output in (coverage, ink, mode):
            wire(custom, output)
        ink_blend = node(material, unreal.MaterialExpressionLinearInterpolate, 2400, -1300,
                         desc='HAL Ground Paint Ink v1')
        wire(border, ink_blend, 'A')
        wire(ink, ink_blend, 'B')
        wire(mode, ink_blend, 'Alpha')
        wire(ink_blend, palette, 'Alpha')
        opacity = node(material, unreal.MaterialExpressionMultiply, 2650, -1500,
                       desc='HAL Ground Paint Coverage v1')
        wire(old_mask, opacity, 'A', old_mask_output)
        wire(coverage, opacity, 'B')
        if not MAT.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY_MASK):
            raise RuntimeError('Could not append paint to the existing dissolve mask')

    MAT.recompile_material(material)
    stats = MAT.get_statistics(material)
    report['statistics'] = {name: stats.get_editor_property(name) for name in (
        'num_vertex_shader_instructions', 'num_pixel_shader_instructions', 'num_samplers')}
    if report['statistics']['num_pixel_shader_instructions'] <= 0:
        raise RuntimeError('No compiled paint shader statistics')
    report['parameters_after'] = parameters(instance)
    if any(report['parameters_after'].get(name) != value for name,value in report['parameters_before'].items()):
        raise RuntimeError('An existing palette or parameter changed')
    report['nodes'] = MAT.get_num_material_expressions(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Could not save paint-edge material')
    report['status'] = 'updated'
    unreal.log('GROUND_PAINT_EDGE_MATERIAL ' + json.dumps(report))
except Exception:
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
    unreal.log_error(report['error'])
    raise
finally:
    (WORK/'Material_Update.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
