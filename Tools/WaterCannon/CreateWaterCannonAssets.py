"""Create Car09's production assets using Editor APIs. Existing artist tuning is preserved."""
import array
import math
import pathlib
import wave

import unreal

ROOT = pathlib.Path(unreal.Paths.project_dir()).resolve()
FX = '/Game/Assets/FX/WaterCannon'
SKILL = '/Game/Data/Abilities/DA_Ability_Car09_WaterCannon'
CAR = '/Game/Data/MVP_B/DA_Vehicle_Car_09'
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EDIT = unreal.MaterialEditingLibrary


def existing(path):
    return unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None


def save(asset):
    if not ASSETS.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + asset.get_path_name())

def set_flow_surface(result, shader):
    code = (ROOT / 'Tools/WaterCannon/ToonWater.hlsl').read_text(encoding='utf-8')
    shader.set_editor_property('code', code)
    result.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    opacity = EDIT.get_material_property_input_node(result, unreal.MaterialProperty.MP_OPACITY)
    if not opacity:
        opacity = EDIT.create_material_expression(result, unreal.MaterialExpressionScalarParameter, -100, 480)
        opacity.set_editor_property('parameter_name', 'SprayOpacity')
        opacity.set_editor_property('default_value', 1.0)
        if not EDIT.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY):
            raise RuntimeError('Missing spray opacity output')
    elif not isinstance(opacity, unreal.MaterialExpressionScalarParameter) or str(opacity.get_editor_property('parameter_name')) != 'SprayOpacity':
        raise RuntimeError('The water material has a custom opacity connection')
    errors = EDIT.recompile_material(result)
    if errors:
        raise RuntimeError(str(errors))
    save(result)


def material(refresh=False):
    name = 'M_WaterCannon_ToonFlow'
    result = existing(FX + '/' + name)
    if result:
        if refresh:
            shader = EDIT.get_material_property_input_node(result, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            if not isinstance(shader, unreal.MaterialExpressionCustom):
                raise RuntimeError('Expected the production water flow shader')
            set_flow_surface(result, shader)
        return result
    result = TOOLS.create_asset(name, FX, unreal.Material, unreal.MaterialFactoryNew())
    result.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    result.set_editor_property('two_sided', True)
    shader = EDIT.create_material_expression(result, unreal.MaterialExpressionCustom, 0, 0)
    shader.set_editor_property('description', 'Soft three-tone water and travelling foam')
    shader.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    pins = []
    for pin_name in ('UV', 'Travel', 'Soft', 'Shadow', 'Middle', 'Crest'):
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', pin_name)
        pins.append(pin)
    shader.set_editor_property('inputs', pins)

    def wire(node, pin, output=''):
        if not EDIT.connect_material_expressions(node, output, shader, pin):
            raise RuntimeError('Water material could not connect ' + pin)

    wire(EDIT.create_material_expression(result, unreal.MaterialExpressionTextureCoordinate, -520, -140), 'UV')
    for index, (name, pin, default) in enumerate((('FlowMeters', 'Travel', 0.), ('ToneSoftness', 'Soft', .11))):
        node = EDIT.create_material_expression(result, unreal.MaterialExpressionScalarParameter, -520, index * 110)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('default_value', default)
        wire(node, pin)
    for index, (name, pin, color) in enumerate((('ShadowColor', 'Shadow', (.018, .18, .48)),
                                                ('MidColor', 'Middle', (.04, .64, .94)),
                                                ('LightColor', 'Crest', (.83, .98, 1.25)))):
        node = EDIT.create_material_expression(result, unreal.MaterialExpressionVectorParameter, -520, 240 + 180 * index)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('default_value', unreal.LinearColor(*color, 1.))
        wire(node, pin, 'RGB')
    if not EDIT.connect_material_property(shader, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Missing water material output')
    set_flow_surface(result, shader)
    return result


def siren():
    name = 'SW_Car09_FireHiLo'
    result = existing(FX + '/' + name)
    if result:
        return result
    # An original two-note mechanical horn. Integer cycles and a smooth glide give a seamless mono loop.
    sample_rate, length = 24000, 2
    samples = array.array('h')
    phase = 0.
    for i in range(sample_rate * length):
        t = i / sample_rate
        lift = .5 + .5 * math.tanh(7. * math.sin(math.tau * t / length))
        phase += math.tau * (340. + 120. * lift) / sample_rate
        pressure = .9 + .1 * math.cos(math.tau * 4. * t)
        horn = math.sin(phase) + .3 * math.sin(2 * phase) + .23 * math.sin(3 * phase)
        samples.append(round(12500 * pressure * horn))
    path = ROOT / 'Saved/WaterCannon/Source' / (name + '.wav')
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), 'wb') as output:
        output.setparams((1, 2, sample_rate, 0, 'NONE', 'not compressed'))
        output.writeframes(samples.tobytes())
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(path))
    task.set_editor_property('destination_path', FX)
    task.set_editor_property('automated', True)
    TOOLS.import_asset_tasks([task])
    result = unreal.load_asset(FX + '/' + name)
    if not isinstance(result, unreal.SoundWave):
        raise RuntimeError('Fire siren import failed')
    result.set_editor_property('looping', True)
    save(result)
    return result


def main():
    mat, sound = material(), siren()
    config = existing(SKILL)
    if not config:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('DataAssetClass', unreal.HALWaterCannonConfig)
        package, name = SKILL.rsplit('/', 1)
        config = TOOLS.create_asset(name, package, unreal.HALWaterCannonConfig, factory)
        config.set_editor_property('water_material', mat)
        config.set_editor_property('siren', sound)
        save(config)
    vehicle = unreal.load_asset(CAR)
    slot = vehicle.get_editor_property('abilities')
    grants = slot.get_editor_property('grants')
    ability = unreal.HALWaterCannonAbility.static_class()
    if grants:
        if len(grants) != 1 or grants[0].get_editor_property('ability') != ability or grants[0].get_editor_property('skill_config') != config:
            raise RuntimeError('Car09 already has a different configured primary')
    else:
        model = unreal.load_asset('/Game/Data/MVP_B/DA_Vehicle_Car_03').get_editor_property('abilities')
        grant = unreal.HALAbilityGrant()
        # Grant members are EditDefaultsOnly. Build the complete value before assigning the DA's array.
        primary = model.get_editor_property('grants')[0].get_editor_property('input_tag').export_text()
        grant.import_text(f'(Ability="{ability.get_path_name()}",InputTag={primary},Level=1,SkillConfig="{config.get_path_name()}")')
        if (grant.get_editor_property('ability') != ability or grant.get_editor_property('skill_config') != config
                or grant.get_editor_property('input_tag').export_text() != primary):
            raise RuntimeError('Unreal could not construct the native primary grant')
        slot.set_editor_property('version', model.get_editor_property('version'))
        slot.set_editor_property('grants', [grant])
        vehicle.set_editor_property('abilities', slot)
        save(vehicle)
    unreal.log('CAR09_WATER_AUTHORED ' + config.get_path_name() + ' ' + str(slot))


if '-refreshwatermaterial' in unreal.SystemLibrary.get_command_line().lower().split():
    material(refresh=True)
else:
    main()
