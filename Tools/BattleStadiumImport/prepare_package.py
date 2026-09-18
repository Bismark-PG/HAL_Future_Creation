"""Prepare an isolated Unity project without executing the asset vendor's scripts."""
import io
import gzip
import json
import pathlib
import shutil
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
WORK = ROOT / 'Saved/BattleStadiumMigration'
STAGE = WORK / 'UnityStage'
PACKAGE = pathlib.Path('C:/Users/USER/Downloads/Battle Stadium - Low Poly 3D Models Pack 2.0.unitypackage')
ALLOWED = {'.fbx', '.prefab', '.unity', '.mat', '.png', '.hdr', '.asset', '.anim', '.controller', '.physicmaterial', '.shader', '.shadergraph', '.shadersubgraph'}

def unpack(fileobj):
    with tarfile.open(fileobj=io.BytesIO(gzip.decompress(fileobj.read())), mode='r:') as archive:
        members = {m.name: m for m in archive.getmembers()}
        for name, member in members.items():
            if not name.endswith('/pathname'):
                continue
            rel = archive.extractfile(member).read().decode('utf-8-sig').splitlines()[0].strip('\x00')
            path = pathlib.PurePosixPath(rel)
            if path.is_absolute() or '..' in path.parts:
                raise ValueError(rel)
            if path.parts[0] != 'Assets':
                continue
            guid_dir = name.rsplit('/', 1)[0]
            if path.name == 'Unity_2021_Built-In_source.unitypackage':
                nested.append(archive.extractfile(members[guid_dir + '/asset']).read())
            if path.suffix.lower() not in ALLOWED:
                continue
            target = STAGE.joinpath(*path.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            for source_suffix, output in [('asset', target), ('asset.meta', pathlib.Path(str(target) + '.meta'))]:
                entry = members.get(guid_dir + '/' + source_suffix)
                if entry and entry.isfile():
                    output.write_bytes(archive.extractfile(entry).read())

if __name__ == '__main__':
    STAGE.mkdir(parents=True, exist_ok=True)
    nested = []
    with PACKAGE.open('rb') as handle:
        unpack(handle)
    for data in list(nested):
        unpack(io.BytesIO(data))
    (STAGE / 'Packages').mkdir(exist_ok=True)
    modules = ['animation', 'audio', 'cloth', 'director', 'imageconversion', 'imgui', 'jsonserialize', 'particlesystem', 'physics', 'terrain', 'terrainphysics', 'ui', 'uielements', 'umbra', 'vehicles', 'video']
    (STAGE / 'Packages/manifest.json').write_text(json.dumps({'dependencies': {'com.unity.modules.' + m: '1.0.0' for m in modules}}, indent=2))
    (STAGE / 'ProjectSettings').mkdir(exist_ok=True)
    (STAGE / 'ProjectSettings/ProjectVersion.txt').write_text('m_EditorVersion: 6000.4.10f1\n')
    editor = STAGE / 'Assets/Editor'
    editor.mkdir(exist_ok=True)
    shutil.copy2(ROOT / 'Tools/BattleStadiumImport/StadiumExporter.cs', editor / 'StadiumExporter.cs')
    print('Prepared isolated project:', STAGE)
