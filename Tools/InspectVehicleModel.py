"""Read-only Car_01 import audit. Never saves, reimports, commits a modifier or edits a Blueprint.

Run in UE with process-only PythonScriptPlugin,GeometryScripting enabled. Geometry is
copied into a transient DynamicMesh solely for querying source vertices and weights.
"""
import argparse
import collections
import datetime
import hashlib
import json
import math
import pathlib
import subprocess

import unreal

PROJECT = pathlib.Path(unreal.Paths.project_dir()).resolve()


def manifest():
    return {p.relative_to(PROJECT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted((PROJECT / "Content").rglob("*"))
            if p.is_file() and p.suffix in (".uasset", ".umap")}


def vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def transform(value):
    q = value.rotation
    x, y, z, w = q.x, q.y, q.z, q.w
    return {"position_cm": vec(value.translation), "scale": vec(value.scale3d),
            "quaternion_xyzw": [x, y, z, w],
            "local_axes_in_mesh": {
                "X": [1-2*(y*y+z*z), 2*(x*y+w*z), 2*(x*z-w*y)],
                "Y": [2*(x*y-w*z), 1-2*(x*x+z*z), 2*(y*z+w*x)],
                "Z": [2*(x*z+w*y), 2*(y*z-w*x), 1-2*(x*x+y*y)]}}


def object_path(obj):
    return obj.get_path_name() if obj else None


def expand(bounds, point):
    for axis in range(3):
        bounds[0][axis] = min(bounds[0][axis], point[axis])
        bounds[1][axis] = max(bounds[1][axis], point[axis])


def empty_bounds():
    return [[float("inf")]*3, [float("-inf")]*3]


def source_geometry(mesh, lod_index, report):
    dynamic = unreal.DynamicMesh()
    requested = unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,
                                                lod_index=lod_index)
    result = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        mesh, dynamic, unreal.GeometryScriptCopyMeshFromAssetOptions(), requested)
    if "SUCCESS" not in str(result[-1]).upper():
        raise RuntimeError("Source geometry query failed: " + str(result))
    info_result = unreal.GeometryScript_BoneWeights.get_all_bones_info(dynamic)
    bone_info = info_result[-1]
    index_to_name = {int(b.index): str(b.name) for b in bone_info}
    max_id_method = next(n for n in dir(dynamic) if n.startswith("get_num_vertex") and "count" not in n)
    max_id = int(getattr(dynamic, max_id_method)())
    per_bone = {}
    all_bounds = empty_bounds()
    invalid_weights = multi_weights = cross_wheel = vertex_count = 0
    wheel_points = collections.defaultdict(list)
    for vertex in range(max_id):
        position, valid = dynamic.get_vertex_position(vertex)
        if not valid:
            continue
        vertex_count += 1
        point = vec(position)
        expand(all_bounds, point)
        _, weights, valid_weights = unreal.GeometryScript_BoneWeights.get_vertex_bone_weights(dynamic, vertex)
        if not valid_weights or not weights:
            invalid_weights += 1
            continue
        influences = [(index_to_name.get(int(w.bone_index), "INVALID_INDEX_"+str(w.bone_index)), float(w.weight))
                      for w in weights if float(w.weight) > 1e-6]
        if len(influences) > 1:
            multi_weights += 1
        wheel_ids = {name.split("_")[1] for name, _ in influences if name.startswith("Wheel_")}
        if len(wheel_ids) > 1:
            cross_wheel += 1
        if abs(sum(weight for _, weight in influences)-1) > 0.002:
            invalid_weights += 1
        for name, weight in influences:
            entry = per_bone.setdefault(name, {"vertices": 0, "weight_min": 1., "weight_max": 0., "bounds_cm": empty_bounds()})
            entry["vertices"] += 1
            entry["weight_min"] = min(entry["weight_min"], weight)
            entry["weight_max"] = max(entry["weight_max"], weight)
            expand(entry["bounds_cm"], point)
            if name.endswith("_spin_jnt"):
                wheel_points[name].append(point)
    radii = {}
    for name, points in wheel_points.items():
        center = report["bones"][name]["global"]["position_cm"]
        distances = [math.hypot(p[0]-center[0], p[2]-center[2]) for p in points]
        bounds = per_bone[name]["bounds_cm"]
        radii[name] = {"max_radius_in_mesh_xz_cm": max(distances),
                       "half_x_span_cm": (bounds[1][0]-bounds[0][0])/2,
                       "half_z_span_cm": (bounds[1][2]-bounds[0][2])/2,
                       "bounds_center_cm": [(a+b)/2 for a,b in zip(*bounds)],
                       "note": "Geometry estimate at mesh scale 1; world RadiusCm must include final component scale."}
    return {"source_vertex_count": vertex_count, "source_triangle_count": int(dynamic.get_triangle_count()),
            "bounds_cm": all_bounds, "size_cm": [b-a for a,b in zip(*all_bounds)],
            "invalid_or_unnormalized_weight_vertices": invalid_weights,
            "multi_influence_vertices": multi_weights, "cross_wheel_weight_vertices": cross_wheel,
            "per_bone_weights": per_bone, "wheel_geometry": radii}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args, _ = parser.parse_known_args()
    output = pathlib.Path(args.output).resolve()
    if PROJECT not in output.parents or output.exists():
        raise RuntimeError("Output must be a NEW file inside the project")
    before = manifest()
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    packages = sorted(str(data.package_name) for data in registry.get_assets_by_path("/Game/Assets", recursive=True))
    report = {"schema_version": 1, "sampled_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "client_date": "2026-10-20", "engine": unreal.SystemLibrary.get_engine_version(),
              "git_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=PROJECT, text=True).strip(),
              "scope": "Read saved import assets and transient copied LOD source geometry. No compile/save/reimport/PIE.",
              "inspector_sha256": hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
              "assets": {}, "missing_dependencies": [], "bones": {}, "lods": [], "errors": []}
    for package in packages:
        asset = unreal.load_asset(package)
        if not asset:
            raise RuntimeError("Cannot load imported asset: " + package)
        entry = {"class": asset.get_class().get_path_name()}
        dependencies = registry.get_dependencies(package, unreal.AssetRegistryDependencyOptions())
        entry["package_dependencies"] = sorted(str(p) for p in dependencies)
        for dependency in dependencies:
            dependency = str(dependency)
            if dependency.startswith("/") and not dependency.startswith("/Script/") and not unreal.EditorAssetLibrary.does_asset_exist(dependency):
                report["missing_dependencies"].append({"source": package, "dependency": dependency})
        if isinstance(asset, unreal.MaterialInstanceConstant):
            entry["parent"] = object_path(asset.get_editor_property("parent"))
            entry["parent_asset_exists"] = bool(entry["parent"] and unreal.EditorAssetLibrary.does_asset_exist(entry["parent"]))
            entry["texture_overrides"] = [{"parameter": str(v.get_editor_property("parameter_info").get_editor_property("name")),
                                           "texture": object_path(v.get_editor_property("parameter_value"))}
                                          for v in asset.get_editor_property("texture_parameter_values")]
        if isinstance(asset, unreal.Texture2D):
            entry["srgb"] = bool(asset.get_editor_property("srgb"))
            entry["compression"] = str(asset.get_editor_property("compression_settings"))
        report["assets"][package] = entry
    mesh = unreal.load_asset("/Game/Assets/Cars/SKM_Car_01")
    report["mesh"] = object_path(mesh)
    report["skeleton"] = object_path(mesh.get_editor_property("skeleton"))
    report["physics_asset"] = object_path(mesh.get_editor_property("physics_asset"))
    report["material_slots"] = [{"slot": str(slot.get_editor_property("material_slot_name")),
                                  "material": object_path(slot.get_editor_property("material_interface"))}
                                 for slot in mesh.get_editor_property("materials")]
    imported = mesh.get_editor_property("asset_import_data")
    report["import_data"] = {"class": imported.get_class().get_path_name(), "first_filename": imported.get_first_filename(),
                             "source_file_exists_on_this_device": pathlib.Path(imported.get_first_filename()).is_file()}
    modifier = unreal.SkeletonModifier()
    if not modifier.set_skeletal_mesh(mesh):
        raise RuntimeError("Cannot read the reference skeleton copy")
    for bone in modifier.get_all_bone_names():
        report["bones"][str(bone)] = {"parent": str(modifier.get_parent_name(bone)),
                                     "local": transform(modifier.get_bone_transform(bone, False)),
                                     "global": transform(modifier.get_bone_transform(bone, True))}
    expected_parents = {"Root_jnt": "None", "Chassis_jnt": "Root_jnt"}
    for slot in ("FL", "FR", "BL", "BR"):
        expected_parents.update({f"Wheel_{slot}_suspension_jnt": "Root_jnt",
                                f"Wheel_{slot}_steer_jnt": f"Wheel_{slot}_suspension_jnt",
                                f"Wheel_{slot}_spin_jnt": f"Wheel_{slot}_steer_jnt"})
    report["rig_contract"] = {"method": "Compare actual bone names/direct parents against existing native ValidateRig contract; native method is not Python-exposed.",
                               "mismatches": [{"bone": name, "expected_parent": parent, "actual": report["bones"].get(name)}
                                              for name, parent in expected_parents.items()
                                              if report["bones"].get(name, {}).get("parent") != parent]}
    report["axis_candidates"] = {}
    for slot in ("FL", "FR", "BL", "BR"):
        steer_axes = report["bones"][f"Wheel_{slot}_steer_jnt"]["global"]["local_axes_in_mesh"]
        spin_axes = report["bones"][f"Wheel_{slot}_spin_jnt"]["global"]["local_axes_in_mesh"]
        steer_axis = max(steer_axes, key=lambda axis: abs(steer_axes[axis][2]))
        spin_axis = max(spin_axes, key=lambda axis: abs(spin_axes[axis][1]))
        report["axis_candidates"][slot] = {"steering_axis": steer_axis, "steering_sign": 1 if steer_axes[steer_axis][2] > 0 else -1,
                                              "steering_up_alignment": abs(steer_axes[steer_axis][2]),
                                              "spin_axis": spin_axis, "spin_sign": 1 if spin_axes[spin_axis][1] > 0 else -1,
                                              "spin_right_alignment": abs(spin_axes[spin_axis][1]),
                                              "basis": "Mesh +X forward/+Y right/+Z up; final sign and pose still require Editor single-axis preview."}
    editor = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    for index in range(editor.get_lod_count(mesh)):
        report["lods"].append({"index": index, "render_vertices": editor.get_num_verts(mesh, index),
                               "render_sections": editor.get_num_sections(mesh, index),
                               "source_geometry": source_geometry(mesh, index, report)})
    report["presentation_assets"] = []
    for data in registry.get_assets_by_path("/Game", recursive=True):
        kind = str(data.asset_class_path.asset_name)
        if kind in ("AnimBlueprint", "HALVehiclePresentationConfig"):
            report["presentation_assets"].append({"package": str(data.package_name), "class": kind})
    definition = unreal.load_asset("/Game/Data/MVP_B/DA_Vehicle_Player_TestBaseline_v1")
    report["player_definition_presentation"] = object_path(definition.get_editor_property("presentation")) if definition else None
    after = manifest()
    changed = [p for p in set(before) | set(after) if before.get(p) != after.get(p)]
    report["asset_protection"] = {"before_count": len(before), "after_count": len(after), "changed_paths": sorted(changed), "sha256": after}
    if changed:
        raise RuntimeError("Asset files changed during inspection: " + str(changed))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")
    unreal.log("CAR01_INSPECTION_COMPLETE " + str(output))


if __name__ == "__main__":
    main()
