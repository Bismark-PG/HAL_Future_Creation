"""Author the arena generator's hologram forecast row. Idempotent: reruns rebuild the material graph in place.

Run inside the Unreal Editor (Python). Creates/updates:
  /Game/Assets/FX/ArenaGenerator/T_ArenaForecast_{Slippery,BlackHole,Electric}  placeholder icons
  /Game/Assets/FX/ArenaGenerator/M_ArenaForecast_Hologram                       one-quad row material
AHALArenaEffectGenerator drives Position / Lock / Progress / LockColor and the layout parameters at runtime.
"""
from pathlib import Path
import unreal as u

FX = '/Game/Assets/FX/ArenaGenerator'
ART = Path(__file__).parent / 'Art' if '__file__' in globals() else Path(u.Paths.project_dir()) / 'Tools/ArenaGenerator/Art'
assets = u.EditorAssetLibrary
tools = u.AssetToolsHelpers.get_asset_tools()
edit = u.MaterialEditingLibrary

# UV is the quad's 0..1 space with V down. Everything is laid out in world cm so C++ and the look agree.
SHADER = """
float2 P=(UV-.5)*PanelSize.xy;
float S=IconSize, G=Spacing, rowY=-PanelSize.y*.5+S*.72;
float2 gx=ddx(UV)*PanelSize.xy, gy=ddy(UV)*PanelSize.xy;
float3 holo=HoloColor, lockc=lerp(holo,LockColor,Lock);
float3 col=0; float a=0;

float2 pb=abs(P-float2(0,rowY))-float2(PanelSize.x*.5-S*.16,S*.56);
float pd=length(max(pb,0.))+min(max(pb.x,pb.y),0.)-S*.1;
float plate=1.-smoothstep(-1.5,1.5,pd);
col=holo*.16; a=plate*.34;
float rim=(1.-smoothstep(S*.008,S*.022,abs(pd)))*.55;
col=lerp(col,holo,rim); a=max(a,rim);

float y0=rowY+S*.98, y1=PanelSize.y*.5;
float bt=saturate((P.y-y0)/max(1.,y1-y0));
float bw=lerp(PanelSize.x*.34,S*.05,bt);
float beam=step(y0,P.y)*(1.-smoothstep(bw*.55,bw,abs(P.x)))*(.16+.34*bt);
col=lerp(col,holo*1.2,saturate(beam*3.)); a=max(a,beam);

float c=round(Position);
for(int i=-2;i<=2;i++)
{
    float k=c+i, d=k-Position, w=saturate(abs(d));
    float sz=S*lerp(1.,SideScale,w);
    float2 q=(P-float2(d*G,rowY))/sz+.5;
    if(q.x>0.&&q.x<1.&&q.y>0.&&q.y<1.)
    {
        float m=fmod(fmod(k,3.)+3.,3.);
        float2 ex=gx/sz, ey=gy/sz;
        float4 t=m<.5?Texture2DSampleGrad(IconSlippery,IconSlipperySampler,q,ex,ey)
            :m<1.5?Texture2DSampleGrad(IconBlackHole,IconBlackHoleSampler,q,ex,ey)
            :Texture2DSampleGrad(IconElectric,IconElectricSampler,q,ex,ey);
        float fade=1.-smoothstep(1.05,1.55,abs(d));
        float ia=t.a*fade*lerp(1.,.7,w);
        float3 ic=lerp(holo,LockColor,Lock*(1.-w))*lerp(1.,SideBrightness,w)*t.rgb;
        col=lerp(col,ic,ia); a=max(a,ia);
    }
}

float2 fb=abs(P-float2(0,rowY))-S*.44;
float fd=length(max(fb,0.))+min(max(fb.x,fb.y),0.)-S*.12;
float frame=1.-smoothstep(S*.014,S*.03,abs(fd));
col=lerp(col,lockc*1.25,frame); a=max(a,frame);

if(Progress>=0.)
{
    float2 r=P-float2(0,rowY+S*.8); float hw=S*.9, hh=S*.032;
    float track=(1.-smoothstep(hw-1.,hw+1.,abs(r.x)))*(1.-smoothstep(hh-1.,hh+1.,abs(r.y)));
    float fill=track*step(r.x,-hw+2.*hw*saturate(Progress));
    col=lerp(col,holo*.22,track*.8); a=max(a,track*.5);
    col=lerp(col,lockc*1.25,fill); a=max(a,fill);
}

float scan=.80+.20*sin(P.y/S*95.-Clock*5.);
float flick=.96+.04*sin(Clock*41.);
return float4(col*Glow*scan*flick,saturate(a)*Opacity*lerp(.86,1.,scan));
"""

SCALARS = [('Position', 1.), ('Lock', 1.), ('Progress', .6), ('IconSize', 220.), ('Spacing', 230.),
           ('SideScale', .65), ('SideBrightness', .45), ('Glow', 1.6), ('Opacity', .92)]
VECTORS = [('PanelSize', (746., 462., 0.)), ('HoloColor', (.16, .78, 1.)), ('LockColor', (.58, .10, 1.))]
ICONS = [('IconSlippery', 'T_ArenaForecast_Slippery'), ('IconBlackHole', 'T_ArenaForecast_BlackHole'),
         ('IconElectric', 'T_ArenaForecast_Electric')]


def existing(path):
    return assets.load_asset(path) if assets.does_asset_exist(path) else None


def save(obj):
    if not assets.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + obj.get_path_name())


def icon(name):
    source = ART / (name + '.png')
    if not source.exists():
        raise RuntimeError('Missing %s; run MakePlaceholderIcons.py first' % source)
    task = u.AssetImportTask()
    task.filename = str(source)
    task.destination_path = FX
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    texture = existing(FX + '/' + name)
    if not texture:
        raise RuntimeError('Could not import ' + name)
    texture.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('address_x', u.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', u.TextureAddress.TA_CLAMP)
    texture.set_editor_property('srgb', True)
    save(texture)
    return texture


def hologram(textures):
    name = 'M_ArenaForecast_Hologram'
    mat = existing(FX + '/' + name)
    if mat:
        edit.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(name, FX, u.Material, u.MaterialFactoryNew())
    mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('two_sided', True)
    shader = edit.create_material_expression(mat, u.MaterialExpressionCustom, 0, 0)
    shader.set_editor_property('description', name)
    shader.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT4)
    wires, pins, row = [], [], 0

    def add(cls, key, output=''):
        nonlocal row
        node = edit.create_material_expression(mat, cls, -420, row * 84)
        row += 1
        pin = u.CustomInput()
        pin.set_editor_property('input_name', key)
        pins.append(pin)
        wires.append((node, output, key))
        return node

    add(u.MaterialExpressionTextureCoordinate, 'UV')
    add(u.MaterialExpressionTime, 'Clock')
    for key, value in SCALARS:
        node = add(u.MaterialExpressionScalarParameter, key)
        node.set_editor_property('parameter_name', key)
        node.set_editor_property('default_value', value)
    for key, value in VECTORS:
        node = add(u.MaterialExpressionVectorParameter, key, 'RGB')
        node.set_editor_property('parameter_name', key)
        node.set_editor_property('default_value', u.LinearColor(*value, 1.))
    for (key, _), texture in zip(ICONS, textures):
        node = add(u.MaterialExpressionTextureObjectParameter, key)
        node.set_editor_property('parameter_name', key)
        node.set_editor_property('texture', texture)
        node.set_editor_property('sampler_type', u.MaterialSamplerType.SAMPLERTYPE_COLOR)
    shader.set_editor_property('inputs', pins)
    shader.set_editor_property('code', SHADER)
    for node, output, key in wires:
        if not edit.connect_material_expressions(node, output, shader, key):
            raise RuntimeError('Missing material input ' + key)
    for alpha in (False, True):
        mask = edit.create_material_expression(mat, u.MaterialExpressionComponentMask, 220, 200 if alpha else 0)
        for channel in 'rgba':
            mask.set_editor_property(channel, (channel == 'a') == alpha)
        if not edit.connect_material_expressions(shader, '', mask, ''):
            raise RuntimeError('Missing material output')
        if alpha:
            ok = edit.connect_material_property(mask, '', u.MaterialProperty.MP_OPACITY)
        else:
            # Same convention as the other arena FX: keep authored colours under the night map's auto exposure.
            exposure = edit.create_material_expression(mat, u.MaterialExpressionEyeAdaptationInverse, 460, 0)
            ok = edit.connect_material_expressions(mask, '', exposure, '') \
                and edit.connect_material_property(exposure, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
        if not ok:
            raise RuntimeError('Missing material binding')
    edit.recompile_material(mat)
    save(mat)
    return mat


assets.make_directory(FX)
material = hologram([icon(name) for _, name in ICONS])
print('ArenaForecast assets ready:', material.get_path_name())
