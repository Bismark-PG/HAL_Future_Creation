"""Read saved vehicle/contact settings and map primitives. Never edits or saves assets.

Enable PythonScriptPlugin for this command-line process only; output must be a new
JSON file inside the project. Material priority is inferred from saved properties;
the native solver diagnostic independently queries the actual runtime material.
"""
import argparse
import datetime
import hashlib
import json
import pathlib
import subprocess

import unreal

PROJECT = pathlib.Path(unreal.Paths.project_dir()).resolve()


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


def props(obj, names):
    return {name: encode(obj.get_editor_property(name)) for name in names}


def optional(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None


def material(mat):
    return {"path": mat.get_path_name(), "values": props(mat, (
        "Friction", "StaticFriction", "Restitution", "FrictionCombineMode",
        "RestitutionCombineMode", "bOverrideFrictionCombineMode",
        "bOverrideRestitutionCombineMode", "Density"))} if mat else None


def body(instance):
    return {"export_text": instance.export_text(), "values": props(instance, (
        "bSimulatePhysics", "bEnableGravity", "bUseCCD", "bOverrideMass",
        "MassInKgOverride", "LinearDamping", "AngularDamping", "COMNudge",
        "InertiaTensorScale", "PhysMaterialOverride", "bOverrideIterationCounts",
        "PositionSolverIterationCount", "VelocitySolverIterationCount",
        "bOverrideMaxDepenetrationVelocity", "MaxDepenetrationVelocity")),
        "material_override": material(instance.get_editor_property("PhysMaterialOverride"))}


def mesh_data(mesh):
    if not mesh:
        return None
    setup = mesh.get_editor_property("BodySetup")
    agg = setup.get_editor_property("AggGeom") if setup else None
    result = {"path": mesh.get_path_name(), "bounds_cm": encode(mesh.get_bounding_box()),
              "body_setup": encode(setup), "simple_material": material(optional(setup, "PhysMaterial")) if setup else None,
              "collision_trace_flag": encode(optional(setup, "CollisionTraceFlag")) if setup else None}
    if agg:
        result["geometry_counts"] = {name: len(agg.get_editor_property(name))
                                     for name in ("SphereElems", "BoxElems", "SphylElems", "ConvexElems")}
        result["convexes"] = []
        for elem in agg.get_editor_property("ConvexElems"):
            points = optional(elem, "VertexData")
            bounds = None
            if points:
                bounds = [[min(float(getattr(p, axis)) for p in points) for axis in ("x", "y", "z")],
                          [max(float(getattr(p, axis)) for p in points) for axis in ("x", "y", "z")]]
            result["convexes"].append({"vertex_count": len(points) if points is not None else None,
                                       "bounds_cm": bounds, "export_text": elem.export_text(),
                                       "vertex_query_requires_native_probe": points is None})
    return result


def component(comp):
    instance = comp.get_editor_property("BodyInstance")
    mesh = comp.get_editor_property("StaticMesh") if isinstance(comp, unreal.StaticMeshComponent) else None
    slots = []
    for index in range(comp.get_num_materials()):
        slot = comp.get_material(index)
        physical = slot.get_physical_material() if slot else None
        slots.append({"slot": index, "material": encode(slot), "physical_material": material(physical)})
    data = {"path": comp.get_path_name(), "class": comp.get_class().get_path_name(),
            "mobility": encode(comp.get_editor_property("Mobility")),
            "relative_transform": props(comp, ("RelativeLocation", "RelativeRotation", "RelativeScale3D")),
            "collision_enabled": str(comp.get_collision_enabled()),
            "collision_profile": str(comp.get_collision_profile_name()),
            "body": body(instance), "mesh": mesh_data(mesh), "material_slots": slots}
    return data


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args, _ = parser.parse_known_args()
    output = pathlib.Path(args.output).resolve()
    if PROJECT not in output.parents or output.exists():
        raise RuntimeError("Output must be a new file inside this project")
    before = manifest()
    report = {"schema_version": 1, "sampled_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "engine_version": unreal.SystemLibrary.get_engine_version(),
              "git_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=PROJECT, text=True).strip(),
              "scope": "saved_DA_CDO_maps_no_PIE_no_save_no_explicit_compile",
              "script_sha256": hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
              "assets": {}, "maps": {}}
    for name in ("DA_Vehicle_Player_TestBaseline_v1", "DA_Vehicle_Passive_TestBaseline_v1", "DA_Ball_Normal_TestBaseline_v1"):
        obj = unreal.load_asset("/Game/Data/MVP_B/" + name)
        if not obj:
            raise RuntimeError("Missing asset: " + name)
        fields = ("bReadyForUse", "Physics") + (("Radius", "Gameplay") if "Ball_" in name else
                  ("CollisionShape", "CollisionMesh", "BoxExtent", "Movement", "BallControlPoint", "Combat", "Impact", "Presentation"))
        if "Passive" in name:
            fields = tuple(f for f in fields if f != "Presentation")
        data = {"path": obj.get_path_name(), "properties": props(obj, fields),
                "body": body(obj.get_editor_property("Physics").get_editor_property("Body"))}
        if "Ball_" not in name:
            data["collision_mesh"] = mesh_data(obj.get_editor_property("CollisionMesh"))
        if "Player" in name:
            presentation = obj.get_editor_property("Presentation")
            if presentation:
                data["presentation"] = props(presentation, ("Mesh", "AnimClass", "RelativeTransform", "Wheels"))
        report["assets"][name] = data
    combat = unreal.load_asset("/Game/Data/MVP_B/DA_Combat_TestBaseline_v1")
    report["combat_rules"] = props(combat, ("bReadyForUse", "BallImpacts", "CollisionImpacts"))
    report["engine_default_material"] = material(unreal.load_asset("/Engine/EngineMaterials/DefaultPhysicalMaterial"))
    report["physics_settings"] = props(unreal.get_default_object(unreal.PhysicsSettings), (
        "bTickPhysicsAsync", "AsyncFixedTimeStepSize", "bSubstepping", "BounceThresholdVelocity",
        "DefaultGravityZ", "MaxDepenetrationVelocity", "RestitutionCombineMode", "FrictionCombineMode"))
    for map_name in ("Test", "MVPB_NetTest"):
        world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/" + map_name)
        if not world:
            raise RuntimeError("Cannot load map: " + map_name)
        mode = world.get_world_settings().get_editor_property("DefaultGameMode")
        mode_cdo = unreal.get_default_object(mode)
        pawn_class = mode_cdo.get_editor_property("DefaultPawnClass")
        pawn_cdo = unreal.get_default_object(pawn_class)
        actors = []
        for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
            entries = [component(comp) for comp in actor.get_components_by_class(unreal.PrimitiveComponent)
                       if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION]
            if not entries:
                continue
            actors.append({"path": actor.get_path_name(), "label": actor.get_actor_label(),
                           "class": actor.get_class().get_path_name(), "transform": encode(actor.get_actor_transform()),
                           "components": entries})
        report["maps"][map_name] = {"game_mode": encode(mode), "pawn_class": encode(pawn_class),
                                   "pawn_configuration": props(pawn_cdo, ("ConfigurationSource", "Definition")),
                                   "collidable_actors": sorted(actors, key=lambda entry: entry["path"])}
    after = manifest()
    if before != after:
        raise RuntimeError("Asset content changed during read-only inspection")
    report["asset_manifest"] = before
    report["protected_asset_count"] = len(before)
    report["assets_unchanged"] = True
    report["limitations"] = ["Saved properties and loaded Editor map components only; no gameplay runtime was sampled.",
                              "Native solver diagnostic must independently validate effective material and contact response.",
                              "Physics material values are independent of combat PhysicalSupplement restitution budget."]
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")
    unreal.log("VEHICLE_STABILITY_BASELINE_WRITTEN: " + str(output))


if __name__ == "__main__":
    main()
