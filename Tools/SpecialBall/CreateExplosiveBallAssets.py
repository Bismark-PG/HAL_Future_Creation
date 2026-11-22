"""Build the original dark-sun materials and seed one editable production DA in Unreal Editor."""
import unreal

FX = '/Game/Assets/FX/SpecialBalls'
DATA = '/Game/Data/Balls'
edit = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def existing(folder, name):
    path = folder + '/' + name
    return unreal.load_asset(path) if assets.does_asset_exist(path) else None


def save(asset):
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + asset.get_path_name())


def connect(node, output, material_input):
    if not edit.connect_material_property(node, output, material_input):
        raise RuntimeError('Unconnected ' + str(material_input))


def custom_expression(mat, label, x, y, output_type, bindings, code):
    shader = edit.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    shader.set_editor_property('description', label)
    shader.set_editor_property('output_type', output_type)
    pins = []
    for key, _, _ in bindings:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', key)
        pins.append(pin)
    shader.set_editor_property('inputs', pins)
    shader.set_editor_property('code', code)
    for key, source, output in bindings:
        if not edit.connect_material_expressions(source, output, shader, key):
            raise RuntimeError('Unconnected ' + label + ' input ' + key)
    return shader


def material(name):
    mat = tools.create_asset(name, FX, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    return mat


def surface():
    name = 'M_ExplosiveBall_DarkSun'
    authored = existing(FX, name)
    if authored:
        return authored
    mat = material(name)
    shader = edit.create_material_expression(mat, unreal.MaterialExpressionCustom, 0, 0)
    shader.set_editor_property('description', 'Solar convection, hot filaments and a thin chromosphere')
    shader.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    pins = []
    nodes = []

    def pin(key, node, output=''):
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', key)
        pins.append(entry)
        nodes.append((node, output, key))

    pin('UV', edit.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -700, -420))
    pin('Clock', edit.create_material_expression(mat, unreal.MaterialExpressionTime, -700, -300))
    rim = edit.create_material_expression(mat, unreal.MaterialExpressionFresnel, -700, -180)
    rim.set_editor_property('exponent', 3.8)
    pin('Rim', rim)
    for index, (key, value) in enumerate((('FlowSpeed', .65), ('Glow', 1.65), ('WarnAt', 1.e15), ('HolderPulse', 0.))):
        node = edit.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, index * 100)
        node.set_editor_property('parameter_name', key)
        node.set_editor_property('default_value', value)
        pin(key, node)
    for index, (key, value) in enumerate((('CoreColor', (.075, .009, .023)), ('CurrentColor', (.54, .052, .012)),
                                          ('CoronaColor', (1., .52, .075)), ('HolderColor', (1., 1., 1.)))):
        node = edit.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 400 + index * 145)
        node.set_editor_property('parameter_name', key)
        node.set_editor_property('default_value', unreal.LinearColor(*value, 1.))
        pin(key, node, 'RGB')
    shader.set_editor_property('inputs', pins)
    shader.set_editor_property('code', """
float theta = 6.283185 * UV.x, phi = 3.141593 * UV.y;
float3 p = float3(cos(theta)*sin(phi), sin(theta)*sin(phi), cos(phi));
float t = Clock * FlowSpeed;
float3 q = p * 4.4 + .65*sin(p.yzx*3.3 + float3(.31,-.21,.17)*t);
float convection = sin(q.x + sin(q.y-.41*t)) + .72*sin(q.y + sin(q.z + .38*t)) + .46*sin(1.75*q.z + .28*t);
float veins = abs(sin(2.3*convection + .38*sin(q.x*2.6-q.y + .45*t)));
float ridge = 1.-smoothstep(.08,.39,veins);
float hot = (1.-smoothstep(.015,.11,veins))*smoothstep(.3,1.1,convection);
float3 ink = lerp(CoreColor, CurrentColor, .13 + .29*smoothstep(-1.4,1.2,convection) + .53*ridge);
ink = lerp(ink, CoronaColor, .78*hot);
ink += CoronaColor * smoothstep(.33,.93,Rim) * (.34+.27*smoothstep(-1.,1.,convection));
float pulse = lerp(1., .45+.55*pow(.5+.5*cos(10.*(Clock-WarnAt)),2.), step(WarnAt,Clock));
return Glow*(ink*pulse + HolderColor*HolderPulse*.28*smoothstep(.19,.86,Rim));
""")
    for node, output, key in nodes:
        if not edit.connect_material_expressions(node, output, shader, key):
            raise RuntimeError('Unconnected dark sun input ' + key)
    connect(shader, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(mat)
    save(mat)
    return mat


def corona():
    name = 'M_SpecialBall_Corona'
    authored = existing(FX, name)
    if authored:
        return authored
    mat = material(name)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('two_sided', True)
    color = edit.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -220, 0)
    connect(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    connect(color, 'A', unreal.MaterialProperty.MP_OPACITY)
    edit.recompile_material(mat)
    save(mat)
    return mat


def body_corona():
    name = 'M_ExplosiveBall_BodyCorona'
    authored = existing(FX, name)
    if authored:
        return authored
    mat = material(name)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    shader = edit.create_material_expression(mat, unreal.MaterialExpressionCustom, 0, 0)
    shader.set_editor_property('description', 'Animated golden rim and broad prominences on the solar atmosphere')
    shader.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)

    def node(cls, x, y):
        return edit.create_material_expression(mat, cls, x, y)

    def scalar(key, value, y):
        param = node(unreal.MaterialExpressionScalarParameter, -850, y)
        param.set_editor_property('parameter_name', key)
        param.set_editor_property('default_value', value)
        return param

    def colour(key, value, y):
        param = node(unreal.MaterialExpressionVectorParameter, -850, y)
        param.set_editor_property('parameter_name', key)
        param.set_editor_property('default_value', unreal.LinearColor(*value, 1.))
        return param

    def wire(a, output, b, pin):
        if not edit.connect_material_expressions(a, output, b, pin):
            raise RuntimeError('Could not connect body corona ' + pin)

    width = scalar('Width', .30, -400)
    radius = scalar('RadiusCm', 100., -550)
    distance = node(unreal.MaterialExpressionMultiply, -420, -460)
    offset = node(unreal.MaterialExpressionMultiply, -120, -460)
    wire(width, '', distance, 'A')
    wire(radius, '', distance, 'B')
    wire(node(unreal.MaterialExpressionVertexNormalWS, -420, -620), '', offset, 'A')
    wire(distance, '', offset, 'B')
    connect(offset, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    bindings = [
        ('UV', node(unreal.MaterialExpressionTextureCoordinate, -850, -260), ''),
        ('N', node(unreal.MaterialExpressionPixelNormalWS, -850, -140), ''),
        ('V', node(unreal.MaterialExpressionCameraVectorWS, -850, -40), ''),
        ('Clock', node(unreal.MaterialExpressionTime, -850, 60), ''),
        ('Width', width, ''), ('Intensity', scalar('Intensity', 2.4, 170), ''),
        ('FlowSpeed', scalar('FlowSpeed', .65, 280), ''), ('WarnAt', scalar('WarnAt', 1.e9, 390), ''),
        ('CurrentColor', colour('CurrentColor', (.54, .052, .012), 520), 'RGB'),
        ('CoronaColor', colour('CoronaColor', (1., .52, .075), 660), 'RGB'),
    ]
    inputs = []
    for key, _, _ in bindings:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', key)
        inputs.append(pin)
    shader.set_editor_property('inputs', inputs)
    for key, expression, output in bindings:
        wire(expression, output, shader, key)
    shader.set_editor_property('code', """
float mu = saturate(dot(normalize(N), normalize(V)));
float r = (1. + Width) * sqrt(saturate(1. - mu*mu));
float d = saturate((r - 1.) / max(.001, Width));
float phi = 3.141593*UV.y, theta = 6.283185*UV.x, t = Clock*FlowSpeed;
float3 p = float3(sin(phi)*cos(theta), sin(phi)*sin(theta), cos(phi));
float wave = .5 + .5*sin(7.*p.x - 4.*p.z + 1.15*t + 1.8*sin(5.*p.y - .7*t));
float tip = .30 + .58*wave;
float mantle = 1.-smoothstep(tip-.17, tip+.08, d);
float arc = (1.-smoothstep(.055, .22, abs(d-tip))) * (.3+.7*wave);
float lip = 1.-smoothstep(.06, .38, d);
float inside = smoothstep(.90, 1.01, r);
float alpha = inside * saturate(.72*mantle+.72*arc+.38*lip) * (1.-smoothstep(.90, 1., d));
float3 ink = lerp(CurrentColor, CoronaColor, saturate(.15+.7*lip+.65*arc));
float warning = lerp(1., .48+.52*pow(.5+.5*cos(10.*(Clock-WarnAt)),2.), step(WarnAt,Clock));
return float4(ink * Intensity * (1.+.4*arc) * warning, alpha);
""")
    for y, channels, output in ((0, ('r','g','b'), unreal.MaterialProperty.MP_EMISSIVE_COLOR),
                                (140, ('a',), unreal.MaterialProperty.MP_OPACITY)):
        mask = node(unreal.MaterialExpressionComponentMask, 290, y)
        for channel in ('r','g','b','a'):
            mask.set_editor_property(channel, channel in channels)
        wire(shader, '', mask, '')
        connect(mask, '', output)
    edit.recompile_material(mat)
    save(mat)
    return mat


def add_prominence_flow(mat):
    label = 'Painted currents along solar prominences'
    code = """
// UV.x goes from the eruption to the foot of each arch; UV.y wraps its section.
// The original shader owns growth and coverage. Only the colour of the arches is layered here.
if (Data.y >= .5 || Strength <= 0.) return Base;
float u = UV.x, tau = 6.283185;
float across = .5 - .5*cos(tau*UV.y);
float delay = frac(Data.y);
float travel = max(Progress - delay, 0.) * Travel;
float phase = 1.65*(u - travel) + 4.*delay;
float t = frac(phase);
float head = smoothstep(.08,.46,t)*(1. - smoothstep(.60,.93,t));
float veinCenter = .51 + .13*sin(tau*phase) + .035*sin(10.*u);
float veinWidth = .023 + .105*head;
float vein = head*(1. - smoothstep(.35*veinWidth,veinWidth,abs(across-veinCenter)));
float emberCenter = .21 + .085*sin(tau*(phase+.12));
float emberWidth = .022 + .073*head;
float filament = head*(1. - smoothstep(.32*emberWidth,emberWidth,abs(across-emberCenter)));

// A copper wake crosses the sunlit face where a gold streak alone would wash out.
float w = frac(phase + .46);
float wakeHead = smoothstep(.04,.42,w)*(1. - smoothstep(.59,.96,w));
float wakeCenter = .84 + .115*sin(tau*(phase+.34)) - .025*sin(8.*u);
float wakeWidth = .026 + .115*wakeHead;
float wake = wakeHead*(1. - smoothstep(.32*wakeWidth,wakeWidth,abs(across-wakeCenter)));

float weight = saturate(Strength) * smoothstep(.035,.15,u) * (1.-smoothstep(.86,.98,u));
float3 copper = Glow*(.80*CurrentColor + .02*CoronaColor);
float3 ember = Glow*lerp(.82*CurrentColor,CoronaColor,.27);
float3 sunlit = Glow*(.84*lerp(CoronaColor,float3(1.,.85,.43),.22));
float3 tinted = lerp(Base,copper,weight*wake);
// A warmer filament stays readable on the shadowed fold in the overhead view.
tinted = lerp(tinted,ember,weight*filament);
return lerp(tinted,sunlit,weight*vein);
"""
    expressions = edit.get_material_expressions(mat)
    previous = [n for n in expressions if isinstance(n, unreal.MaterialExpressionCustom)
                and n.get_editor_property('description') == label]
    if previous:
        if len(previous) != 1:
            raise RuntimeError('Ambiguous prominence paint in ' + mat.get_path_name())
        if previous[0].get_editor_property('code') == code:
            return False
        previous[0].set_editor_property('code', code)
        return True

    emissive = unreal.MaterialProperty.MP_EMISSIVE_COLOR
    original = edit.get_material_property_input_node(mat, emissive)
    if not original:
        raise RuntimeError('Missing solar eruption colour output')
    old_output = edit.get_material_property_input_node_output_name(mat, emissive)
    uv = {n.get_editor_property('coordinate_index'): n for n in expressions
          if isinstance(n, unreal.MaterialExpressionTextureCoordinate)}
    params = {str(n.get_editor_property('parameter_name')): n for n in expressions
              if isinstance(n, (unreal.MaterialExpressionScalarParameter, unreal.MaterialExpressionVectorParameter))}
    bindings = [('Base', original, old_output), ('UV', uv[0], ''), ('Data', uv[1], ''),
                ('Progress', params['Progress'], ''), ('Glow', params['Glow'], ''),
                ('CurrentColor', params['CurrentColor'], 'RGB'), ('CoronaColor', params['CoronaColor'], 'RGB')]
    for key, name, value, y in [('Travel', 'ProminenceFlowTravel', .729, 870), ('Strength', 'ProminenceFlowStrength', .70, 980)]:
        param = edit.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -950, y)
        param.set_editor_property('parameter_name', name)
        param.set_editor_property('default_value', value)
        bindings.append((key, param, ''))
    shader = custom_expression(mat, label, 200, 100, unreal.CustomMaterialOutputType.CMOT_FLOAT3, bindings, code)
    connect(shader, '', emissive)
    return True


ERUPTION_POSE = """
float delay = frac(Data.y);
float age = saturate((Progress - delay)/(1. - delay));
float growth;
if (Data.y < .5)
{
    // Keep the original attack, then draw the arch back along its tested cover rays as it cools.
    float release = smoothstep(lerp(.48,1.,delay), 1. + TailRatio, Progress);
    growth = (.20 + .80*smoothstep(0.,.35,age)) * (1. - .14*release);
}
else
{
    growth = Data.y < 2. ? (.16 + .84*smoothstep(0.,.48,age)) * (1. - .10*smoothstep(.66,1.,age)) : 1.;
}
return P * (min(growth, saturate(Data.x)) - 1.);
"""


def add_prominence_tail(mat):
    label = 'Cooling folds and feathered arch afterglow'
    code = """
// Smoke and impact marks finish on the base clock. UV.y is periodic around each closed ribbon.
if (Data.y >= .5) return float4(Paint, Progress < 1. ? Coverage : 0.);
float delay = Data.y;
float onset = lerp(.48, 1., delay);
if (Progress <= onset) return float4(Paint, Coverage);

float release = saturate((Progress - onset)/(1. + TailRatio - onset));
float u = UV.x;
float fold = 6.283185*UV.y + .78*sin(5.*u + 48.*delay) + 1.15*u;
float ridge = .5 + .5*cos(2.*fold - .65*release + 43.*delay);
float breaks = .5 + .5*cos(8.7*u + sin(fold) + 65.*delay);
// Broad, curved strands cool at different times. There is no advancing crosscut.
float life = .38 + .17*u + .31*ridge + .12*breaks;
// Leave a few narrow crests alight after the broad faces have thinned.
life = lerp(life, .99, .62*ridge*ridge*ridge);
float density = 1. - smoothstep(max(.025,life-.46), min(.995,life+.025), release);
float cooling = smoothstep(max(0.,life-.50), life, release);
float3 ember = .28*Paint + .20*Glow*CurrentColor;
return float4(lerp(Paint, ember, .82*cooling), density*step(.008,Data.x));
"""
    expressions = edit.get_material_expressions(mat)
    custom = {str(n.get_editor_property('description')): n for n in expressions if isinstance(n, unreal.MaterialExpressionCustom)}
    pose = custom['Grow along cover-clipped rays']
    existing_tail = custom.get(label)
    changed = False
    if not existing_tail:
        uv = {n.get_editor_property('coordinate_index'): n for n in expressions if isinstance(n, unreal.MaterialExpressionTextureCoordinate)}
        params = {str(n.get_editor_property('parameter_name')): n for n in expressions
                  if isinstance(n, (unreal.MaterialExpressionScalarParameter, unreal.MaterialExpressionVectorParameter))}
        ratio = edit.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -950, 1120)
        ratio.set_editor_property('parameter_name', 'ProminenceTailRatio')
        ratio.set_editor_property('default_value', 0.)
        # Append the pin without rebuilding the existing geometry or its cover inputs.
        pins = list(pose.get_editor_property('inputs'))
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', 'TailRatio')
        pose.set_editor_property('inputs', pins + [pin])
        if not edit.connect_material_expressions(ratio, '', pose, 'TailRatio'):
            raise RuntimeError('Unconnected arch recoil duration')
        bindings = []
        for key, prop in [('Paint', unreal.MaterialProperty.MP_EMISSIVE_COLOR), ('Coverage', unreal.MaterialProperty.MP_OPACITY_MASK)]:
            source = edit.get_material_property_input_node(mat, prop)
            if not source:
                raise RuntimeError('Missing solar eruption ' + key)
            bindings.append((key, source, edit.get_material_property_input_node_output_name(mat, prop)))
        bindings += [('UV', uv[0], ''), ('Data', uv[1], ''), ('Progress', params['Progress'], ''), ('TailRatio', ratio, ''),
                     ('Glow', params['Glow'], ''), ('CurrentColor', params['CurrentColor'], 'RGB')]
        existing_tail = custom_expression(mat, label, 480, 120, unreal.CustomMaterialOutputType.CMOT_FLOAT4, bindings, code)
        for channels, y, prop in [('rgb', 100, unreal.MaterialProperty.MP_EMISSIVE_COLOR), ('a', 250, unreal.MaterialProperty.MP_OPACITY_MASK)]:
            mask = edit.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 775, y)
            for c in 'rgba':
                mask.set_editor_property(c, c in channels)
            if not edit.connect_material_expressions(existing_tail, '', mask, ''):
                raise RuntimeError('Unconnected solar afterglow ' + channels)
            connect(mask, '', prop)
        changed = True
    for node, source in [(pose, ERUPTION_POSE), (existing_tail, code)]:
        if node.get_editor_property('code') != source:
            node.set_editor_property('code', source)
            changed = True
    # A fractional density feathers the paint against the actual arena while retaining masked depth and cover.
    for key, value in [('dither_opacity_mask', True), ('opacity_mask_clip_value', .5)]:
        if mat.get_editor_property(key) != value:
            mat.set_editor_property(key, value)
            changed = True
    return changed


def eruption_material():
    name = 'M_ExplosiveBall_Eruption'
    mat = existing(FX, name)
    if mat:
        return mat
    mat = material(name)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('two_sided', True)

    def node(cls, x, y):
        return edit.create_material_expression(mat, cls, x, y)

    def wire(source, output, dest, pin):
        if not edit.connect_material_expressions(source, output, dest, pin):
            raise RuntimeError('Unconnected solar eruption input ' + pin)

    def scalar(key, value, y):
        param = node(unreal.MaterialExpressionScalarParameter, -950, y)
        param.set_editor_property('parameter_name', key)
        param.set_editor_property('default_value', value)
        return param

    def colour(key, value, y):
        param = node(unreal.MaterialExpressionVectorParameter, -950, y)
        param.set_editor_property('parameter_name', key)
        param.set_editor_property('default_value', unreal.LinearColor(*value, 1.))
        return param

    world = node(unreal.MaterialExpressionWorldPosition, -1250, -500)
    world.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    offset = node(unreal.MaterialExpressionSubtract, -900, -460)
    wire(world, '', offset, 'A')
    wire(node(unreal.MaterialExpressionActorPositionWS, -1250, -380), '', offset, 'B')
    data = node(unreal.MaterialExpressionTextureCoordinate, -950, -230)
    data.set_editor_property('coordinate_index', 1)
    progress = scalar('Progress', 0., -110)
    shared = [('Data', data, ''), ('Progress', progress, '')]
    pose = custom_expression(mat, 'Grow along cover-clipped rays', -480, -450, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                             [('P', offset, '')] + shared, ERUPTION_POSE)
    connect(pose, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    bindings = shared + [
        ('UV', node(unreal.MaterialExpressionTextureCoordinate, -950, 10), ''),
        ('N', node(unreal.MaterialExpressionPixelNormalWS, -950, 110), ''),
        ('Face', node(unreal.MaterialExpressionTwoSidedSign, -950, 210), ''),
        ('Glow', scalar('Glow', 1.65, 310), ''),
        ('CoreColor', colour('CoreColor', (.075, .009, .023), 420), 'RGB'),
        ('CurrentColor', colour('CurrentColor', (.54, .052, .012), 560), 'RGB'),
        ('CoronaColor', colour('CoronaColor', (1., .52, .075), 700), 'RGB'),
    ]
    ink = custom_expression(mat, 'Painted solar ribbons and ember clouds', -420, 110, unreal.CustomMaterialOutputType.CMOT_FLOAT4, bindings, """
float delay = frac(Data.y);
float age = saturate((Progress - delay)/(1. - delay));
float3 normal = normalize(N) * Face;
float light = .5 + .5*dot(normal, normalize(float3(-.40,-.52,.75)));
float tau = 6.283185, u = UV.x, v = UV.y;
float3 dark = .43*CoreColor + .012*CurrentColor;
float3 orange = .82*CurrentColor + .025*CoronaColor;
float3 gold = .84*lerp(CoronaColor, float3(1.,.85,.43), .22);
float mask;
float3 color;
if (Data.y < .5)
{
    // Keep the inner fold in shadow even at steep player camera angles.
    float across = .5 - .5*cos(tau*v);
    float brush = .09*sin(9.*u - age) + .13*pow(saturate(sin(19.*u + 1.6*age)),5.);
    float key = across + .09*(light-.5) + brush*sin(3.14159*u);
    color = lerp(dark, orange, smoothstep(.40,.46,key));
    color = lerp(color, gold, smoothstep(.78,.83,key));
    float stroke = (1. - smoothstep(.016,.043,abs(across-.63-.08*sin(8.*u-age)))) *
        smoothstep(.12,.32,u)*(1.-smoothstep(.50,.77,u));
    color = lerp(color, gold, stroke);
    float front = 1.08*smoothstep(0.,.36,age);
    float tail = 1.13*smoothstep(.55,1.,age) - .065;
    float cut = u + .035*sin(tau*v + 3.*u);
    mask = step(cut,front)*step(tail,cut);
}
else if (Data.y < 2.)
{
    // Each puff has a soft scalloped cap, a red-orange cheek and a dark underside.
    float cheek = .065*sin(tau*u*2. + 1.3*sin(tau*v) + delay*20.);
    float key = light + cheek - .10;
    color = lerp(dark*.95, orange*.53, smoothstep(.46,.52,key));
    color = lerp(color, lerp(orange,gold,.27), smoothstep(.75,.81,key));
    float breakup = .52 + .19*cos(2.*tau*u + sin(tau*v)) + .13*sin(1.5*tau*v + delay*15.);
    mask = step(.025, age)*step(smoothstep(.54,.96,age),breakup);
}
else
{
    // Two small counterposed strokes show where each rising ribbon lands.
    color = lerp(orange, dark, smoothstep(.57,.65,v));
    color = lerp(color, gold*.88, (1.-smoothstep(.16,.23,v)) * smoothstep(.13,.43,u));
    mask = step(u,1.05*smoothstep(0.,.22,age))*step(smoothstep(.34,.87,age),u+.08);
}
return float4(color * Glow, mask * step(.008,Data.x) * step(delay,Progress));
""")
    for y, channels, output in ((100, ('r', 'g', 'b'), unreal.MaterialProperty.MP_EMISSIVE_COLOR),
                                (240, ('a',), unreal.MaterialProperty.MP_OPACITY_MASK)):
        mask = node(unreal.MaterialExpressionComponentMask, -70, y)
        for channel in ('r', 'g', 'b', 'a'):
            mask.set_editor_property(channel, channel in channels)
        wire(ink, '', mask, '')
        connect(mask, '', output)
    add_prominence_flow(mat)
    add_prominence_tail(mat)
    edit.recompile_material(mat)
    save(mat)
    return mat


def apply_body_size_and_corona(ball, atmosphere):
    diameter_cm = 200.
    visual = ball.get_editor_property('visual')
    transform = visual.get_editor_property('relative_transform')
    # The production sphere mesh is 100 cm across, with its origin at the centre.
    scale = diameter_cm / 100.
    transform.set_editor_property('scale3d', unreal.Vector(scale, scale, scale))
    visual.set_editor_property('relative_transform', transform)
    ball.set_editor_property('visual', visual)
    ball.set_editor_property('radius', diameter_cm * .5)
    ball.set_editor_property('body_corona_material', atmosphere)


def build():
    body, cue, atmosphere = surface(), corona(), body_corona()
    name = 'DA_Ball_Explosive'
    ball = existing(DATA, name)
    if ball:
        flags = unreal.SystemLibrary.get_command_line()
        changed = False
        eruption_to_save = None
        if '-UpgradeExplosiveBallCorona' in flags and not ball.get_editor_property('body_corona_material'):
            apply_body_size_and_corona(ball, atmosphere)
            changed = True
        link_eruption = '-UpgradeExplosiveBallEruption' in flags and not ball.get_editor_property('eruption_material')
        if link_eruption:
            ball.set_editor_property('eruption_material', eruption_material())
            changed = True
        if '-UpgradeExplosiveBallProminenceFlow' in flags:
            mat = existing(FX, 'M_ExplosiveBall_Eruption')
            if mat and mat == ball.get_editor_property('eruption_material'):
                first_upgrade = 'ProminenceFlowStrength' not in [str(n) for n in edit.get_scalar_parameter_names(mat)]
                if add_prominence_flow(mat):
                    edit.recompile_material(mat)
                    eruption_to_save = mat
                if (first_upgrade or link_eruption) and abs(ball.get_editor_property('prominence_height') - .67) < .0001:
                    ball.set_editor_property('prominence_height', .50)
                    changed = True
        if '-UpgradeExplosiveBallProminenceTail' in flags:
            mat = existing(FX, 'M_ExplosiveBall_Eruption')
            if mat and mat == ball.get_editor_property('eruption_material'):
                first_tail = 'ProminenceTailRatio' not in [str(n) for n in edit.get_scalar_parameter_names(mat)]
                if add_prominence_tail(mat):
                    edit.recompile_material(mat)
                    eruption_to_save = mat
                if (first_tail or link_eruption) and ball.get_editor_property('prominence_tail_seconds') == 0.:
                    ball.set_editor_property('prominence_tail_seconds', .26)
                    changed = True
        if changed:
            save(ball)
        # Save the graph last so an interrupted production DA upgrade can be retried.
        if eruption_to_save:
            save(eruption_to_save)
        return
    baseline = unreal.load_asset('/Game/Data/MVP_B/DA_Ball_Normal_TestBaseline_v1')
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('DataAssetClass', unreal.HALExplosiveBallDefinition)
    ball = tools.create_asset(name, DATA, unreal.HALExplosiveBallDefinition, factory)
    free_spin_damping = ball.get_editor_property('physics').get_editor_property('body').get_editor_property('angular_damping')
    for field in ('physics', 'radius', 'presentation'):
        ball.set_editor_property(field, baseline.get_editor_property(field))
    physics = ball.get_editor_property('physics')
    rigid_body = physics.get_editor_property('body')
    rigid_body.set_editor_property('angular_damping', free_spin_damping)
    physics.set_editor_property('body', rigid_body)
    ball.set_editor_property('physics', physics)
    visual = unreal.VehicleVisualConfig()
    visual.set_editor_property('mesh', unreal.load_asset('/Engine/BasicShapes/Sphere'))
    visual.set_editor_property('materials', [body])
    ball.set_editor_property('visual', visual)
    apply_body_size_and_corona(ball, atmosphere)
    ball.set_editor_property('gameplay', baseline.get_editor_property('gameplay'))
    gameplay = ball.get_editor_property('gameplay')
    state = gameplay.get_editor_property('state')
    state.set_editor_property('ground_roll_release_speed', 0.)
    gameplay.set_editor_property('state', state)
    damage = gameplay.get_editor_property('damage')
    damage.set_editor_property('version', unreal.HALCombatConfigVersion.LEGACY_FIXED_DAMAGE)
    damage.set_editor_property('vehicle_hit_damage', 25.)
    gameplay.set_editor_property('damage', damage)
    impact = gameplay.get_editor_property('impact')
    impact.set_editor_property('tier', unreal.HALImpactTier.HEAVY)
    impact.set_editor_property('push_preset', unreal.HALImpactPushPreset.STANDARD)
    gameplay.set_editor_property('impact', impact)
    ball.set_editor_property('gameplay', gameplay)
    ball.set_editor_property('cue_material', cue)
    ball.set_editor_property('eruption_material', eruption_material())
    ball.set_editor_property('prominence_height', .50)
    ball.set_editor_property('prominence_tail_seconds', .26)
    ball.set_editor_property('ready_for_use', True)
    save(ball)


build()
unreal.log('SPECIAL_BALL_ASSETS_READY')
