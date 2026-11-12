"""Add orientation-driven three-tone color before the existing dots/strokes.

Only M_SlipperyTrail_Graffiti is saved through official Editor material APIs.
All MI overrides, WPO, coverage, dissolve, assets and runtime C++ are preserved.
ToonStrength=0 restores the existing palette and volume shading.
"""
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(unreal.Paths.project_dir()).resolve()
WORK = ROOT/'Saved/SlipperyTrail/EdgeBuffer_20261112'
WORK.mkdir(parents=True, exist_ok=True)
MAT = unreal.MaterialEditingLibrary
MARKER = 'HAL Ground Three Tone v1'
GROUP = '07 Three tone shading'
report = {}


def parameters(instance):
    result = {}
    for name in MAT.get_scalar_parameter_names(instance):
        result[str(name)] = MAT.get_material_instance_scalar_parameter_value(instance, name)
    for name in MAT.get_vector_parameter_names(instance):
        c = MAT.get_material_instance_vector_parameter_value(instance, name)
        result[str(name)] = [c.r, c.g, c.b, c.a]
    return result


def node(material, cls, x, y, **properties):
    result = MAT.create_material_expression(material, cls, x, y)
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


def wire(source, target, input_name='', output=''):
    if not MAT.connect_material_expressions(source, output, target, input_name):
        raise RuntimeError('Failed three-tone connection: ' + input_name)


def property_root(material, prop):
    source = MAT.get_material_property_input_node(material, prop)
    output = MAT.get_material_property_input_node_output_name(material, prop)
    return [source.get_name() if source else None, output]


try:
    material = unreal.load_asset('/Game/Assets/FX/Materials/M_SlipperyTrail_Graffiti')
    instance = unreal.load_asset('/Game/Assets/FX/Materials/MI_SlipperyTrail_Graffiti')
    if not material or not instance or material.get_editor_property('blend_mode') != unreal.BlendMode.BLEND_MASKED:
        raise RuntimeError('Existing masked graffiti material and MI are required')
    report['parameters_before'] = parameters(instance)
    report['wpo_before'] = property_root(material, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    report['coverage_before'] = property_root(material, unreal.MaterialProperty.MP_OPACITY_MASK)
    expressions = MAT.get_material_expressions(material)
    source = (ROOT/'Tools/SlipperyTrailGraffiti/ThreeToneShading.hlsl').read_text(encoding='utf-8')
    existing = [n for n in expressions if isinstance(n, unreal.MaterialExpressionCustom)
                and n.get_editor_property('description') == MARKER]
    if existing:
        if len(existing) != 1:
            raise RuntimeError('Ambiguous three-tone graph')
        existing[0].set_editor_property('code', source)
    else:
        def described(description):
            return next(n for n in expressions if n.get_editor_property('desc') == description)

        def parameter(name):
            return next(n for n in expressions if isinstance(n, (unreal.MaterialExpressionScalarParameter,
                unreal.MaterialExpressionVectorParameter)) and str(n.get_editor_property('parameter_name')) == name)

        old_palette = described('Hard illustrated top highlight')
        dots = described('Printed dots stay in the shadow band')
        original_ink = described('Outer ink and purple side')
        volume = described('HAL Ground Volume Shading v1')
        if MAT.get_inputs_for_material_expression(material, dots)[0] != old_palette:
            raise RuntimeError('Palette layout changed; refusing to replace artist work')
        ink_alpha = MAT.get_inputs_for_material_expression(material, original_ink)[2]
        if not ink_alpha:
            raise RuntimeError('A connected contour alpha is required')
        custom = node(material, unreal.MaterialExpressionCustom, 1200, -2600,
            description=MARKER, desc=MARKER, code=source, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        names = ['OldPalette','InkColor','InkAlpha','GroundNormal','Gradient','UV','WaveUV',
                 'Weight','Height','Amplitude','Phase','InvTileSize','ToneStrength','NormalStrength',
                 'LightDirection','ShadowThreshold','LitThreshold','ToneSoftness','ShadowColor','MidColor','LitColor']
        inputs = []
        for name in names:
            item = unreal.CustomInput()
            item.set_editor_property('input_name', name)
            inputs.append(item)
        custom.set_editor_property('inputs', inputs)
        wire(old_palette, custom, 'OldPalette')
        wire(parameter('InkColor'), custom, 'InkColor', 'RGB')
        wire(ink_alpha, custom, 'InkAlpha')
        wire(next(n for n in expressions if isinstance(n, unreal.MaterialExpressionPixelNormalWS)), custom, 'GroundNormal')
        for channel, pin in ((0,'UV'), (1,'Gradient')):
            wire(next(n for n in expressions if isinstance(n, unreal.MaterialExpressionTextureCoordinate)
                and n.get_editor_property('coordinate_index') == channel), custom, pin)
        wire(next(n for n in expressions if isinstance(n, unreal.MaterialExpressionVertexColor)), custom, 'Weight', 'R')
        for name, pin, vector in (('HAL_GroundWaveUV','WaveUV',True),('HAL_GroundBulgeHeight','Height',False),
            ('HAL_GroundWaveAmplitude','Amplitude',False),('HAL_GroundWavePhase','Phase',False),
            ('HAL_GroundInvTileSize','InvTileSize',False)):
            wire(parameter(name), custom, pin, 'RGB' if vector else '')
        new_scalars = {}
        for index,(name,pin,default,low,high) in enumerate((
            ('ToonStrength','ToneStrength',1.,0.,1.),
            ('ToonNormalStrength','NormalStrength',4.,0.,8.),
            ('ToonShadowThreshold','ShadowThreshold',-.04,-1.,1.),
            ('ToonLitThreshold','LitThreshold',.04,-1.,1.),
            ('ToonSoftness','ToneSoftness',.015,0.,.5))):
            new_scalars[name] = node(material, unreal.MaterialExpressionScalarParameter, 300, -2300 + index*180,
                parameter_name=name, default_value=default, group=GROUP, slider_min=low, slider_max=high)
            wire(new_scalars[name], custom, pin)
        colors = report['parameters_before']
        lit = [min(1., max(a,b)*1.15) for a,b in zip(colors['TopColor'][:3], colors['BodyColor'][:3])]+[1.]
        for index,(name,pin,value) in enumerate((
            ('ToonShadowColor','ShadowColor',colors['ShadowColor']),
            ('ToonMidColor','MidColor',colors['BodyColor']),
            ('ToonLitColor','LitColor',lit),
            ('ToonLightDirection','LightDirection',[.3,-.35,.89,0.]))):
            color = node(material, unreal.MaterialExpressionVectorParameter, 650, -2300 + index*220,
                parameter_name=name, default_value=unreal.LinearColor(*value), group=GROUP)
            wire(color, custom, pin, 'RGB')
        wire(custom, dots, 'A')
        # The two shade styles share their downstream artwork; prevent double shading.
        strength = node(material, unreal.MaterialExpressionSaturate, 1500, -2100, desc='HAL Ground Three Tone Strength v1')
        wire(new_scalars['ToonStrength'], strength)
        inverse = node(material, unreal.MaterialExpressionOneMinus, 1750, -2100)
        wire(strength, inverse)
        gate = node(material, unreal.MaterialExpressionMultiply, 2000, -2100, desc='HAL Ground Legacy Volume Gate v1')
        wire(parameter('VolumeStrength'), gate, 'A')
        wire(inverse, gate, 'B')
        wire(gate, volume, 'Strength')
    # Upgrade existing and newly created graphs together; no MI overrides change.
    expressions = MAT.get_material_expressions(material)
    custom = next(n for n in expressions if isinstance(n, unreal.MaterialExpressionCustom)
        and n.get_editor_property('description') == MARKER)
    inputs = list(custom.get_editor_property('inputs'))
    names = {str(item.get_editor_property('input_name')) for item in inputs}
    for name in ('EdgeDepth','EdgeOutlineWidth','EdgeColor','EdgeBufferWidth','EdgeBufferSoftness'):
        if name not in names:
            item = unreal.CustomInput()
            item.set_editor_property('input_name', name)
            inputs.append(item)
    custom.set_editor_property('inputs', inputs)
    paint = next(n for n in expressions if isinstance(n, unreal.MaterialExpressionCustom)
        and n.get_editor_property('description') == 'HAL Ground Paint Edge v1')
    paint.set_editor_property('code', (ROOT/'Tools/SlipperyTrailGraffiti/PaintEdge.hlsl').read_text(encoding='utf-8'))
    paint.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    depth_nodes = [n for n in expressions if n.get_editor_property('desc') == 'HAL Ground Paint Interior Depth v1']
    depth = depth_nodes[0] if depth_nodes else node(material, unreal.MaterialExpressionComponentMask, 2200, -1800,
        desc='HAL Ground Paint Interior Depth v1', r=False, g=False, b=False, a=True)
    wire(paint, depth)
    wire(depth, custom, 'EdgeDepth')
    outline = next(n for n in expressions if isinstance(n, unreal.MaterialExpressionScalarParameter)
        and str(n.get_editor_property('parameter_name')) == 'PaintEdgeOutlineWidth')
    wire(outline, custom, 'EdgeOutlineWidth')
    for index,(name,pin,default,high) in enumerate((
        ('ToonEdgeBufferWidth','EdgeBufferWidth',24.,200.),
        ('ToonEdgeBufferSoftness','EdgeBufferSoftness',12.,100.))):
        found = [n for n in expressions if isinstance(n, unreal.MaterialExpressionScalarParameter)
            and str(n.get_editor_property('parameter_name')) == name]
        scalar = found[0] if found else node(material, unreal.MaterialExpressionScalarParameter,
            800, -2900-index*180, parameter_name=name, default_value=default, group=GROUP,
            slider_min=0., slider_max=high)
        wire(scalar, custom, pin)
    edge_colors = [n for n in expressions if isinstance(n, unreal.MaterialExpressionVectorParameter)
        and str(n.get_editor_property('parameter_name')) == 'ToonEdgeColor']
    shadow = report['parameters_before'].get('ToonShadowColor', report['parameters_before']['ShadowColor'])
    edge_color = edge_colors[0] if edge_colors else node(material, unreal.MaterialExpressionVectorParameter,
        1100, -3000, parameter_name='ToonEdgeColor', group=GROUP,
        default_value=unreal.LinearColor(shadow[0]*.55, shadow[1]*.55, shadow[2]*.55, 1.))
    wire(edge_color, custom, 'EdgeColor', 'RGB')
    MAT.recompile_material(material)
    stats = MAT.get_statistics(material)
    report['statistics'] = {n:stats.get_editor_property(n) for n in (
        'num_vertex_shader_instructions','num_pixel_shader_instructions','num_samplers')}
    if report['statistics']['num_pixel_shader_instructions'] <= 0:
        raise RuntimeError('No compiled three-tone shader statistics')
    report['parameters_after'] = parameters(instance)
    if any(report['parameters_after'].get(n) != value for n,value in report['parameters_before'].items()):
        raise RuntimeError('An existing material parameter changed')
    report['wpo_after'] = property_root(material, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    report['coverage_after'] = property_root(material, unreal.MaterialProperty.MP_OPACITY_MASK)
    if report['wpo_before'] != report['wpo_after'] or report['coverage_before'] != report['coverage_after']:
        raise RuntimeError('WPO or mask root changed')
    report['nodes'] = MAT.get_num_material_expressions(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Could not save updated material')
    report['status'] = 'updated'
    unreal.log('GROUND_THREE_TONE_MATERIAL ' + json.dumps(report))
except Exception:
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
    unreal.log_error(report['error'])
    raise
finally:
    (WORK/'Material_Update.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
