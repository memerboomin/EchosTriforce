# Importe un personnage de Twilight Princess converti par Tools/tp/blender_tp.py (Tools/art/tp/<preset>) :
# textures, matériau M_TPChar (même éclairage « dessin » et même teinte de tunique que M_ZToonTex), instances par
# matériau d'origine, maillage squelettique (nouveau squelette), objets tenus (même squelette) et animations.
# Éditeur complet (l'import de maillages squelettiques plante en commandlet) :
#   TP_PRESET=link UnrealEditor EchosTriforce.uproject -ExecutePythonScript=Tools/ue/import_tp.py -unattended -nosplash
# TP_ANIMS=0 saute les animations ; TP_ANIMS=waits,atl n'importe que celles-là.
import json
import os

import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
PRESET = os.environ.get("TP_PRESET", "link")
SRC = os.path.join(PROJ, "Tools", "art", "tp", PRESET)
with open(os.path.join(SRC, "manifest.json")) as f:
    MANIFEST = json.load(f)
NAME = MANIFEST["name"]
DEST = "/Game/Art/TP/" + NAME
TEX_DIR = DEST + "/Textures"
MAT_DIR = DEST + "/Materials"
ANIM_DIR = DEST + "/Anims"
M_DIR = "/Game/Art/TP/Materials"
AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

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


def log(msg):
    unreal.log_warning("ECHOS TP " + msg)


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


WRAP = {0: unreal.TextureAddress.TA_CLAMP, 1: unreal.TextureAddress.TA_WRAP, 2: unreal.TextureAddress.TA_MIRROR}


def import_texture(fname, wrap):
    name = fname[:-4]
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, fname)
    t.destination_path = TEX_DIR
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = False
    AT.import_asset_tasks([t])
    tex = unreal.load_asset(TEX_DIR + "/" + name)
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
    tex.set_editor_property("address_x", WRAP.get(wrap[0], unreal.TextureAddress.TA_WRAP))
    tex.set_editor_property("address_y", WRAP.get(wrap[1], unreal.TextureAddress.TA_WRAP))
    # textures minuscules (≤ 256 px) : chargées entières. Sans cela, le streaming ne charge qu'un mip flou sur les
    # grands maillages de décor (densité de texels inconnue)
    tex.set_editor_property("never_stream", True)
    EAL.save_loaded_asset(tex)
    return tex


def master_material(default_tex):
    """Texture d'origine × teinte, teinte de tunique, auto-éclairage + liseré (comme M_ZToonTex) ; masque d'opacité
    branché (utilisé quand une instance passe en mode Masked ou Translucent)."""
    name = "M_TPChar"
    path = M_DIR + "/" + name
    if EAL.does_asset_exist(path):
        m = unreal.load_asset(path)
        return m
    m = AT.create_asset(name, M_DIR, unreal.Material, unreal.MaterialFactoryNew())
    tex = node(m, unreal.MaterialExpressionTextureObjectParameter, -1700, 0, parameter_name="Tex", texture=default_tex,
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    smp = node(m, unreal.MaterialExpressionTextureSample, -1400, 0, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    link(tex, "", smp, "Tex")
    link(node(m, unreal.MaterialExpressionTextureCoordinate, -1700, 200, coordinate_index=0), "", smp, "UVs")
    c = node(m, unreal.MaterialExpressionCustom, -1100, 0)
    c.set_editor_property("code", TUNIC_HLSL)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property("description", "TunicShift")
    ins = []
    for n in ("C", "Hue", "Sat", "Val"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    link(smp, "RGB", c, "C")
    link(sparam(m, "TunicHue", -1.0, -1400, 250), "", c, "Hue")
    link(sparam(m, "TunicSat", 1.0, -1400, 350), "", c, "Sat")
    link(sparam(m, "TunicVal", 1.0, -1400, 450), "", c, "Val")
    tint = node(m, unreal.MaterialExpressionVectorParameter, -1100, 250, parameter_name="Tint",
                default_value=unreal.LinearColor(1, 1, 1, 1))
    tinted = node(m, unreal.MaterialExpressionMultiply, -850, 50)
    link(c, "", tinted, "A")
    link(tint, "", tinted, "B")
    # les textures de TP sont sombres (le jeu les éclaire fort) : gain réglable
    base = node(m, unreal.MaterialExpressionMultiply, -650, 50)
    link(tinted, "", base, "A")
    link(sparam(m, "Brightness", 1.35, -850, 250), "", base, "B")
    fres = node(m, unreal.MaterialExpressionFresnel, -850, 500, exponent=3.0, base_reflect_fraction=0.0)
    rim = node(m, unreal.MaterialExpressionMultiply, -650, 500)
    link(fres, "", rim, "A")
    link(sparam(m, "RimStrength", 0.3, -850, 650), "", rim, "B")
    glow = node(m, unreal.MaterialExpressionAdd, -500, 450)
    link(sparam(m, "SelfLight", 0.3, -650, 400), "", glow, "A")
    link(rim, "", glow, "B")
    glow2 = node(m, unreal.MaterialExpressionAdd, -400, 450)
    link(glow, "", glow2, "A")
    link(sparam(m, "Emissive", 0.0, -500, 600), "", glow2, "B")
    emi = node(m, unreal.MaterialExpressionMultiply, -250, 250)
    link(base, "", emi, "A")
    link(glow2, "", emi, "B")
    to_prop(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(emi, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    to_prop(sparam(m, "Roughness", 0.85, -250, 650), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(node(m, unreal.MaterialExpressionConstant, -250, 750, r=0.15), "", unreal.MaterialProperty.MP_SPECULAR)
    to_prop(smp, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    to_prop(smp, "A", unreal.MaterialProperty.MP_OPACITY)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("used_with_static_lighting", False)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("matériau " + path)
    return m


def env_material(default_tex):
    """Décor : calcul du TEV de TP en espace gamma, sat(texture × couleur de sommet × Gain). Dans le jeu l'étage vaut
    (KONST + (TEXC − KONST) × RASC) × 4, où RASC = couleur de sommet × ambiance de la salle (≈ 0,3) : Gain ≈ 1,3.
    Surtout émissif (l'éclairage cuit d'origine est gardé) ; une petite part de couleur de base laisse jouer les lumières."""
    name = "M_TPEnv"
    path = M_DIR + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = AT.create_asset(name, M_DIR, unreal.Material, unreal.MaterialFactoryNew())
    tex = node(m, unreal.MaterialExpressionTextureObjectParameter, -1700, 0, parameter_name="Tex", texture=default_tex,
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    smp = node(m, unreal.MaterialExpressionTextureSample, -1400, 0, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    link(tex, "", smp, "Tex")
    link(node(m, unreal.MaterialExpressionTextureCoordinate, -1700, 200, coordinate_index=0), "", smp, "UVs")
    vc = node(m, unreal.MaterialExpressionVertexColor, -1400, 300)
    c = node(m, unreal.MaterialExpressionCustom, -1000, 100)
    c.set_editor_property("code", "float3 t = pow(max(T, 0), 1.0 / 2.2);\nfloat3 v = pow(max(V, 0), 1.0 / 2.2);\n"
                                  "return pow(saturate(t * v * Gain), 2.2);")
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property("description", "TevTP")
    ins = []
    for n in ("T", "V", "Gain"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    link(smp, "RGB", c, "T")
    link(vc, "", c, "V")
    link(sparam(m, "Gain", 1.3, -1300, 450), "", c, "Gain")
    tint = node(m, unreal.MaterialExpressionVectorParameter, -800, 300, parameter_name="Tint",
                default_value=unreal.LinearColor(1, 1, 1, 1))
    col = node(m, unreal.MaterialExpressionMultiply, -600, 100)
    link(c, "", col, "A")
    link(tint, "", col, "B")
    emi = node(m, unreal.MaterialExpressionMultiply, -350, 250)
    link(col, "", emi, "A")
    link(sparam(m, "SelfLight", 1.0, -600, 400), "", emi, "B")
    base = node(m, unreal.MaterialExpressionMultiply, -350, 50)
    link(col, "", base, "A")
    link(sparam(m, "LitPart", 0.1, -600, 0), "", base, "B")
    alpha = node(m, unreal.MaterialExpressionMultiply, -800, 600)
    link(smp, "A", alpha, "A")
    link(vc, "A", alpha, "B")
    to_prop(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(emi, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    to_prop(sparam(m, "Roughness", 0.9, -350, 500), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(node(m, unreal.MaterialExpressionConstant, -350, 600, r=0.0), "", unreal.MaterialProperty.MP_SPECULAR)
    to_prop(alpha, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    to_prop(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    m.set_editor_property("used_with_static_lighting", True)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("matériau " + path)
    return m


_MI_NAMES = {}


def mi_name(mname):
    """Nom d'asset unique : l'éditeur ignore la casse (aa_Tile01_v et aa_tile01_v écriraient le même fichier)."""
    base = "MI_TP_" + "".join(c if c.isalnum() or c == "_" else "_" for c in mname)
    n = _MI_NAMES.get(base.lower(), 0)
    _MI_NAMES[base.lower()] = n + 1
    return base if n == 0 else "%s_%d" % (base, n)


def material_instance(mname, info, master, textures):
    name = mi_name(mname)
    path = MAT_DIR + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mi = AT.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, master)
    if info.get("texture") and info["texture"] in textures:
        MEL.set_material_instance_texture_parameter_value(mi, "Tex", textures[info["texture"]])
    o = unreal.MaterialInstanceBasePropertyOverrides()
    if info.get("two_sided"):
        o.set_editor_property("override_two_sided", True)
        o.set_editor_property("two_sided", True)
    if info.get("alpha") in ("mask", "blend"):
        o.set_editor_property("override_blend_mode", True)
        o.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED if info["alpha"] == "mask"
                              else unreal.BlendMode.BLEND_TRANSLUCENT)
    mi.set_editor_property("base_property_overrides", o)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi)
    return mi


def import_skeletal(fname, name, skeleton):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    if skeleton:
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
    t.filename = os.path.join(SRC, fname)
    t.destination_path = DEST
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = True
    t.options = ui
    AT.import_asset_tasks([t])
    return unreal.load_asset(DEST + "/" + name)


def assign_materials(mesh, mis):
    mats = mesh.get_editor_property("materials")
    out = []
    for sm in mats:
        slot = str(sm.get_editor_property("material_slot_name"))
        mi = mis.get(slot)
        if mi is None:  # l'importeur peut suffixer le nom de l'emplacement
            mi = next((v for k, v in mis.items() if slot.startswith(k)), None)
        if mi:
            sm.set_editor_property("material_interface", mi)
        else:
            log("emplacement sans matériau : %s" % slot)
        out.append(sm)
    mesh.set_editor_property("materials", out)
    EAL.save_loaded_asset(mesh)


def import_anim(fname, skeleton):
    name = fname[:-4]
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
    t.filename = os.path.join(SRC, "anims", fname)
    t.destination_path = ANIM_DIR
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = True
    t.options = ui
    return t


def import_static(fname, name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    sd = ui.get_editor_property("static_mesh_import_data")
    sd.set_editor_property("combine_meshes", True)
    sd.set_editor_property("auto_generate_collision", False)
    sd.set_editor_property("generate_lightmap_u_vs", False)
    sd.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    sd.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, fname)
    t.destination_path = DEST
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = True
    t.options = ui
    AT.import_asset_tasks([t])
    return unreal.load_asset(DEST + "/" + name)


def import_stage():
    """Décor de TP : maillages statiques en coordonnées monde, collision « complexe comme simple », carte de test."""
    wraps = {}
    for info in MANIFEST["materials"].values():
        if info.get("texture"):
            wraps.setdefault(info["texture"], info.get("wrap", [1, 1]))
    textures = {f: import_texture(f, w) for f, w in sorted(wraps.items())}
    log("%d textures" % len(textures))
    master = env_material(next(iter(textures.values())))
    mis = {m: material_instance(m, info, master, textures) for m, info in MANIFEST["materials"].items()}
    log("%d instances de matériau" % len(mis))
    meshes = {}
    for mname, info in sorted(MANIFEST["meshes"].items()):
        sm = import_static(info["file"], mname)
        if not isinstance(sm, unreal.StaticMesh):
            log("ÉCHEC import " + mname)
            continue
        mats = sm.get_editor_property("static_materials")
        out = []
        for sl in mats:
            slot = str(sl.get_editor_property("material_slot_name"))
            mi = mis.get(slot) or next((v for k, v in mis.items() if slot.startswith(k)), None)
            if mi:
                sl.set_editor_property("material_interface", mi)
            out.append(sl)
        sm.set_editor_property("static_materials", out)
        bs = sm.get_editor_property("body_setup")
        if bs:
            bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        EAL.save_loaded_asset(sm)
        b = sm.get_bounding_box()
        log("%s min=(%.0f,%.0f,%.0f) max=(%.0f,%.0f,%.0f)" % (mname, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))
        meshes[mname] = (sm, info)
    # carte de test : toutes les salles à l'origine (coordonnées monde de TP), départ de Link, repère ZTPStage
    level = "/Game/Maps/L_" + NAME
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if EAL.does_asset_exist(level):
        les.load_level(level)
        for a in eas.get_all_level_actors():
            eas.destroy_actor(a)
    else:
        les.new_level(level)
    for mname, (sm, info) in sorted(meshes.items()):
        a = eas.spawn_actor_from_object(sm, unreal.Vector(0, 0, 0))
        a.set_actor_label(mname)
        a.tags = [unreal.Name("ZTPStage")]
        comp = a.get_component_by_class(unreal.StaticMeshComponent)
        if info.get("translucent"):
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    start = MANIFEST.get("start")
    if start:
        p = start["ue"]
        ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(p[0], p[1], p[2] + 100), unreal.Rotator(0, 0, start["yaw"]))
        ps.tags = [unreal.Name("ZTPStage")]
    les.save_current_level()
    log("carte " + level)


def main():
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX False")
    if MANIFEST.get("kind") == "stage":
        import_stage()
        unreal.SystemLibrary.quit_editor()
        return
    # textures
    wraps = {}
    for info in MANIFEST["materials"].values():
        if info.get("texture"):
            wraps.setdefault(info["texture"], info.get("wrap", [1, 1]))
    textures = {f: import_texture(f, w) for f, w in sorted(wraps.items())}
    log("%d textures" % len(textures))
    master = master_material(next(iter(textures.values())) if textures else None)
    mis = {m: material_instance(m, info, master, textures) for m, info in MANIFEST["materials"].items()}
    log("%d instances de matériau" % len(mis))
    # maillage principal (crée le squelette), puis objets sur le même squelette
    main_name = "SKM_" + NAME
    # squelette existant réutilisé : les animations déjà importées restent valides
    skel_path = DEST + "/" + main_name + "_Skeleton"
    existing = unreal.load_asset(skel_path) if EAL.does_asset_exist(skel_path) else None
    body = import_skeletal(MANIFEST["meshes"][main_name]["file"], main_name, existing)
    if body is None:
        log("ÉCHEC import " + main_name)
        unreal.SystemLibrary.quit_editor()
        return
    skeleton = body.skeleton
    # le squelette créé par l'import est un paquet à part : sans cette sauvegarde, maillages et animations le perdent
    EAL.save_loaded_asset(skeleton)
    assign_materials(body, mis)
    log("importé %s squelette=%s os=%d" % (main_name, skeleton.get_name(), len(MANIFEST["bones"])))
    for mname, info in MANIFEST["meshes"].items():
        if mname == main_name:
            continue
        m = import_skeletal(info["file"], mname, skeleton)
        if m:
            assign_materials(m, mis)
            log("importé " + mname)
        else:
            log("ÉCHEC import " + mname)
    # animations
    only = os.environ.get("TP_ANIMS", "")
    if only != "0":
        wanted = set(x for x in only.split(",") if x)
        tasks = []
        for act, info in sorted(MANIFEST["anims"].items()):
            if wanted and info["bck"] not in wanted:
                continue
            f = act + ".fbx"
            if os.path.exists(os.path.join(SRC, "anims", f)):
                tasks.append(import_anim(f, skeleton))
        for i in range(0, len(tasks), 40):
            AT.import_asset_tasks(tasks[i:i + 40])
            log("animations %d / %d" % (min(i + 40, len(tasks)), len(tasks)))
        bad = [t.destination_name for t in tasks if not isinstance(unreal.load_asset(ANIM_DIR + "/" + t.destination_name), unreal.AnimSequence)]
        log("animations importées %d, échecs %d %s" % (len(tasks) - len(bad), len(bad), bad[:10]))
    EAL.save_directory(DEST, only_if_is_dirty=True, recursive=True)
    log("squelette sauvegardé : %s" % EAL.does_asset_exist(skeleton.get_path_name().split(".")[0]))
    unreal.SystemLibrary.quit_editor()


main()
