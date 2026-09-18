"""Reload generated maps, verify references and capture a real editor preview."""
import json
import pathlib
import time
import traceback
import unreal

WORK = pathlib.Path(unreal.Paths.project_saved_dir()).resolve() / 'BattleStadiumMigration'
DATA = json.loads((WORK/'Export/scene.json').read_text(encoding='utf-8'))
REPORT = json.loads((WORK/'import_report.json').read_text(encoding='utf-8'))
LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
results = []
for entry in reversed(REPORT['maps']):
    assert LEVELS.load_level(entry['path']), entry['path']
    actors = [a for a in ACTORS.get_all_level_actors() if isinstance(a, unreal.StaticMeshActor)]
    assert len(actors) == entry['mesh_actors'], entry['path']
    max_error = 0.0
    for actor in actors:
        tags = [str(t) for t in actor.tags]
        idx = int(next(t for t in tags if t.startswith('UnityNode_')).split('_')[1])
        node = DATA['nodes'][idx]
        component = actor.static_mesh_component
        assert component.static_mesh.get_name() == node['mesh']
        for slot in range(component.get_num_materials()):
            assert component.get_material(slot), actor.get_actor_label()
        pos = actor.get_actor_location()
        m = node['matrix']
        error = max(abs(a-b) for a,b in zip((pos.x,pos.y,pos.z),(m[11]*100,m[3]*100,m[7]*100)))
        assert error < 0.1, actor.get_actor_label()
        max_error = max(max_error,error)
    results.append({'map':entry['path'], 'actors_verified':len(actors), 'max_position_error_cm':max_error})

(WORK/'reload_verification.json').write_text(json.dumps(results,indent=2))
location = unreal.Vector(-48000,-48000,44000)
rotation = unreal.MathLibrary.find_look_at_rotation(location,unreal.Vector(0,0,5000))
LEVELS.set_level_viewport_camera_info(location,rotation,unreal.Name(''))
LEVELS.save_current_level()
unreal.SystemLibrary.execute_console_command(None,'r.RayTracing 0')
unreal.SystemLibrary.execute_console_command(None,'r.ScreenPercentage 100')
camera = ACTORS.spawn_actor_from_class(unreal.CameraActor,location,rotation)
camera.camera_component.set_field_of_view(70.0)
start = time.monotonic()
task = None

def tick(delta):
    global task
    try:
        elapsed = time.monotonic()-start
        if task is None and elapsed > 40:
            task = unreal.AutomationLibrary.take_high_res_screenshot(1600,1000,str(WORK/'stadium_preview.png'),camera=camera,delay=2.0)
        elif task is not None and task.is_task_done():
            unreal.unregister_slate_post_tick_callback(handle)
            ACTORS.destroy_actor(camera)
            unreal.log('STADIUM_VERIFICATION_COMPLETE')
            unreal.SystemLibrary.quit_editor()
        elif elapsed > 240:
            raise RuntimeError('Preview screenshot timeout')
    except Exception:
        (WORK/'preview_error.txt').write_text(traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()

handle = unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
