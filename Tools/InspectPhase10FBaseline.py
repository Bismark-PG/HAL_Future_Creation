"""Read saved 10-F-1 DA/class/map/network baselines. No compile, save or PIE API."""
import argparse
import datetime
import hashlib
import json
import pathlib
import subprocess

import unreal

PROJECT = pathlib.Path(unreal.Paths.project_dir()).resolve()
FIELDS = {
    "DA_Vehicle_Player_TestBaseline_v1": ("bReadyForUse", "CollisionShape", "CollisionMesh", "BoxExtent", "Physics", "Movement", "BallControl", "BallControlPoint", "Health", "Combat", "Impact", "LocalConfig", "Visual"),
    "DA_Vehicle_Passive_TestBaseline_v1": ("bReadyForUse", "CollisionShape", "BoxExtent", "Physics", "Movement", "BallControl", "Health", "Combat", "Impact", "Visual"),
    "DA_Ball_Normal_TestBaseline_v1": ("bReadyForUse", "Radius", "Physics", "Gameplay", "Visual"),
    "DA_Combat_TestBaseline_v1": ("bReadyForUse", "BallImpacts", "CollisionImpacts"),
    "DA_Local_TestBaseline_v1": ("bReadyForUse", "DefaultMappingContext", "SteeringAction", "ThrottleAction", "BrakeAction", "HandbrakeAction", "LaunchAction", "PrimarySkillAction", "Camera"),
}


def manifest():
    return {p.relative_to(PROJECT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted((PROJECT / "Content").rglob("*"))
            if p.is_file() and p.suffix in (".uasset", ".umap")}


def encode(value):
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, (list, tuple, unreal.Array)):
        return [encode(item) for item in value]
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, "export_text"):
        return value.export_text()
    return str(value)


def properties(obj, fields):
    return {field: encode(obj.get_editor_property(field)) for field in fields}


def class_defaults(path, fields):
    cls = unreal.load_class(None, path)
    if not cls:
        raise RuntimeError("Missing saved class: " + path)
    return {"class": cls.get_path_name(), "properties": properties(unreal.get_default_object(cls), fields)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args, _ = parser.parse_known_args()
    output = pathlib.Path(args.output).resolve()
    if PROJECT not in output.parents or output.exists():
        raise RuntimeError("Output must be a new file within this project")
    before = manifest()
    report = {
        "schema_version": 1,
        "sampled_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "git_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=PROJECT, text=True).strip(),
        "scope": "saved_DA_CDO_map_bindings_no_PIE_no_compile_no_save",
        "assets": {}, "classes": {}, "maps": {},
    }
    for name, fields in FIELDS.items():
        asset = unreal.load_asset("/Game/Data/MVP_B/" + name)
        if not asset:
            raise RuntimeError("Missing saved DA: " + name)
        report["assets"][name] = {"path": asset.get_path_name(), "properties": properties(asset, fields)}
    network = unreal.load_asset("/Game/Data/Network/DA_NetworkPhysics_MVPB")
    if not network:
        raise RuntimeError("Missing vehicle network physics DA")
    report["assets"]["DA_NetworkPhysics_MVPB"] = properties(network, ("Settings",))
    component_settings = network.get_editor_property("Settings").get_editor_property("NetworkPhysicsComponentSettings")
    report["network_component_overrides"] = properties(component_settings, (
        "bOverrideCompareStateToTriggerRewind", "bCompareStateToTriggerRewind",
        "bOverrideCompareInputToTriggerRewind", "bCompareInputToTriggerRewind",
        "bOverrideEnableReliableFlow", "bEnableReliableFlow",
        "bOverrideRedundantStates", "RedundantStates"))
    report["physics_settings"] = properties(unreal.get_default_object(unreal.PhysicsSettings), (
        "bTickPhysicsAsync", "AsyncFixedTimeStepSize", "PhysicsPrediction"))
    class_specs = {
        "player": ("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C", ("ConfigurationSource", "Definition")),
        "passive": ("/Game/Blueprints/Vehicles/BP_PassiveTestVehicle.BP_PassiveTestVehicle_C", ("ConfigurationSource", "Definition")),
        "ball": ("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C", ("ConfigurationSource", "Definition")),
        "player_state": ("/Game/Blueprints/PlayerState/BP_HALPlayerState.BP_HALPlayerState_C", ("DefaultAbilities",)),
        "immunity_GA": ("/Game/Blueprints/GA/GA_ImpactImmunity.GA_ImpactImmunity_C", ("ImmunityEffectClass", "CooldownGameplayEffectClass")),
        "immunity_GE": ("/Game/Blueprints/GE/GE_ImpactImmunity.GE_ImpactImmunity_C", ("DurationPolicy", "DurationMagnitude")),
        "cooldown_GE": ("/Game/Blueprints/GE/GE_ImpactImmunityCooldown.GE_ImpactImmunityCooldown_C", ("DurationPolicy", "DurationMagnitude")),
    }
    for name, (path, fields) in class_specs.items():
        report["classes"][name] = class_defaults(path, fields)
    for map_name in ("Test", "MVPB_NetTest"):
        world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/" + map_name)
        if not world:
            raise RuntimeError("Missing saved map: " + map_name)
        mode = world.get_world_settings().get_editor_property("DefaultGameMode")
        mode_cdo = unreal.get_default_object(mode)
        pawn_cls = mode_cdo.get_editor_property("DefaultPawnClass")
        pawn_cdo = unreal.get_default_object(pawn_cls)
        settings = pawn_cdo.get_component_by_class(unreal.NetworkPhysicsSettingsComponent)
        actors = []
        for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
            if isinstance(actor, (unreal.TestVehiclePawn, unreal.PassiveTestVehicle, unreal.BasicBallActor)):
                actors.append({"path": actor.get_path_name(), "class": actor.get_class().get_path_name(),
                               "configuration": properties(actor, ("ConfigurationSource", "Definition"))})
        report["maps"][map_name] = {
            "game_mode": mode.get_path_name(),
            "game_mode_bindings": properties(mode_cdo, ("PlayerStateClass", "DefaultPawnClass")),
            "spawned_pawn_defaults": properties(pawn_cdo, ("ConfigurationSource", "Definition")),
            "pawn_network_settings": properties(settings, ("SettingsDataAsset",)) if settings else None,
            "placed_gameplay_actors": sorted(actors, key=lambda item: item["path"]),
        }
    report["engine_console_defaults"] = {
        name: unreal.SystemLibrary.get_console_variable_int_value(name)
        for name in ("np2.Resim.RecordStatePostSolve", "np2.Resim.CompareStateToTriggerRewind",
                     "np2.Resim.EnableReliableFlow", "np2.Resim.RedundantStates")
    }
    after = manifest()
    if before != after:
        raise RuntimeError("Asset bytes changed during read-only inspection")
    report["asset_manifest"] = before
    report["protected_asset_count"] = len(before)
    report["assets_unchanged"] = True
    report["limitations"] = [
        "Saved assets, loaded class defaults and editor-level references only; no PIE or runtime instance was started.",
        "DA struct text includes disabled fields: override flags decide whether network settings take effect.",
        "Inactive passive movement fields are recorded for comparison, not evidence they are consumed.",
        "No movement AttributeSet/GE/bridge exists at this baseline; neutral future multipliers are a contract, not a runtime sample.",
    ]
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")
    unreal.log("PHASE10F1_BASELINE_WRITTEN: " + str(output))


if __name__ == "__main__":
    main()
