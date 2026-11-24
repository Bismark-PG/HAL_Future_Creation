"""Author the Car12 skill DA, ink material and two-note lock signal through Unreal Editor."""
import array
import math
from pathlib import Path
import wave

import unreal


FX = '/Game/Assets/FX/Batarang'
CAR = '/Game/Data/MVP_B/DA_Vehicle_Car_12'
CONFIG = '/Game/Data/Abilities/DA_Ability_Car12_Batarang'
LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def load(path):
    return unreal.load_asset(path) if LIB.does_asset_exist(path) else None


def save(asset):
    if not LIB.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + asset.get_path_name())


def ink_material():
    name = 'M_BatarangInk'
    mat = load(FX + '/' + name)
    if mat:
        return mat
    mat = TOOLS.create_asset(name, FX, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    edit = unreal.MaterialEditingLibrary
    color = edit.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, -80)
    color.set_editor_property('parameter_name', 'Tint')
    color.set_editor_property('default_value', unreal.LinearColor(.65, .75, .15, 1))
    bright = edit.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 80)
    bright.set_editor_property('parameter_name', 'Brightness')
    bright.set_editor_property('default_value', 1.)
    product = edit.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, 0)
    if not (edit.connect_material_expressions(color, 'RGB', product, 'A')
            and edit.connect_material_expressions(bright, '', product, 'B')
            and edit.connect_material_property(product, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)):
        raise RuntimeError('Dart material connection failed')
    edit.recompile_material(mat)
    save(mat)
    return mat


def signal_sound():
    name = 'SW_BatarangLock'
    sound = load(FX + '/' + name)
    if sound:
        return sound
    path = Path(unreal.Paths.project_saved_dir()).resolve() / 'Batarang' / (name + '.wav')
    path.parent.mkdir(parents=True, exist_ok=True)
    rate = 24000
    samples = array.array('h')
    for i in range(round(.22 * rate)):
        t = i / rate
        start, freq = (0., 587.33) if t < .092 else (.092, 880.)
        u = t - start
        gain = min(1., u / .006) * math.exp(-u * 35.) * min(1., (.22-t) / .012)
        samples.append(round(11000 * gain * math.sin(math.tau * freq * u)))
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
        out.writeframes(samples.tobytes())
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(path))
    task.set_editor_property('destination_path', FX)
    task.set_editor_property('automated', True)
    TOOLS.import_asset_tasks([task])
    sound = load(FX + '/' + name)
    if not isinstance(sound, unreal.SoundWave):
        raise RuntimeError('Lock signal import failed')
    save(sound)
    return sound


def main():
    car = unreal.load_asset(CAR)
    slot = car.get_editor_property('abilities')
    grants = slot.get_editor_property('grants')
    ability = unreal.HALBatarangAbility.static_class()
    if grants and (len(grants) != 1 or grants[0].get_editor_property('ability') != ability):
        raise RuntimeError('Car12 already has a different primary: ' + str(grants[0].get_editor_property('ability')))
    combat = car.get_editor_property('combat')
    power = combat.get_editor_property('attack_power')
    if power <= 0:
        raise RuntimeError('Car12 needs a positive base attack for the 10 damage baseline')
    config = load(CONFIG)
    if not config:
        mat, sound = ink_material(), signal_sound()
        folder, name = CONFIG.rsplit('/', 1)
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('DataAssetClass', unreal.HALBatarangConfig)
        config = TOOLS.create_asset(name, folder, unreal.HALBatarangConfig, factory)
        config.set_editor_property('damage_multiplier', 10. / power)
        config.set_editor_property('dart_material', mat)
        config.set_editor_property('pin_signal', sound)
        save(config)
    if not grants:
        model = unreal.load_asset('/Game/Data/MVP_B/DA_Vehicle_Car_09').get_editor_property('abilities')
        primary = model.get_editor_property('grants')[0].get_editor_property('input_tag').export_text()
        grant = unreal.HALAbilityGrant()
        grant.import_text(f'(Ability="{ability.get_path_name()}",InputTag={primary},Level=1,SkillConfig="{config.get_path_name()}")')
        if grant.get_editor_property('ability') != ability or grant.get_editor_property('skill_config') != config:
            raise RuntimeError('Grant did not resolve')
        slot.set_editor_property('version', model.get_editor_property('version'))
        slot.set_editor_property('grants', [grant])
        car.set_editor_property('abilities', slot)
        save(car)
    elif grants[0].get_editor_property('skill_config') != config:
        raise RuntimeError('Car12 already references a different skill config')
    unreal.log(f"CAR12_AUTHORED power={power} multiplier={config.get_editor_property('damage_multiplier')} "
               f"recharge={slot.get_editor_property('full_recharge_seconds')} fuse={config.get_editor_property('fuse_seconds')}")
    # Read both local test rosters without modifying either game mode Blueprint.
    car_class = LIB.load_blueprint_class('/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_12')
    for name in ('BP_VehicleTestGameMode', 'BP_FredSkillTestGameMode'):
        gm = LIB.load_blueprint_class('/Game/Blueprints/Gamemode/' + name)
        if gm:
            roster = unreal.get_default_object(gm).get_editor_property('test_vehicle_classes')
            unreal.log(f'CAR12_TEST_ROSTER {name} included={car_class in roster}')


main()
