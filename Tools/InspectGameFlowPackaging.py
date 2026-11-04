"""Read-only GF packaging preflight, run with UnrealEditor-Cmd -run=pythonscript.

Checks saved defaults and package dependencies. Does not cook, compile or save assets.
The JSON report is evidence of preflight only, never of a packaged runtime test.
"""
import argparse
import hashlib
import json
import pathlib

import unreal

PROJECT = pathlib.Path(unreal.Paths.project_dir()).resolve()


def path(obj):
    return obj.get_path_name() if obj else None


def prop(obj, name):
    return obj.get_editor_property(name)


def load_class(value):
    return value if isinstance(value, unreal.Class) else unreal.load_class(None, reference(value))


def reference(value):
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    return value.export_text().strip('"') if hasattr(value, "export_text") else str(value)


def manifest():
    return {p.relative_to(PROJECT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted((PROJECT / "Content").rglob("*"))
            if p.is_file() and p.suffix in (".uasset", ".umap")}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--require-vehicle-abilities", action="store_true",
                        help="Reject catalog vehicles still using the explicit legacy PlayerState list")
    args, _ = parser.parse_known_args()
    output = pathlib.Path(args.output).resolve()
    if PROJECT not in output.parents or output.exists():
        raise RuntimeError("Use a new report path inside the project")
    before = manifest()
    errors = []

    def check(condition, message):
        if not condition:
            errors.append(message)

    maps = unreal.get_default_object(unreal.load_class(None, "/Script/EngineSettings.GameMapsSettings"))
    packaging = unreal.get_default_object(unreal.load_class(None, "/Script/DeveloperToolSettings.ProjectPackagingSettings"))
    cook_maps = sorted(str(prop(item, "FilePath")) for item in prop(packaging, "MapsToCook"))
    cook_dirs = sorted(str(prop(item, "Path")) for item in prop(packaging, "DirectoriesToAlwaysCook"))
    gi_class = load_class(prop(maps, "GameInstanceClass"))
    gi = unreal.get_default_object(gi_class)
    entry = str(prop(gi, "EntryMapPath"))
    practice = str(prop(gi, "MatchMapPath"))
    transition = reference(prop(maps, "TransitionMap"))
    report = {"kind": "saved_asset_packaging_preflight_only", "cook_maps": cook_maps,
              "always_cook_directories": cook_dirs, "game_instance": path(gi_class),
              "entry": entry, "practice": practice, "transition": transition,
              "screens": {key: path(prop(gi, key)) for key in
                          ("EntryScreenClass", "RoomScreenClass", "PauseScreenClass")},
              "rooms": [], "worlds": {}, "errors": errors}
    roots = set(cook_maps)
    roots.add(str(gi_class.get_path_name()).split(".")[0])
    if transition and transition != "None":
        check(transition.split(".")[0] in cook_maps, "Configured transition map missing from MapsToCook")
    for name in (entry, practice):
        check(name.split(".")[0] in cook_maps, "GI map missing from MapsToCook: " + name)

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    room_assets = registry.get_assets_by_path("/Game/Data/Gameflow", recursive=True)
    for item in room_assets:
        roots.add(str(item.package_name))
        asset = item.get_asset()
        if not isinstance(asset, unreal.HALRoomConfig):
            continue
        catalog = prop(asset, "Catalog")
        match = reference(prop(asset, "MatchMap"))
        match_package = match.split(".")[0]
        room = {"asset": path(asset), "profile": str(prop(prop(asset, "Rules"), "Profile")),
                "travel": bool(prop(asset, "bEnableRoundTravel")), "match": match,
                "catalog": path(catalog), "vehicles": []}
        report["rooms"].append(room)
        check(room["travel"], "Round travel disabled: " + path(asset))
        check(match_package in cook_maps, "Room match missing from MapsToCook: " + match)
        check(catalog is not None, "Missing catalog: " + path(asset))
        if not catalog:
            continue
        for vehicle in prop(catalog, "Vehicles"):
            cls = load_class(prop(vehicle, "VehicleClass"))
            check(cls is not None, "Unresolvable vehicle class")
            if not cls:
                continue
            definition = prop(unreal.get_default_object(cls), "Definition")
            check(definition is not None, "Vehicle Definition missing: " + path(cls))
            check(definition and prop(definition, "bReadyForUse"), "Vehicle Definition not ready: " + path(cls))
            record = {"id": str(prop(vehicle, "VehicleId")), "class": path(cls),
                      "definition": path(definition), "abilities": []}
            room["vehicles"].append(record)
            if definition:
                config = prop(definition, "Abilities")
                version = prop(config, "Version")
                v1 = version == unreal.HALVehicleAbilityVersion.VEHICLE_DEFINITION_V1
                check(v1 or version == unreal.HALVehicleAbilityVersion.LEGACY_PLAYER_STATE,
                      "Unknown vehicle ability version: " + path(definition))
                record["ability_version"] = str(version)
                record["ability_migration_pending"] = not v1
                if args.require_vehicle_abilities:
                    check(v1, "Vehicle ability migration pending: " + path(definition))
                grants = prop(config, "Grants")
                tags = []
                for grant in grants:
                    ga = load_class(prop(grant, "Ability"))
                    tag = str(prop(prop(grant, "InputTag"), "TagName"))
                    level = int(prop(grant, "Level"))
                    record["abilities"].append({"class": path(ga), "input": tag, "level": level})
                    tags.append(tag)
                    check(ga is not None and level >= 1, "Invalid vehicle GA/level: " + path(definition))
                check((v1 and tags == ["Input.Ability.Primary"]) or (not v1 and not tags),
                      "Vehicle needs one V1 Primary, or an empty explicit Legacy list: " + path(definition))
        check(len(room["vehicles"]) >= 2, "Both test vehicles must resolve: " + path(asset))
    check(len(report["rooms"]) >= 2, "Development and Exhibition Room DAs required")
    check("/Game/Data/Gameflow" in cook_dirs, "Both room profiles must be included as explicit cook roots")

    for name in sorted({entry, practice, *(r["match"].split(".")[0] for r in report["rooms"])}):
        world = unreal.EditorLoadingAndSavingUtils.load_map(name)
        check(world is not None, "Map cannot load: " + name)
        if not world:
            continue
        mode_class = prop(world.get_world_settings(), "DefaultGameMode")
        source = "map_override"
        if not mode_class and name == entry:
            # Entry deliberately uses the project's default mode; it is not a practice room.
            mode_class = load_class(prop(maps, "GlobalDefaultGameMode"))
            source = "project_default"
        check(mode_class is not None, "Map has no resolvable GameMode: " + name)
        record = {"mode": path(mode_class), "mode_source": source}
        report["worlds"][name] = record
        if not mode_class:
            continue
        mode = unreal.get_default_object(mode_class)
        ps = prop(mode, "PlayerStateClass")
        record["player_state"] = path(ps)
        if ps and isinstance(unreal.get_default_object(ps), unreal.HALPlayerState):
            record["abilities"] = [path(prop(grant, "Ability")) for grant in
                                   prop(unreal.get_default_object(ps), "DefaultAbilities")]
        if name != entry:
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
            slots = sorted(prop(a, "SpawnSlot") for a in actors if isinstance(a, unreal.HALPlayerStart))
            record["spawn_slots"] = slots
            check(slots == [0, 1, 2, 3], "Exactly four unique spawn slots required: " + name)

    # Follow serialized hard AND soft package references, including BP->DA->GA/GE/UI/FX.
    options = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True, include_hard_package_references=True,
        include_searchable_names=False, include_soft_management_references=False,
        include_hard_management_references=False)
    reachable = set()
    pending = list(roots)
    while pending:
        package = pending.pop()
        if package in reachable or not package.startswith("/Game/"):
            continue
        reachable.add(package)
        relative = PROJECT / "Content" / package.removeprefix("/Game/")
        check(relative.with_suffix(".uasset").exists() or relative.with_suffix(".umap").exists(),
              "Referenced project package missing: " + package)
        pending.extend(str(dep) for dep in registry.get_dependencies(package, options))
    for room in report["rooms"]:
        for vehicle in room["vehicles"]:
            for key in ("class", "definition"):
                check(vehicle[key] and vehicle[key].split(".")[0] in reachable,
                      "Vehicle resource absent from cook dependency graph: " + str(vehicle[key]))
            for grant in vehicle["abilities"]:
                package = grant["class"].split(".")[0] if grant["class"] else ""
                check(package.startswith("/Script/") or package in reachable,
                      "Vehicle GA absent from cook dependency graph: " + str(grant["class"]))
    report["project_package_dependencies"] = sorted(reachable)
    report["assets_unchanged"] = before == manifest()
    report["protected_asset_count"] = len(before)
    check(report["assets_unchanged"], "Saved assets changed during read-only inspection")
    report["passed"] = not errors
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if errors:
        raise RuntimeError("GameFlow preflight failed: " + "; ".join(errors))
    unreal.log("HAL_GF6_PACKAGING_PREFLIGHT_PASSED: " + str(output))


if __name__ == "__main__":
    main()
