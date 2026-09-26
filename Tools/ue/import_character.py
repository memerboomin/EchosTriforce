# Importe les personnages texturés produits par Tools/blender/make_character.py (Tools/art/out/<nom>/) :
# atlas Body/Gear, matériau M_ZToonTex (dessin + teinte de tunique), instances et maillages squelettiques
# sur le squelette du mannequin UE5, rangés dans /Game/Art/Characters/<Nom>.
# Éditeur complet (l'import de maillages squelettiques plante en commandlet) :
#   UnrealEditor EchosTriforce.uproject -ExecutePythonScript=Tools/ue/import_character.py -unattended -nosplash
import os
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
NAMES = ["link"]
M_DIR = "/Game/Art/Materials"
AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("ECHOS " + msg)


def node(mat, cls, x, y, **props):
    e = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def sparam(mat, name, value, x, y):
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def link(a, a_out, b, b_in):
    if not MEL.connect_material_expressions(a, a_out, b, b_in):
        log("liaison échouée %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def to_prop(e, out, prop):
    if not MEL.connect_material_property(e, out, prop):
        log("propriété échouée %s -> %s" % (e.get_name(), prop))


def import_texture(path, dest, name, normal=False):
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = dest
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = False
    AT.import_asset_tasks([t])
    tex = unreal.load_asset(dest + "/" + name)
    if normal:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER_NORMAL_MAP)
    else:
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
    EAL.save_loaded_asset(tex)
    return tex


# Décalage de teinte des verts (tunique, bonnet) : Hue < 0 = dessin d'origine
TUNIC_HLSL = """
float3 c = C;
if (Hue < 0) return c;
float mx = max(c.r, max(c.g, c.b));
float mn = min(c.r, min(c.g, c.b));
float d = mx - mn;
float h = 0;
if (d > 1e-5)
{
    if (mx == c.r) h = (c.g - c.b) / d;
    else if (mx == c.g) h = (c.b - c.r) / d + 2;
    else h = (c.r - c.g) / d + 4;
    h = frac(h / 6 + 1);
}
float s = mx > 1e-5 ? d / mx : 0;
float hd = h * 360;
float w = smoothstep(58, 78, hd) * (1 - smoothstep(168, 188, hd)) * smoothstep(0.06, 0.18, s);
float3 k = float3(1.0, 2.0 / 3.0, 1.0 / 3.0);
float3 p = saturate(abs(frac(Hue + k) * 6 - 3) - 1);
float3 rgb = mx * Val * lerp(float3(1, 1, 1), p, saturate(s * Sat));
return lerp(c, rgb, w);
"""


def vparam(mat, name, color, x, y):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*color))


def toon_tex_material(default_tex):
    """Dessin projeté : UV0 = planche de face, UV1 = planche de dos, UV2 = (part de la face, part peinte),
    UV3 = (part du vert du bonnet, luminosité) pour les côtés de la tête peints (cf. make_character.py)."""
    name = "M_ZToonTex"
    path = M_DIR + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = AT.create_asset(name, M_DIR, unreal.Material, unreal.MaterialFactoryNew())
    tex = node(m, unreal.MaterialExpressionTextureObjectParameter, -1900, -300, parameter_name="Tex", texture=default_tex,
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    def tc(i, y):
        return node(m, unreal.MaterialExpressionTextureCoordinate, -1900, y, coordinate_index=i)
    samples = []
    for i, y in ((1, -100), (0, 100)):
        smp = node(m, unreal.MaterialExpressionTextureSample, -1600, y, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        link(tex, "", smp, "Tex")
        link(tc(i, y), "", smp, "UVs")
        samples.append(smp)
    def mask(src, r, g, y):
        mk = node(m, unreal.MaterialExpressionComponentMask, -1600, y, r=r, g=g, b=False, a=False)
        link(src, "", mk, "")
        return mk
    data = tc(2, 300)
    side = tc(3, 500)
    front_w = mask(data, True, False, 300)
    side_w = mask(data, False, True, 380)
    cap_k = mask(side, True, False, 500)
    bright = mask(side, False, True, 580)
    drawn = node(m, unreal.MaterialExpressionLinearInterpolate, -1300, 0)
    link(samples[0], "RGB", drawn, "A")
    link(samples[1], "RGB", drawn, "B")
    link(front_w, "", drawn, "Alpha")
    painted = node(m, unreal.MaterialExpressionLinearInterpolate, -1300, 450)
    link(vparam(m, "HairSide", (0.44, 0.30, 0.08, 1), -1600, 700), "", painted, "A")
    link(vparam(m, "CapSide", (0.10, 0.16, 0.07, 1), -1600, 850), "", painted, "B")
    link(cap_k, "", painted, "Alpha")
    shaded = node(m, unreal.MaterialExpressionMultiply, -1100, 450)
    link(painted, "", shaded, "A")
    link(bright, "", shaded, "B")
    col = node(m, unreal.MaterialExpressionLinearInterpolate, -950, 150)
    link(drawn, "", col, "A")
    link(shaded, "", col, "B")
    link(side_w, "", col, "Alpha")
    c = node(m, unreal.MaterialExpressionCustom, -750, 150)
    c.set_editor_property("code", TUNIC_HLSL)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property("description", "TunicShift")
    ins = []
    for n in ("C", "Hue", "Sat", "Val"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    link(col, "", c, "C")
    link(sparam(m, "TunicHue", -1.0, -1000, 650), "", c, "Hue")
    link(sparam(m, "TunicSat", 1.0, -1000, 750), "", c, "Sat")
    link(sparam(m, "TunicVal", 1.0, -1000, 850), "", c, "Val")
    tint = node(m, unreal.MaterialExpressionVectorParameter, -750, 400, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
    base = node(m, unreal.MaterialExpressionMultiply, -500, 200)
    link(c, "", base, "A")
    link(tint, "", base, "B")
    # Le dessin porte déjà ses ombres : part d'auto-éclairage + liseré de lumière, éclairage dynamique atténué
    fres = node(m, unreal.MaterialExpressionFresnel, -750, 600, exponent=3.0, base_reflect_fraction=0.0)
    rim = node(m, unreal.MaterialExpressionMultiply, -500, 600)
    link(fres, "", rim, "A")
    link(sparam(m, "RimStrength", 0.35, -750, 750), "", rim, "B")
    glow = node(m, unreal.MaterialExpressionAdd, -350, 500)
    link(sparam(m, "SelfLight", 0.28, -500, 450), "", glow, "A")
    link(rim, "", glow, "B")
    glow2 = node(m, unreal.MaterialExpressionAdd, -250, 500)
    link(glow, "", glow2, "A")
    link(sparam(m, "Emissive", 0.0, -350, 650), "", glow2, "B")
    emi = node(m, unreal.MaterialExpressionMultiply, -150, 300)
    link(base, "", emi, "A")
    link(glow2, "", emi, "B")
    to_prop(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(emi, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    to_prop(sparam(m, "Roughness", 0.85, -300, 750), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(node(m, unreal.MaterialExpressionConstant, -300, 850, r=0.15), "", unreal.MaterialProperty.MP_SPECULAR)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("used_with_static_lighting", False)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("matériau " + path)
    return m


def toon_model_material(t_draw, t_albedo, t_normal):
    """Modèle 3D riggé (Tools/blender/rig_model.py) : UV0/UV1 = planches de face/dos, UV2 = texture et normales du
    modèle, UV3 = (part de la face, part du dos). Même éclairage « dessin » et même teinte de tunique que M_ZToonTex."""
    name = "M_ZToonModel"
    path = M_DIR + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = AT.create_asset(name, M_DIR, unreal.Material, unreal.MaterialFactoryNew())
    def tobj(pname, tex, y, sampler):
        return node(m, unreal.MaterialExpressionTextureObjectParameter, -2100, y, parameter_name=pname, texture=tex, sampler_type=sampler)
    draw = tobj("Draw", t_draw, -400, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    albedo = tobj("Albedo", t_albedo, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    nmap = tobj("Normal", t_normal, 400, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    def sample(tex_node, uv_index, y, sampler):
        smp = node(m, unreal.MaterialExpressionTextureSample, -1750, y, sampler_type=sampler)
        link(tex_node, "", smp, "Tex")
        link(node(m, unreal.MaterialExpressionTextureCoordinate, -2000, y + 60, coordinate_index=uv_index), "", smp, "UVs")
        return smp
    front = sample(draw, 0, -500, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    back = sample(draw, 1, -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    alb = sample(albedo, 2, -100, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    nrm = sample(nmap, 2, 400, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    data = node(m, unreal.MaterialExpressionTextureCoordinate, -2000, 200, coordinate_index=3)
    wf = node(m, unreal.MaterialExpressionComponentMask, -1750, 150, r=True, g=False, b=False, a=False)
    link(data, "", wf, "")
    wb = node(m, unreal.MaterialExpressionComponentMask, -1750, 250, r=False, g=True, b=False, a=False)
    link(data, "", wb, "")
    c1 = node(m, unreal.MaterialExpressionLinearInterpolate, -1450, -200)
    link(alb, "RGB", c1, "A"); link(front, "RGB", c1, "B"); link(wf, "", c1, "Alpha")
    c2 = node(m, unreal.MaterialExpressionLinearInterpolate, -1250, -150)
    link(c1, "", c2, "A"); link(back, "RGB", c2, "B"); link(wb, "", c2, "Alpha")
    c = node(m, unreal.MaterialExpressionCustom, -1050, -150)
    c.set_editor_property("code", TUNIC_HLSL)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property("description", "TunicShift")
    ins = []
    for n in ("C", "Hue", "Sat", "Val"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    link(c2, "", c, "C")
    link(sparam(m, "TunicHue", -1.0, -1300, 200), "", c, "Hue")
    link(sparam(m, "TunicSat", 1.0, -1300, 300), "", c, "Sat")
    link(sparam(m, "TunicVal", 1.0, -1300, 400), "", c, "Val")
    tint = node(m, unreal.MaterialExpressionVectorParameter, -1050, 100, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
    base = node(m, unreal.MaterialExpressionMultiply, -800, -100)
    link(c, "", base, "A")
    link(tint, "", base, "B")
    fres = node(m, unreal.MaterialExpressionFresnel, -1050, 600, exponent=3.0, base_reflect_fraction=0.0)
    rim = node(m, unreal.MaterialExpressionMultiply, -800, 600)
    link(fres, "", rim, "A")
    link(sparam(m, "RimStrength", 0.3, -1050, 750), "", rim, "B")
    glow = node(m, unreal.MaterialExpressionAdd, -650, 500)
    link(sparam(m, "SelfLight", 0.38, -800, 450), "", glow, "A")
    link(rim, "", glow, "B")
    glow2 = node(m, unreal.MaterialExpressionAdd, -550, 500)
    link(glow, "", glow2, "A")
    link(sparam(m, "Emissive", 0.0, -650, 650), "", glow2, "B")
    emi = node(m, unreal.MaterialExpressionMultiply, -400, 200)
    link(base, "", emi, "A")
    link(glow2, "", emi, "B")
    to_prop(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(emi, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    to_prop(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    to_prop(sparam(m, "Roughness", 0.8, -400, 750), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(node(m, unreal.MaterialExpressionConstant, -400, 850, r=0.2), "", unreal.MaterialProperty.MP_SPECULAR)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_skeletal_mesh", True)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("matériau " + path)
    return m


def material_instance(name, dest, parent, tex, vectors=None):
    path = dest + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    inst = AT.create_asset(name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(inst, parent)
    MEL.set_material_instance_texture_parameter_value(inst, "Tex", tex)
    for k, v in (vectors or {}).items():
        MEL.set_material_instance_vector_parameter_value(inst, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    EAL.save_loaded_asset(inst)
    return inst


def import_skeletal(path, dest, name, skeleton):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("skeleton", skeleton)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("create_physics_asset", False)
    sk = ui.get_editor_property("skeletal_mesh_import_data")
    sk.set_editor_property("import_morph_targets", False)
    sk.set_editor_property("update_skeleton_reference_pose", False)
    sk.set_editor_property("use_t0_as_ref_pose", False)
    sk.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = dest
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = True
    t.options = ui
    AT.import_asset_tasks([t])
    return unreal.load_asset(dest + "/" + name)


def assign_materials(mesh, by_keyword):
    mats = mesh.get_editor_property("materials")
    out = []
    for sm in mats:
        slot = str(sm.get_editor_property("material_slot_name"))
        for key, mi in by_keyword.items():
            if key in slot:
                sm.set_editor_property("material_interface", mi)
        out.append(sm)
    mesh.set_editor_property("materials", out)
    EAL.save_loaded_asset(mesh)
    log("matériaux %s : %s" % (mesh.get_name(), [str(s.get_editor_property("material_slot_name")) for s in out]))


unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX False")
skeleton = unreal.load_asset("/Game/Characters/Mannequins/Meshes/SK_Mannequin")
master = None
for name in NAMES:
    cap = name.capitalize()
    src = os.path.join(PROJ, "Tools", "art", "out", name)
    dest = "/Game/Art/Characters/" + cap
    if os.path.exists(os.path.join(src, "T_%s_Albedo.png" % cap)):
        # personnage issu d'un modèle 3D riggé (rig_model.py) : un seul matériau, pas de maillage d'équipement séparé
        t_draw = import_texture(os.path.join(src, "T_%s_Draw.png" % cap), dest, "T_%s_Draw" % cap)
        t_alb = import_texture(os.path.join(src, "T_%s_Albedo.png" % cap), dest, "T_%s_Albedo" % cap)
        t_nrm = import_texture(os.path.join(src, "T_%s_Normal.png" % cap), dest, "T_%s_Normal" % cap, normal=True)
        mmat = toon_model_material(t_draw, t_alb, t_nrm)
        path = dest + "/MI_%s_Model" % cap
        if EAL.does_asset_exist(path):
            EAL.delete_asset(path)
        mi = AT.create_asset("MI_%s_Model" % cap, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, mmat)
        EAL.save_loaded_asset(mi)
        old_gear = dest + "/SKM_%s_Gear" % cap
        if EAL.does_asset_exist(old_gear) and not os.path.exists(os.path.join(src, "SKM_%s_Gear.fbx" % cap)):
            EAL.delete_asset(old_gear)
            log("supprimé " + old_gear)
        for part in ("", "_Hilt"):
            n = "SKM_%s%s" % (cap, part)
            f = os.path.join(src, n + ".fbx")
            if not os.path.exists(f):
                continue
            mesh = import_skeletal(f, dest, n, skeleton)
            if mesh is None:
                log("échec import " + n)
                continue
            assign_materials(mesh, {"": mi})
            log("importé %s squelette=%s" % (n, mesh.skeleton.get_name()))
        continue
    t_body = import_texture(os.path.join(src, "T_%s_Body.png" % cap), dest, "T_%s_Body" % cap)
    t_gear = import_texture(os.path.join(src, "T_%s_Gear.png" % cap), dest, "T_%s_Gear" % cap)
    if master is None:
        master = toon_tex_material(t_body)
    side = {}
    sc_path = os.path.join(src, "side_colors.json")
    if os.path.exists(sc_path):
        import json
        with open(sc_path) as f:
            side = json.load(f)
    mi_body = material_instance("MI_%s_Body" % cap, dest, master, t_body, side)
    mi_gear = material_instance("MI_%s_Gear" % cap, dest, master, t_gear, side)
    for part in ("", "_Gear", "_Hilt"):
        n = "SKM_%s%s" % (cap, part)
        f = os.path.join(src, n + ".fbx")
        if not os.path.exists(f):
            continue
        mesh = import_skeletal(f, dest, n, skeleton)
        if mesh is None:
            log("échec import " + n)
            continue
        assign_materials(mesh, {"Body": mi_body, "Gear": mi_gear})
        log("importé %s squelette=%s" % (n, mesh.skeleton.get_name()))
# Animations (Tools/blender/make_anims.py) sur le squelette du mannequin (après Link : l'os weapon_r existe)
ANIM_SRC = os.path.join(PROJ, "Tools", "art", "anims")
if os.path.isdir(ANIM_SRC):
    for f in sorted(os.listdir(ANIM_SRC)):
        if not f.endswith(".fbx"):
            continue
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", False)
        ui.set_editor_property("import_as_skeletal", True)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
        ui.set_editor_property("skeleton", skeleton)
        ui.set_editor_property("import_animations", True)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ad = ui.get_editor_property("anim_sequence_import_data")
        ad.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        ad.set_editor_property("remove_redundant_keys", False)
        t = unreal.AssetImportTask()
        t.filename = os.path.join(ANIM_SRC, f)
        t.destination_path = "/Game/Art/Anims"
        t.destination_name = f[:-4]
        t.replace_existing = True
        t.automated = True
        t.save = True
        t.options = ui
        AT.import_asset_tasks([t])
        a = unreal.load_asset("/Game/Art/Anims/" + f[:-4])
        # weapon_r : le squelette du mannequin a son propre weapon_r (1,8 cm de la main) et l'UE met à l'échelle la
        # translation animée (7 cm) par le rapport maillage / squelette, d'où une épée à 28 cm du poing. Avec le maillage
        # de Link comme référence, le rapport vaut 1 (ses autres os sont ceux du mannequin).
        link_mesh = unreal.load_asset("/Game/Art/Characters/Link/SKM_Link")
        if a and link_mesh:
            a.set_editor_property("retarget_source_asset", link_mesh)
            EAL.save_loaded_asset(a)
        log("animation %s -> %s %s" % (f, type(a).__name__, ("%.2f s" % a.get_play_length()) if a else ""))
unreal.SystemLibrary.quit_editor()
