"""D3D12 two-player PIE on MVPB_Match. Test setup moves actors only in the transient play world."""
import json
import time
import traceback
from pathlib import Path
import unreal as u

lagged = '-ClaspLag' in u.SystemLibrary.get_command_line()
check_expiry = '-ClaspExpiry' in u.SystemLibrary.get_command_line()
view_name = 'MatchLife' if check_expiry else ('MatchLag' if lagged else 'Match')
out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / 'TimedBomb' / view_name
out.mkdir(parents=True, exist_ok=True)
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
game = u.GameplayStatics
vec = u.Vector
# Southeast quadrant, outside the opening vortex and spring-fist lanes.
lane = vec(3000.,-2850.,-200.274)
cam = actors.spawn_actor_from_class(u.CameraActor,lane+vec(-100,-430,1350),u.Rotator())
cam.set_actor_rotation(u.MathLibrary.find_look_at_rotation(cam.get_actor_location(),lane),False)
cam.camera_component.set_field_of_view(58.)
cam.set_editor_property('tags',['ClaspQA'])
capture_template=actors.spawn_actor_from_class(u.SceneCapture2D,cam.get_actor_location(),cam.get_actor_rotation())
capture_template.set_editor_property('tags',['ClaspQAImage'])
capture_template.get_component_by_class(u.SceneCaptureComponent2D).set_editor_property('capture_every_frame',False)
stage = 'begin'
server = client = bomb = None
run_start = time.monotonic()
entered = 0.
result = {'phase': [], 'jaw_spread_cm': {'server':[], 'client':[]}}
impact_origins = {}
cameras = []
capture_actor = render_target = None


def now():
    return game.get_time_seconds(server) if server else 0.


def advance(next_stage):
    global stage, entered
    stage, entered = next_stage, now()
    result['phase'].append([stage,entered])
    u.log('CLASP_QA '+stage)


def cmd(world, text):
    u.SystemLibrary.execute_console_command(world,text,game.get_player_controller(world,0))


def all_of(world, cls):
    return list(game.get_all_actors_of_class(world,cls))


def psid(car):
    return car.player_state.player_id if car and car.player_state else None


def healths(world):
    return {str(psid(c)): c.get_component_by_class(u.VehicleHealthComponent).get_current_hp()
            for c in all_of(world,u.TestVehiclePawn) if psid(c) is not None}


def sample_blast_motion(kind, targets):
    """Measure chassis displacement on both peers against their own settled pre-fuse poses."""
    time_from_fuse = now() - deadline
    if time_from_fuse < -.5: return
    readings = result.setdefault('blast_motion', {}).setdefault(kind, {})
    for side, world in (('server', server), ('client', client)):
        for pawn in all_of(world, u.TestVehiclePawn):
            pid = psid(pawn)
            if pid not in targets: continue
            key = (kind, side, pid)
            body = pawn.root_component
            position = body.get_world_location()
            if time_from_fuse < 0.:
                impact_origins[key] = position
                continue
            delta = position - impact_origins[key]
            away = targets[pid]
            sample = readings.setdefault(side, {}).setdefault(str(pid), {'away_cm': 0., 'rise_cm': 0., 'up_cm_s': 0.})
            sample['away_cm'] = max(sample['away_cm'], round(delta.x * away.x + delta.y * away.y, 1))
            sample['rise_cm'] = max(sample['rise_cm'], round(delta.z, 1))
            sample['up_cm_s'] = max(sample['up_cm_s'], round(body.get_physics_linear_velocity().z, 1))


def check_blast_motion(kind, target_count):
    for side in ('server', 'client'):
        samples = result['blast_motion'][kind][side]
        assert len(samples) == target_count, (kind, side, samples)
        for sample in samples.values():
            assert sample['away_cm'] > 25. and sample['rise_cm'] > 15., (kind, side, sample)


def place(actor, where, yaw=0.):
    actor.set_actor_location_and_rotation(where,u.Rotator(yaw=yaw),False,True)
    actor.root_component.set_physics_linear_velocity(vec())
    actor.root_component.set_physics_angular_velocity_in_degrees(vec())


def spawn_test_ball(pawn, command, cls):
    # Reserve a clear patch above the lane for the normal debug spawn placement check.
    previous = all_of(server, cls)
    start = pawn.get_actor_location()
    place(pawn, lane+vec(-650,-450,480))
    cmd(server,command)
    place(pawn,start)
    return next(b for b in all_of(server,cls) if b not in previous)


def capture(name):
    # Read the remote player's scene explicitly. With two PIE viewports the global
    # 'shot' command can capture the editor's overhead viewport instead.
    u.RenderingLibrary.export_render_target(client,render_target,str(out),name+'.png')


def make_peer_capture():
    global capture_actor,render_target
    capture_actor=game.get_all_actors_of_class_with_tag(client,u.SceneCapture2D,'ClaspQAImage')[0]
    component=capture_actor.get_component_by_class(u.SceneCaptureComponent2D)
    render_target=u.RenderingLibrary.create_render_target2d(client,960,640,u.TextureRenderTargetFormat.RTF_RGBA8)
    component.set_editor_property('capture_source',u.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    component.set_editor_property('texture_target',render_target)
    component.set_editor_property('capture_on_movement',False)
    component.set_editor_property('capture_every_frame',True)


def visible_clasp_effect(world,near=None):
    for cue in all_of(world,u.HALSpecialBallCue):
        if near is not None and (cue.get_actor_location()-near).length()>350.: continue
        mesh=cue.get_component_by_class(u.DynamicMeshComponent)
        mat=mesh.get_material(0) if mesh else None
        if mat and 'TimedBomb_Pulse' in mat.get_name() and mesh.get_dynamic_mesh().get_triangle_count()>0:
            return True
    return False


def frame(point, offset, fov):
    eye = point + offset
    angle = u.MathLibrary.find_look_at_rotation(eye,point)
    for camera in cameras:
        camera.set_actor_location_and_rotation(eye,angle,False,True)
        camera.camera_component.set_field_of_view(fov)
    capture_actor.set_actor_location_and_rotation(eye,angle,False,True)
    capture_actor.get_component_by_class(u.SceneCaptureComponent2D).set_editor_property('fov_angle',fov)


def bomb_on_peer():
    return next((b for b in all_of(client,u.HALTimedBombBall) if abs(b.get_fuse_deadline()-deadline)<.001),None)


def jaw_gap(ball):
    jaw = next(c for c in ball.get_components_by_class(u.StaticMeshComponent) if c.get_name()=='LeftMagnet')
    return abs(jaw.get_editor_property('relative_location').y)


def finish(error=None):
    result['complete'] = error is None
    result['error'] = error
    if error: u.log_error(error)
    (out/'Result.json').write_text(json.dumps(result,indent=2))
    u.unregister_slate_post_tick_callback(handle)
    if level.is_in_play_in_editor(): level.editor_request_end_play()
    u.SystemLibrary.quit_editor()


def tick(_):
    global server,client,car,other,primary,second,bomb,deadline,first_hp,second_hp,origin,shot_blast,transfer_seen
    global blast_directions
    try:
        if time.monotonic()-run_start>175 or (server and now()-entered>27.):
            state = [(a.get_name(),str(a.get_actor_location())) for a in all_of(server,u.BasicBallActor)+all_of(server,u.TestVehiclePawn)] if server else []
            raise RuntimeError('Timeout '+stage+' hp='+str(healths(server) if server else {})+' actors='+str(state))
        if stage=='begin':
            level.editor_request_begin_play()
            advance('connect')
            return
        if stage=='connect':
            worlds=u.EditorLevelLibrary.get_pie_worlds(False)
            server=next((w for w in worlds if game.get_game_mode(w)),None)
            client=next((w for w in worlds if not game.get_game_mode(w)),None)
            if not server or not client or len(healths(client))!=2: return
            if lagged:
                cmd(server,'NetEmulation.PktLag 80')
            cmd(server,'HALStartMatch')
            advance('playing')
            return
        age=now()-entered
        if stage=='playing':
            if game.get_game_state(server).get_editor_property('match_phase')!=u.HALMatchPhase.PLAYING: return
            car=game.get_player_pawn(server,0)
            other=next(c for c in all_of(server,u.TestVehiclePawn) if psid(c) is not None and c!=car)
            first_hp=healths(server)
            for w in (server,client):
                cmd(w,'DisableAllScreenMessages')
                cmd(w,'ShowHUD')
                cmd(w,'t.MaxFPS 60')
                ctrl=game.get_player_controller(w,0)
                c=game.get_all_actors_of_class_with_tag(w,u.CameraActor,'ClaspQA')[0]
                ctrl.set_view_target_with_blend(c,0.)
                cameras.append(c)
            make_peer_capture()
            # One ordinary primary and one attack ball are already controlled before the tag.
            place(car,lane+vec(-520,0,100))
            place(other,lane+vec(485,0,100))
            primary=next(b for b in all_of(server,u.BasicBallActor) if b.get_class().get_name()=='BP_BasicBall_C')
            place(primary,car.get_actor_location()+vec(230,0,12))
            second=spawn_test_ball(car,'HALSpawnExplosiveBall',u.HALExplosiveBall)
            place(second,other.get_actor_location()+vec(230,0,12))
            bomb=spawn_test_ball(car,'HALSpawnTimedBombBall',u.HALTimedBombBall)
            place(bomb,lane+vec(-1500,-950,103))
            advance('fill_slots')
            return
        if stage=='fill_slots':
            if primary.get_controlled_by()!=car or second.get_controlled_by()!=other: return
            place(bomb,lane+vec(-700,-820,103))
            origin=bomb.get_actor_location()
            advance('patrol')
            return
        if stage=='patrol':
            frame(bomb.get_actor_location(),vec(-80,-480,700),43.)
            if age<.9: return
            distance=(bomb.get_actor_location()-origin).length()
            assert distance>110. and bomb.get_fuse_deadline()<0., distance
            result['patrol_cm']=distance
            capture('Patrol')
            advance('patrol_photo')
            return
        if stage=='patrol_photo' and age>.18:
            # Into the side panel so a normal nose pickup could not explain the result.
            place(bomb,car.get_actor_location()+vec(0,-128,2))
            advance('first_contact')
            return
        if stage=='first_contact':
            if bomb.get_controlled_by()!=car: return
            deadline=bomb.get_fuse_deadline()
            peer=bomb_on_peer()
            if not peer or psid(peer.get_controlled_by())!=psid(car): return
            gameplay = bomb.get_editor_property('definition').get_editor_property('gameplay')
            impact = gameplay.get_editor_property('impact')
            result['impact_config'] = {'tier': str(impact.get_editor_property('tier')),
                                       'push_preset': str(impact.get_editor_property('push_preset')),
                                       'damage': gameplay.get_editor_property('damage').get_editor_property('vehicle_hit_damage')}
            assert not primary.get_controlled_by() and deadline>now()
            result['deadline']=deadline
            cmd(server,'Input.+action IA_VehicleLaunch true')
            advance('locked')
            return
        if stage=='locked' and age>.22:
            assert bomb.get_controlled_by()==car and bomb.get_fuse_deadline()==deadline
            cmd(server,'Input.-action IA_VehicleLaunch')
            place(primary,lane+vec(-1300,-900,120))
            # Side pickup can twist the chassis. Start the deliberate ram from a straight lane.
            place(car, lane+vec(-530,0,112))
            place(other, lane+vec(380,0,112))
            cmd(server,'Input.+action IA_VehicleThrottle 1.0')
            transfer_seen=None
            advance('ram')
            return
        if stage=='ram':
            frame((car.get_actor_location()+other.get_actor_location())*.5+car.get_actor_forward_vector()*120.,
                  vec(-35,-330,1390),57.)
            samples=result.setdefault('approach',[])
            if age <= 3. and (not samples or age-samples[-1][0]>.10):
                p,q=car.get_actor_location(),other.get_actor_location()
                v=car.get_velocity()
                samples.append([round(x,2) for x in (age,p.x,p.y,p.z,q.x,q.y,q.z,v.x,v.y,car.get_actor_rotation().yaw)])
            peer=bomb_on_peer()
            if bomb.get_controlled_by()==other:
                result['jaw_spread_cm']['server'].append(jaw_gap(bomb))
            if peer and psid(peer.get_controlled_by())==psid(other):
                if transfer_seen is None: transfer_seen=now()
                result['jaw_spread_cm']['client'].append(jaw_gap(peer))
                age_on_peer=now()-transfer_seen
                views=result.setdefault('handoff_frames',[])
                times=(.02,.11,.20,.30,.40)
                if len(views)<len(times) and age_on_peer>=times[len(views)]:
                    capture('Handoff%02d'%len(views))
                    views.append(age_on_peer)
                if now()-transfer_seen>.55:
                    cmd(server,'Input.-action IA_VehicleThrottle')
                    assert not second.get_controlled_by() and bomb.get_fuse_deadline()==deadline
                    result['ram_peer_deadline']=peer.get_fuse_deadline()
                    capture('Mounted')
                    # Give the holder room to recoil while the attached car and ball settle together.
                    place(car,lane+vec(-1100,0,100))
                    blast_directions = {}
                    shot_blast=False
                    advance('mounted_fuse')
            return
        if stage=='mounted_fuse':
            if now()<deadline:
                assert bomb.get_controlled_by()==other, 'Clasp detached before the mounted fuse expired'
                nose = other.get_actor_forward_vector()
                backwards = vec(-nose.x,-nose.y,0.)
                blast_directions[psid(other)] = backwards / backwards.length()
            sample_blast_motion('mounted', blast_directions)
            frame(other.get_actor_location()+other.get_actor_forward_vector()*130.,vec(-80,-390,880),50.)
            if now()>deadline-1.4 and 'urgent_view' not in result:
                capture('Urgent')
                result['urgent_view']=True
            if now()>deadline+.11 and not shot_blast:
                if not all(visible_clasp_effect(w) for w in (server,client)): return
                capture('MountedBurst')
                result['mounted_cue_on_both']=True
                shot_blast=True
            expected=first_hp.copy()
            expected[str(psid(other))]-=result['impact_config']['damage']
            if now()<deadline+.8 or not shot_blast or healths(server)!=expected or healths(client)!=expected: return
            check_blast_motion('mounted', 1)
            result['mounted_hp']=[healths(server),healths(client)]
            for side,samples in result['jaw_spread_cm'].items():
                assert max(samples)>4. and samples[-1]<1., (side,samples)
            second_hp=expected
            place(car,lane+vec(-475,0,100))
            place(other,lane+vec(445,0,100))
            advance('second_round')
            return
        if stage=='second_round' and age>.7:
            bomb=spawn_test_ball(car,'HALSpawnTimedBombBall',u.HALTimedBombBall)
            place(bomb,car.get_actor_location()+vec(0,-128,2))
            advance('second_contact')
            return
        if stage=='second_contact':
            if bomb.get_controlled_by()!=car: return
            deadline=bomb.get_fuse_deadline()
            cmd(server,'HALStun '+str(psid(car))+' 9')
            advance('dropped')
            return
        if stage=='dropped':
            if bomb.get_controlled_by(): return
            place(bomb,lane+vec(0,0,101))
            place(car,lane+vec(-340,0,100))
            place(other,lane+vec(340,0,100))
            blast_directions = {psid(car): vec(-1,0,0), psid(other): vec(1,0,0)}
            shot_blast=False
            advance('ground_fuse')
            return
        if stage=='ground_fuse':
            if now()<deadline:
                assert not bomb.get_controlled_by(), 'Car captured the bomb before the ground fuse expired'
            sample_blast_motion('ground', blast_directions)
            frame(lane+vec(0,0,70),vec(-70,-360,1750),56.)
            if now()>deadline+.16 and not shot_blast:
                if not all(visible_clasp_effect(w) for w in (server,client)): return
                capture('GroundBurst')
                result['ground_cue_on_both']=True
                shot_blast=True
            expected={pid:hp-result['impact_config']['damage'] for pid,hp in second_hp.items()}
            if now()<deadline+.8 or not shot_blast or healths(server)!=expected or healths(client)!=expected: return
            check_blast_motion('ground', 2)
            result['ground_hp']=[healths(server),healths(client)]
            advance('screens')
            return
        if stage=='screens' and age>.6:
            if not check_expiry:
                finish()
                return
            # Launch a third ball into an empty lane. Follow it until its own free-life
            # clock retires it and both worlds display the harmless departure cue.
            place(car,lane+vec(-1450,0,110),90.)
            place(other,lane+vec(950,-1100,110))
            previous=all_of(server,u.HALTimedBombBall)
            cmd(server,'HALSpawnTimedBombBall')
            bomb=next(b for b in all_of(server,u.HALTimedBombBall) if b not in previous)
            place(car,vec(-2800.,2650.,lane.z+110.))
            place(other,vec(-2000.,2650.,lane.z+110.))
            origin=bomb.get_actor_location()
            result['free_spawn_time']=now()
            advance('watch_free_life')
            return
        if stage=='watch_free_life':
            if bomb in all_of(server,u.HALTimedBombBall):
                assert bomb.get_fuse_deadline()<0. and not bomb.get_controlled_by()
                origin=bomb.get_actor_location()
                # Keep both live cars clear for this case without changing anyone's physics.
                northwest=vec(-2800.,2650.,lane.z+110.)
                southeast=lane+vec(0,0,110.)
                farther=max((northwest,southeast),key=lambda p:(p-origin).length())
                for index,parked in enumerate((car,other)):
                    if (parked.get_actor_location()-origin).length()<1300.:
                        place(parked,farther+vec(index*750.,0,0))
                frame(origin,vec(-70,-450,1050),52.)
                if .12<age<.45 and 'arrival_on_both' not in result and all(visible_clasp_effect(w,origin) for w in (server,client)):
                    capture('Arrival')
                    result['arrival_on_both']=True
                return
            if not all(visible_clasp_effect(w,origin) for w in (server,client)): return
            result['free_life_seconds']=age
            assert 19.8<=age<=20.8 and result.get('arrival_on_both') and healths(server)==healths(client)==result['ground_hp'][0]
            result['natural_expiry_on_both']=True
            capture('Expiration')
            advance('done')
            return
        if stage=='done' and age>.3: finish()
    except Exception:
        finish(traceback.format_exc())


handle=u.register_slate_post_tick_callback(tick)
u.EditorPythonScripting.set_keep_python_script_alive(True)
