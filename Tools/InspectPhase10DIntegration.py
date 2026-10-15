"""Read saved 10-D migration data and tuned fields. Never compile or save assets."""
import argparse
import hashlib
import json
import pathlib
import unreal

PROJECT = pathlib.Path(unreal.Paths.project_dir()).resolve()


def manifest():
    return {str(p.relative_to(PROJECT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted((PROJECT / "Content").rglob("*"))
            if p.is_file() and p.suffix in (".uasset", ".umap")}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args, _ = parser.parse_known_args()
    before = manifest()
    report = {"engine_version": unreal.SystemLibrary.get_engine_version(),
              "scope": "saved_assets_only_no_compile_no_save", "assets": {}}
    fields = {
        "DA_Vehicle_Player_TestBaseline_v1": ("impact", "combat", "physics", "movement", "ball_control", "health"),
        "DA_Vehicle_Passive_TestBaseline_v1": ("impact", "combat", "physics", "health"),
        "DA_Ball_Normal_TestBaseline_v1": ("gameplay", "physics"),
        "DA_Combat_TestBaseline_v1": ("collision_impacts", "ball_impacts"),
    }
    for name, properties in fields.items():
        asset = unreal.load_asset("/Game/Data/MVP_B/" + name)
        if not asset:
            raise RuntimeError("Missing saved asset: " + name)
        report["assets"][name] = {prop: asset.get_editor_property(prop).export_text() for prop in properties}
    after = manifest()
    if before != after:
        raise RuntimeError("Asset bytes changed during read-only inspection")
    report["protected_asset_count"] = len(before)
    report["assets_unchanged"] = True
    report["limitations"] = ["Default-looking fields with Version=Legacy are NOT a completed migration.",
                             "Loading data is not a physics or network acceptance test."]
    path = pathlib.Path(args.output)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    unreal.log("PHASE10D_SAVED_CONFIG_WRITTEN: " + str(path))


if __name__ == "__main__":
    main()
