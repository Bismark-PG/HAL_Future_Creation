"""Sample a horizontal cut of each car's visible mesh for a cooked, low-cost pin anchor."""
import math
import unreal as u


def sample_section(mesh):
    geo = u.DynamicMesh()
    read_lod = u.GeometryScriptMeshReadLOD(lod_type=u.GeometryScriptLODType.SOURCE_MODEL, lod_index=0)
    _, outcome = u.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        mesh, geo, u.GeometryScriptCopyMeshFromAssetOptions(), read_lod)
    if outcome != u.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError('Cannot read vehicle geometry: ' + mesh.get_path_name())

    query = u.GeometryScript_MeshQueries
    bounds = query.get_mesh_bounding_box(geo)
    lo, hi = bounds.min, bounds.max
    center = u.Vector((lo.x+hi.x)*.5, (lo.y+hi.y)*.5, lo.z+(hi.z-lo.z)*.42)
    segments = []
    for i in range(geo.get_triangle_count()):
        points = [v for v in query.get_triangle_positions(geo, i) if isinstance(v, u.Vector)]
        if len(points) != 3:
            continue
        crossings = []
        for a, b in zip(points, points[1:]+points[:1]):
            if (a.z <= center.z) == (b.z <= center.z):
                continue
            t = (center.z-a.z)/(b.z-a.z)
            crossings.append((a.x+(b.x-a.x)*t-center.x, a.y+(b.y-a.y)*t-center.y))
        if len(crossings) == 2:
            segments.append(crossings)

    # Find the outer visible triangle on each ray. Wheels and armour count when they cover the shell.
    radii = []
    for i in range(96):
        angle = math.tau*i/96
        dx, dy = math.cos(angle), math.sin(angle)
        radius = 0.
        for (x, y), (ex, ey) in segments:
            vx, vy = ex-x, ey-y
            cross = dx*vy-dy*vx
            if abs(cross) < 1.e-7:
                continue
            t, along = (x*vy-y*vx)/cross, (x*dy-y*dx)/cross
            if 0 <= along <= 1 and t > radius:
                radius = t
        radii.append(round(radius, 3))
    if min(radii) <= 0:
        raise RuntimeError('Car body cut has an open sector: ' + mesh.get_path_name())
    return center, radii


def update_profiles(skill):
    assets = u.EditorAssetLibrary
    meshes = {}
    for path in assets.list_assets('/Game/Data/Car_Presentation', recursive=True, include_folder=False):
        data = u.load_asset(path)
        if isinstance(data, u.HALVehiclePresentationConfig):
            mesh = data.get_editor_property('mesh')
            if mesh:
                meshes[mesh.get_path_name()] = mesh
    if not meshes:
        raise RuntimeError('No vehicle presentation meshes found')
    profiles = list(skill.get_editor_property('pin_profiles'))
    found = {p.get_editor_property('mesh').get_path_name(): i for i, p in enumerate(profiles)
             if p.get_editor_property('mesh')}
    refresh = '-HALRefreshDartProfiles' in u.SystemLibrary.get_command_line()
    changed = False
    for path, mesh in sorted(meshes.items()):
        if path in found and not refresh:
            continue
        center, radii = sample_section(mesh)
        profile = u.HALBatarangPinProfile()
        profile.set_editor_property('mesh', mesh)
        profile.set_editor_property('center', center)
        profile.set_editor_property('radii', radii)
        if path in found:
            profiles[found[path]] = profile
        else:
            profiles.append(profile)
        changed = True
    if changed:
        skill.set_editor_property('pin_profiles', profiles)
    return changed
