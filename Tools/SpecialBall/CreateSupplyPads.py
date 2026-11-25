"""Author the production pad mesh, materials, shared supply DA and two native actors in MVPB_Match."""
import unreal as u

ASSETS = u.EditorAssetLibrary
TOOLS = u.AssetToolsHelpers.get_asset_tools()
EDIT = u.MaterialEditingLibrary
FOLDER = '/Game/Assets/ArenaMechanics/SpecialBallPad'
SOURCE = '/Game/Imported/Platformer14SciFi/Prefabs/Decorations/SM_0204_PF_flower_001'
DATA = '/Game/Data/Balls'
LEVEL = '/Game/Maps/MVPB_Match'


def load(path):
    return ASSETS.load_asset(path) if ASSETS.does_asset_exist(path) else None


def save(obj):
    if not ASSETS.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError('Failed saving ' + obj.get_path_name())


def wire(source, pin, target, name):
    if not EDIT.connect_material_expressions(source, pin, target, name):
        raise RuntimeError('Could not connect ' + name)


def add_clasp_warning(mat):
    shader = EDIT.get_material_property_input_node(mat, u.MaterialProperty.MP_EMISSIVE_COLOR)
    code = shader.get_editor_property('code') if isinstance(shader,u.MaterialExpressionCustom) else ''
    if 'Kind > 3.5' in code:
        return
    marker = 'float focus ='
    if marker not in code:
        raise RuntimeError('Cannot locate the six-port warning in ' + mat.get_path_name())
    # Preserve the other three authored pad signals when upgrading an existing material.
    insert = """if (Kind > 3.5) {
    float tick = smoothstep(.28,.78,cos(12.*a));
    float clasp = smoothstep(.16,.40,abs(sin(a))) * (1.-smoothstep(.065,.16,abs(r-.6+.23*t)));
    tint = lerp(float3(.91,.026,.07),float3(1.,.60,.16),tick*ring);
    light = .84*clasp+.68*ring*tick;
}
"""
    shader.set_editor_property('code',code.replace(marker,insert+marker,1))
    EDIT.recompile_material(mat)
    save(mat)


def pad_material():
    name = 'M_SpecialBallPad'
    old = load(FOLDER + '/' + name)
    if old:
        add_clasp_warning(old)
        return old
    mat = TOOLS.create_asset(name, FOLDER, u.Material, u.MaterialFactoryNew())

    def node(cls, x, y):
        return EDIT.create_material_expression(mat, cls, x, y)

    def scalar(key, value, y, index=None):
        p = node(u.MaterialExpressionScalarParameter, -650, y)
        p.set_editor_property('parameter_name', key)
        p.set_editor_property('default_value', value)
        if index is not None:
            p.set_editor_property('use_custom_primitive_data', True)
            p.set_editor_property('primitive_data_index', index)
        return p

    def output(source, prop):
        if not EDIT.connect_material_property(source, '', prop):
            raise RuntimeError('Unconnected pad material ' + str(prop))

    base = node(u.MaterialExpressionVectorParameter, 0, -390)
    base.set_editor_property('parameter_name', 'Finish')
    base.set_editor_property('default_value', u.LinearColor(.075, .13, .17, 1.))
    output(base, u.MaterialProperty.MP_BASE_COLOR)
    output(scalar('Metallic', .72, -380), u.MaterialProperty.MP_METALLIC)
    output(scalar('Roughness', .34, -280), u.MaterialProperty.MP_ROUGHNESS)
    bindings = [('Pos', node(u.MaterialExpressionWorldPosition, -900, 0)),
                ('Origin', node(u.MaterialExpressionObjectPositionWS, -900, 110)),
                ('BoundsRadius', node(u.MaterialExpressionObjectRadius, -900, 220)),
                ('Clock', node(u.MaterialExpressionTime, -900, 330)),
                ('Kind', scalar('SupplyKind', 0., 450, 0)),
                ('StartTime', scalar('SupplyLocalStart', 0., 540, 1)),
                ('Duration', scalar('SupplyDuration', 1., 630, 2)),
                ('Lens', scalar('Lens', 0., 720))]
    shader = node(u.MaterialExpressionCustom, 0, 80)
    shader.set_editor_property('description', 'Six deck ports feed a solar pulse, a violet spiral, or a meteor streak')
    shader.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
    pins = []
    for key, _ in bindings:
        p = u.CustomInput()
        p.set_editor_property('input_name', key)
        pins.append(p)
    shader.set_editor_property('inputs', pins)
    shader.set_editor_property('code', """
float2 p = (Pos.xy - Origin.xy) / max(1., BoundsRadius * .71);
float r = length(p);
float a = atan2(p.y, p.x + .00001);
float3 idle = float3(.08,.41,.49) * Lens * (.65 + .35*(1.-smoothstep(.08,.29,r)));
if (Kind < .5) return idle;
float t = saturate((Clock-StartTime)/max(.01,Duration));
float gain = smoothstep(0.,.16,t);
float ring = pow(saturate(.5+.5*cos(6.283*(2.*r+2.35*t))),7.);
float rays = smoothstep(.76,.965,cos(6.*a));
float head = 1.-smoothstep(.025,.10,abs(r-(.82-.73*t)));
float light = .50*ring*rays + .8*head;
float3 tint = float3(1.,.30,.035);
if (Kind > 1.5 && Kind < 2.5) {
    tint = float3(.48,.13,1.);
    light = .9*smoothstep(.50,.94,cos(3.*a+8.*r-10.*t))*(.38+.62*ring);
} else if (Kind > 2.5) {
    tint = float3(1.,.14,.055);
    float streak = smoothstep(.37,.94,cos(6.*a + 1.1*r));
    light = ring*streak + head*.35;
}
float focus = (1.-smoothstep(.08,.29,r))*(.3+1.1*t*t);
float outer = 1.-smoothstep(.82,.97,r);
return idle*(1.-gain) + gain*tint*(Lens*(.78+.58*ring+focus) + outer*(light+focus)*(.55+.6*t));
""")
    for key, source in bindings:
        wire(source, '', shader, key)
    output(shader, u.MaterialProperty.MP_EMISSIVE_COLOR)
    add_clasp_warning(mat)
    return mat


def finish(parent, label, rgb, metal, roughness, lens):
    name = 'MI_SpecialBallPad_' + label
    old = load(FOLDER + '/' + name)
    if old:
        return old
    mi = TOOLS.create_asset(name, FOLDER, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    EDIT.set_material_instance_parent(mi, parent)
    EDIT.set_material_instance_vector_parameter_value(mi, 'Finish', u.LinearColor(*rgb, 1.))
    for key, v in [('Metallic', metal), ('Roughness', roughness), ('Lens', lens)]:
        EDIT.set_material_instance_scalar_parameter_value(mi, key, v)
    save(mi)
    return mi


def make_mesh(materials):
    path = FOLDER + '/SM_SpecialBallPad'
    mesh = load(path) or ASSETS.duplicate_asset(SOURCE, path)
    if not mesh:
        raise RuntimeError('Could not duplicate the flower casting')
    if any(mesh.get_material(i) != mat for i, mat in enumerate(materials)):
        for i, mat in enumerate(materials):
            mesh.set_material(i, mat)
        save(mesh)
    return mesh


def make_da(mesh):
    name = 'DA_Arena_SpecialBallSupply'
    da = load(DATA + '/' + name)
    if not da:
        factory = u.DataAssetFactory()
        factory.set_editor_property('data_asset_class', u.HALSpecialBallSupplyDefinition)
        da = TOOLS.create_asset(name, DATA, u.HALSpecialBallSupplyDefinition, factory)
    if da.get_editor_property('pad_mesh') != mesh:
        da.set_editor_property('pad_mesh', mesh)
        save(da)


def install_sites(mesh):
    if not u.EditorLoadingAndSavingUtils.load_map(LEVEL):
        raise RuntimeError('Could not open ' + LEVEL)
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    placed = {a.get_actor_label(): a for a in actors.get_all_level_actors() if isinstance(a, u.HALSpecialBallPad)}
    changed = False
    # Opposite intermediate lanes, outside the kickoff, punch paths and deck lamps.
    for order, suffix, x, y in [(0, 'NE', 2941.262, 2923.811), (1, 'SW', -3058.738, -2576.189)]:
        label = 'SpecialBallPad_' + suffix
        if label in placed:
            continue
        pad = actors.spawn_actor_from_class(u.HALSpecialBallPad, u.Vector(x, y, -200.274))
        pad.set_actor_label(label)
        pad.set_folder_path('Gimmick/Special Ball Pads')
        pad.set_editor_property('rotation_order', order)
        # Both footprints cross the 3.75 cm raised portions of the hex deck.
        pad.set_editor_property('deck_finish_offset', 3.75)
        surface = pad.get_component_by_class(u.StaticMeshComponent)
        if surface.static_mesh != mesh or surface.get_collision_enabled() != u.CollisionEnabled.NO_COLLISION:
            raise RuntimeError('Check mesh and collision for ' + label)
        changed = True
    if changed and not u.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError('Failed to save ' + LEVEL)


if __name__ == '__main__':
    ASSETS.make_directory(FOLDER)
    master = pad_material()
    deck = make_mesh([
        finish(master, 'Housing', (.055,.12,.16), .68, .35, 0.),
        finish(master, 'Lenses', (.08,.19,.22), .21, .25, 1.),
        finish(master, 'Steel', (.23,.29,.33), .83, .28, 0.)])
    make_da(deck)
    install_sites(deck)
    u.log('Special ball supply assets and production pad sites ready.')
