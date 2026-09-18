"""Read saved Unreal templates and a test map; never compile or save assets.

Run with UnrealEditor-Cmd -EnablePlugins=PythonScriptPlugin -run=pythonscript
-Script=".../Tools/InspectMVPBaseline.py --map /Game/Maps/Test --output ...json"
The built-in Python plugin is enabled for that process only, not in the project.
This is editor inspection tooling, not a gameplay dependency or asset migration.
"""

import argparse
import datetime
import hashlib
import inspect
import json
import pathlib
import re
import subprocess

import unreal


PROJECT = pathlib.Path(unreal.Paths.project_dir()).resolve()
SOURCE = PROJECT / "Source" / "HAL_Future_Creation"
ASSETS = {
    "player": "/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn",
    "ball": "/Game/Blueprints/Balls/BP_BasicBall",
    "passive": "/Game/Blueprints/Vehicles/BP_PassiveTestVehicle",
    "game_mode": "/Game/Blueprints/Gamemode/BP_MVPA_GameMode",
}
HEADERS = {
    "TestVehiclePawn": "TestVehiclePawn.h",
    "BasicBallActor": "BasicBallActor.h",
    "PassiveTestVehicle": "PassiveTestVehicle.h",
    "ArcadeVehicleMovementComponent": "ArcadeVehicleMovementComponent.h",
    "BallControlComponent": "BallControlComponent.h",
    "VehicleHealthComponent": "VehicleHealthComponent.h",
    "VehicleKnockbackSettings": "VehicleKnockbackSettings.h",
}
COMPONENT_FIELDS = {
    "player": ["CollisionRoot", "VisualMesh", "ForwardArrow", "ArcadeMovement",
               "BallControlPoint", "BallConstraint", "BallControl", "Health",
               "CameraBoom", "TopDownCamera"],
    "ball": ["PhysicsRoot", "VisualMesh"],
    "passive": ["CollisionRoot", "VisualMesh", "Health"],
}


def header_properties(filename):
    """Return UPROPERTY fields and metadata using balanced macro parentheses."""
    source = (SOURCE / filename).read_text(encoding="utf-8-sig")
    owner = pathlib.Path(filename).stem
    declaration = re.search(r"\bclass\s+HAL_FUTURE_CREATION_API\s+[UA]" + re.escape(owner) + r"\b", source)
    if declaration:
        source = source[declaration.end():]
    fields = {}
    for match in re.finditer(r"\bUPROPERTY\s*\(", source):
        start = match.end()
        position, depth = start, 1
        while position < len(source) and depth:
            if source[position] == "(":
                depth += 1
            elif source[position] == ")":
                depth -= 1
            position += 1
        declaration = source[position:source.find(";", position)]
        # Multicast delegates and pointer declarations are also reflected fields.
        field_match = re.search(r"\b(\w+)\s*(?:=[\s\S]*)?$", declaration.strip())
        if field_match:
            fields[field_match.group(1)] = source[start:position - 1]
    return fields


PROPERTIES = {name: header_properties(filename) for name, filename in HEADERS.items()}


def encode(value):
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, unreal.Object):
        return {"object_path": value.get_path_name(), "class": value.get_class().get_path_name()}
    if isinstance(value, (list, tuple, unreal.Array)):
        return [encode(item) for item in value]
    if hasattr(value, "export_text"):
        return {"struct_type": type(value).__name__, "export_text": value.export_text()}
    return {"type": type(value).__name__, "text": str(value)}


def snapshot(obj):
    result = {"path": obj.get_path_name(), "class": obj.get_class().get_path_name(),
              "properties": {}, "read_errors": {}, "native_metadata": {}}
    fields = set()
    # Python descriptors cover exposed engine and Blueprint properties.
    for name in dir(type(obj)):
        if name.startswith("_"):
            continue
        if inspect.isdatadescriptor(inspect.getattr_static(type(obj), name, None)):
            fields.add(name)
    # Explicit native names also cover private/protected editor-only fields.
    for class_name, metadata in PROPERTIES.items():
        native_type = getattr(unreal, class_name, None)
        if ((native_type is not None and isinstance(obj, native_type))
                or obj.get_class().get_name() == class_name):
            editor_metadata = {field: value for field, value in metadata.items()
                               if "Transient" not in value and "BlueprintAssignable" not in value}
            fields.update(editor_metadata)
            result["native_metadata"].update(editor_metadata)
    # Prefer the native name over its Python alias to keep JSON keys unambiguous.
    native_names = {name.replace("_", "").lower() for name in result["native_metadata"]}
    fields = {name for name in fields if name in result["native_metadata"]
              or name.replace("_", "").lower() not in native_names}
    for field in sorted(fields):
        try:
            result["properties"][field] = encode(obj.get_editor_property(field))
        except Exception as error:
            result["read_errors"][field] = str(error)
    missing_config = [field for field, metadata in result["native_metadata"].items()
                      if ("EditDefaultsOnly" in metadata or "Config," in metadata)
                      and field not in result["properties"]]
    if missing_config:
        raise RuntimeError("Cannot read required native configuration: {}: {}".format(
            obj.get_path_name(), ", ".join(missing_config)))
    return result


def snapshot_actor(obj, role):
    result = snapshot(obj)
    result["components"] = {}
    for field in COMPONENT_FIELDS.get(role, []):
        component = obj.get_editor_property(field)
        if component is None:
            raise RuntimeError("Missing required component: {}.{}".format(obj.get_path_name(), field))
        result["components"][field] = snapshot(component)
    # Include added scene components without modifying the component graph.
    for component in obj.get_components_by_class(unreal.ActorComponent):
        if all(item["path"] != component.get_path_name() for item in result["components"].values()):
            result["components"][component.get_name()] = snapshot(component)
    return result


def tracked_files():
    paths = [PROJECT / "HAL_Future_Creation.uproject"]
    for directory, suffixes in [("Source", {".h", ".cpp", ".cs"}),
                                ("Config", {".ini"}),
                                ("Content", {".uasset", ".umap"})]:
        paths.extend(path for path in (PROJECT / directory).rglob("*")
                     if path.is_file() and path.suffix in suffixes)
    return {path.relative_to(PROJECT).as_posix(): {
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "size": path.stat().st_size,
    } for path in sorted(paths)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--map", default="/Game/Maps/Test")
    parser.add_argument("--output", default=str(PROJECT / "Docs" / "Baselines" /
                        ("MVP_A_Saved_Test_" + datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + ".json")))
    args = parser.parse_args()
    output = pathlib.Path(args.output).resolve()
    if PROJECT not in output.parents or output.exists():
        raise RuntimeError("Output must be a NEW file inside the project; existing baseline is never overwritten.")
    before = tracked_files()
    report = {
        "schema_version": 1,
        "sampled_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "map": args.map,
        "sampling_scope": "saved_blueprint_cdo_and_editor_level_instances_not_PIE",
        "templates": {}, "level_actors": [],
        "limitations": ["No PIE was run; runtime effective settings and gameplay remain unverified.",
                        "Native editor metadata and Python descriptors are captured; read errors require review.",
                        "Loading a Blueprint/map can perform transient engine fixups; no compile/save API is called."],
    }
    classes = {}
    for role, asset in ASSETS.items():
        actor_class = unreal.load_class(None, asset + "." + asset.rsplit("/", 1)[1] + "_C")
        if actor_class is None:
            raise RuntimeError("Cannot load saved generated class: " + asset)
        classes[role] = actor_class
        report["templates"][role] = snapshot_actor(unreal.get_default_object(actor_class), role)
    player = unreal.get_default_object(classes["player"])
    report["input_assets"] = {}
    for field in ["DefaultMappingContext", "SteeringAction", "ThrottleAction", "BrakeAction", "HandbrakeAction", "LaunchAction"]:
        asset = player.get_editor_property(field)
        if asset is None:
            raise RuntimeError("Unassigned baseline input asset: " + field)
        report["input_assets"][field] = snapshot(asset)
    settings_class = unreal.load_class(None, "/Script/HAL_Future_Creation.VehicleKnockbackSettings")
    if settings_class is None:
        raise RuntimeError("Cannot load VehicleKnockbackSettings.")
    report["combat_settings"] = snapshot(unreal.get_default_object(settings_class))
    world = unreal.EditorLoadingAndSavingUtils.load_map(args.map)
    if world is None:
        raise RuntimeError("Cannot load test map: " + args.map)
    report["world_settings"] = snapshot(world.get_world_settings())
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    for actor in sorted(actors, key=lambda item: item.get_path_name()):
        if isinstance(actor, unreal.WorldSettings):
            report["world_settings"] = snapshot(actor)
        for role, actor_class in classes.items():
            if actor.get_class() == actor_class:
                item = snapshot_actor(actor, role)
                item["role"] = role
                report["level_actors"].append(item)
                break
    report["file_manifest"] = before
    after = tracked_files()
    report["source_config_content_unchanged"] = before == after
    if before != after:
        raise RuntimeError("Source/Config/Content/project files changed during inspection; no valid baseline is emitted.")
    for command, key in [(["git", "rev-parse", "HEAD"], "git_head"),
                         (["git", "status", "--short", "--branch"], "git_status")]:
        report[key] = subprocess.check_output(command, cwd=str(PROJECT), text=True).strip()
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")
    unreal.log("MVP_BASELINE_WRITTEN: " + str(output))


if __name__ == "__main__":
    main()
