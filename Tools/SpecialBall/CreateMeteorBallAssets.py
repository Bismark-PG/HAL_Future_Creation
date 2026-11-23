"""Create the editable meteor type, its FX materials and the approved B marker layers."""
import math
from pathlib import Path
import unreal as u

FX = '/Game/Assets/FX/SpecialBalls/Meteor'
DATA = '/Game/Data/Balls'
assets = u.EditorAssetLibrary
edit = u.MaterialEditingLibrary
tools = u.AssetToolsHelpers.get_asset_tools()
CORE = (.006, .008, .010)
CURRENT = (.86, .065, .004)
CORONA = (1.65, .54, .06)
REFRESH_VOLUME = '-RefreshMeteorVolume' in u.SystemLibrary.get_command_line().split()


def load(path):
    obj = u.load_asset(path)
    if not obj:
        raise RuntimeError('Missing ' + path)
    return obj


def save(obj):
    if not assets.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError('Cannot save ' + obj.get_path_name())


def compensate_impact_exposure(mat, color):
    # Protect the painted orange, charcoal and cream values in the bright arena.
    exposure = edit.create_material_expression(mat, u.MaterialExpressionEyeAdaptationInverse, 440, 0)
    if not edit.connect_material_expressions(color, '', exposure, ''):
        raise RuntimeError('Cannot bind impact exposure')
    return exposure


def bind_lobe_swelling(mat, phase, banks):
    normal = edit.create_material_expression(mat, u.MaterialExpressionVertexNormalWS, -470, 920)
    radius = edit.create_material_expression(mat, u.MaterialExpressionObjectRadius, -470, 1020)
    amount = edit.create_material_expression(mat, u.MaterialExpressionScalarParameter, -470, 1120)
    amount.set_editor_property('parameter_name', 'LobeSwelling')
    amount.set_editor_property('default_value', 1.)
    offset = edit.create_material_expression(mat, u.MaterialExpressionCustom, 0, 850)
    offset.set_editor_property('description', 'Inflate cloud lobes once, with a stable base in each fold')
    offset.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
    args = [('N', normal), ('Radius', radius), ('Amount', amount), ('Phase', phase), ('Banks', banks)]
    pins = []
    for name, _ in args:
        pin = u.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    offset.set_editor_property('inputs', pins)
    offset.set_editor_property('code', """
float inflation=smoothstep(.035+.12*Banks.b,.45+.18*Banks.b,Phase);
return N*Radius*(.074*Amount)*inflation*(1.-Banks.r)*(.55+.45*Banks.g);
""")
    for name, node in args:
        if not edit.connect_material_expressions(node, '', offset, name):
            raise RuntimeError('Missing lobe displacement pin ' + name)
    if not edit.connect_material_property(offset, '', u.MaterialProperty.MP_WORLD_POSITION_OFFSET):
        raise RuntimeError('Cannot bind lobe displacement')


def make_material(name, inputs, shader_code, translucent=False):
    if assets.does_asset_exist(FX + '/' + name):
        mat = load(FX + '/' + name)
        if not (name == 'M_Meteor_ImpactVolume' and REFRESH_VOLUME):
            return mat
        edit.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(name, FX, u.Material, u.MaterialFactoryNew())
    mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    if translucent:
        mat.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property('two_sided', name != 'M_Meteor_ImpactVolume')
    shader = edit.create_material_expression(mat, u.MaterialExpressionCustom, 0, 0)
    shader.set_editor_property('description', name)
    shader.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT4 if translucent else u.CustomMaterialOutputType.CMOT_FLOAT3)
    pins, links = [], []
    classes = {'UV': u.MaterialExpressionTextureCoordinate, 'Clock': u.MaterialExpressionTime,
               'Lane': u.MaterialExpressionVertexColor, 'Banks': u.MaterialExpressionVertexColor,
               'N': u.MaterialExpressionPixelNormalWS, 'V': u.MaterialExpressionCameraVectorWS, 'Rim': u.MaterialExpressionFresnel}
    for index, (key, value) in enumerate(inputs):
        if isinstance(value, u.Texture2D):
            cls = u.MaterialExpressionTextureObjectParameter
        else:
            cls = classes[key] if value is None else (u.MaterialExpressionVectorParameter if isinstance(value, tuple) else u.MaterialExpressionScalarParameter)
        node = edit.create_material_expression(mat, cls, -480, (index-3)*100)
        if value is not None:
            node.set_editor_property('parameter_name', key)
            if isinstance(value, u.Texture2D):
                node.set_editor_property('texture', value)
            else:
                node.set_editor_property('default_value', u.LinearColor(*value, 1.) if isinstance(value, tuple) else value)
        pin = u.CustomInput()
        pin.set_editor_property('input_name', key)
        pins.append(pin)
        links.append((node, 'RGB' if isinstance(value, tuple) else '', key))
    shader.set_editor_property('inputs', pins)
    shader.set_editor_property('code', shader_code)
    for node, output, key in links:
        if not edit.connect_material_expressions(node, output, shader, key):
            raise RuntimeError('Missing pin ' + key)
    if name == 'M_Meteor_ImpactVolume':
        nodes = {key: node for node, _, key in links}
        bind_lobe_swelling(mat, nodes['Phase'], nodes['Banks'])
    for alpha, prop in [(False, u.MaterialProperty.MP_EMISSIVE_COLOR)] + ([(True, u.MaterialProperty.MP_OPACITY)] if translucent else []):
        out = shader
        if translucent:
            out = edit.create_material_expression(mat, u.MaterialExpressionComponentMask, 230, 120 if alpha else 0)
            for channel in ('r', 'g', 'b', 'a'):
                out.set_editor_property(channel, (channel == 'a') == alpha)
            if not edit.connect_material_expressions(shader, '', out, ''):
                raise RuntimeError('Missing colour/alpha mask')
        if not alpha and name in ('M_Meteor_ImpactBillow', 'M_Meteor_ImpactFront', 'M_Meteor_ImpactVolume'):
            out = compensate_impact_exposure(mat, out)
        if alpha and name == 'M_Meteor_ImpactVolume':
            contact = edit.create_material_expression(mat, u.MaterialExpressionDepthFade, 440, 150)
            contact.set_editor_property('fade_distance_default', 12.)
            if not edit.connect_material_expressions(out, '', contact, ''):
                raise RuntimeError('Cannot soften the cloud at an intersecting surface')
            out = contact
        if not edit.connect_material_property(out, '', prop):
            raise RuntimeError('Missing material output')
    if name in ('M_Meteor_Plume', 'M_Meteor_RollingSmoke'):
        mat.set_editor_property('used_with_instanced_static_meshes', True)
    edit.recompile_material(mat)
    save(mat)
    return mat


def meteor_texture(name, *, ui):
    path = FX + '/' + name
    if assets.does_asset_exist(path):
        return load(path)
    source = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / 'Tools' / 'SpecialBall' / 'Art' / (name+'.png')
    task = u.AssetImportTask()
    task.set_editor_property('filename', str(source))
    task.set_editor_property('destination_path', FX)
    task.set_editor_property('automated', True)
    tools.import_asset_tasks([task])
    texture = load(path)
    texture.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON if ui else u.TextureCompressionSettings.TC_BC7)
    texture.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI if ui else u.TextureGroup.TEXTUREGROUP_EFFECTS)
    if not ui:
        texture.set_editor_property('address_x', u.TextureAddress.TA_CLAMP)
        texture.set_editor_property('address_y', u.TextureAddress.TA_CLAMP)
        texture.set_editor_property('power_of_two_mode', u.TexturePowerOfTwoSetting.RESIZE_TO_SPECIFIC_RESOLUTION)
        texture.set_editor_property('resize_during_build_x', 1024)
        texture.set_editor_property('resize_during_build_y', 1024)
    texture.set_editor_property('srgb', True)
    save(texture)
    return texture


surface = make_material('M_Meteor_Cinder', [('UV',None),('N',None),('Rim',None),('Clock',None),
    ('CoreColor',CORE),('CurrentColor',CURRENT),('CoronaColor',CORONA),('Glow',1.25),('FlowSpeed',.8),
    ('WarnAt',1.e15),('HolderPulse',0.),('HolderColor',(1.,1.,1.))], """
float a = UV.x*6.283185, b=UV.y*3.141593;
float3 p=float3(cos(a)*sin(b),sin(a)*sin(b),cos(b))*2.7;
float3 cell=floor(p), f=frac(p);
float nearD=10., nextD=10., facet=.5;
[unroll] for(int x=-1;x<=1;x++) [unroll] for(int y=-1;y<=1;y++) [unroll] for(int z=-1;z<=1;z++){
    float3 v=float3(x,y,z), q=cell+v;
    float3 h=frac(sin(float3(dot(q,float3(12.1,73.6,18.2)),dot(q,float3(28.2,15.3,71.3)),dot(q,float3(63.3,19.3,24.1))))*4375.89);
    float3 d=v+.17+.66*h-f; float k=dot(d,d);
    if(k<nearD){nextD=nearD; nearD=k; facet=h.x;} else nextD=min(nextD,k);
}
float edge=sqrt(nextD)-sqrt(nearD), t=Clock*FlowSpeed;
float light=saturate(.55+.43*dot(normalize(N),normalize(float3(-.4,-.55,.72)))+.18*(facet-.5));
float3 stone=CoreColor*(.54+.75*smoothstep(.28,.33,light)+.95*smoothstep(.70,.76,light));
stone+=float3(.008,.011,.015)*smoothstep(.71,.80,light);
float furnace=.74+.22*sin(1.8*t+dot(p,float3(.7,1.6,.9)));
float3 col=lerp(stone,CurrentColor*furnace,1.-smoothstep(.045,.080,edge));
col=lerp(col,CoronaColor*furnace,1.-smoothstep(.010,.024,edge));
float warn=lerp(1.,.75+.25*cos(10.*(Clock-WarnAt)),step(WarnAt,Clock));
return Glow*(col*warn + Rim*(CurrentColor*.34+HolderColor*HolderPulse*.33));
""")
plume = make_material('M_Meteor_Plume', [('N',None),('Tint',CURRENT)], """
float f=dot(normalize(N),normalize(float3(-.40,-.60,.69)));
return Tint*(.40 + .50*smoothstep(-.27,-.18,f) + .52*smoothstep(.49,.59,f));
""")
wake = make_material('M_Meteor_RollingSmoke', [('UV',None),('N',None),('Tint',(.012,.014,.016)),('SmokeTime',0.)], """
float a=6.283185*UV.x, b=3.141593*UV.y;
float3 p=float3(cos(a)*sin(b),sin(a)*sin(b),cos(b));
float curl=sin(dot(p,float3(3.4,-2.1,1.7))+SmokeTime*4.1+.75*sin(p.z*3.-SmokeTime*2.5));
float shade=dot(normalize(N),normalize(float3(-.42,-.55,.72)))+.34*curl;
float mid=smoothstep(-.28,-.12,shade), light=smoothstep(.47,.68,shade);
return Tint*(.55+1.45*mid+2.05*light);
""")
wind = make_material('M_Meteor_Slipstream', [('UV',None),('Lane',None),('MotionTime',0.),('FlowSpeed',2.4),
    ('Opacity',.72),('Envelope',1.)], """
float t=MotionTime*FlowSpeed, phase=6.283185*(1.55*UV.x-t+Lane.r);
float crest=.5+.5*cos(phase);
float center=.18*sin(phase*.87+Lane.r*3.);
float side=abs(UV.y*2.-1.-center);
float tip=smoothstep(.055,.21,UV.x)*(1.-smoothstep(.68,.99,UV.x));
float width=lerp(.16,.46,crest)*tip;
float stroke=1.-smoothstep(.43*width,width+.055,side);
float3 color=lerp(float3(.20,.31,.39),float3(.68,.82,.83),crest);
return float4(color,Envelope*Opacity*tip*stroke*(.16+.82*smoothstep(.12,.80,crest)));
""", translucent=True)
warning = make_material('M_Meteor_Landing', [('UV',None),('RimColor',CORONA),('FallProgress',0.),('ImpactProgress',-1.)], """
float2 v=2.*UV-1.; float r=length(v); float aa=.007;
float angle=atan2(v.y,v.x); float p=saturate(FallProgress);
float outer=1.-smoothstep(.014,.014+aa,abs(r-.978)); outer*=1.-smoothstep(.995,1.,r);
float shadowR=lerp(.10,.93,p*p*(3.-2.*p));
float shadow=1.-smoothstep(shadowR-.09,shadowR+.025,r);
float inner=(1.-smoothstep(.008,.008+aa,abs(r-.918)))*smoothstep(.3,.7,cos(12.*angle));
float3 ink=float3(.0006,.00035,.00065);
if(ImpactProgress<0.) {
    float rimMask=max(outer,inner*.68), opacity=max(shadow*(.30+.48*p),rimMask*.90);
    return float4(lerp(ink,RimColor*(.30+.52*p),rimMask),opacity);
}
float t=saturate(ImpactProgress), front=1.-pow(1.-t,3.);
float wave=(1.-smoothstep(.02,.065,abs(r-front*.975)))*(1.-t);
float wedge=.18+.115*cos(9.*angle)+.07*cos(5.*angle+.8);
float scorch=(1.-smoothstep(wedge+.22,wedge+.32,r))*(1.-smoothstep(.32,1.,t));
float cracks=(1.-smoothstep(.035,.095,abs(sin(7.*angle+2.*r))))*(1.-smoothstep(.30,.78,r))*scorch;
float flash=max(wave,max(cracks,(1.-smoothstep(.01,.17,t))*scorch));
return float4(lerp(ink,RimColor*2.2,flash),max(scorch*.65,flash)*(1.-smoothstep(.88,1.,t)));
""", translucent=True)


def seed():
    name = 'DA_Ball_Meteor'
    if assets.does_asset_exist(DATA+'/'+name):
        return load(DATA+'/'+name)
    base = load(DATA+'/DA_Ball_Explosive')
    core, jaws = meteor_texture('T_MeteorLockCore', ui=True), meteor_texture('T_MeteorLockJaws', ui=True)
    factory = u.DataAssetFactory()
    factory.set_editor_property('data_asset_class', u.HALMeteorBallDefinition)
    ball = tools.create_asset(name, DATA, u.HALMeteorBallDefinition, factory)
    for field in ('physics','radius','visual','gameplay','presentation','free_life_seconds','warning_seconds','cue_material'):
        ball.set_editor_property(field,base.get_editor_property(field))
    visual = ball.get_editor_property('visual')
    visual.set_editor_property('materials',[surface])
    ball.set_editor_property('visual',visual)
    for key,value in [('ground_plane',load('/Engine/BasicShapes/Plane')),('flame_mesh',load('/Engine/BasicShapes/Cone')),
                      ('warning_material',warning),('plume_material',plume),
                      ('marker_core',core),('marker_jaws',jaws),('marker_size',96.),('core_color',u.LinearColor(*CORE,1.)),
                      ('current_color',u.LinearColor(*CURRENT,1.)),('corona_color',u.LinearColor(*CORONA,1.)),
                      ('glow',1.25),('surface_flow_speed',.8),('arrival_seconds',.62),('expiration_seconds',.65),
                      ('impact_seconds',.80),('ready_for_use',True)]:
        ball.set_editor_property(key,value)
    save(ball)
    return ball


def impact_cloud_mesh(material):
    path = FX + '/SM_Meteor_ImpactCrest'
    if assets.does_asset_exist(path):
        return load(path)
    # The sheet climbs from a low hot tongue into a rolled lip. UVs follow its surface.
    segments = 16
    vertices, texcoords, triangles = [], [], []
    for j in range(segments + 1):
        v = j / segments
        for i in range(segments + 1):
            s = i / segments
            x = 100.*s - 10.*s**3
            y = 90.*(v - .5) + 4.*math.sin(math.pi*s)
            z = 40.*math.sin(math.pi*s**1.42)**1.15 * (.56 + .44*math.sin(math.pi*v)) + 5.*s
            vertices.append(u.Vector(x, y, z))
            texcoords.append(u.Vector2D(s, v))
            if i < segments and j < segments:
                a = j*(segments+1) + i
                b, c = a+1, a+segments+1
                triangles.extend([u.IntVector(a, c, b), u.IntVector(b, c, c+1)])
    buffers = u.GeometryScriptSimpleMeshBuffers()
    buffers.vertices, buffers.uv0, buffers.triangles = vertices, texcoords, triangles
    dynamic = u.DynamicMesh()
    u.GeometryScript_MeshEdits.append_buffers_to_mesh(dynamic, buffers)
    options = u.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.enable_collision = False
    options.enable_recompute_normals = True
    options.enable_recompute_tangents = True
    mesh, _ = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dynamic, path, options)
    if not mesh:
        raise RuntimeError('Failed to create ' + path)
    mesh.set_material(0, material)
    save(mesh)
    return mesh


paint = meteor_texture('T_Meteor_ImpactBillow', ui=False)
cloud = make_material('M_Meteor_ImpactBillow', [('UV',None),('Paint',paint),('Tint',(1.,1.,1.)),
    ('CoolColor',(.094,.052,.082)),('Phase',.32),('Curl',1.)], """
float t=saturate(Phase);
float2 pivot=float2(.43,.58), q=UV-pivot;
float inside=1.-smoothstep(.14,.70,length(q));
float turn=(smoothstep(.07,.78,t)-.44)*1.55*Curl*inside;
float sn,cs; sincos(turn,sn,cs);
float2 uv=pivot+float2(cs*q.x-sn*q.y,sn*q.x+cs*q.y);
uv.y+=.017*Curl*sin(uv.x*8.5-t*8.)*sin(3.14159*saturate(uv.x));
float4 art=Texture2DSample(Paint,PaintSampler,uv);
float luminance=dot(art.rgb,float3(.299,.587,.114));
float fold=.5+.5*sin(uv.x*11.+uv.y*6.+2.7*sin(uv.y*7.-t*3.2)-t*4.8);
float cool=smoothstep(.36,.88,t+.10*(fold-.5));
float3 soot=CoolColor*(.48+1.85*saturate(luminance))+.024*luminance;
float3 litPaint=art.rgb*Tint*float3(1.26,.87,.80)*(1.03+.12*(1.-t));
float3 color=lerp(litPaint,soot,cool);
float breakUp=.27+.43*fold+.20*uv.x+.08*sin(uv.x*20.+uv.y*14.);
float erode=1.-smoothstep(breakUp-.14,breakUp+.14,smoothstep(.62,1.,t));
float alpha=art.a*smoothstep(0.,.06,t)*(1.-smoothstep(.77,1.,t))*erode;
return float4(color,alpha);
""", translucent=True)
ground_impact = make_material('M_Meteor_ImpactFront', [('UV',None),('Progress',.32),('RimColor',CORONA),
    ('Accent',(.24,.87,1.)),('RingWidth',.046),('RingIntensity',1.25)], """
float2 pos=UV*2.-1.; float r=length(pos), t=saturate(Progress);
float angle=atan2(pos.y,pos.x), aa=clamp(fwidth(r),.001,.02);
// One travelling band. All its ink, warm light and cold glints stay inside its leading edge.
float travel=saturate(t/.44);
float front=lerp(.10,1.,1.-pow(1.-travel,2.2));
float brush=smoothstep(.12,.92,.5+.5*cos(3.*angle+1.1*sin(2.*angle-.5)-t*1.7));
float width=clamp(RingWidth,.003,.24)*lerp(.24,1.80,brush);
float behind=front-r, slice=behind/max(width,.001);
float ring=smoothstep(0.,aa,behind)*(1.-smoothstep(width-aa,width+aa,behind));
float cool=smoothstep(.60,.92,.5+.5*sin(2.*angle+.75*sin(3.*angle)+.5));
float3 edgeColor=lerp(RimColor,Accent,cool);
float glint=(1.-smoothstep(.05,.39,abs(slice-.36)))*(.53+.47*brush);
float3 ringCol=lerp(edgeColor,float3(1.7,1.45,1.12),glint);
ringCol=lerp(ringCol,float3(.16,.065,.095),smoothstep(.60,.96,slice)*(.45+.47*brush))*RingIntensity;
float ringAlpha=ring*saturate(RingIntensity)*(.46+.54*brush)*smoothstep(0.,.033,t)*(1.-smoothstep(.45,.81,t));
// A scalloped bed of fire joins the hot roots during the opening of the three crests.
float growth=1.-pow(1.-saturate(t/.38),2.65);
float lobe=.5+.5*cos(3.*angle-.9+2.1*r+.36*sin(angle*5.-t*5.));
float bedR=(.11+.89*growth)*(.34+.23*lobe+.025*sin(11.*angle+t*6.));
float bed=(1.-smoothstep(bedR-.023,bedR+.013,r))*smoothstep(0.,.06,t)*(1.-smoothstep(.36,.73,t));
float heat=1.-smoothstep(.22,.94,r/max(.02,bedR)+.13*sin(4.*angle+r*13.-t*7.));
float3 bedCol=lerp(float3(.32,.025,.011),float3(1.40,.26,.018),smoothstep(.12,.32,heat));
bedCol=lerp(bedCol,float3(2.2,1.08,.18),smoothstep(.60,.77,heat));
// A short white-hot splash and a quieter scorched footprint ground the taller crests.
float spokes=pow(.5+.5*cos(7.*angle+.6*sin(3.*angle)),5.);
float flashR=(.24+.29*spokes)*(.68+.32*smoothstep(0.,.06,t));
float flash=(1.-smoothstep(.55*flashR,flashR,r))*(1.-smoothstep(.025,.21,t));
float scorch=(1.-smoothstep(.21,.74+.06*sin(3.*angle+.4),r))*(1.-smoothstep(.30,.97,t))*.50;
// Tapered fragments use separate azimuths and flight times to avoid forming another ring.
float lane=frac(angle/6.283185+.5)*13., id=floor(lane);
float seed=frac(sin(id*78.23+1.2)*4375.54), age=saturate((t-.035-.09*seed)/.75);
float dist=(.15+.76*(1.-pow(1.-age,2.)))*(.75+.23*seed);
float along=(r-dist)/(.018+.045*(1.-age)), crosswise=((frac(lane)-.5)*.4833*r+.014*along)/(.009+.008*seed);
float ember=(1.-smoothstep(.72,1.1,abs(along)+abs(crosswise)))*smoothstep(0.,.14,age)*(1.-smoothstep(.53,.99,age));
float alpha=max(bed,max(scorch,max(flash,max(ringAlpha,ember))));
float3 ink=float3(.022,.011,.021);
float3 color=lerp(ink,bedCol,bed/max(.001,alpha));
color=lerp(color,lerp(RimColor*1.4,float3(2.7,2.3,1.6),1.-smoothstep(.06,.24,r)),flash/max(.001,alpha));
color=lerp(color,ringCol,ringAlpha/max(.001,alpha));
color=lerp(color,RimColor*1.7,ember/max(.001,alpha));
return float4(color,alpha);
""", translucent=True)

volume_material = make_material('M_Meteor_ImpactVolume',
    [('UV',None),('N',None),('V',None),('Banks',None),('Tint',(1.,1.,1.)),('CoolColor',(.094,.052,.082)),
     ('CoreDirection',(-.7,-.3,-.45)),('Phase',.30),('Heat',.88),('Seed',0.)], """
float az=6.283185*UV.x, el=3.141593*UV.y;
float3 patch=float3(cos(az)*sin(el),sin(az)*sin(el),cos(el));
// Density stays attached to the material. The moving silhouette comes from the swelling geometry.
float density=saturate(.50+.26*sin(dot(patch,float3(3.9,-2.1,1.8))+Seed)
                          +.18*cos(dot(patch,float3(1.6,3.3,-2.8))-.8*Seed)+.06*patch.z);
float3 n=normalize(N);
float fold=saturate(Banks.r);
float light=saturate(.42+.56*dot(n,normalize(float3(-.54,-.39,.75)))-.19*fold+.06*(density-.5));
float middle=smoothstep(.26,.46,light), top=smoothstep(.69,.87,light);
float3 soot=lerp(CoolColor,dot(CoolColor,float3(.24,.63,.13)).xxx,.54);
float3 ash=lerp(soot*.52,soot*1.10+float3(.029,.036,.044),middle);
ash=lerp(ash,soot*1.36+float3(.13,.145,.15),top);
float3 flame=lerp(float3(.43,.037,.007),float3(1.55,.235,.017),middle);
flame=lerp(flame,float3(2.05,.90,.15),top);
flame=lerp(flame,float3(2.80,1.78,.58),.62*smoothstep(.84,.98,light+.07*density)*Heat);
// Cool from the outer caps towards the warmer base and blast center, in broad patches.
float core=saturate(.5+.5*dot(n,normalize(CoreDirection)));
float ember=smoothstep(.38,.91,fold)*Heat*core;
flame=lerp(flame,float3(2.35,.66,.08),.29*ember);
float threshold=.17+.43*Banks.g+.16*(1.-core)+.16*density-.055*fold;
float fire=smoothstep(threshold-.16,threshold+.18,sqrt(saturate(Heat)));
float3 warmAsh=lerp(ash,Tint*float3(.50,.105,.024),.65);
float3 color=lerp(lerp(ash,warmAsh,smoothstep(.025,.48,fire)),flame*Tint,smoothstep(.25,1.,fire));
// Thinner edges and the less dense pockets clear first. The centers retain smoke for longer.
float body=.57*density+.33*(1.-fold)+.10*Banks.g;
float remaining=1.-smoothstep(.20+.19*body,.82+.18*body,Phase);
float facing=saturate(dot(n,normalize(V)));
float column=smoothstep(0.,.68,facing)*(.71+.29*facing);
float alpha=(1.-exp2(-6.*column*remaining*(.62+.38*remaining)))*smoothstep(0.,.05,Phase);
return float4(color,alpha);
""", translucent=True)


def impact_volume_mesh(material):
    path = FX + '/SM_Meteor_ImpactVolume'
    existing = load(path) if assets.does_asset_exist(path) else None
    if existing and not REFRESH_VOLUME:
        return existing
    # Sculpt one closed puff from broad overlapping lobes. Sample a smooth union on radial rays.
    lobes = [(0.,0.,0.,.52), (-.40,-.22,.26,.53), (.33,-.37,.25,.49),
             (.48,.13,.17,.44), (.04,.44,.15,.50), (-.10,.05,.53,.44),
             (.18,-.14,-.31,.50), (-.33,.16,-.12,.46), (.28,.22,.47,.32),
             (-.44,.18,.33,.32), (.04,-.49,-.04,.33)]

    def distances(point):
        x, y, z = point.x, point.y, point.z
        return [math.sqrt((x-a)**2 + (y-b)**2 + (z-c)**2) - radius for a, b, c, radius in lobes]

    mesh = u.DynamicMesh()
    u.GeometryScript_Primitives.append_sphere_lat_long(mesh, u.GeometryScriptPrimitiveOptions(), u.Transform(), 1., 32, 52)
    queries = u.GeometryScript_MeshQueries
    colors = []
    for index in range(queries.get_vertex_count(mesh)):
        direction, valid = queries.get_vertex_position(mesh, index)
        if not valid:
            raise RuntimeError('Missing vertex in meteor puff')
        lo, hi = 0., 1.5
        for _ in range(15):
            mid = .5*(lo+hi)
            combined = 10.
            for d in distances(direction*mid):
                blend = max(.12 - abs(d-combined), 0.) / .12
                combined = min(combined, d) - .03*blend*blend
            if combined < 0.:
                lo = mid
            else:
                hi = mid
        point = direction*(.5*(lo+hi))
        first, second = sorted(distances(point))[:2]
        fold = max(0., 1.-(second-first)/.24)
        colors.append(u.LinearColor(fold*fold*(3.-2.*fold), .5+.5*point.z, .5+.38*point.x+.16*point.y, 1.))
        u.GeometryScript_MeshEdits.set_vertex_position(mesh, index, point*50., defer_change_notifications=True)
    if not queries.get_is_closed_mesh(mesh):
        raise RuntimeError('Meteor puff has an open silhouette')
    u.GeometryScript_Normals.set_per_vertex_normals(mesh)
    u.GeometryScript_VertexColors.set_mesh_per_vertex_colors(mesh, u.GeometryScript_List.convert_array_to_color_list(colors))
    if existing:
        options = u.GeometryScriptCopyMeshToAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        _, outcome = u.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, existing, options, u.GeometryScriptMeshWriteLOD())
        result = existing
    else:
        options = u.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.enable_collision = False
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        result, outcome = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, path, options)
    if not result or outcome != u.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError('Cannot build ' + path)
    result.set_material(0, material)
    save(result)
    return result


meteor = seed()
updated = False
for field, asset in [('airflow_mesh', load('/Game/Assets/FX/BullCharge/SM_BullCharge_ConnectedFlow')),
                     ('airflow_material', wind),
                     ('descent_smoke_material', wake), ('impact_cloud_mesh', impact_cloud_mesh(cloud)),
                     ('impact_cloud_material', cloud), ('impact_ground_material', ground_impact)]:
    if not meteor.get_editor_property(field):
        meteor.set_editor_property(field, asset)
        updated = True
volume_mesh = impact_volume_mesh(volume_material)
if (not meteor.get_editor_property('impact_volume_mesh')
        and meteor.get_editor_property('impact_cloud_mesh') == load(FX+'/SM_Meteor_ImpactCrest')
        and meteor.get_editor_property('impact_cloud_material') == cloud):
    meteor.set_editor_property('impact_volume_mesh', volume_mesh)
    if abs(meteor.get_editor_property('cloud_lift')-.29) < .0001:
        meteor.set_editor_property('cloud_lift', .42)
    updated = True
if updated:
    save(meteor)
u.log('METEOR_BALL_READY '+meteor.get_path_name())
