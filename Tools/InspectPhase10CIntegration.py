"""Read saved 10-C DA migration versions and GameMode bindings; never compile or save assets."""
import argparse
import hashlib
import json
import pathlib

import unreal

PROJECT = pathlib.Path(unreal.Paths.project_dir()).resolve()


def manifest():
    return {str(p.relative_to(PROJECT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted((PROJECT / "Content").rglob("*")) if p.is_file()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args, _ = parser.parse_known_args()
    before = manifest()
    report = {"engine_version": unreal.SystemLibrary.get_engine_version(),
              "scope": "saved_assets_only_no_PIE_no_compile_no_save", "assets": {}, "maps": {}}
    for name in ("DA_Vehicle_Player_TestBaseline_v1", "DA_Vehicle_Passive_TestBaseline_v1", "DA_Ball_Normal_TestBaseline_v1"):
        asset = unreal.load_asset("/Game/Data/MVP_B/" + name)
        if not asset:
            raise RuntimeError("Missing saved asset: " + name)
        config = (asset.get_editor_property("gameplay").get_editor_property("damage")
                  if name.startswith("DA_Ball") else asset.get_editor_property("combat"))
        report["assets"][name] = {"version": str(config.get_editor_property("version")),
                                  "configuration": config.export_text()}
    for name in ("Test", "MVPB_NetTest"):
        world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/" + name)
        game_mode = world.get_world_settings().get_editor_property("default_game_mode")
        defaults = unreal.get_default_object(game_mode)
        report["maps"][name] = {"game_mode": game_mode.get_path_name(),
                                "player_state_class": defaults.get_editor_property("player_state_class").get_path_name(),
                                "default_pawn_class": defaults.get_editor_property("default_pawn_class").get_path_name()}
    after = manifest()
    if before != after:
        raise RuntimeError("Asset files changed during read-only inspection")
    report["protected_asset_file_count"] = len(before)
    report["assets_unchanged"] = True
    report["limitations"] = ["Saved migration fields are not a gameplay or network acceptance test.",
                             "Legacy fields showing native seed values do not mean assets are migrated."]
    output = pathlib.Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
    unreal.log("PHASE10C_BINDINGS_WRITTEN: " + str(output))


if __name__ == "__main__":
    main()
