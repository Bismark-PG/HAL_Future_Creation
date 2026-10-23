"""Run with UnrealEditor-Cmd -run=pythonscript, using official editor APIs only.

Creates static preview maps, not gameplay Blueprints. Unity behaviours are not ported.
"""
import json
import math
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(unreal.Paths.project_dir()).resolve()
WORK = ROOT / 'Saved/PlatformerSpaceMigration'
EXPORT = WORK / 'Export'
DEST = '/Game/Imported/Platformer11Space'
DATA = json.loads((EXPORT / 'scene.json').read_text(encoding='utf-8'))
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
MAT = unreal.MaterialEditingLibrary
report = {'maps': [], 'mesh_checks': [], 'skipped_components': DATA['skipped'],
          'limitations': ['Prefab appearances baked into static meshes; no Blueprint, animation, scripts, particles or source collision conversion.',
                          'Materials approximated; custom shader effects not reproduced. Complex collision is for static environment only.']}
PREFABS = {r['mesh']: r for r in DATA['nodes']}
def mesh_folder(record):
    node = PREFABS.get(record['id'])
    return DEST + ('/Prefabs/' + str(pathlib.PurePosixPath(node['path']).parent) if node else '/Meshes')

def log(message):
    unreal.log('PLATFORMER: ' + message)
    with (WORK / 'import_progress.txt').open('a', encoding='utf-8') as f:
        f.write(message + '\n')

def import_file(path, folder, name, options=None, force=False):
    existing = unreal.load_asset(folder + '/' + name)
    if existing and not force:
        return existing
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = force
    task.save = True
    if options:
        task.options = options
        task.factory = unreal.FbxFactory()
    TOOLS.import_asset_tasks([task])
    assets = task.get_objects()
    result = next((a for a in assets if isinstance(a, unreal.StaticMesh if options else unreal.Texture)), None)
    if not result:
        raise RuntimeError('Import failed: ' + str(path))
    return result

def expr(material, cls, **props):
    node = MAT.create_material_expression(material, cls)
    for name, value in props.items():
        node.set_editor_property(name, value)
    return node

def make_material(record, textures):
    path = DEST + '/Materials/' + record['id']
    old = unreal.load_asset(path)
    if old:
        return old
    material = TOOLS.create_asset(record['id'], DEST + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('two_sided', True)
    color = record['color']
    tint = expr(material, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(*color))
    base = tint
    texture = textures.get(record['texture'])
    sample = None
    if texture:
        sample = expr(material, unreal.MaterialExpressionTextureSample, texture=texture)
        scale = record.get('uvScale', [1, 1]); offset = record.get('uvOffset', [0, 0])
        coord = expr(material, unreal.MaterialExpressionTextureCoordinate, u_tiling=scale[0], v_tiling=scale[1])
        # FBX importer flips V; Unity UV transforms therefore need the corresponding offset.
        uv_offset = expr(material, unreal.MaterialExpressionConstant2Vector, r=offset[0], g=1-scale[1]-offset[1])
        add = expr(material, unreal.MaterialExpressionAdd)
        MAT.connect_material_expressions(coord, '', add, 'A')
        MAT.connect_material_expressions(uv_offset, '', add, 'B')
        MAT.connect_material_expressions(add, '', sample, 'Coordinates')
        base = expr(material, unreal.MaterialExpressionMultiply)
        MAT.connect_material_expressions(sample, 'RGB', base, 'A')
        MAT.connect_material_expressions(tint, '', base, 'B')
    MAT.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = expr(material, unreal.MaterialExpressionConstant, r=max(0.12, 1-record.get('smoothness', 0.3)))
    metal = expr(material, unreal.MaterialExpressionConstant, r=record.get('metallic', 0.0))
    MAT.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MAT.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    if color[3] < 0.99:
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        alpha = expr(material, unreal.MaterialExpressionConstant, r=color[3])
        MAT.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    elif sample and ('Viewers' in record['name'] or 'stadium_text' in record['name']):
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        MAT.connect_material_property(sample, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    if 'Emission' in record['name'] or 'Hologram' in record['name']:
        MAT.connect_material_property(base, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MAT.layout_material_expressions(material)
    MAT.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material

def node_transform(values):
    # Unity (x right, y up, z forward) -> Unreal (x forward, y right, z up).
    order = (2, 0, 1)
    columns = [[values[order[row]*4 + order[col]] for row in range(3)] for col in range(3)]
    translation = [100 * values[i*4 + 3] for i in order]
    matrix = unreal.Matrix(
        x_plane=unreal.Plane(*columns[0], 0),
        y_plane=unreal.Plane(*columns[1], 0),
        z_plane=unreal.Plane(*columns[2], 0),
        w_plane=unreal.Plane(*translation, 1))
    transform = matrix.transform()
    # Check actual UE decomposition against the original affine mapping, including negative scale.
    error = 0.0
    for axis in ([0,0,0], [100,0,0], [0,100,0], [0,0,100]):
        actual = transform.transform_location(unreal.Vector(*axis))
        expected = [translation[row] + sum(columns[col][row]*axis[col] for col in range(3)) for row in range(3)]
        error = max(error, max(abs(a-b) for a,b in zip((actual.x,actual.y,actual.z),expected)))
    if error > 0.1:
        raise RuntimeError('Transform decomposition error: ' + str(error))
    return transform, error

def add_preview_lighting():
    sun = ACTORS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0,0,6000), unreal.Rotator(-50,-30,0))
    sun.set_actor_label('Preview_Sun')
    sun.set_folder_path('PreviewLighting')
    light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    light.set_mobility(unreal.ComponentMobility.MOVABLE)
    light.set_intensity(5.0)
    sky = ACTORS.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0,0,5000))
    sky.set_actor_label('Preview_SkyLight')
    sky.set_folder_path('PreviewLighting')
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    cube = unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap')
    if cube:
        sky_component.set_cubemap(cube)
    sky_component.set_intensity(0.8)
    sky_component.set_editor_property('lower_hemisphere_is_black', False)

def main():
    log('Starting import')
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.OBJ 0')
    textures = {}
    for name in sorted(p.name for p in (EXPORT/'Textures').glob('*.png')):
        textures[name] = import_file(EXPORT / 'Textures' / name, DEST + '/Textures', 'T_' + pathlib.Path(name).stem)
    log('Textures imported: ' + str(len(textures)))
    materials = {r['id']: make_material(r, textures) for r in DATA['materials']}
    log('Materials created: ' + str(len(materials)))
    options = unreal.FbxImportUI()
    options.set_editor_property('automated_import_should_detect_type', False)
    options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property('import_mesh', True)
    options.set_editor_property('import_as_skeletal', False)
    options.set_editor_property('import_materials', False)
    options.set_editor_property('import_textures', False)
    sm_options = options.static_mesh_import_data
    for key, value in {'combine_meshes':True, 'auto_generate_collision':False, 'generate_lightmap_u_vs':False,
                       'convert_scene':False, 'convert_scene_unit':False, 'import_uniform_scale':1.0,
                       'normal_import_method':unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
                       'coordinate_system_policy':unreal.CoordinateSystemPolicy.KEEP_XYZ_AXES}.items():
        sm_options.set_editor_property(key, value)
    meshes = {}
    for index, record in enumerate(DATA['meshes']):
        mesh = import_file(EXPORT / 'Meshes' / (record['id']+'.obj'), mesh_folder(record), record['id'], options)
        body = mesh.get_editor_property('body_setup')
        if body:
            body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        bound = mesh.get_bounding_box()
        # Unity baked skin bounds can contain unused vertices. Validate rendered triangles.
        vertices, used = [], set()
        for line in (EXPORT/'Meshes'/(record['id']+'.obj')).read_text().splitlines():
            if line.startswith('v '):
                x,y,z = map(float,line.split()[1:4]); vertices.append((x,-y,z))
            elif line.startswith('f '):
                used.update(int(c.split('/')[0])-1 for c in line.split()[1:])
        expected = [min(vertices[i][axis] for i in used) for axis in range(3)] + [max(vertices[i][axis] for i in used) for axis in range(3)]
        actual = [bound.min.x,bound.min.y,bound.min.z,bound.max.x,bound.max.y,bound.max.z]
        error = max(abs(x-y) for x,y in zip(expected, actual))
        report['mesh_checks'].append({'mesh': record['id'], 'bounds_error_cm':error, 'slots':len(mesh.static_materials)})
        if error > 0.5:
            raise RuntimeError('Mesh axis/scale mismatch '+record['id']+': '+str(actual)+' vs '+str(expected))
        if len(mesh.static_materials) != record['submeshes']:
            raise RuntimeError('Material slot mismatch: '+record['id'])
        node = PREFABS.get(record['id'])
        if node:
            for slot_index, slot in enumerate(mesh.static_materials):
                slot_name = str(slot.get_editor_property('material_slot_name'))
                source_index = int(slot_name.split('_')[-1]) if slot_name.startswith('Slot_') else slot_index
                material_id = node['materials'][source_index]
                if material_id:
                    mesh.set_material(slot_index, materials[material_id])
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        meshes[record['id']] = mesh
        if index % 25 == 0:
            log('Meshes: '+str(index+1)+'/'+str(len(DATA['meshes'])))
    log('All mesh bounds and material slot counts verified')
    map_path = DEST + '/Maps/Prefab_Overview'
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        raise RuntimeError('Overview already exists; refusing to replace user edits')
    if not LEVELS.new_level(map_path):
        raise RuntimeError('Cannot create overview')
    max_extent = max(max(meshes[r['mesh']].get_bounding_box().max.x-meshes[r['mesh']].get_bounding_box().min.x,
                         meshes[r['mesh']].get_bounding_box().max.y-meshes[r['mesh']].get_bounding_box().min.y) for r in DATA['nodes'])
    spacing = max(600, max_extent + 300)
    report['prefabs'] = []
    for index, record in enumerate(DATA['nodes']):
        mesh = meshes[record['mesh']]
        bound = mesh.get_bounding_box()
        position = unreal.Vector((index%12)*spacing, (index//12)*spacing, -bound.min.z)
        actor = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, position)
        actor.set_actor_label(record['name'])
        actor.set_folder_path(str(pathlib.PurePosixPath(record['path']).parent))
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
        actor.set_editor_property('tags', [unreal.Name('PlatformerSpaceMigration'), unreal.Name('UnityNode_'+str(index))])
        report['prefabs'].append({'source':record['path'], 'asset':mesh.get_path_name(), 'slots':len(mesh.static_materials), 'position':[position.x,position.y,position.z]})
    add_preview_lighting()
    assert LEVELS.save_current_level()
    report['maps'].append({'path':map_path,'mesh_actors':len(DATA['nodes']), 'spacing_cm':spacing})
    report.update({'status':'complete','meshes':len(meshes),'materials':len(materials),'textures':len(textures)})
    (WORK/'import_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    log('IMPORT_COMPLETE')

try:
    main()
except Exception:
    report['status']='failed'
    report['error']=traceback.format_exc()
    (WORK/'import_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log_error(report['error'])
    raise

