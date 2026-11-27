"""Author the arcade homing pod and register its ready DA with the existing match pads."""
import math
import sys
from pathlib import Path

import unreal as u

FX = '/Game/Assets/FX/SpecialBalls/Homing'
DATA = '/Game/Data/Balls'
assets = u.EditorAssetLibrary
edit = u.MaterialEditingLibrary
tools = u.AssetToolsHelpers.get_asset_tools()
navy = (.021, .046, .085)
turquoise = (.022, .65, .56)
tangerine = (1., .24, .035)


def load(path):
    return assets.load_asset(path) if assets.does_asset_exist(path) else None


def save(obj):
    if not obj or not assets.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError('Failed to save ' + str(obj))


def make_material(name, exhaust):
    old = load(FX+'/'+name)
    if old:
        return old
    mat = tools.create_asset(name, FX, u.Material, u.MaterialFactoryNew())
    if exhaust:
        mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    def node(cls, x, y):
        return edit.create_material_expression(mat, cls, x, y)
    def wire(source, out, target, pin):
        if not edit.connect_material_expressions(source, out, target, pin):
            raise RuntimeError(name + ' could not connect ' + pin)
    def bind(source, prop):
        if not edit.connect_material_property(source, '', prop):
            raise RuntimeError(name + ' could not connect ' + str(prop))
    inputs = [('UV', node(u.MaterialExpressionTextureCoordinate, -660, -230), ''),
              ('Clock', node(u.MaterialExpressionTime, -660, -100), '')]
    for label, value in [('CoreColor',navy), ('CurrentColor',turquoise), ('CoronaColor',tangerine),
                         ('Glow',1.), ('WarnAt',1.e9)]:
        vector = isinstance(value, tuple)
        p = node(u.MaterialExpressionVectorParameter if vector else u.MaterialExpressionScalarParameter, -680, 95*len(inputs))
        p.set_editor_property('parameter_name', label)
        p.set_editor_property('default_value', u.LinearColor(*value,1.) if vector else value)
        inputs.append((label,p,'RGB' if vector else ''))
    shader = node(u.MaterialExpressionCustom, -220, 0)
    shader.set_editor_property('description', 'Thruster shock cells' if exhaust else 'Enamel race shell, visor and thruster bands')
    shader.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT4)
    pins = []
    for key, _, _ in inputs:
        pin = u.CustomInput()
        pin.set_editor_property('input_name', key)
        pins.append(pin)
    shader.set_editor_property('inputs', pins)
    shader.set_editor_property('code', """
float cell=abs(frac(UV.x*3.0-Clock*3.0)-.5)*2.;
float hot=smoothstep(.38,.72,cell+.28*cos(UV.y*18.85));
float3 tint=lerp(CurrentColor,CoronaColor,smoothstep(.28,.86,UV.x));
tint=lerp(tint,float3(.90,1.,.90),hot*(1.-UV.x)*.95);
return float4(tint*(.70+.36*hot),Glow);
""" if exhaust else """
float x=UV.x, a=UV.y*6.283185;
float3 ivory=float3(.82,.87,.82);
float3 paint=ivory;
float lamp=0.;
if (UV.y>1.1) {
    paint=lerp(CoreColor,CurrentColor,step(.23,x));
    if (x>.71) paint=CoronaColor;
    lamp=.23*step(.70,x);
} else {
    if(x<.29) paint=CoreColor;
    if(x<.13) {paint=lerp(CurrentColor,CoronaColor,step(.08,x)); lamp=.7;}
    float belt=x-.018*cos(4.*a);
    if (belt>.30 && belt<.42) paint=CurrentColor;
    if (x>.64 && x<.685) paint=CoreColor;
    if (x>.735) paint=CoronaColor;
    if (x>.93) {paint=float3(.97,.78,.39); lamp=.45;}
    float vents=step(.53,x)*step(x,.61)*smoothstep(.35,.62,cos(8.*a+11.*x));
    paint=lerp(paint,CoreColor,vents);
    if (belt>.322 && belt<.354) {paint=ivory; lamp=.42;}
}
float warning=1.+step(WarnAt,Clock)*(.5+.5*cos((Clock-WarnAt)*12.));
return float4(paint,Glow*(.10+lamp*warning));
""")
    for key, source, out in inputs:
        wire(source, out, shader, key)
    def mask(alpha, x, y):
        m = node(u.MaterialExpressionComponentMask, x, y)
        for channel in 'rgba':
            m.set_editor_property(channel, (channel == 'a') == alpha)
        wire(shader, '', m, '')
        return m
    color, intensity = mask(False,20,0), mask(True,20,100)
    if not exhaust:
        bind(color,u.MaterialProperty.MP_BASE_COLOR)
        for prop,value,y in [(u.MaterialProperty.MP_METALLIC,.46,260),(u.MaterialProperty.MP_ROUGHNESS,.31,350)]:
            constant = node(u.MaterialExpressionConstant,40,y)
            constant.set_editor_property('r',value)
            bind(constant,prop)
    mul = node(u.MaterialExpressionMultiply,225,90)
    wire(color,'',mul,'A')
    wire(intensity,'',mul,'B')
    inverse = node(u.MaterialExpressionEyeAdaptationInverse,425,90)
    wire(mul,'',inverse,'')
    bind(inverse,u.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(mat)
    save(mat)
    return mat


def make_mesh(name, material, exhaust=False):
    old = load(FX+'/'+name)
    if old:
        return old
    vertices, uvs, triangles = [], [], []
    def vertex(point, uv):
        vertices.append(u.Vector(*point))
        uvs.append(u.Vector2D(*uv))
        return len(vertices)-1
    def tri(a,b,c):
        triangles.append(u.IntVector(a,b,c))
    # The nose is +X. All hull parts fit inside the pickup sphere; the nozzle starts at -38.
    rings = ([(0,9),(-6,13),(-15,10),(-21,12),(-33,6),(-49,.4)] if exhaust else
             [(-39,1),(-40,13),(-38,20),(-32,23),(-28,28),(-19,32),(-9,37),
              (2,38),(15,35),(27,28),(38,18),(46,8),(49,.3)])
    sides = 12 if exhaust else 32
    for i,(x,r) in enumerate(rings):
        for j in range(sides+1):
            angle = j*math.tau/sides
            vertex((x,r*math.cos(angle),r*math.sin(angle)), (abs(x)/49 if exhaust else (x+40)/89,j/sides))
    for i in range(len(rings)-1):
        for j in range(sides):
            a=i*(sides+1)+j
            b=a+sides+1
            if exhaust:
                tri(a,b+1,b)
                tri(a,a+1,b+1)
            else:
                tri(a,b,b+1)
                tri(a,b+1,a+1)
    if not exhaust:
        for fin in range(4):
            a=fin*math.pi/2+.56
            def fin_vertex(x,r,s):
                return (x,r*math.cos(a)-s*math.sin(a),r*math.sin(a)+s*math.cos(a))
            profile=[(-34,20,.08),(-32,37,.44),(-23,44,.94),(-13,46,1.),(5,34,.25),(-9,28,.12)]
            faces=[]
            for side in [-2.4,2.4]:
                face=[vertex(fin_vertex(x,r,side),(ink,1.5)) for x,r,ink in profile]
                faces.append(face)
                for j in range(1,len(face)-1):
                    if side>0: tri(face[0],face[j],face[j+1])
                    else: tri(face[0],face[j+1],face[j])
            for j in range(len(profile)):
                k=(j+1)%len(profile)
                lo,hi=faces
                tri(lo[j],lo[k],hi[k])
                tri(lo[j],hi[k],hi[j])
    buff=u.GeometryScriptSimpleMeshBuffers()
    buff.vertices,buff.uv0,buff.triangles=vertices,uvs,triangles
    dynamic=u.DynamicMesh()
    u.GeometryScript_MeshEdits.append_buffers_to_mesh(dynamic,buff)
    opts=u.GeometryScriptCreateNewStaticMeshAssetOptions()
    opts.enable_collision=False
    opts.enable_recompute_normals=True
    opts.enable_recompute_tangents=True
    mesh,outcome=u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dynamic,FX+'/'+name,opts)
    if not mesh or outcome!=u.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError('Cannot author '+name)
    mesh.set_material(0,material)
    save(mesh)
    return mesh


assets.make_directory(FX)
hull_mat=make_material('M_HomingBall_RaceShell',False)
hull=make_mesh('SM_HomingBall_Pod',hull_mat)
jet=make_mesh('SM_HomingBall_Jet',make_material('M_HomingBall_Jet',True),True)
ball=load(DATA+'/DA_Ball_Homing')
new_ball=not ball
if not ball:
    base=load(DATA+'/DA_Ball_Explosive')
    factory=u.DataAssetFactory()
    factory.set_editor_property('data_asset_class',u.HALHomingBallDefinition)
    ball=tools.create_asset('DA_Ball_Homing',DATA,u.HALHomingBallDefinition,factory)
    for key in ('physics','radius','visual','gameplay','presentation'):
        ball.set_editor_property(key,base.get_editor_property(key))
    visual=ball.get_editor_property('visual')
    visual.set_editor_property('mesh',hull)
    visual.set_editor_property('materials',[hull_mat])
    ball.set_editor_property('visual',visual)
    for key,value in [('exhaust_mesh',jet),('cue_material',load('/Game/Assets/FX/SpecialBalls/TimedBomb/M_TimedBomb_Pulse')),
                      ('core_color',u.LinearColor(*navy,1.)),('current_color',u.LinearColor(*turquoise,1.)),
                      ('corona_color',u.LinearColor(*tangerine,1.)),('body_corona_width',0.),
                      ('glow',1.15),('arrival_seconds',.62),('expiration_seconds',.48),('impact_seconds',.56),('ready_for_use',True)]:
        ball.set_editor_property(key,value)
physics=ball.get_editor_property('physics')
pose=ball.get_editor_property('presentation')
if new_ball or not physics.get_editor_property('hidden_in_game') or not pose.get_editor_property('enabled'):
    # Let the rendered hull follow the ordinary controlled-ball pose. The Chaos sphere is only the game collider.
    physics.set_editor_property('hidden_in_game',True)
    pose.set_editor_property('enabled',True)
    ball.set_editor_property('physics',physics)
    ball.set_editor_property('presentation',pose)
    save(ball)
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))/'Tools'/'SpecialBall'))
from CreateSupplyPads import pad_material
pad_material()
supply=load(DATA+'/DA_Arena_SpecialBallSupply')
pool=list(supply.get_editor_property('ball_pool'))
if u.HALPadBall.HOMING not in pool:
    supply.set_editor_property('ball_pool',pool+[u.HALPadBall.HOMING])
    save(supply)
u.log('HOMING_BALL_READY '+ball.get_path_name()+' pool='+str(list(supply.get_editor_property('ball_pool'))))
