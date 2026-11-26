"""Author the magnetic clasp and clock face. Existing ball tuning and supply settings are preserved."""
import math
import sys
from pathlib import Path
import unreal as u

FX = '/Game/Assets/FX/SpecialBalls/TimedBomb'
DATA = '/Game/Data/Balls'
assets = u.EditorAssetLibrary
tools = u.AssetToolsHelpers.get_asset_tools()
edit = u.MaterialEditingLibrary
source_dir = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))/'Tools'/'SpecialBall'
graphite = (.065, .055, .095)
ember = (.29, .019, .040)
red = (.83, .025, .075)
ivory = (.98, .83, .59)
amber = (1.25, .50, .042)


def existing(path):
    return assets.load_asset(path) if assets.does_asset_exist(path) else None


def save(obj):
    if not assets.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + obj.get_path_name())


def protect_painted_colors(mat):
    prop = u.MaterialProperty.MP_EMISSIVE_COLOR
    output = edit.get_material_property_input_node(mat,prop)
    if isinstance(output,u.MaterialExpressionEyeAdaptationInverse):
        return
    exposure = edit.create_material_expression(mat,u.MaterialExpressionEyeAdaptationInverse,460,0)
    if not output or not edit.connect_material_expressions(output,'',exposure,'') or not edit.connect_material_property(exposure,'',prop):
        raise RuntimeError('Cannot protect night-race colors on '+mat.get_path_name())
    edit.recompile_material(mat)
    save(mat)


def material(name, inputs, code, translucent=False):
    old = existing(FX + '/' + name)
    flags = u.SystemLibrary.get_command_line().split()
    refresh = (name in ('M_TimedBomb_ClaspBurst','M_TimedBomb_CageSteel','M_TimedBomb_EmberCore') and '-RefreshClaspArt' in flags
               or name == 'M_TimedBomb_ClaspBurst' and '-RefreshCageBurst' in flags
               or name == 'M_TimedBomb_ClaspArrival' and '-RefreshClaspArrival' in flags)
    if old and not refresh:
        protect_painted_colors(old)
        return old
    mat = old or tools.create_asset(name, FX, u.Material, u.MaterialFactoryNew())
    if old:
        edit.delete_all_material_expressions(mat)
    mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)
    if translucent:
        mat.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
    shader = edit.create_material_expression(mat, u.MaterialExpressionCustom, 0, 0)
    shader.set_editor_property('description', name)
    shader.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT4)
    nodes, pins = [], []
    sources = {'UV': u.MaterialExpressionTextureCoordinate, 'N': u.MaterialExpressionPixelNormalWS,
               'V': u.MaterialExpressionCameraVectorWS}
    for index, (key, value) in enumerate(inputs):
        if isinstance(value, u.Texture2D):
            cls = u.MaterialExpressionTextureObjectParameter
        else:
            cls = sources[key] if value is None else (u.MaterialExpressionVectorParameter if isinstance(value, tuple)
                                                       else u.MaterialExpressionScalarParameter)
        node = edit.create_material_expression(mat, cls, -380, index*82)
        if value is not None:
            node.set_editor_property('parameter_name', key)
            if isinstance(value, u.Texture2D):
                node.set_editor_property('texture', value)
            else:
                node.set_editor_property('default_value', u.LinearColor(*value, 1.) if isinstance(value, tuple) else value)
        pin = u.CustomInput()
        pin.set_editor_property('input_name', key)
        pins.append(pin)
        nodes.append((node, 'RGB' if isinstance(value, tuple) else '', key))
    shader.set_editor_property('inputs', pins)
    shader.set_editor_property('code', code)
    for node, output, key in nodes:
        if not edit.connect_material_expressions(node, output, shader, key):
            raise RuntimeError('Missing material input ' + key)
    mask = edit.create_material_expression(mat, u.MaterialExpressionComponentMask, 200, 0)
    for channel in 'rgba':
        mask.set_editor_property(channel, channel != 'a')
    if not edit.connect_material_expressions(shader, '', mask, ''):
        raise RuntimeError('Missing material output')
    if not edit.connect_material_property(mask, '', u.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Missing material binding')
    if translucent:
        alpha = edit.create_material_expression(mat, u.MaterialExpressionComponentMask, 200, 160)
        for channel in 'rgba':
            alpha.set_editor_property(channel, channel == 'a')
        if not (edit.connect_material_expressions(shader, '', alpha, '')
                and edit.connect_material_property(alpha, '', u.MaterialProperty.MP_OPACITY)):
            raise RuntimeError('Missing burst opacity')
    protect_painted_colors(mat)
    return mat


def static_mesh(name, positions, uv, faces, mat):
    path = FX + '/' + name
    old = existing(path)
    if old:
        return old
    buffers = u.GeometryScriptSimpleMeshBuffers()
    buffers.vertices = [u.Vector(*v) for v in positions]
    buffers.uv0 = [u.Vector2D(*v) for v in uv]
    buffers.triangles = [u.IntVector(*t) for t in faces]
    dynamic = u.DynamicMesh()
    u.GeometryScript_MeshEdits.append_buffers_to_mesh(dynamic, buffers)
    opts = u.GeometryScriptCreateNewStaticMeshAssetOptions()
    opts.enable_collision = False
    opts.enable_recompute_normals = True
    opts.enable_recompute_tangents = True
    mesh, outcome = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dynamic, path, opts)
    if not mesh or outcome != u.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError('Could not create ' + path)
    mesh.set_material(0, mat)
    save(mesh)
    return mesh


def cage_mesh(mat, part):
    # The same cross sections, sweep radii and bevel masks build the moving half
    # and the individual pieces that separate when its inner core bursts.
    vertices, uv, faces = [], [], []

    def face(a, b, c, outward):
        v = [vertices[b][i]-vertices[a][i] for i in range(3)]
        w = [vertices[c][i]-vertices[a][i] for i in range(3)]
        normal = [v[1]*w[2]-v[2]*w[1], v[2]*w[0]-v[0]*w[2], v[0]*w[1]-v[1]*w[0]]
        faces.append((a,c,b) if sum(n*d for n,d in zip(normal,outward))>0 else (a,b,c))

    def sweep(frames, width, depth, bevel, pad=False, broken=False):
        w, d, b = width/2., depth/2., bevel
        section = [(-w+b,d),(w-b,d),(w,d-b),(w,-d+b),(w-b,-d),(-w+b,-d),(-w,-d+b),(-w,d-b)]
        rings = [[tuple(center[k]+s*side[k]+h*up[k] for k in range(3)) for s,h in section]
                 for center,side,up in frames]
        if broken:
            # A sheared end runs obliquely across the strap; each side has a different tear.
            for end,near,tear in ((0,1,(.18,-.23,-.20,-.31,-.12,.19,.26,.22)),
                                  (-1,-2,(-.16,.25,.29,.18,.24,-.15,-.26,-.12))):
                tangent = [frames[end][0][k]-frames[near][0][k] for k in range(3)]
                length = math.sqrt(sum(x*x for x in tangent))
                rings[end] = [tuple(p[k]+tangent[k]/length*width*offset for k in range(3))
                              for p,offset in zip(rings[end],tear)]
        # Each bevel has its own normals and material mask; the long arch is smooth along its sweep.
        for j in range(8):
            start = len(vertices)
            tag = (2 if pad else 0) + j%2
            for i, ring in enumerate(rings):
                p, q = ring[j], ring[(j+1)%8]
                vertices.extend((p,q))
                uv.extend(((i/(len(rings)-1),tag),)*2)
                if i:
                    a = start+2*(i-1)
                    outward = [(p[k]+q[k])*.5-frames[i][0][k] for k in range(3)]
                    face(a,a+1,a+2,outward)
                    face(a+2,a+1,a+3,outward)
        for i, neighbor in ((0,1),(-1,-2)):
            start = len(vertices)
            vertices.extend(rings[i])
            uv.extend(((0.,1. if broken else (2. if pad else 0.)),)*8)
            outward = [frames[i][0][k]-frames[neighbor][0][k] for k in range(3)]
            for j in range(1,7):
                face(start,start+j,start+j+1,outward)

    def meridian(azimuth, first=-63., last=56., steps=18, broken=False):
        a = math.radians(azimuth)
        c, s = math.cos(a), math.sin(a)
        frames = []
        for i in range(steps+1):
            lat = math.radians(first+(last-first)*i/steps)
            r, z = math.cos(lat), math.sin(lat)
            frames.append(((47*r*c,47*r*s,43*z),(-s,c,0.),(r*c,r*s,z)))
        sweep(frames,15.5,5.3,1.5,broken=broken)

    def rail(radius, z, first, last, height, depth, bevel, pad=False, broken=False):
        frames = []
        steps = max(3,math.ceil((last-first)/8.))
        for i in range(steps+1):
            a = math.radians(first+(last-first)*i/steps)
            c, s = math.cos(a), math.sin(a)
            frames.append(((radius*c,radius*s,z),(0.,0.,1.),(c,s,0.)))
        sweep(frames,height,depth,bevel,pad,broken)

    if part == 'CageHalf':
        meridian(24.)
        meridian(114.)
        rail(45.4,-6.,4.,176.,6.,5.,1.3)
        rail(30.3,34.,3.,177.,5.5,5.,1.)
        rail(24.,-36.,4.,176.,4.,4.,.8)
        rail(47.,-4.,70.,110.,10.,5.,1.4)
        rail(49.,-3.,74.,106.,8.,4.,1.25,True)
        pivot = (0.,0.,0.)
    elif part == 'RibFragment':
        meridian(0.,-30.,50.,12,broken=True)
        pivot = (46.3,0.,7.5)
    elif part == 'LockFragment':
        rail(45.4,-6.,-31.,29.,6.,5.,1.3,broken=True)
        rail(47.,-4.,-20.,20.,10.,5.,1.4)
        rail(49.,-3.,-16.,16.,8.,4.,1.25,True)
        pivot = (47.,0.,-4.)
    elif part == 'CollarFragment':
        rail(30.3,34.,-31.,26.,5.5,5.,1.,broken=True)
        pivot = (30.3,0.,34.)
    else:
        raise ValueError(part)
    centered = [tuple(p[k]-pivot[k] for k in range(3)) for p in vertices]
    return static_mesh('SM_TimedBomb_'+part, centered, uv, faces, mat)


def dial_mesh(mat):
    vertices, uv, faces = [(0.,0.,6.)], [(.5,.5)], []
    sectors = 64
    for r, z in [(40.,4.),(48.,1.5),(50.,0.),(50.,-4.)]:
        for i in range(sectors):
            angle = math.tau*i/sectors
            x, y = r*math.cos(angle), r*math.sin(angle)
            vertices.append((x,y,z))
            uv.append((.5+x/100.,.5+y/100.))
    for i in range(sectors):
        j = (i+1)%sectors
        faces.append((0,1+j,1+i))
        for ring in range(3):
            a, b = 1+ring*sectors+i, 1+ring*sectors+j
            faces.extend([(a,b,b+sectors), (a,b+sectors,a+sectors)])
    return static_mesh('SM_TimedBomb_Dial', vertices, uv, faces, mat)


def smoke_texture():
    name = 'T_TimedBomb_ClaspSmoke'
    texture = existing(FX+'/'+name)
    if texture:
        return texture
    task = u.AssetImportTask()
    task.set_editor_property('filename', str(source_dir/'Art'/(name+'.png')))
    task.set_editor_property('destination_path', FX)
    task.set_editor_property('automated', True)
    tools.import_asset_tasks([task])
    texture = existing(FX+'/'+name)
    if not texture:
        raise RuntimeError('Cannot import clasp smoke texture')
    texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_BC7)
    texture.set_editor_property('lod_group',u.TextureGroup.TEXTUREGROUP_EFFECTS)
    texture.set_editor_property('address_x',u.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y',u.TextureAddress.TA_CLAMP)
    texture.set_editor_property('power_of_two_mode',u.TexturePowerOfTwoSetting.RESIZE_TO_SPECIFIC_RESOLUTION)
    texture.set_editor_property('resize_during_build_x',512)
    texture.set_editor_property('resize_during_build_y',512)
    save(texture)
    return texture


assets.make_directory(FX)
shell = material('M_TimedBomb_EmberCore',[('N',None),('V',None),('CoreColor',ember)], """
float3 n=normalize(N), v=normalize(V), l=normalize(float3(-.45,-.38,.82));
float3 facet=normalize(lerp(n,round(n*7.)/7.,.62));
float d=dot(facet,l), rim=1.-saturate(dot(n,v));
float band=.23+.52*smoothstep(-.12,.05,d)+.49*smoothstep(.57,.70,d);
float glint=smoothstep(.91,.99,dot(n,normalize(l+v)));
return float4(CoreColor*band + float3(.11,.018,.025)*glint + .10*CoreColor*rim,1);
""")
steel = material('M_TimedBomb_CageSteel',[('UV',None),('N',None),('V',None),('CurrentColor',red),
                 ('PoleTipColor',ivory),('CageSteelColor',(.19,.175,.20))], """
float3 n=normalize(N),v=normalize(V),l=normalize(float3(-.46,-.30,.84));
float d=dot(n,l), h=dot(n,normalize(l+v)), rim=1.-saturate(dot(n,v));
float3 graphite=CageSteelColor*lerp(float3(.12,.14,.16),float3(.51,.54,.58),smoothstep(-.19,.02,d));
graphite=lerp(graphite,CageSteelColor,smoothstep(.53,.72,d));
graphite+=float3(.12,.12,.15)*smoothstep(.86,.99,h);
float bevel=fmod(round(UV.y),2.);
graphite+=bevel*(.035+smoothstep(.35,.80,d)*.27)*float3(1.,.64,.44);
float3 pole=CurrentColor*(.35+.47*smoothstep(-.2,.1,d)+.45*smoothstep(.61,.78,d));
pole+=bevel*PoleTipColor*(.09+.22*smoothstep(.45,.81,d));
return float4(lerp(graphite,pole,step(1.5,UV.y))+.012*rim,1);
""")
face = material('M_TimedBomb_Chronograph',[('UV',None),('Remaining',1.),('Urgency',0.),('Beat',0.),
                ('Segments',12.),('Glow',1.6),('AlertColor',red),('LampColor',amber)], """
float2 p=2.*(UV-.5); float r=length(p); float a=frac(atan2(p.y,p.x)/6.283185+.25);
float div=max(4.,round(Segments)); float cell=a*div;
float sector=smoothstep(.035,.095,frac(cell))*(1.-smoothstep(.88,.96,frac(cell)));
float ring=smoothstep(.54,.59,r)*(1.-smoothstep(.814,.845,r))*sector;
float3 col=lerp(float3(.025,.021,.035),float3(.11,.085,.095),smoothstep(.60,.93,r));
col=lerp(col,float3(.48,.35,.24),smoothstep(.90,.925,r)*(1.-smoothstep(.942,.968,r)));
float3 lamp=lerp(LampColor,AlertColor,smoothstep(.01,.42,Urgency));
float lit=step(floor(cell)+.5,ceil(Remaining*div-.0001));
col=lerp(col,lerp(float3(.16,.085,.044),lamp*(.92+Glow*.26+Beat*.60),lit),ring);
float h=(1.-Remaining)*6.283185-1.570796; float2 v=float2(cos(h),sin(h));
float d=dot(p,v), side=abs(dot(p,float2(-v.y,v.x)));
float needle=smoothstep(-.17,-.11,d)*(1.-smoothstep(.54,.61,d))*(1.-smoothstep(.025,.052,side+.033*d));
col=lerp(col,lerp(AlertColor,float3(1.,.40,.16),Beat*.40),needle);
col=lerp(col,float3(.79,.63,.43),1.-smoothstep(.083,.112,r));
return float4(col,1);
""")
jaw, dial = cage_mesh(steel,'CageHalf'), dial_mesh(face)
arrival = material('M_TimedBomb_ClaspArrival',[('UV',None),('Progress',.28),('Segments',12.),('Glow',1.6),
                   ('Steel',(.19,.175,.20)),('Pole',red),('Lamp',amber),('Tip',ivory)],
                   (source_dir/'TimedBombClaspArrival.hlsl').read_text(encoding='utf-8'),translucent=True)
burst = material('M_TimedBomb_ClaspBurst',[('UV',None),('Paint',smoke_texture()),('Progress',.3),
                 ('GroundPulse',0.),('SmokeSize',.54),('Glow',1.6)],
                 (source_dir/'TimedBombClaspBurst.hlsl').read_text(encoding='utf-8'),translucent=True)
base = existing(DATA+'/DA_Ball_Explosive')
solar_cue = base.get_editor_property('cue_material')
pulse = existing(FX+'/M_TimedBomb_Pulse')
if not pulse:
    pulse = assets.duplicate_asset(solar_cue.get_path_name().split('.')[0], FX+'/M_TimedBomb_Pulse')
protect_painted_colors(pulse)
ball = existing(DATA+'/DA_Ball_TimedBomb')
new_ball = not ball
if not ball:
    factory = u.DataAssetFactory()
    factory.set_editor_property('data_asset_class',u.HALTimedBombBallDefinition)
    ball = tools.create_asset('DA_Ball_TimedBomb',DATA,u.HALTimedBombBallDefinition,factory)
    for key in ('physics','radius','visual','gameplay','presentation'):
        ball.set_editor_property(key,base.get_editor_property(key))
    physics = ball.get_editor_property('physics')
    physics.set_editor_property('hidden_in_game',True)
    ball.set_editor_property('physics',physics)
    presentation = ball.get_editor_property('presentation')
    presentation.set_editor_property('enabled',True)
    ball.set_editor_property('presentation',presentation)
    visual = ball.get_editor_property('visual')
    visual.set_editor_property('materials',[shell])
    ball.set_editor_property('visual',visual)
    gameplay = ball.get_editor_property('gameplay')
    damage = gameplay.get_editor_property('damage')
    damage.set_editor_property('version',u.HALCombatConfigVersion.LEGACY_FIXED_DAMAGE)
    damage.set_editor_property('vehicle_hit_damage',25.)
    gameplay.set_editor_property('damage',damage)
    ball.set_editor_property('gameplay',gameplay)
    for key, value in [('core_color',u.LinearColor(*ember,1)),('current_color',u.LinearColor(*red,1)),
                       ('corona_color',u.LinearColor(*amber,1)),('pole_tip_color',u.LinearColor(*ivory,1)),
                       ('jaw_mesh',jaw),('dial_mesh',dial),
                       ('impact_seconds',.73),('arrival_seconds',.58),('expiration_seconds',.6),('ready_for_use',True)]:
        ball.set_editor_property(key,value)
changed = new_ball
for key,part in (('rib_fragment_mesh','RibFragment'),('lock_fragment_mesh','LockFragment'),('collar_fragment_mesh','CollarFragment')):
    if not ball.get_editor_property(key):
        ball.set_editor_property(key,cage_mesh(steel,part))
        changed = True
if not ball.get_editor_property('cue_material') or ball.get_editor_property('cue_material') == solar_cue:
    ball.set_editor_property('cue_material',pulse)
    changed = True
if not ball.get_editor_property('clasp_burst_material'):
    ball.set_editor_property('clasp_burst_material',burst)
    changed = True
if not ball.get_editor_property('clasp_arrival_material'):
    ball.set_editor_property('clasp_arrival_material',arrival)
    changed = True
if ball.get_editor_property('jaw_mesh') == existing(FX+'/SM_TimedBomb_Pole'):
    ball.set_editor_property('jaw_mesh',jaw)
    changed = True
visual = ball.get_editor_property('visual')
slots = list(visual.get_editor_property('materials'))
if slots and slots[0] == existing(FX+'/M_TimedBomb_Graphite'):
    slots[0] = shell
    visual.set_editor_property('materials',slots)
    ball.set_editor_property('visual',visual)
    old_tint = ball.get_editor_property('core_color')
    if all(abs(v-w)<.001 for v,w in zip((old_tint.r,old_tint.g,old_tint.b),graphite)):
        ball.set_editor_property('core_color',u.LinearColor(*ember,1))
    changed = True
if changed:
    save(ball)

supply = existing(DATA+'/DA_Arena_SpecialBallSupply')
pool = list(supply.get_editor_property('ball_pool'))
tuning = ('pad_diameter','pad_thickness','drop_interval','first_drop_seconds','blocked_grace','spawn_clearance','warning_seconds')
before = {key:supply.get_editor_property(key) for key in tuning}
u.log('TIMED_SUPPLY_BEFORE '+str((pool,before)))
if u.HALPadBall.TIMED_BOMB not in pool:
    supply.set_editor_property('ball_pool',pool+[u.HALPadBall.TIMED_BOMB])
    save(supply)
if {key:supply.get_editor_property(key) for key in tuning} != before:
    raise RuntimeError('Supply tuning changed during ball registration')
sys.dont_write_bytecode = True
sys.path.insert(0,str(Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))/'Tools'/'SpecialBall'))
from CreateSupplyPads import pad_material
pad_material()
u.log('TIMED_BALL_READY '+ball.get_path_name()+' pool='+str(list(supply.get_editor_property('ball_pool'))))
