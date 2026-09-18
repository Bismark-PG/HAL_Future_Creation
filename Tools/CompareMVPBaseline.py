"""Compare saved native tuning and migrated component settings; no Unreal API/write.

Run with a normal Python runtime: CompareMVPBaseline.py before.json after.json.
JSON on stdout can be archived separately; this does not verify PIE or hand feel.
"""
import argparse
import hashlib
import json
import pathlib


COMPONENT_FIELDS = {
    "body_instance", "static_mesh", "relative_location", "relative_rotation",
    "relative_scale3d", "box_extent", "sphere_radius", "generate_overlap_events",
    "hidden_in_game", "visible", "cast_shadow", "overlay_material",
    "arrow_color", "arrow_length", "arrow_size", "absolute_location",
    "absolute_rotation", "absolute_scale", "target_arm_length", "socket_offset",
    "target_offset", "do_collision_test", "probe_size", "probe_channel",
    "use_pawn_control_rotation", "inherit_pitch", "inherit_yaw", "inherit_roll",
    "enable_camera_lag", "enable_camera_rotation_lag", "camera_lag_speed",
    "camera_rotation_lag_speed", "camera_lag_max_distance", "use_camera_lag_substepping",
    "camera_lag_max_time_step", "clamp_to_max_physics_delta_time", "draw_debug_lag_markers",
    "field_of_view", "aspect_ratio", "constrain_aspect_ratio", "post_process_settings",
    "post_process_blend_weight", "projection_mode",
}


def compare(before, after, project):
    differences = []
    native_count = 0
    component_count = 0

    def compare_actor(old, new, label):
        nonlocal native_count, component_count
        objects = [("actor", old, new)]
        for name, obj in old["components"].items():
            objects.append((name, obj, new["components"][name]))
        for owner, a, b in objects:
            for field, metadata in a["native_metadata"].items():
                if "EditDefaultsOnly" in metadata:
                    native_count += 1
                    if a["properties"][field] != b["properties"].get(field):
                        differences.append(f"{label}.{owner}.{field}")
            if owner != "actor":
                for field in COMPONENT_FIELDS & a["properties"].keys():
                    component_count += 1
                    if a["properties"][field] != b["properties"].get(field):
                        differences.append(f"{label}.{owner}.{field}")

    for role, actor in before["templates"].items():
        compare_actor(actor, after["templates"][role], "template." + role)
    level = {actor["path"]: actor for actor in after["level_actors"]}
    for actor in before["level_actors"]:
        if actor["path"] not in level:
            differences.append("missing level actor " + actor["path"])
        else:
            compare_actor(actor, level[actor["path"]], actor["path"])
    for field, value in before["combat_settings"]["properties"].items():
        if value != after["combat_settings"]["properties"].get(field):
            differences.append("combat_settings." + field)
    for field, obj in before["input_assets"].items():
        if obj["properties"] != after["input_assets"][field]["properties"]:
            differences.append("input_asset." + field)

    protected = {key for key in before["file_manifest"]
                 if key.startswith(("Content/", "Config/")) or key.endswith(".uproject")}
    changed_files = [key for key in sorted(protected) if not (project / key).is_file()
                     or hashlib.sha256((project / key).read_bytes()).hexdigest()
                     != before["file_manifest"][key]["sha256"]]
    new_protected = [key for key in after["file_manifest"] if key not in protected
                     and (key.startswith(("Content/", "Config/")) or key.endswith(".uproject"))]
    return {
        "schema_version": 1,
        "scope": "saved_templates_level_instances_input_and_global_rules_not_PIE",
        "native_field_comparisons": native_count,
        "component_field_comparisons": component_count,
        "configuration_differences": sorted(differences),
        "protected_file_count": len(protected),
        "modified_protected_files": changed_files,
        "added_protected_files": new_protected,
        "new_snapshot_internal_unchanged": after["source_config_content_unchanged"],
        "definition_sources": {role: actor["properties"].get("ConfigurationSource")
                               for role, actor in after["templates"].items() if role != "game_mode"},
        "limitations": ["Does not compare every engine property, graph or runtime instance.",
                        "Override material slots require a manual Editor check.",
                        "No PIE hand-feel acceptance is implied."],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=pathlib.Path)
    parser.add_argument("after", type=pathlib.Path)
    args = parser.parse_args()
    result = compare(json.loads(args.before.read_text(encoding="utf-8")),
                     json.loads(args.after.read_text(encoding="utf-8")),
                     pathlib.Path(__file__).resolve().parents[1])
    result["before_snapshot"] = str(args.before)
    result["after_snapshot"] = str(args.after)
    print(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False))
    if result["configuration_differences"] or result["modified_protected_files"] or result["added_protected_files"]:
        raise SystemExit(1)
