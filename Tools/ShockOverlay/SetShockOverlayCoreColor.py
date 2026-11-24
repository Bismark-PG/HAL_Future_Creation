"""Set only the CoreColor default of M_FX_VehicleShock_Overlay, leaving the rest of the graph as it is.

The full graph is owned by Tools/UnrealBridge/build_shock_overlay_material.py (a local, git-ignored folder); this narrow, idempotent step exists so a colour change
does not have to rebuild a graph that may have been tuned since. Keep CORE in step with that script's PARAMS.
Run headless with the editor closed:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<abs path>/Tools/ShockOverlay/SetShockOverlayCoreColor.py" -AllowCommandletRendering
Writes Saved/ShockOverlay/CoreColor.json.
"""
import json
import pathlib
import unreal

PATH = '/Game/Assets/FX/Materials/M_FX_VehicleShock_Overlay'
CORE = unreal.LinearColor(0.96, 0.70, 1.0, 1.0)  # pale shiso purple: the filaments are near white-hot
MEL = unreal.MaterialEditingLibrary

material = unreal.load_asset(PATH)
if material is None:
    raise RuntimeError('Missing ' + PATH)
found = [item for item in MEL.get_material_expressions(material)
         if isinstance(item, unreal.MaterialExpressionVectorParameter) and str(item.get_editor_property('parameter_name')) == 'CoreColor']
if not found:
    raise RuntimeError('No CoreColor parameter in ' + PATH)
before = found[0].get_editor_property('default_value')
# The graph reads the parameter through more than one node of the same name: they must all agree.
for item in found:
    item.set_editor_property('default_value', CORE)
MEL.recompile_material(material)
if not unreal.EditorAssetLibrary.save_asset(PATH):
    raise RuntimeError('Could not save ' + PATH)
after = MEL.get_material_default_vector_parameter_value(material, 'CoreColor')
out = pathlib.Path(unreal.Paths.project_dir()).resolve() / 'Saved/ShockOverlay'
out.mkdir(parents=True, exist_ok=True)
(out / 'CoreColor.json').write_text(json.dumps({'before': [before.r, before.g, before.b], 'after': [after.r, after.g, after.b]}))
