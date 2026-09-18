import json
import pathlib
import unreal

out = pathlib.Path(unreal.Paths.project_saved_dir()) / 'BattleStadiumMigration/unreal_probe.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({
    'engine': unreal.SystemLibrary.get_engine_version(),
    'asset_tools': bool(unreal.AssetToolsHelpers.get_asset_tools()),
    'level': bool(unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)),
    'fbx_options': str(unreal.FbxImportUI()),
    'coordinate_policies': str(dir(unreal.CoordinateSystemPolicy)),
    'matrix_doc': unreal.Matrix.__doc__,
    'matrix_methods': str(dir(unreal.Matrix)),
}, indent=2))
unreal.log('STADIUM_PROBE_OK')
