import unreal, pathlib, time, traceback, json
WORK = pathlib.Path(unreal.Paths.project_saved_dir()).resolve() / 'PlatformerSciFiMigration'
report=json.loads((WORK/'import_report.json').read_text())
LEVELS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert LEVELS.load_level(report['maps'][0]['path'])
spacing=report['maps'][0]['spacing_cm']
center=unreal.Vector(spacing*5.5,spacing*5.5,0)
location=center+unreal.Vector(-spacing*3,-spacing*5,spacing*12)
rotation=unreal.MathLibrary.find_look_at_rotation(location,center)
camera=ACTORS.spawn_actor_from_class(unreal.CameraActor,location,rotation)
camera.camera_component.set_field_of_view(70)
unreal.SystemLibrary.execute_console_command(None,'r.ScreenPercentage 100')
start = time.monotonic()
task = None

def tick(delta):
    global task
    try:
        elapsed = time.monotonic()-start
        if task is None and elapsed > 20:
            task = unreal.AutomationLibrary.take_high_res_screenshot(1600,1000,str(WORK/'prefab_overview.png'),camera=camera,delay=2.0)
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
