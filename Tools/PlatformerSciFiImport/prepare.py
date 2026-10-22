"""Stage only the four requested folders; never modify or execute source-project scripts."""
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path('D:/Unity Shit/Game_Jam_2026_07/MAP_Making/Assets/ithappy/Platformer_14_Sci-Fi')
WORK = ROOT / 'Saved/PlatformerSciFiMigration'
STAGE = WORK / 'UnityStage'
if STAGE.exists():
    raise RuntimeError('Stage already exists; inspect it rather than overwriting')
inventory = []
for folder in ('Prefabs', 'Textures', 'Materials', 'Meshes'):
    for file in sorted((SOURCE / folder).rglob('*')):
        if not file.is_file():
            continue
        if file.suffix.lower() not in ('.meta', '.prefab', '.png', '.mat', '.fbx'):
            raise RuntimeError('Unexpected source type: ' + str(file))
        target = STAGE / 'Assets/ithappy/Platformer_14_Sci-Fi' / file.relative_to(SOURCE)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file, target)
        inventory.append({'path': str(file.relative_to(SOURCE)), 'sha256': hashlib.sha256(file.read_bytes()).hexdigest()})
(STAGE / 'Packages').mkdir(parents=True)
(STAGE / 'Packages/manifest.json').write_text(json.dumps({'dependencies': {'com.unity.modules.' + m: '1.0.0' for m in ['animation','audio','cloth','imageconversion','imgui','jsonserialize','particlesystem','physics','ui','uielements']}}))
(STAGE / 'ProjectSettings').mkdir()
(STAGE / 'ProjectSettings/ProjectVersion.txt').write_text('m_EditorVersion: 6000.4.10f1\n')
(STAGE / 'Assets/Editor').mkdir()
shutil.copy2(ROOT / 'Tools/PlatformerSciFiImport/PrefabExporter.cs', STAGE / 'Assets/Editor/PrefabExporter.cs')
(WORK / 'source_inventory.json').write_text(json.dumps(inventory, indent=2))
print('Staged', len(inventory), 'files')
