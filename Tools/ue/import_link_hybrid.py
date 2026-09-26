# Importe le Link « hybride » (Tools/art/CH_Link, skill blender-image-to-3d) à côté de SKM_Link, sans le remplacer :
# textures cuites (Head 2k, Body 4k, Gear 2k), matériau M_ZToonBaked (même éclairage « dessin » et même teinte de tunique
# que M_ZToonTex, mais couleur et normales cuites), instances, SKM_LinkHybrid (LOD0 + LOD1/LOD2) et SKM_LinkHybrid_Hilt,
# sur le squelette du mannequin, dans /Game/Art/Characters/LinkHybrid.
# Éditeur complet (l'import de maillages squelettiques plante en commandlet) :
#   UnrealEditor EchosTriforce.uproject -ExecutePythonScript=Tools/ue/import_link_hybrid.py -unattended -nosplash
import os
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJ, "Tools", "art", "CH_Link")
DEST = "/Game/Art/Characters/LinkHybrid"
M_DIR = "/Game/Art/Materials"
NAME = "SKM_LinkHybrid"
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


def import_texture(path, name, kind):
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = DEST
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = False
    AT.import_asset_tasks([t])
    tex = unreal.load_asset(DEST + "/" + name)
    if kind == "normal":
        # cuite en OpenGL +Y (Blender) : l'UE attend DirectX -Y
        tex.set_editor_property("flip_green_channel", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER_NORMAL_MAP)
    else:
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
    EAL.save_loaded_asset(tex)
    return tex


def toon_baked_material(default_col, default_nrm):
    name = "M_ZToonBaked"
    path = M_DIR + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = AT.create_asset(name, M_DIR, unreal.Material, unreal.MaterialFactoryNew())
    col = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1300, 0, parameter_name="BaseColor", texture=default_col,
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    nrm = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1300, 700, parameter_name="Normal", texture=default_nrm,
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    c = node(m, unreal.MaterialExpressionCustom, -950, 0)
    c.set_editor_property("code", TUNIC_HLSL)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property("description", "TunicShift")
    ins = []
    for n in ("C", "Hue", "Sat", "Val"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    link(col, "RGB", c, "C")
    link(sparam(m, "TunicHue", -1.0, -1200, 300), "", c, "Hue")
    link(sparam(m, "TunicSat", 1.0, -1200, 380), "", c, "Sat")
    link(sparam(m, "TunicVal", 1.0, -1200, 460), "", c, "Val")
    tint = node(m, unreal.MaterialExpressionVectorParameter, -950, 250, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
    base = node(m, unreal.MaterialExpressionMultiply, -700, 100)
    link(c, "", base, "A")
    link(tint, "", base, "B")
    fres = node(m, unreal.MaterialExpressionFresnel, -950, 500, exponent=3.0, base_reflect_fraction=0.0)
    rim = node(m, unreal.MaterialExpressionMultiply, -700, 500)
    link(fres, "", rim, "A")
    link(sparam(m, "RimStrength", 0.35, -950, 620), "", rim, "B")
    glow = node(m, unreal.MaterialExpressionAdd, -550, 450)
    link(sparam(m, "SelfLight", 0.28, -700, 400), "", glow, "A")
    link(rim, "", glow, "B")
    glow2 = node(m, unreal.MaterialExpressionAdd, -450, 450)
    link(glow, "", glow2, "A")
    link(sparam(m, "Emissive", 0.0, -550, 580), "", glow2, "B")
    emi = node(m, unreal.MaterialExpressionMultiply, -300, 250)
    link(base, "", emi, "A")
    link(glow2, "", emi, "B")
    to_prop(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(emi, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    to_prop(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    to_prop(sparam(m, "Roughness", 0.85, -450, 700), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(node(m, unreal.MaterialExpressionConstant, -450, 800, r=0.15), "", unreal.MaterialProperty.MP_SPECULAR)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("used_with_static_lighting", False)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("matériau " + path)
    return m


def instance(name, parent, col, nrm):
    path = DEST + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mi = AT.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    MEL.set_material_instance_texture_parameter_value(mi, "BaseColor", col)
    MEL.set_material_instance_texture_parameter_value(mi, "Normal", nrm)
    EAL.save_loaded_asset(mi)
    return mi


def import_skeletal(path, name, skeleton):
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
    t.destination_path = DEST
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = True
    t.options = ui
    AT.import_asset_tasks([t])
    return unreal.load_asset(DEST + "/" + name)


def import_lod(mesh, index, path):
    for getter in (lambda: unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem), lambda: unreal.EditorSkeletalMeshLibrary):
        try:
            lib = getter()
            r = lib.import_lod(mesh, index, path)
            log("LOD%d importé (%s) -> %s" % (index, type(lib).__name__, r))
            return True
        except Exception as e:
            log("import_lod via %s : %s" % (getter, e))
    return False


def assign(mesh, by_slot):
    mats = mesh.get_editor_property("materials")
    out = []
    for sm in mats:
        slot = str(sm.get_editor_property("material_slot_name"))
        for key, mi in by_slot.items():
            if key in slot:
                sm.set_editor_property("material_interface", mi)
        out.append(sm)
    mesh.set_editor_property("materials", out)
    EAL.save_loaded_asset(mesh)
    log("matériaux %s : %s" % (mesh.get_name(), [str(s.get_editor_property("material_slot_name")) for s in out]))


def report(mesh):
    b = mesh.get_bounds()
    ext = b.box_extent
    org = b.origin
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    n = sub.get_lod_count(mesh)
    verts = [sub.get_num_verts(mesh, i) for i in range(n)]
    log("%s : squelette=%s, %d LOD, sommets moteur %s, boîte %.1f x %.1f x %.1f cm, sommet z=%.1f cm, %d emplacements" % (
        mesh.get_name(), mesh.skeleton.get_name(), n, verts, ext.x * 2, ext.y * 2, ext.z * 2, org.z + ext.z,
        len(mesh.get_editor_property("materials"))))


unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX False")
skeleton = unreal.load_asset("/Game/Characters/Mannequins/Meshes/SK_Mannequin")
tex = {}
for s in ("Head", "Body", "Gear"):
    tex[s] = (import_texture(os.path.join(SRC, "textures", "T_Link_%s_BaseColor.png" % s), "T_LinkH_%s_BaseColor" % s, "color"),
              import_texture(os.path.join(SRC, "textures", "T_Link_%s_Normal.png" % s), "T_LinkH_%s_Normal" % s, "normal"))
mat = toon_baked_material(*tex["Body"])
mis = {"M_Link_%s" % s: instance("MI_LinkH_%s" % s, mat, *tex[s]) for s in ("Head", "Body", "Gear")}
mesh = import_skeletal(os.path.join(SRC, "exports", NAME + "_LOD0.fbx"), NAME, skeleton)
if mesh:
    for k in (1, 2):
        import_lod(mesh, k, os.path.join(SRC, "exports", "%s_LOD%d.fbx" % (NAME, k)))
    assign(mesh, mis)
    try:
        report(mesh)
    except Exception as e:
        log("rapport : %s" % e)
else:
    log("échec import " + NAME)
hilt = import_skeletal(os.path.join(SRC, "exports", NAME + "_Hilt.fbx"), NAME + "_Hilt", skeleton)
if hilt:
    assign(hilt, mis)
    try:
        report(hilt)
    except Exception as e:
        log("rapport : %s" % e)
unreal.SystemLibrary.quit_editor()
