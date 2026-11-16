"""Import the generated tire-mark mask and author M_FX_TireMark_Ribbon.

Run via the bridge, with the editor open:
    python Tools/UnrealBridge/ue_remote.py exec Tools/TireMarks/BuildTireMarkAssets.py

Idempotent: re-running replaces the texture in place and rebuilds the material graph
from scratch, so this file stays the single source of truth for both assets.

The mask comes from Tools/TireMarks/GenerateTireMarks.py. It is a long strip that
tiles along X, which is the travel direction, so in the ribbon it maps:

    TexCoord0.x  along the ribbon   -> tiled by TileAlong
    TexCoord0.y  across the ribbon  -> 0..1, untiled

Unlit translucent. UE composites unlit translucency as

    final = Emissive * Opacity + Background * (1 - Opacity)

so the mask belongs on Opacity alone and the colour on Emissive; multiplying the
mask into Emissive as well would darken the mark twice. Unlit is deliberate: a
coloured mark that identifies a car has to read the same in shadow as in the open,
which a Base Color mark does not.

Per-car colour arrives as Particle Color from the emitter, so one material and one
texture serve all four cars with no material instance per vehicle.
"""

import unreal

TEXTURE_FOLDER = "/Game/Assets/FX/Textures"
TEXTURE_NAME = "T_TireMark_Spray_Worn_Mask"
TEXTURE_PATH = TEXTURE_FOLDER + "/" + TEXTURE_NAME

MATERIAL_FOLDER = "/Game/Assets/FX/Materials"
MATERIAL_NAME = "M_FX_TireMark_Ribbon"
MATERIAL_PATH = MATERIAL_FOLDER + "/" + MATERIAL_NAME

SOURCE_PNG = r"C:\UEProjects\HAL_Future_Creation\SourceArt\TireMarks\T_TireMark_Spray_Worn_Mask.png"

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

# Keep TileAlong at 1. The ribbon renderer already repeats the strip, through
# UV0Settings.TilingLength (world cm per repeat), so anything other than 1 here
# multiplies on top of that and squashes the tread along the direction of travel:
# at TileAlong 6 against a 136 cm renderer tiling the strip repeated every ~23 cm
# and the lug rows collapsed into fine cross-hatching. Change the scale on the
# renderer, not here.
SCALARS = {
    "TileAlong": (1.0, "Pattern", 0),
    "Strength": (0.85, "Pattern", 1),
    "Brightness": (1.0, "Colour", 0),
}

_failures = []


def wire(src, src_out, dst, dst_inputs):
    names = (dst_inputs,) if isinstance(dst_inputs, str) else dst_inputs
    for name in names:
        if MEL.connect_material_expressions(src, src_out, dst, name):
            return True
    _failures.append("%s -> %s %s" % (src.get_name(), dst.get_name(), names))
    return False


def expr(material, cls, x, y, **props):
    node = MEL.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        node.set_editor_property(key, value)
    return node


def scalar(material, name, x, y):
    default, group, priority = SCALARS[name]
    return expr(material, unreal.MaterialExpressionScalarParameter, x, y,
                parameter_name=name, default_value=default, group=group, sort_priority=priority)


def mul(material, x, y, a, b):
    node = expr(material, unreal.MaterialExpressionMultiply, x, y)
    wire(a, "", node, ("A", ""))
    wire(b, "", node, "B")
    return node


def import_mask():
    # Re-importing over a texture the running editor already has open fails, so only
    # import when it is genuinely missing. The settings below are reapplied either
    # way, which is what makes this script safe to re-run.
    texture = EAL.load_asset(TEXTURE_PATH)
    if not isinstance(texture, unreal.Texture2D):
        task = unreal.AssetImportTask()
        task.filename = SOURCE_PNG
        task.destination_path = TEXTURE_FOLDER
        task.destination_name = TEXTURE_NAME
        task.automated = True
        task.replace_existing = True
        task.save = True
        TOOLS.import_asset_tasks([task])
        texture = EAL.load_asset(TEXTURE_PATH)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("tire mark mask did not import: " + TEXTURE_PATH)

    # A mask is data, not a picture: linear, single channel, and it must tile along X.
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    EAL.save_asset(TEXTURE_PATH)
    return texture


def build_material(texture):
    # Rebuild from nothing. MaterialEditingLibrary.delete_all_material_expressions
    # only removes part of the graph and leaves orphans behind, and Material.Expressions
    # is protected so Python cannot walk the graph to finish the job. Re-running then
    # appends a second copy of every node; lookup is by parameter name, so the stale
    # node keeps winning and new defaults silently never apply. Deleting the asset is
    # the only deterministic clear. The Niagara renderer references this by path and
    # re-resolves on load, but a live editor session is left holding the old object, so
    # after running this re-assign the material on NS_VehicleTireSkid's ribbon renderer.
    if EAL.does_asset_exist(MATERIAL_PATH):
        EAL.delete_asset(MATERIAL_PATH)
    material = TOOLS.create_asset(
        MATERIAL_NAME, MATERIAL_FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    for usage in ("used_with_niagara_ribbons", "used_with_particle_sprites"):
        try:
            material.set_editor_property(usage, True)
        except Exception as error:
            print("note: could not preset %s (%s)" % (usage, error))

    # Tile along the ribbon only. Scaling V as well would squash the tread across
    # the contact patch and the lug rows would stop reading as a tire.
    uv = expr(material, unreal.MaterialExpressionTextureCoordinate, -1400, 0, coordinate_index=0)
    along = expr(material, unreal.MaterialExpressionComponentMask, -1200, -80,
                 r=True, g=False, b=False, a=False)
    wire(uv, "", along, ("Input", ""))
    across = expr(material, unreal.MaterialExpressionComponentMask, -1200, 120,
                  r=False, g=True, b=False, a=False)
    wire(uv, "", across, ("Input", ""))

    tiled = mul(material, -1000, -80, along, scalar(material, "TileAlong", -1200, -280))
    coords = expr(material, unreal.MaterialExpressionAppendVector, -820, 0)
    wire(tiled, "", coords, ("A", ""))
    wire(across, "", coords, "B")

    sample = expr(material, unreal.MaterialExpressionTextureSample, -620, 0, texture=texture)
    try:
        sample.set_editor_property(
            "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    except Exception as error:
        print("note: could not preset sampler_type (%s)" % error)
    wire(coords, "", sample, ("Coordinates", "UVs", ""))

    particle = expr(material, unreal.MaterialExpressionParticleColor, -620, 420)

    # Opacity = mask * particle alpha * Strength. Particle alpha is where the
    # emitter's own fade-out lives, so the mark ages for free. Take the node's own
    # A pin: masking .a out of its 3-component RGB pin reads as zero and the whole
    # ribbon disappears.
    masked = expr(material, unreal.MaterialExpressionMultiply, -240, 180)
    wire(sample, "", masked, ("A", ""))
    wire(particle, "A", masked, "B")
    opacity = mul(material, -40, 180, masked, scalar(material, "Strength", -240, 400))
    if not MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY):
        _failures.append("opacity -> MP_OPACITY")

    emissive = mul(material, -40, -200, particle, scalar(material, "Brightness", -240, -340))
    if not MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        _failures.append("emissive -> MP_EMISSIVE_COLOR")

    if hasattr(MEL, "layout_material_expressions"):
        MEL.layout_material_expressions(material)
    MEL.recompile_material(material)
    EAL.save_asset(MATERIAL_PATH)
    return material


texture = import_mask()
print("imported %s  %dx%d" % (TEXTURE_PATH, texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
material = build_material(texture)

# delete_all_material_expressions has been seen to silently leave the old graph in
# place, which appends a second copy of every node. Lookup is by parameter name, so
# the stale node keeps winning and the new defaults never take effect while the
# material still compiles and renders. Read the defaults back and fail loudly.
for name, (expected, _group, _priority) in SCALARS.items():
    actual = MEL.get_material_default_scalar_parameter_value(material, name)
    if abs(actual - expected) > 1e-4:
        raise RuntimeError(
            "%s default is %r, expected %r: the previous graph was not cleared, so a "
            "stale duplicate node is shadowing this parameter." % (name, actual, expected))

stats = MEL.get_statistics(material)
print("built %s" % MATERIAL_PATH)
print("pixel instructions: %d" % stats.num_pixel_shader_instructions)
print("parameters: %s" % sorted(SCALARS))
print("failures: %d" % len(_failures))
for failure in _failures:
    print("  FAILED %s" % failure)
