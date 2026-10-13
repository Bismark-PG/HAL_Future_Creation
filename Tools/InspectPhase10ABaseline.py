"""Export saved MVP-B combat configuration without changing Unreal assets.

Run with UnrealEditor-Cmd -EnablePlugins=PythonScriptPlugin -run=pythonscript
-Script=".../Tools/InspectPhase10ABaseline.py --output .../Docs/Combat/MVPB_Phase10A_Baseline_20260930.json"
The Python plugin is enabled for this inspection process only.
"""

import argparse
import datetime
import hashlib
import json
import pathlib
import subprocess

import unreal


PROJECT = pathlib.Path(unreal.Paths.project_dir()).resolve()
ASSETS = {
    "player_vehicle": ("/Game/Data/MVP_B/DA_Vehicle_Player_TestBaseline_v1", [
        "bReadyForUse", "CollisionShape", "CollisionMesh", "BoxExtent",
        "Physics", "Movement", "BallControl", "Health", "LocalConfig", "Visual",
    ]),
    "passive_vehicle": ("/Game/Data/MVP_B/DA_Vehicle_Passive_TestBaseline_v1", [
        "bReadyForUse", "CollisionShape", "CollisionMesh", "BoxExtent",
        "Physics", "Health", "Visual",
    ]),
    "ball": ("/Game/Data/MVP_B/DA_Ball_Normal_TestBaseline_v1", [
        "bReadyForUse", "Radius", "Physics", "Gameplay", "Visual",
    ]),
    "combat": ("/Game/Data/MVP_B/DA_Combat_TestBaseline_v1", [
        "bReadyForUse", "BallImpacts",
    ]),
    "local": ("/Game/Data/MVP_B/DA_Local_TestBaseline_v1", [
        "bReadyForUse", "DefaultMappingContext", "SteeringAction", "ThrottleAction",
        "BrakeAction", "HandbrakeAction", "LaunchAction", "Camera",
    ]),
}
MANIFEST = [
    "HAL_Future_Creation.uproject",
    "Config/DefaultGame.ini",
    "Content/Blueprints/Vehicles/BP_RoundedVehiclePawn.uasset",
    "Content/Blueprints/Vehicles/BP_PassiveTestVehicle.uasset",
    "Content/Blueprints/Balls/BP_BasicBall.uasset",
    "Content/Blueprints/Gamemode/BP_MVPB_ListenGameMode.uasset",
    "Content/Maps/MVPB_NetEntry.umap",
    "Content/Maps/MVPB_NetTest.umap",
    "Content/Data/Network/DA_NetworkPhysics_MVPB.uasset",
] + ["Content" + asset[0][5:] + ".uasset" for asset in ASSETS.values()]


def encode(value):
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, "export_text"):
        return value.export_text()
    return str(value)


def manifest():
    result = {}
    for relative in MANIFEST:
        path = PROJECT / relative
        if not path.is_file():
            raise RuntimeError("Missing baseline file: " + relative)
        content = path.read_bytes()
        result[relative] = {
            "size": len(content),
            "sha256": hashlib.sha256(content).hexdigest(),
        }
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    output = pathlib.Path(args.output).resolve()
    if PROJECT not in output.parents or output.exists():
        raise RuntimeError("Output must be a new file inside the project.")

    before = manifest()
    report = {
        "schema_version": 1,
        "purpose": "Read-only saved-asset baseline for MVP-B Phase 10-A; not runtime validation",
        "sampled_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "git_head": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=str(PROJECT), text=True).strip(),
        "assets": {},
        "file_manifest": before,
        "limitations": [
            "Values are saved Data Asset properties, not runtime effective values or hand-feel.",
            "Blueprint class defaults, level instance overrides and live PIE state are not inspected.",
            "Struct values use Unreal export_text and retain engine-specific formatting.",
            "No asset compile or save API is called.",
        ],
    }
    for role, (path, fields) in ASSETS.items():
        asset = unreal.load_asset(path)
        if asset is None:
            raise RuntimeError("Could not load Data Asset: " + path)
        report["assets"][role] = {
            "path": asset.get_path_name(),
            "class": asset.get_class().get_path_name(),
            "properties": {field: encode(asset.get_editor_property(field)) for field in fields},
        }

    if manifest() != before:
        raise RuntimeError("Protected project/config/content files changed during inspection.")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")
    unreal.log("PHASE10A_BASELINE_WRITTEN: " + str(output))


if __name__ == "__main__":
    main()
