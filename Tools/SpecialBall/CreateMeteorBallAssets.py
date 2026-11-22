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


def make_material(name, inputs, shader_code, translucent=False):
    if assets.does_asset_exist(FX + '/' + name):
        return load(FX + '/' + name)
    mat = tools.create_asset(name, FX, u.Material, u.MaterialFactoryNew())
    mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    if translucent:
        mat.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property('two_sided', True)
    shader = edit.create_material_expression(mat, u.MaterialExpressionCustom, 0, 0)
    shader.set_editor_property('description', name)
    shader.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT4 if translucent else u.CustomMaterialOutputType.CMOT_FLOAT3)
    pins, links = [], []
    classes = {'UV': u.MaterialExpressionTextureCoordinate, 'Clock': u.MaterialExpressionTime, 'Lane': u.MaterialExpressionVertexColor,
               'N': u.MaterialExpressionPixelNormalWS, 'Rim': u.MaterialExpressionFresnel}
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
    for alpha, prop in [(False, u.MaterialProperty.MP_EMISSIVE_COLOR)] + ([(True, u.MaterialProperty.MP_OPACITY)] if translucent else []):
        out = shader
        if translucent:
            out = edit.create_material_expression(mat, u.MaterialExpressionComponentMask, 230, 120 if alpha else 0)
            for channel in ('r', 'g', 'b', 'a'):
                out.set_editor_property(channel, (channel == 'a') == alpha)
            if not edit.connect_material_expressions(shader, '', out, ''):
                raise RuntimeError('Missing colour/alpha mask')
        if not alpha and name in ('M_Meteor_ImpactBillow', 'M_Meteor_ImpactFront'):
            out = compensate_impact_exposure(mat, out)
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
                      ('marker_core',core),('marker_jaws',jaws),('core_color',u.LinearColor(*CORE,1.)),
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

meteor = seed()
updated = False
for field, asset in [('airflow_mesh', load('/Game/Assets/FX/BullCharge/SM_BullCharge_ConnectedFlow')),
                     ('airflow_material', wind),
                     ('descent_smoke_material', wake), ('impact_cloud_mesh', impact_cloud_mesh(cloud)),
                     ('impact_cloud_material', cloud), ('impact_ground_material', ground_impact)]:
    if not meteor.get_editor_property(field):
        meteor.set_editor_property(field, asset)
        updated = True
if updated:
    save(meteor)
u.log('METEOR_BALL_READY '+meteor.get_path_name())
