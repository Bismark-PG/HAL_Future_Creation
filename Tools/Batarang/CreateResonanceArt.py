"""Build Car12's presentation assets with the editor. Run after CreateBatarangAssets."""
from pathlib import Path
import runpy
import unreal as u

FX = '/Game/Assets/FX/Batarang'
SKILL = '/Game/Data/Abilities/DA_Ability_Car12_Batarang'
SOURCE = Path(u.Paths.project_dir()).resolve() / 'Tools' / 'Batarang'
LIB = u.EditorAssetLibrary
EDIT = u.MaterialEditingLibrary
TOOLS = u.AssetToolsHelpers.get_asset_tools()


def load(path):
    return u.load_asset(path) if LIB.does_asset_exist(path) else None


def save(asset):
    if not LIB.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + asset.get_path_name())


def connect_black_metal(material, shader, colour, exposure):
    """Use scene reflections for the facets and a small exposure-stable lift on the bevels."""
    prop = u.MaterialProperty
    material.set_editor_property('shading_model', u.MaterialShadingModel.MSM_DEFAULT_LIT)
    # The top-facing Car12 paint samples use sRGB #242425 on its shared colour atlas.
    ink = next(node for node in EDIT.get_inputs_for_material_expression(material, shader)
               if isinstance(node, u.MaterialExpressionVectorParameter)
               and str(node.get_editor_property('parameter_name')) == 'Ink')
    ink.set_editor_property('default_value', u.LinearColor(.01764, .01764, .01850, 1))
    rough = EDIT.create_material_expression(material, u.MaterialExpressionComponentMask, 355, 180)
    for channel in 'rgb':
        rough.set_editor_property(channel, False)
    rough.set_editor_property('a', True)
    metal = EDIT.create_material_expression(material, u.MaterialExpressionScalarParameter, 355, 280)
    metal.set_editor_property('parameter_name', 'BladeMetallic')
    metal.set_editor_property('default_value', .90)
    lift = EDIT.create_material_expression(material, u.MaterialExpressionMultiply, 555, -160)
    lift.set_editor_property('const_b', .10)
    if not all((EDIT.connect_material_property(colour, '', prop.MP_BASE_COLOR),
                EDIT.connect_material_expressions(shader, '', rough, ''),
                EDIT.connect_material_property(rough, '', prop.MP_ROUGHNESS),
                EDIT.connect_material_property(metal, '', prop.MP_METALLIC),
                EDIT.connect_material_expressions(colour, '', lift, 'A'),
                EDIT.connect_material_expressions(lift, '', exposure,
                                                 EDIT.get_material_expression_input_names(exposure)[0]))):
        raise RuntimeError('Could not connect the black metal finish')


def surface(kind, phase_params, translucent):
    name = 'M_Car12_' + kind
    material = load(FX + '/' + name)
    code = (SOURCE / (kind + '.hlsl')).read_text(encoding='utf-8')
    marker = 'Car12 ' + kind
    if material:
        rgb = EDIT.get_material_property_input_node(material, u.MaterialProperty.MP_BASE_COLOR) if kind == 'Blade' else None
        if not rgb:
            rgb = EDIT.get_material_property_input_node(material, u.MaterialProperty.MP_EMISSIVE_COLOR)
        if isinstance(rgb, u.MaterialExpressionEyeAdaptationInverse):
            rgb = next((pin for pin in EDIT.get_inputs_for_material_expression(material, rgb)
                        if isinstance(pin, u.MaterialExpressionComponentMask)), None)
        nodes = EDIT.get_inputs_for_material_expression(material, rgb) if rgb else []
        shader = next((node for node in nodes if isinstance(node, u.MaterialExpressionCustom)
                       and node.get_editor_property('description') == marker), None)
        if not shader:
            raise RuntimeError('Cannot refresh the owned shader in ' + name)
        changed = shader.get_editor_property('code') != code
    else:
        material = TOOLS.create_asset(name, FX, u.Material, u.MaterialFactoryNew())
        material.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property('two_sided', True)
        if translucent:
            material.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
        shader = EDIT.create_material_expression(material, u.MaterialExpressionCustom, 30, 0)
        shader.set_editor_property('description', marker)
        shader.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT4)
        defaults = {'UV': None, 'Ink': (.014, .022, .04), 'Livery': (.56, .92, .055),
                    'SeatColor': (.68, .92, .09), **phase_params}
        pins = []
        for pin_name in defaults:
            pin = u.CustomInput()
            pin.set_editor_property('input_name', pin_name)
            pins.append(pin)
        shader.set_editor_property('inputs', pins)
        for row, (pin_name, default) in enumerate(defaults.items()):
            if default is None:
                node = EDIT.create_material_expression(material, u.MaterialExpressionTextureCoordinate, -470, row*110)
                output = ''
            else:
                vector = isinstance(default, tuple)
                cls = u.MaterialExpressionVectorParameter if vector else u.MaterialExpressionScalarParameter
                node = EDIT.create_material_expression(material, cls, -470, row*110)
                node.set_editor_property('parameter_name', pin_name)
                node.set_editor_property('default_value', u.LinearColor(*default, 1) if vector else default)
                output = 'RGB' if vector else ''
            if not EDIT.connect_material_expressions(node, output, shader, pin_name):
                raise RuntimeError('Missing shader input ' + pin_name)
        outputs = [(True, u.MaterialProperty.MP_EMISSIVE_COLOR)]
        if translucent:
            outputs.append((False, u.MaterialProperty.MP_OPACITY))
        for rgb, output in outputs:
            mask = EDIT.create_material_expression(material, u.MaterialExpressionComponentMask, 355, 0 if rgb else 170)
            for channel in 'rgb':
                mask.set_editor_property(channel, rgb)
            mask.set_editor_property('a', not rgb)
            if not EDIT.connect_material_expressions(shader, '', mask, '') or not EDIT.connect_material_property(mask, '', output):
                raise RuntimeError('Missing RGB/opacity output on ' + name)
        changed = True
    if kind == 'Wake' and not material.get_editor_property('enable_responsive_aa'):
        material.set_editor_property('enable_responsive_aa', True)
        changed = True
    colour_output = EDIT.get_material_property_input_node(material, u.MaterialProperty.MP_EMISSIVE_COLOR)
    if isinstance(colour_output, u.MaterialExpressionEyeAdaptationInverse):
        exposure = colour_output
    else:
        # Keep ink, silver and enamel distinct when the night arena opens up its exposure.
        exposure = EDIT.create_material_expression(material, u.MaterialExpressionEyeAdaptationInverse, 565, 0)
        light_pin = EDIT.get_material_expression_input_names(exposure)[0]
        if not EDIT.connect_material_expressions(colour_output, '', exposure, light_pin):
            raise RuntimeError('Missing exposure colour input on ' + name)
        if not EDIT.connect_material_property(exposure, '', u.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError('Missing emissive output on ' + name)
        changed = True
    if kind == 'Blade' and not EDIT.get_material_property_input_node(material, u.MaterialProperty.MP_BASE_COLOR):
        colour = EDIT.get_inputs_for_material_expression(material, exposure)[0]
        connect_black_metal(material, shader, colour, exposure)
        changed = True
    if changed:
        shader.set_editor_property('code', code)
        errors = EDIT.recompile_material(material)
        if errors:
            raise RuntimeError(str(errors))
        save(material)
    instance_name = 'MI_Car12_' + kind
    instance = load(FX + '/' + instance_name)
    if not instance:
        instance = TOOLS.create_asset(instance_name, FX, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
        EDIT.set_material_instance_parent(instance, material)
        save(instance)
    return instance


def sculpt(material):
    path = FX + '/SM_Car12_ResonanceBlade'
    existing = load(path)
    if existing and '-HALRefreshDartMesh' not in u.SystemLibrary.get_command_line():
        return existing
    positions, texcoords, triangles = [], [], []

    # Hard edges and palette UVs are per face. Overall span is 100 cm, tip points along +X.
    def face(points, tone):
        start = len(positions)
        for index, point in enumerate(points):
            positions.append(u.Vector(*point))
            texcoords.append(u.Vector2D((tone + (.75 if index in (1, 2) else .2))*.1,
                                        .85 if index > 1 else .14))
        for i in range(1, len(points)-1):
            triangles.append(u.IntVector(start, start+i, start+i+1))

    # Y station, leading edge, scalloped trailing edge, dorsal ridge Z.
    stations = [(0,29,-22,5.0), (7,26,-11,5.6), (13,23,-7,5.0), (19,19,-19,4.1),
                (26,13,-9,3.2), (33,6,-17,2.5), (40,-3,-13,1.6), (50,-24,-24,.4)]
    for sign in (-1, 1):
        def section(station):
            y, lead, rear, height = station
            width = lead-rear
            return [(lead, sign*y, .45), (lead-width*.14, sign*y, height*.72),
                    (lead-width*.42, sign*y, height), (rear+width*.10, sign*y, height*.33),
                    (rear, sign*y, .05), (rear+width*.03, sign*y, -2.0),
                    (lead-width*.10, sign*y, -2.6)]

        for i in range(len(stations)-1):
            a, b = section(stations[i]), section(stations[i+1])
            bands = (3, 2 if i in (1, 4) else 1, 1 if i in (1, 4) else 2, 0, 1, 1, 0)
            for j, tone in enumerate(bands):
                k = (j+1) % len(bands)
                face([a[j], b[j], b[k], a[k]], tone)
        # Recess and enamel inlay along the inner wing. The core remains recognisable at race-camera scale.
        for i in range(1, 6):
            a, b = section(stations[i]), section(stations[i+1])
            def inlay(s, offset):
                v, w = s[1], s[2]
                return (v[0]*(1-offset)+w[0]*offset, v[1], v[2]*(1-offset)+w[2]*offset+.18)
            face([inlay(a,.14),inlay(b,.14),inlay(b,.65),inlay(a,.65)], 4)

    # Raised, bevelled central transducer with a seat-coloured lens and ivory glint.
    def diamond(x, half_x, half_y, z):
        return [(x+half_x,0,z),(x,half_y,z),(x-half_x,0,z),(x,-half_y,z)]
    rings = [diamond(1,28,10,5.8),diamond(1,22,8,8.0),diamond(3,13,5.5,8.6),diamond(3,9,3.8,9.4)]
    for ring, inner, tone in zip(rings,rings[1:],(0,3,5)):
        for j in range(4):
            k = (j+1) % 4
            face([ring[j],ring[k],inner[k],inner[j]],tone)
    face(rings[-1],6)
    buffers = u.GeometryScriptSimpleMeshBuffers()
    buffers.vertices, buffers.uv0, buffers.triangles = positions, texcoords, triangles
    dynamic = u.DynamicMesh()
    u.GeometryScript_MeshEdits.append_buffers_to_mesh(dynamic, buffers)
    if existing:
        opts = u.GeometryScriptCopyMeshToAssetOptions()
        _, result = u.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dynamic, existing, opts, u.GeometryScriptMeshWriteLOD())
        mesh = existing
    else:
        opts = u.GeometryScriptCreateNewStaticMeshAssetOptions()
        opts.enable_collision = False
        opts.enable_recompute_normals = True
        opts.enable_recompute_tangents = True
        mesh, result = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dynamic, path, opts)
    if not mesh or result != u.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError('Could not author ' + path)
    mesh.set_material(0, material)
    save(mesh)
    return mesh


def main():
    config = load(SKILL)
    if not config:
        raise RuntimeError('Create the base Car12 skill DA before the art pass')
    blade = sculpt(surface('Blade', {'Signal': .7}, False))
    wake = surface('Wake', {}, True)
    resonance = surface('Resonance', {'Burst': 1., 'Phase': .34, 'Signal': .85}, True)
    changed = runpy.run_path(str(SOURCE / 'BakePinProfiles.py'))['update_profiles'](config)
    cloud = load('/Game/Assets/FX/SpecialBalls/Meteor/SM_Meteor_ImpactVolume')
    if not cloud:
        raise RuntimeError('Missing the shared sculpted burst puff')
    # The original 240 cm wave was saved explicitly on some copies of the skill.
    if not config.get_editor_property('burst_cloud_mesh') and config.get_editor_property('burst_reach') == 240.:
        config.set_editor_property('burst_reach', 145.)
        changed = True
    for name, value in [('blade_mesh',blade), ('wake_material',wake), ('resonance_material',resonance), ('burst_cloud_mesh',cloud)]:
        if not config.get_editor_property(name):
            config.set_editor_property(name,value)
            changed = True
    if changed:
        save(config)
    u.log('CAR12_ART bound=' + ', '.join(config.get_editor_property(name).get_name()
          for name in ('blade_mesh','wake_material','resonance_material')))


main()
