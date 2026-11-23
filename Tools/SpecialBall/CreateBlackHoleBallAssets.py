"""Create the ball-only vortex material and seed its editable DA. Existing artist assets are preserved."""
import unreal

FX = '/Game/Assets/FX/SpecialBalls'
DATA = '/Game/Data/Balls'
assets = unreal.EditorAssetLibrary
materials = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
CORE = (.002, .0007, .004)
CURRENT = (.15, .025, .46)
CORONA = (.56, .30, 1.)
GLOW = 1.12


def required(path):
    obj = unreal.load_asset(path)
    if not obj:
        raise RuntimeError('Missing asset ' + path)
    return obj


def save(obj):
    if not assets.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + obj.get_path_name())


def vortex_surface():
    name = 'M_BlackHoleBall_Vortex'
    if assets.does_asset_exist(FX + '/' + name):
        return required(FX + '/' + name)
    mat = tools.create_asset(name, FX, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)

    def node(cls, y):
        return materials.create_material_expression(mat, cls, -700, y)

    bindings = [('SurfaceNormal', node(unreal.MaterialExpressionPixelNormalWS, -420), ''),
                ('Clock', node(unreal.MaterialExpressionTime, -320), ''),
                ('ViewRim', node(unreal.MaterialExpressionFresnel, -220), '')]
    for i, (key, value) in enumerate((('FlowSpeed', 1.15), ('SwirlSign', 1.), ('Glow', GLOW),
                                      ('WarnAt', 1.e15), ('HolderPulse', 0.))):
        p = node(unreal.MaterialExpressionScalarParameter, i * 95)
        p.set_editor_property('parameter_name', key)
        p.set_editor_property('default_value', value)
        bindings.append((key, p, ''))
    for i, (key, rgb) in enumerate((('CoreColor', CORE), ('CurrentColor', CURRENT),
                                    ('CoronaColor', CORONA), ('HolderColor', (1., 1., 1.)))):
        p = node(unreal.MaterialExpressionVectorParameter, 520 + i * 145)
        p.set_editor_property('parameter_name', key)
        p.set_editor_property('default_value', unreal.LinearColor(*rgb, 1.))
        bindings.append((key, p, 'RGB'))
    shader = materials.create_material_expression(mat, unreal.MaterialExpressionCustom, 0, 0)
    shader.set_editor_property('description', 'Dark eye, winding violet bands and orbiting gold motes')
    shader.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    pins = []
    for key, _, _ in bindings:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', key)
        pins.append(pin)
    shader.set_editor_property('inputs', pins)
    shader.set_editor_property('code', """
float3 n = normalize(SurfaceNormal);
float r = length(n.xy);
float a = atan2(n.y, n.x) * SwirlSign;
float t = Clock * FlowSpeed;
// World-oriented spherical paint stays coherent when the physical ball rolls or is carried.
float winding = 3.0*(a - 1.8*t + 3.3*r + .85*(1.-n.z));
float ribbon = .5 + .5*cos(winding);
float mid = smoothstep(.30,.76,ribbon);
float lit = smoothstep(.84,.98,ribbon) * (.60+.40*smoothstep(-.3,.8,n.z));
float3 ink = lerp(CoreColor, CurrentColor, .06 + .68*mid);
ink = lerp(ink, CoronaColor, .54*lit);
// An opaque event horizon and a thin bright lip keep the silhouette legible on the arena's lit deck.
ink = lerp(CoreColor*.32, ink, smoothstep(.32,.41,r));
float lip = 1.0-smoothstep(.010,.041, abs(r-.345-.008*cos(2.*a-t)));
ink = lerp(ink, CoronaColor, lip*.86);
float motes = smoothstep(.86,.99,cos(5.*(a-1.55*t))) * (1.-smoothstep(.014,.055,abs(n.z-.60-.12*sin(3.*a-4.65*t))));
ink = lerp(ink, float3(1.6,.77,.15), motes*.9);
ink += CoronaColor * (.24*smoothstep(.18,.9,ViewRim));
float warning = lerp(1., .58+.42*(.5+.5*cos(10.*(Clock-WarnAt))), step(WarnAt,Clock));
return Glow*(ink*warning + HolderColor*HolderPulse*.30*smoothstep(.16,.8,ViewRim));
""")
    for key, source, output in bindings:
        if not materials.connect_material_expressions(source, output, shader, key):
            raise RuntimeError('Missing vortex input ' + key)
    if not materials.connect_material_property(shader, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Failed to wire vortex colour')
    materials.recompile_material(mat)
    save(mat)
    return mat


def create_ball():
    name = 'DA_Ball_BlackHole'
    if assets.does_asset_exist(DATA + '/' + name):
        return required(DATA + '/' + name)
    reference = required(DATA + '/DA_Ball_Explosive')
    field = assets.load_blueprint_class('/Game/Blueprints/MapMechanic/BP_HALBlackHole')
    if not field:
        raise RuntimeError('Missing arena black hole Blueprint')
    collapse = required('/Game/Assets/FX/NS_BlackHole_Over')
    surface = vortex_surface()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.HALBlackHoleBallDefinition)
    ball = tools.create_asset(name, DATA, unreal.HALBlackHoleBallDefinition, factory)
    for key in ('physics', 'radius', 'gameplay', 'presentation', 'cue_material', 'free_life_seconds', 'warning_seconds'):
        ball.set_editor_property(key, reference.get_editor_property(key))
    ball.set_editor_property('visual', reference.get_editor_property('visual'))
    visual = ball.get_editor_property('visual')
    visual.set_editor_property('materials', [surface])
    ball.set_editor_property('visual', visual)
    ball.set_editor_property('field_class', field)
    ball.set_editor_property('expiration_vortex', collapse)
    ball.set_editor_property('core_color', unreal.LinearColor(*CORE, 1.))
    ball.set_editor_property('current_color', unreal.LinearColor(*CURRENT, 1.))
    ball.set_editor_property('corona_color', unreal.LinearColor(*CORONA, 1.))
    ball.set_editor_property('surface_flow_speed', 1.15)
    ball.set_editor_property('glow', GLOW)
    ball.set_editor_property('corona_size', 2.15)
    ball.set_editor_property('arrival_seconds', .65)
    ball.set_editor_property('expiration_seconds', .60)
    ball.set_editor_property('ready_for_use', True)
    save(ball)
    return ball


unreal.log('BLACK_HOLE_BALL_READY ' + create_ball().get_path_name())
