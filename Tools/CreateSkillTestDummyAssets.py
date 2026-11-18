"""Create native DA examples only. Never edits Blueprint assets or existing resources.

Run once with process-only PythonScriptPlugin after a complete Editor build.
"""
import json
import pathlib
import unreal

ROOT = pathlib.Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/Data/Testing'
DEFINITION = DEST + '/DA_Vehicle_SkillDummy_Default'
CONFIG = DEST + '/DA_SkillTestDummy_Default'

def main():
    for path in (DEFINITION, CONFIG):
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            raise RuntimeError('Refusing to overwrite existing asset: ' + path)
    source = unreal.load_asset('/Game/Data/MVP_B/DA_Vehicle_Car_02')
    if not source:
        raise RuntimeError('Missing Car02 source Definition')
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    definition = tools.duplicate_asset('DA_Vehicle_SkillDummy_Default', DEST, source)
    health = definition.get_editor_property('Health')
    values = health.get_editor_property('Health')
    values.set_editor_property('MaxHP', 1000.0)
    health.set_editor_property('Health', values)
    definition.set_editor_property('Health', health)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('DataAssetClass', unreal.HALSkillTestDummyConfig)
    config = tools.create_asset('DA_SkillTestDummy_Default', DEST, unreal.HALSkillTestDummyConfig, factory)
    config.set_editor_property('VehicleDefinition', definition)
    config.set_editor_property('Label', 'TARGET')
    for obj in (definition, config):
        if not unreal.EditorAssetLibrary.save_loaded_asset(obj):
            raise RuntimeError('Save failed: ' + obj.get_path_name())
    output = ROOT / 'Saved/SkillTestDummy/20261115/CreatedAssets.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({'definition': definition.get_path_name(), 'config': config.get_path_name(), 'note': 'Only new native DAs authored; source and Blueprint assets unchanged'}, indent=2), encoding='utf-8')
    unreal.log('HAL_SKILL_DUMMY_ASSETS_CREATED')

main()
