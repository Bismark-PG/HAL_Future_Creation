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
graphite = (.065, .055, .095)
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


def material(name, inputs, code, cutout=False):
    old = existing(FX + '/' + name)
    if old:
        if name != 'M_TimedBomb_PatrolWake':
            protect_painted_colors(old)
            return old
        output = edit.get_material_property_input_node(old, u.MaterialProperty.MP_EMISSIVE_COLOR)
        if isinstance(output,u.MaterialExpressionEyeAdaptationInverse):
            return old
        sources = edit.get_inputs_for_material_expression(old, output) if output else []
        if any(isinstance(node, u.MaterialExpressionCustom) and 'Snap' in node.get_editor_property('code') for node in sources):
            protect_painted_colors(old)
            return old
        edit.delete_all_material_expressions(old)
    mat = old or tools.create_asset(name, FX, u.Material, u.MaterialFactoryNew())
    mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    if cutout:
        mat.set_editor_property('blend_mode', u.BlendMode.BLEND_MASKED)
    mat.set_editor_property('two_sided', True)
    shader = edit.create_material_expression(mat, u.MaterialExpressionCustom, 0, 0)
    shader.set_editor_property('description', name)
    shader.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT4)
    nodes, pins = [], []
    sources = {'UV': u.MaterialExpressionTextureCoordinate, 'N': u.MaterialExpressionPixelNormalWS,
               'V': u.MaterialExpressionCameraVectorWS, 'Clock': u.MaterialExpressionTime}
    for index, (key, value) in enumerate(inputs):
        cls = sources[key] if value is None else (u.MaterialExpressionVectorParameter if isinstance(value, tuple)
                                                   else u.MaterialExpressionScalarParameter)
        node = edit.create_material_expression(mat, cls, -380, index*82)
        if value is not None:
            node.set_editor_property('parameter_name', key)
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
    for alpha in ([False, True] if cutout else [False]):
        mask = edit.create_material_expression(mat, u.MaterialExpressionComponentMask, 200, 180 if alpha else 0)
        for channel in 'rgba':
            mask.set_editor_property(channel, (channel == 'a') == alpha)
        if not edit.connect_material_expressions(shader, '', mask, ''):
            raise RuntimeError('Missing material output')
        if not edit.connect_material_property(mask, '', u.MaterialProperty.MP_OPACITY_MASK if alpha else u.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError('Missing material binding')
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


def jaw_mesh(mat):
    # Eight-sided bevel section swept around a half hoop. End faces are actual ivory pole shoes.
    section = [(32., -14.), (36., -20.), (46., -18.), (49., -11.),
               (49.5, 10.), (46., 18.), (36., 20.), (32., 14.)]
    vertices, uv, faces = [], [], []
    slices = 40
    for i in range(slices+1):
        t = i/slices
        a = math.radians(9.+162.*t)
        for j, (radius, z) in enumerate(section):
            vertices.append((radius*math.cos(a), radius*math.sin(a), z+3.))
            uv.append((t, j/8.))
    for i in range(slices):
        for j in range(8):
            a, b = i*8+j, i*8+(j+1)%8
            faces.extend([(a,b+8,b), (a,a+8,b+8)])
    for i in (0, slices):
        base = i*8
        for j in range(1,7):
            face = (base, base+j, base+j+1)
            faces.append(face if i else face[::-1])
    return static_mesh('SM_TimedBomb_Pole', vertices, uv, faces, mat)


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


assets.make_directory(FX)
shell = material('M_TimedBomb_Graphite',[('N',None),('V',None),('CoreColor',graphite)], """
float3 n=normalize(N), v=normalize(V), l=normalize(float3(-.45,-.38,.82));
float d=dot(n,l); float rim=pow(1.-saturate(dot(n,v)),2.);
float band=.48+.85*smoothstep(-.08,.12,d)+1.0*smoothstep(.62,.76,d);
float polish=pow(saturate(dot(n,normalize(l+v))),24.);
return float4(CoreColor*band+float3(.30,.30,.37)*polish+float3(.11,.13,.18)*rim,1);
""")
pole = material('M_TimedBomb_Pole',[('UV',None),('N',None),('V',None),('CurrentColor',red),('PoleTipColor',ivory)], """
float edge=min(UV.x,1.-UV.x); float tip=1.-smoothstep(.154,.161,edge);
float seam=1.-smoothstep(.003,.009,abs(edge-.158));
float3 n=normalize(N),v=normalize(V),l=normalize(float3(-.42,-.46,.78));
float d=dot(n,l), h=dot(n,normalize(l+v));
float band=.30+.43*smoothstep(-.20,-.08,d)+.44*smoothstep(.54,.68,d);
float3 paint=lerp(CurrentColor,PoleTipColor,tip)*(1.-.72*seam);
return float4(paint*band+float3(.42,.32,.29)*smoothstep(.90,.98,h)+.10*pow(1.-saturate(dot(v,n)),3.),1);
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
wake_shader = """
float2 p=UV*2.-1.; float y=abs(p.y); float bend=.36*p.x*p.x;
if (Snap>.001) {
    float2 v=abs(p)/max(.001,Snap);
    float ray=1.-smoothstep(.83,1.,sqrt(v.x)+sqrt(v.y));
    return float4(LampColor*2.+float3(.5,.5,.38),ray);
}
float t=frac(Clock*3.4*FlowSpeed); float gate=smoothstep(-.84,-.57,p.x)*(1.-smoothstep(.23,.75,p.x));
float width=.024+.028*gate; float strokes=1.-smoothstep(width,width+.016,abs(y-.35-bend));
float second=(1.-smoothstep(.018,.038,abs(y-.62-bend)))*(1.-smoothstep(-.10,.25,p.x));
float wave=.73+.27*cos(6.283*(p.x*.42-t));
return float4(lerp(float3(.60,.65,.79),float3(.96,.80,.60),wave),saturate(strokes+second)*gate*wave);
"""
wake = material('M_TimedBomb_PatrolWake',[('UV',None),('Clock',None),('Snap',0.),('FlowSpeed',.65),('LampColor',amber)],wake_shader,True)
jaw, dial = jaw_mesh(pole), dial_mesh(face)
base = existing(DATA+'/DA_Ball_Explosive')
solar_cue = base.get_editor_property('cue_material')
pulse = existing(FX+'/M_TimedBomb_Pulse')
if not pulse:
    pulse = assets.duplicate_asset(solar_cue.get_path_name().split('.')[0], FX+'/M_TimedBomb_Pulse')
protect_painted_colors(pulse)
ball = existing(DATA+'/DA_Ball_TimedBomb')
if not ball:
    factory = u.DataAssetFactory()
    factory.set_editor_property('data_asset_class',u.HALTimedBombBallDefinition)
    ball = tools.create_asset('DA_Ball_TimedBomb',DATA,u.HALTimedBombBallDefinition,factory)
    for key in ('physics','radius','visual','gameplay','presentation'):
        ball.set_editor_property(key,base.get_editor_property(key))
    visual = ball.get_editor_property('visual')
    visual.set_editor_property('materials',[shell])
    ball.set_editor_property('visual',visual)
    gameplay = ball.get_editor_property('gameplay')
    damage = gameplay.get_editor_property('damage')
    damage.set_editor_property('version',u.HALCombatConfigVersion.LEGACY_FIXED_DAMAGE)
    damage.set_editor_property('vehicle_hit_damage',25.)
    gameplay.set_editor_property('damage',damage)
    ball.set_editor_property('gameplay',gameplay)
    for key, value in [('core_color',u.LinearColor(*graphite,1)),('current_color',u.LinearColor(*red,1)),
                       ('corona_color',u.LinearColor(*amber,1)),('pole_tip_color',u.LinearColor(*ivory,1)),
                       ('jaw_mesh',jaw),('dial_mesh',dial),('patrol_wake_material',wake),
                       ('impact_seconds',.73),('arrival_seconds',.58),('expiration_seconds',.6),('ready_for_use',True)]:
        ball.set_editor_property(key,value)
if not ball.get_editor_property('cue_material') or ball.get_editor_property('cue_material') == solar_cue:
    ball.set_editor_property('cue_material',pulse)
    presentation = ball.get_editor_property('presentation')
    # The visible clasp follows the two bumpers from the replicated transfer, including during a drift.
    presentation.set_editor_property('enabled',False)
    ball.set_editor_property('presentation',presentation)
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
