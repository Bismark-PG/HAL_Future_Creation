"""Run in Unreal Editor with the match open to verify the visible and playable ball envelopes."""
import unreal as u


def verify_ball(ball):
    root = ball.root_component
    shell, = [c for c in ball.get_components_by_class(u.StaticMeshComponent) if c.get_name()=='VisualMesh']
    tr = shell.get_world_transform()
    center = root.get_world_location()
    radius = root.get_scaled_sphere_radius()
    mesh = shell.static_mesh
    if mesh.get_name() != 'SM_PrimaryBall_Orbital' or shell.get_material(0).get_name() != 'MI_PrimaryBall_Orbital':
        raise RuntimeError('The match ball is using a different shell or finish')
    if shell.get_collision_enabled() != u.CollisionEnabled.NO_COLLISION or shell.get_editor_property('hidden_in_game'):
        raise RuntimeError('Check ball mesh visibility and collision')
    assert abs(radius-115.) < .01 and (tr.translation-center).length() < .05
    positions = [tr.transform_location(p)-center for section in range(mesh.get_num_sections(0))
                 for p in u.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,section)[0]]
    lengths = [p.length() for p in positions]
    if not lengths or max(abs(min(lengths)-radius),abs(max(lengths)-radius))>.03:
        raise RuntimeError('Visible surface and Chaos sphere do not agree')
    distances = []
    for axis in (u.Vector(1,0,0),u.Vector(0,1,0),u.Vector(0,0,1)):
        for side in (-1.,1.):
            hit = root.line_trace_component(center+axis*(side*(radius+100.)),center,False,False,False)
            assert hit, 'No radial collision'
            distance = (hit[0]-center).length()
            assert abs(distance-radius)<.05, 'Collision surface has moved'
            distances.append(round(distance,3))
    grazing = []
    for offset in (radius-2.,radius+2.):
        through = center+u.Vector(0,offset,0)
        hit = root.line_trace_component(through-u.Vector(2*radius,0,0),through+u.Vector(2*radius,0,0),False,False,False)
        grazing.append(hit is not None)
    assert grazing==[True,False]
    return dict(mesh=mesh.get_path_name(),material=shell.get_material(0).get_path_name(),
                physical_radius_cm=radius,visible_radii_cm=[round(min(lengths),3),round(max(lengths),3)],
                vertices=len(positions),six_ray_distances_cm=distances,grazing_113_117_cm=grazing)


def verify_match():
    level = u.get_editor_subsystem(u.LevelEditorSubsystem)
    assert level.load_level('/Game/Maps/MVPB_Match')
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    placed, = [a for a in actors.get_all_level_actors() if a.get_class().get_name()=='BP_BasicBall_C']
    report = {'kickoff':verify_ball(placed)}
    respawn = actors.spawn_actor_from_class(placed.get_class(),u.Vector(0,0,18000),transient=True)
    try:
        report['fresh_instance'] = verify_ball(respawn)
    finally:
        actors.destroy_actor(respawn)
    for kind in ('Explosive','BlackHole','Meteor'):
        da = u.load_asset('/Game/Data/Balls/DA_Ball_'+kind)
        physical = da.get_editor_property('physics').get_editor_property('scale')
        radius = da.get_editor_property('radius')*min(abs(x) for x in physical.to_tuple())
        visual = da.get_editor_property('visual')
        transform = visual.get_editor_property('relative_transform')
        mesh = visual.get_editor_property('mesh')
        points = [transform.transform_location(p)*physical for section in range(mesh.get_num_sections(0))
                  for p in u.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,section)[0]]
        assert points and max(abs(p.length()-radius) for p in points)<.05
        assert abs(report['kickoff']['physical_radius_cm']/radius-1.15)<.001
        report[kind+'_diameter_cm'] = 2.*radius
    return report


if __name__=='__main__':
    u.log('ORBITAL_ENVELOPE '+str(verify_match()))
