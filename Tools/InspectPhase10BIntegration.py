"""Read saved GameMode/Pawn/map bindings after the GAS build; never compile or save assets."""

import argparse
import hashlib
import json
import pathlib

import unreal


PROJECT = pathlib.Path(unreal.Paths.project_dir()).resolve()


def path_of(value):
    return value.get_path_name() if value else None


def protected_files():
    paths = list((PROJECT / "Content").rglob("*.uasset")) + list((PROJECT / "Content").rglob("*.umap"))
    return {path.relative_to(PROJECT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    output = pathlib.Path(args.output).resolve()
    if PROJECT not in output.parents or output.exists():
        raise RuntimeError("Output must be a new file inside this project.")
    before = protected_files()
    report = {"schema_version": 1, "engine_version": unreal.SystemLibrary.get_engine_version(),
              "scope": "read_only_saved_editor_bindings_not_gameplay_acceptance", "maps": {}}
    for name in ["Test", "MVPB_NetTest"]:
        world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/" + name)
        mode_class = world.get_world_settings().get_editor_property("DefaultGameMode")
        mode = unreal.get_default_object(mode_class) if mode_class else None
        pawn_class = mode.get_editor_property("DefaultPawnClass") if mode else None
        pawn = unreal.get_default_object(pawn_class) if pawn_class else None
        item = {"game_mode": path_of(mode_class),
                "player_state_class": path_of(mode.get_editor_property("PlayerStateClass")) if mode else None,
                "default_pawn_class": path_of(pawn_class)}
        if pawn and isinstance(pawn, unreal.TestVehiclePawn):
            item["vehicle_configuration_source"] = str(pawn.get_editor_property("ConfigurationSource"))
            item["vehicle_definition"] = path_of(pawn.get_editor_property("Definition"))
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        item["passive_test_vehicle_count"] = sum(isinstance(actor, unreal.PassiveTestVehicle) for actor in actors)
        report["maps"][name] = item
    after = protected_files()
    if before != after:
        raise RuntimeError("Project assets changed during read-only inspection.")
    report["protected_asset_file_count"] = len(before)
    report["assets_unchanged"] = True
    report["limitations"] = ["No asset compile/save API, PIE or runtime gameplay test was used.",
                             "GameMode class defaults do not prove every runtime or level instance override."]
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    unreal.log("PHASE10B_BINDINGS_WRITTEN: " + str(output))


if __name__ == "__main__":
    main()
