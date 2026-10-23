"""Reload all generated assets, assign matching raw-model materials and verify references."""
import json
import pathlib
import unreal

WORK = pathlib.Path(unreal.Paths.project_saved_dir()).resolve() / 'PlatformerSpaceMigration'
DATA = json.loads((WORK/'Export/scene.json').read_text())
REPORT = json.loads((WORK/'import_report.json').read_text())
DEST = '/Game/Imported/Platformer11Space'
nodes = {n['name'].lower(): n for n in DATA['nodes']}
records = {m['id']: m for m in DATA['meshes']}
prefab_ids = {n['mesh'] for n in DATA['nodes']}
checked = []
raw_assigned = []
for record in DATA['meshes']:
    if record['id'] in prefab_ids:
        entry = next(e for e in REPORT['prefabs'] if e['asset'].split('.')[-1] == record['id'])
        path = entry['asset']
    else:
        path = DEST+'/Meshes/'+record['id']
    mesh = unreal.load_asset(path)
    assert isinstance(mesh, unreal.StaticMesh), path
    assert len(mesh.static_materials) == record['submeshes'], path
    if record['id'] not in prefab_ids:
        ids = record.get('defaultMaterials')
        if not ids:
            node = nodes[record['name'].lower()]
            assembled = records[node['mesh']]
            assert all(record[k] == assembled[k] for k in ('vertices','triangles','submeshes')), path
            ids = node['materials']
        assert ids, path
        for i, slot in enumerate(mesh.static_materials):
            name = str(slot.material_slot_name)
            source_index = int(name.split('_')[-1])
            material = unreal.load_asset(DEST+'/Materials/'+ids[min(source_index,len(ids)-1)])
            assert material
            mesh.set_material(i, material)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        raw_assigned.append(path)
    for slot in mesh.static_materials:
        assert slot.material_interface and slot.material_interface.get_path_name().startswith(DEST+'/Materials/'), path
    checked.append(path)
for material in DATA['materials']:
    assert unreal.load_asset(DEST+'/Materials/'+material['id'])
for file in (WORK/'Export/Textures').glob('*.png'):
    assert unreal.load_asset(DEST+'/Textures/T_'+file.stem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(REPORT['maps'][0]['path'])
placed = [a for a in actors.get_all_level_actors() if isinstance(a,unreal.StaticMeshActor)]
assert len(placed) == len(DATA['nodes'])
for actor in placed:
    index = int(next(str(t).split('_')[-1] for t in actor.tags if str(t).startswith('UnityNode_')))
    entry = REPORT['prefabs'][index]
    assert actor.static_mesh_component.static_mesh.get_path_name() == entry['asset']
    pos = actor.get_actor_location()
    assert max(abs(a-b) for a,b in zip([pos.x,pos.y,pos.z],entry['position'])) < 0.01
result = {'status':'passed','meshes_reloaded':len(checked),'raw_materials_assigned':len(raw_assigned),'prefabs_verified':len(placed),'materials':len(DATA['materials']),'textures':len(list((WORK/'Export/Textures').glob('*.png')))}
(WORK/'verification.json').write_text(json.dumps(result,indent=2))
unreal.log('PLATFORMER_VERIFICATION_PASSED '+str(result))

