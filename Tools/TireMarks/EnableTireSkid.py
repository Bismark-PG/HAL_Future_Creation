"""Point every car's presentation Data Asset at NS_VehicleTireSkid and switch it on.

Run via the bridge, with the editor open:
    python Tools/UnrealBridge/ue_remote.py exec Tools/TireMarks/EnableTireSkid.py

Idempotent: it writes the same values every time and saves only the assets that
actually changed, so re-running after a manual tweak in the editor puts the
configuration back to this file's values.

Only the tire-skid block of each asset is touched; every other presentation
setting is left exactly as the team left it.

SurfaceNormalParameter is deliberately set: the C++ only pushes the ground normal
when this name is non-empty, and without it the ribbon keeps facing world up and
sinks into the floor on any slope.

MaximumFadeSeconds has to stay above the Niagara particle lifetime (4s), or the
component is torn down while marks are still visible and they vanish mid-fade.
"""

import unreal

SYSTEM_PATH = "/Game/Assets/FX/NS_VehicleTireSkid"

CONFIGS = [
    "/Game/Data/Car_Presentation/DA_Car_0%d_Presentation" % index for index in range(1, 8)
]

# Rear wheels only: four smoking wheels reads as a bug, not a drift.
WHEELS = [unreal.HALVehicleWheel.BL, unreal.HALVehicleWheel.BR]

SETTINGS = {
    "enabled": True,
    "wheels": WHEELS,
    "slip_ratio": 1.0,
    "surface_normal_parameter": "User.SurfaceNormal",
    "maximum_fade_seconds": 5.0,
    "maximum_segment_distance_cm": 200.0,
    "surface_offset_cm": 1.0,
    "maximum_ground_gap_cm": 8.0,
}

# One mark colour per car, authored as sRGB hex because that is what a colour picker
# shows; converted to linear below because FLinearColor and Niagara expect linear.
#
# Each is the car's dominant body colour, measured from a top-down render of its mesh
# (share of saturated pixels per hue), then brightened: the renders are dimly lit and
# an unlit mark has to read on a bright arena floor. All seven cars share one palette
# atlas and take their colours from UVs, so the texture itself cannot answer this.
#
# Two are judgement calls rather than measurements:
#   Car_02 is split three ways (crimson 35%, teal 34%, violet 30%). Crimson wins
#          narrowly and is the only one not already taken by Car_04 or Car_05/06.
#   Car_06 is a white car; only 5% of it is coloured at all. White marks vanish on a
#          light floor, so it takes its navy stripe colour instead.
MARK_COLORS = {
    1: "#EA1E8C",  # hot pink
    2: "#C4122F",  # crimson
    3: "#F2B705",  # amber yellow
    4: "#10A698",  # teal
    5: "#1E9BE8",  # sky blue
    6: "#24439A",  # navy
    7: "#F0561A",  # orange
}
MARK_OPACITY = 1.0


def linear_from_hex(text, alpha):
    def channel(value):
        value /= 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    r, g, b = (int(text[i:i + 2], 16) for i in (1, 3, 5))
    return unreal.LinearColor(channel(r), channel(g), channel(b), alpha)


# Wheel radius corrections, in cm, measured as the height of each wheel's spin bone
# above the ground plane in the mesh's reference pose.
#
# Every car was authored with RadiusCm 47.5, which is right for six of them. Car_05 is
# the one car with larger rear wheels: its rear spin bones sit 55.2 cm up. A skid only
# emits while |wheel-centre height - RadiusCm| stays within MaximumGroundGapCm (8), so
# Car_05 started 7.7 cm into that budget and any body lean in a drift pushed it over.
# The wheel then counts as airborne and neither marks nor smoke appear. The same value
# drives visual wheel spin, so correcting it also makes the big wheels roll at the
# right rate.
WHEEL_RADIUS_CM = {
    5: {
        unreal.HALVehicleWheel.FL: 48.7,
        unreal.HALVehicleWheel.FR: 48.7,
        unreal.HALVehicleWheel.BL: 55.2,
        unreal.HALVehicleWheel.BR: 55.2,
    },
}

EAL = unreal.EditorAssetLibrary

system = EAL.load_asset(SYSTEM_PATH)
if not system:
    raise RuntimeError("missing Niagara system: " + SYSTEM_PATH)

for index, path in enumerate(CONFIGS, start=1):
    asset = EAL.load_asset(path)
    if not asset:
        print("skip (missing) %s" % path)
        continue
    skid = asset.get_editor_property("tire_skid")
    skid.set_editor_property("system", system)
    for key, value in SETTINGS.items():
        skid.set_editor_property(key, value)
    skid.set_editor_property("color_parameter", "User.MarkColor")
    skid.set_editor_property("mark_color", linear_from_hex(MARK_COLORS[index], MARK_OPACITY))
    asset.set_editor_property("tire_skid", skid)
    if index in WHEEL_RADIUS_CM:
        # Iterating an unreal.Array of structs yields copies, so edits made in place
        # are silently dropped. Collect the edited copies and write the list back.
        wheels = []
        for wheel in asset.get_editor_property("wheels"):
            radius = WHEEL_RADIUS_CM[index].get(wheel.get_editor_property("wheel"))
            if radius is not None:
                wheel.set_editor_property("radius_cm", radius)
            wheels.append(wheel)
        asset.set_editor_property("wheels", wheels)
    EAL.save_asset(path)
    print("enabled tire skid on %s" % path)

print("done: %d presentation assets" % len(CONFIGS))
