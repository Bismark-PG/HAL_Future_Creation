"""Author the A orbital match ball inside Unreal Editor, including its kickoff clearance."""
from pathlib import Path
import unreal as u

LIB = u.EditorAssetLibrary
TOOLS = u.AssetToolsHelpers.get_asset_tools()
EDIT = u.MaterialEditingLibrary
FOLDER = '/Game/Assets/Balls/Primary'
DEFINITION = '/Game/Data/MVP_B/DA_Ball_Normal_TestBaseline_v1'
MATCH = '/Game/Maps/MVPB_Match'
RADIUS = 115.
RETURN_CLEARANCE = 8.  # Same as HALMatchBallLifecycle's safe spawn.


def existing(name):
    path = FOLDER + '/' + name
    return LIB.load_asset(path) if LIB.does_asset_exist(path) else None


def save(asset):
    if not LIB.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + asset.get_path_name())


def orbital_material():
    name = 'M_PrimaryBall_Orbital'
    code_file = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / 'Tools/PrimaryBall/OrbitalSurface.hlsl'
    code = code_file.read_text(encoding='utf-8')
    material = existing(name)
    prop = u.MaterialProperty
    if material:
        shader = EDIT.get_material_property_input_node(material, prop.MP_BASE_COLOR)
        if not isinstance(shader, u.MaterialExpressionCustom) or shader.get_editor_property('description') != 'Orbital league ball':
            raise RuntimeError('The orbital master has been reauthored. Inspect its graph before regenerating.')
        if shader.get_editor_property('code') == code:
            return material
    else:
        material = TOOLS.create_asset(name, FOLDER, u.Material, u.MaterialFactoryNew())
        def node(cls, x, y):
            return EDIT.create_material_expression(material, cls, x, y)
        sources = [
            ('LocalPoint', node(u.MaterialExpressionLocalPosition, -600, -320), ''),
            ('SurfaceNormal', node(u.MaterialExpressionPixelNormalWS, -600, -180), '')]
        for index, (key, rgb) in enumerate([
            ('Pearl', (.88,.78,.62)), ('Cobalt', (.028,.065,.44)),
            ('Brass', (.91,.53,.18)), ('DialColor', (.026,.24,.245)), ('Signal', (.13,.91,.94))]):
            parameter = node(u.MaterialExpressionVectorParameter, -600, index*160)
            parameter.set_editor_property('parameter_name',key)
            parameter.set_editor_property('group','Orbital Finish')
            parameter.set_editor_property('default_value',u.LinearColor(*rgb,1.))
            sources.append((key,parameter,'RGB'))
        power = node(u.MaterialExpressionScalarParameter,-600,810)
        power.set_editor_property('parameter_name','SignalStrength')
        power.set_editor_property('group','Orbital Finish')
        power.set_editor_property('default_value',1.45)
        sources.append(('SignalStrength',power,''))
        shader = node(u.MaterialExpressionCustom,-100,0)
        shader.set_editor_property('description','Orbital league ball')
        shader.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3)
        inputs = []
        for key, _, _ in sources:
            pin = u.CustomInput()
            pin.set_editor_property('input_name',key)
            inputs.append(pin)
        shader.set_editor_property('inputs',inputs)
        outputs = []
        for key, typ in [('Metal','CMOT_FLOAT1'),('Rough','CMOT_FLOAT1'),('Glow','CMOT_FLOAT3')]:
            pin = u.CustomOutput()
            pin.set_editor_property('output_name',key)
            pin.set_editor_property('output_type',getattr(u.CustomMaterialOutputType,typ))
            outputs.append(pin)
        shader.set_editor_property('additional_outputs',outputs)
        for key, source, output in sources:
            if not EDIT.connect_material_expressions(source,output,shader,key):
                raise RuntimeError('Could not connect shader input '+key)
        for key, target in [('return',prop.MP_BASE_COLOR),('Metal',prop.MP_METALLIC),
                            ('Rough',prop.MP_ROUGHNESS),('Glow',prop.MP_EMISSIVE_COLOR)]:
            if not EDIT.connect_material_property(shader,key,target):
                raise RuntimeError('Could not connect shader output '+key)
    shader.set_editor_property('code',code)
    EDIT.recompile_material(material)
    save(material)
    return material


def orbital_mesh(material):
    name = 'SM_PrimaryBall_Orbital'
    mesh = existing(name)
    if not mesh:
        dynamic = u.DynamicMesh()
        u.GeometryScript_Primitives.append_sphere_lat_long(dynamic,u.GeometryScriptPrimitiveOptions(),
                                                           u.Transform(),RADIUS,33,64)
        u.GeometryScript_Normals.set_per_vertex_normals(dynamic)
        options = u.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.enable_collision = False
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        mesh, outcome = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dynamic,FOLDER+'/'+name,options)
        if not mesh or outcome != u.GeometryScriptOutcomePins.SUCCESS:
            raise RuntimeError('Could not create orbital shell')
    bounds = mesh.get_bounds()
    if bounds.origin.length() > .02 or any(abs(x-RADIUS) > .02 for x in bounds.box_extent.to_tuple()):
        raise RuntimeError('The orbital shell must be centered with dimensions 230 x 230 x 230 cm: '+str(bounds))
    if mesh.get_material(0) != material:
        mesh.set_material(0,material)
        save(mesh)
    return mesh


def configure_ball(mesh):
    da = LIB.load_asset(DEFINITION)
    visual = da.get_editor_property('visual')
    body = da.get_editor_property('physics')
    transform = visual.get_editor_property('relative_transform')
    misaligned = (transform.translation.length() > .001 or
                  (transform.scale3d-u.Vector(1.,1.,1.)).length() > .001)
    if (da.get_editor_property('radius') != RADIUS or visual.get_editor_property('mesh') != mesh or
            misaligned or len(visual.get_editor_property('materials')) or not body.get_editor_property('hidden_in_game')):
        visual.set_editor_property('mesh',mesh)
        transform.translation = u.Vector()
        transform.scale3d = u.Vector(1.,1.,1.)
        visual.set_editor_property('relative_transform',transform)
        visual.set_editor_property('materials',[])
        body.set_editor_property('hidden_in_game',True)
        da.set_editor_property('radius',RADIUS)
        da.set_editor_property('visual',visual)
        da.set_editor_property('physics',body)
        save(da)


def align_kickoff(mesh):
    level = u.get_editor_subsystem(u.LevelEditorSubsystem)
    if not level.load_level(MATCH):
        raise RuntimeError('Could not open match map')
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    ball, = [a for a in actors.get_all_level_actors() if a.get_class().get_name()=='BP_BasicBall_C']
    settings = u.load_object(None,'/Script/HAL_Future_Creation.Default__HALMatchBallSettings')
    arena, = [a for a in settings.get_editor_property('Arenas')
              if a.get_editor_property('MatchMap') == u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()]
    # Both deck finish and mesh bounds can sit above/below the authoritative flat drivable surface.
    z = arena.get_editor_property('FloorCenter').z + RADIUS + RETURN_CLEARANCE
    pos = ball.get_actor_location()
    body = ball.root_component
    shell, = [c for c in ball.get_components_by_class(u.StaticMeshComponent) if c.get_name()=='VisualMesh']
    if abs(pos.z-z)<.01 and shell.static_mesh==mesh and abs(body.get_scaled_sphere_radius()-RADIUS)<.01:
        return
    ball.modify()
    body.modify()
    shell.modify()
    ball.set_actor_location(u.Vector(pos.x,pos.y,z),False,True)
    body.set_sphere_radius(RADIUS)
    body.set_hidden_in_game(True,False)
    shell.set_static_mesh(mesh)
    if not u.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError('Could not save the kickoff position in '+MATCH)


LIB.make_directory(FOLDER)
master = orbital_material()
instance = existing('MI_PrimaryBall_Orbital')
if not instance:
    instance = TOOLS.create_asset('MI_PrimaryBall_Orbital',FOLDER,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
    EDIT.set_material_instance_parent(instance,master)
    save(instance)
shell_asset = orbital_mesh(instance)
configure_ball(shell_asset)
align_kickoff(shell_asset)
u.log('ORBITAL_PRIMARY_READY 230 cm')
