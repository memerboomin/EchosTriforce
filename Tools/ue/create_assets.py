# Importe les textures procédurales et construit les matériaux du projet (/Game/Art).
# Lancer : UnrealEditor-Cmd EchosTriforce.uproject -run=pythonscript -script=Tools/ue/create_assets.py
import os
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJ, "Tools", "art", "out")
T_DIR = "/Game/Art/Textures"
M_DIR = "/Game/Art/Materials"
AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("ECHOS " + msg)


def import_texture(file, name, normal=False, srgb=True):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, file)
    task.destination_path = T_DIR
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    AT.import_asset_tasks([task])
    tex = unreal.load_asset(T_DIR + "/" + name)
    if tex is None:
        log("texture manquante " + name)
        return None
    if normal:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("srgb", srgb)
    EAL.save_loaded_asset(tex)
    return tex


def new_material(name):
    path = M_DIR + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    return AT.create_asset(name, M_DIR, unreal.Material, unreal.MaterialFactoryNew())


def node(mat, cls, x, y, **props):
    e = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def vparam(mat, name, color, x, y):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*color))


def sparam(mat, name, value, x, y):
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def link(a, a_out, b, b_in):
    ok = MEL.connect_material_expressions(a, a_out, b, b_in)
    if not ok:
        log("liaison échouée %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def to_prop(e, out, prop):
    ok = MEL.connect_material_property(e, out, prop)
    if not ok:
        log("propriété échouée %s -> %s" % (e.get_name(), prop))


def custom(mat, x, y, code, inputs, out_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, desc="Custom"):
    c = node(mat, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out_type)
    c.set_editor_property("description", desc)
    arr = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        arr.append(ci)
    c.set_editor_property("inputs", arr)
    return c


def finish(mat, ism=True, skel=False):
    if ism:
        mat.set_editor_property("used_with_instanced_static_meshes", True)
    # les maillages du kit importés avec Nanite exigent ce drapeau (sinon matériau par défaut en jeu)
    if mat.get_editor_property("blend_mode") in (unreal.BlendMode.BLEND_OPAQUE, unreal.BlendMode.BLEND_MASKED):
        mat.set_editor_property("used_with_nanite", True)
    if skel:
        mat.set_editor_property("used_with_skeletal_mesh", True)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)


def mi(name, parent, vectors=None, scalars=None, textures=None):
    path = M_DIR + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    inst = AT.create_asset(name, M_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(inst, parent)
    for k, v in (vectors or {}).items():
        MEL.set_material_instance_vector_parameter_value(inst, k, unreal.LinearColor(*v))
    for k, v in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(inst, k, v)
    for k, v in (textures or {}).items():
        MEL.set_material_instance_texture_parameter_value(inst, k, v)
    EAL.save_loaded_asset(inst)
    return inst


# ---------------------------------------------------------------------------------------------------------------
log("textures")
t_stone = import_texture("T_Stone_A.png", "T_Stone_A")
t_stone_h = import_texture("T_Stone_H.png", "T_Stone_H", srgb=False)
t_noise = import_texture("T_Noise.png", "T_Noise", srgb=False)
t_water_n = import_texture("T_Water_N.png", "T_Water_N", normal=True)
t_fall = import_texture("T_Fall.png", "T_Fall", srgb=False)

# --- M_ZToon : accessoires et créatures (couleur, émissif, liseré de lumière) ----------------------------------
log("M_ZToon")
m = new_material("M_ZToon")
col = vparam(m, "Color", (0.6, 0.6, 0.6, 1), -900, -200)
emi = sparam(m, "Emissive", 0.0, -900, 0)
rough = sparam(m, "Roughness", 0.72, -900, 150)
rim = sparam(m, "RimStrength", 0.35, -900, 300)
fres = node(m, unreal.MaterialExpressionFresnel, -900, 450, exponent=3.5, base_reflect_fraction=0.0)
m1 = node(m, unreal.MaterialExpressionMultiply, -600, 0)
link(col, "", m1, "A"); link(emi, "", m1, "B")
m2 = node(m, unreal.MaterialExpressionMultiply, -600, 350)
link(fres, "", m2, "A"); link(rim, "", m2, "B")
m3 = node(m, unreal.MaterialExpressionMultiply, -400, 350)
link(m2, "", m3, "A"); link(col, "", m3, "B")
add = node(m, unreal.MaterialExpressionAdd, -200, 100)
link(m1, "", add, "A"); link(m3, "", add, "B")
spec = node(m, unreal.MaterialExpressionConstant, -400, 200, r=0.35)
to_prop(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
to_prop(add, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
to_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
to_prop(spec, "", unreal.MaterialProperty.MP_SPECULAR)
finish(m, ism=True, skel=True)
m_toon = m

# --- M_ZToonSkin : personnage « peint » par bandes (bottes, jambes, ceinture, tunique, manches, gants, peau, cheveux)
log("M_ZToonSkin")
m = new_material("M_ZToonSkin")
pos = node(m, unreal.MaterialExpressionPreSkinnedPosition, -1400, -600)
names = [("Color", (0.2, 0.45, 0.18, 1)), ("BootColor", (0.3, 0.18, 0.09, 1)), ("LegColor", (0.85, 0.8, 0.68, 1)),
         ("BeltColor", (0.35, 0.22, 0.1, 1)), ("SleeveColor", (0.85, 0.8, 0.68, 1)), ("GloveColor", (0.36, 0.22, 0.12, 1)),
         ("SkinColor", (0.95, 0.76, 0.6, 1)), ("HairColor", (0.95, 0.8, 0.4, 1))]
scal = [("BootH", 22.0), ("LegH", 72.0), ("BeltL", 96.0), ("BeltH", 104.0), ("NeckH", 148.0), ("HairH", 166.0),
        ("ArmX", 23.0), ("HandX", 52.0), ("SkirtH", 70.0)]
inputs = ["P"] + [n for n, _ in names] + [n for n, _ in scal]
code = """
float z = P.z; float ax = abs(P.x);
float3 c = Color;
if (z < SkirtH) c = LegColor;
if (z < BootH) c = BootColor;
if (z > BeltL && z < BeltH) c = BeltColor;
if (ax > ArmX && z > 80.0) c = SleeveColor;
if (ax > HandX) c = GloveColor;
if (z > NeckH) c = SkinColor;
if (z > HairH) c = HairColor;
return c;
"""
cust = custom(m, -700, -300, code, inputs, desc="Peinture par bandes")
interp = node(m, unreal.MaterialExpressionVertexInterpolator, -1150, -600)
link(pos, "", interp, "VS")
link(interp, "", cust, "P")
y = -900
for n, v in names:
    p = vparam(m, n, v, -1100, y)
    link(p, "", cust, n)
    y += 120
for n, v in scal:
    p = sparam(m, n, v, -1100, y)
    link(p, "", cust, n)
    y += 80
rough = sparam(m, "Roughness", 0.78, -700, 200)
rim = sparam(m, "RimStrength", 0.3, -700, 320)
emi = sparam(m, "Emissive", 0.0, -700, 440)
fres = node(m, unreal.MaterialExpressionFresnel, -700, 560, exponent=3.0, base_reflect_fraction=0.0)
r1 = node(m, unreal.MaterialExpressionMultiply, -450, 450)
link(fres, "", r1, "A"); link(rim, "", r1, "B")
r2 = node(m, unreal.MaterialExpressionMultiply, -300, 450)
link(r1, "", r2, "A"); link(cust, "", r2, "B")
e1 = node(m, unreal.MaterialExpressionMultiply, -450, 300)
link(cust, "", e1, "A"); link(emi, "", e1, "B")
eadd = node(m, unreal.MaterialExpressionAdd, -150, 350)
link(r2, "", eadd, "A"); link(e1, "", eadd, "B")
to_prop(cust, "", unreal.MaterialProperty.MP_BASE_COLOR)
to_prop(eadd, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
to_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
finish(m, ism=False, skel=True)

# --- M_ZStone : pierre triplanaire en coordonnées monde, mousse sur les faces supérieures ----------------------
log("M_ZStone")
m = new_material("M_ZStone")
tex = node(m, unreal.MaterialExpressionTextureObjectParameter, -1300, -300, parameter_name="StoneTex", texture=t_stone)
ntex = node(m, unreal.MaterialExpressionTextureObjectParameter, -1300, -100, parameter_name="NoiseTex", texture=t_noise)
wp = node(m, unreal.MaterialExpressionWorldPosition, -1300, 100)
vn = node(m, unreal.MaterialExpressionVertexNormalWS, -1300, 250)
scale = sparam(m, "TexScale", 260.0, -1300, 380)
tint = vparam(m, "Tint", (0.62, 0.57, 0.47, 1), -1300, 500)
moss = vparam(m, "MossColor", (0.24, 0.42, 0.16, 1), -1300, 650)
mossamt = sparam(m, "MossAmount", 0.45, -1300, 800)
code = """
float3 n = abs(N); n = pow(n, 6.0); n /= (n.x + n.y + n.z + 1e-4);
float a = Texture2DSample(Tex, TexSampler, W.yz / S).r * n.x
        + Texture2DSample(Tex, TexSampler, W.xz / S).r * n.y
        + Texture2DSample(Tex, TexSampler, W.xy / S).r * n.z;
float nz = Texture2DSample(Noise, NoiseSampler, W.xy / (S * 3.1)).r;
float nzw = Texture2DSample(Noise, NoiseSampler, (W.xz + W.yz) / (S * 2.3)).r;
float3 stone = Tint * (0.55 + 0.6 * a) * (0.85 + 0.3 * nzw);
float top = saturate((N.z - 0.55) * 4.0);
float m = saturate(top * (nz * 1.6 - 0.5) * MossAmount * 2.2);
float grime = saturate(1.0 - W.z / 180.0) * 0.18;
float3 c = lerp(stone, Moss * (0.75 + 0.5 * nz), m);
c *= (1.0 - grime);
return c;
"""
cst = custom(m, -700, 100, code, ["Tex", "Noise", "W", "N", "S", "Tint", "Moss", "MossAmount"], desc="Pierre triplanaire")
link(tex, "", cst, "Tex"); link(ntex, "", cst, "Noise"); link(wp, "", cst, "W"); link(vn, "", cst, "N")
link(scale, "", cst, "S"); link(tint, "", cst, "Tint"); link(moss, "", cst, "Moss"); link(mossamt, "", cst, "MossAmount")
rough = sparam(m, "Roughness", 0.88, -700, 400)
to_prop(cst, "", unreal.MaterialProperty.MP_BASE_COLOR)
to_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
finish(m, ism=True)
m_stone = m

# --- M_ZStoneLocal : même pierre, projetée dans l'espace de l'objet (créatures qui bougent) ------------------------
log("M_ZStoneLocal")
m = new_material("M_ZStoneLocal")
tex = node(m, unreal.MaterialExpressionTextureObjectParameter, -1300, -300, parameter_name="StoneTex", texture=t_stone)
ntex = node(m, unreal.MaterialExpressionTextureObjectParameter, -1300, -100, parameter_name="NoiseTex", texture=t_noise)
lp = node(m, unreal.MaterialExpressionLocalPosition, -1300, 100)
vn = node(m, unreal.MaterialExpressionVertexNormalWS, -1500, 250)
tr = node(m, unreal.MaterialExpressionTransform, -1300, 250, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
          transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
link(vn, "", tr, "")
scale = sparam(m, "TexScale", 90.0, -1300, 380)
tint = vparam(m, "Tint", (0.62, 0.60, 0.54, 1), -1300, 500)
moss = vparam(m, "MossColor", (0.24, 0.42, 0.16, 1), -1300, 650)
mossamt = sparam(m, "MossAmount", 0.6, -1300, 800)
code = """
float3 nl = normalize(NL);
float3 n = abs(nl); n = pow(n, 6.0); n /= (n.x + n.y + n.z + 1e-4);
float a = Texture2DSample(Tex, TexSampler, W.yz / S).r * n.x
        + Texture2DSample(Tex, TexSampler, W.xz / S).r * n.y
        + Texture2DSample(Tex, TexSampler, W.xy / S).r * n.z;
float nz = Texture2DSample(Noise, NoiseSampler, W.xy / (S * 2.1) + W.z / (S * 5.0)).r;
float3 stone = Tint * (0.55 + 0.6 * a) * (0.85 + 0.3 * nz);
float top = saturate((NW.z - 0.5) * 3.0);
float m = saturate(top * (nz * 1.7 - 0.45) * MossAmount * 2.4);
return lerp(stone, Moss * (0.75 + 0.5 * nz), m);
"""
cst = custom(m, -700, 100, code, ["Tex", "Noise", "W", "NL", "NW", "S", "Tint", "Moss", "MossAmount"], desc="Pierre triplanaire locale")
link(tex, "", cst, "Tex"); link(ntex, "", cst, "Noise"); link(lp, "", cst, "W"); link(tr, "", cst, "NL"); link(vn, "", cst, "NW")
link(scale, "", cst, "S"); link(tint, "", cst, "Tint"); link(moss, "", cst, "Moss"); link(mossamt, "", cst, "MossAmount")
emi = sparam(m, "Emissive", 0.0, -700, 500)
em = node(m, unreal.MaterialExpressionMultiply, -450, 450)
link(cst, "", em, "A"); link(emi, "", em, "B")
to_prop(cst, "", unreal.MaterialProperty.MP_BASE_COLOR)
to_prop(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
to_prop(sparam(m, "Roughness", 0.85, -700, 400), "", unreal.MaterialProperty.MP_ROUGHNESS)
finish(m, ism=True)
m_stone_local = m

# --- M_ZWater : eau turquoise opaque et brillante, normales animées -------------------------------------------
log("M_ZWater")
m = new_material("M_ZWater")
wp = node(m, unreal.MaterialExpressionWorldPosition, -1300, 0)
t = node(m, unreal.MaterialExpressionTime, -1300, 150)
ntex = node(m, unreal.MaterialExpressionTextureObjectParameter, -1300, -200, parameter_name="WaterN", texture=t_water_n)
deep = vparam(m, "DeepColor", (0.02, 0.32, 0.40, 1), -1300, 300)
shallow = vparam(m, "ShallowColor", (0.18, 0.78, 0.80, 1), -1300, 450)
code_n = """
float2 uv = W.xy / 520.0;
float3 a = Texture2DSample(Tex, TexSampler, uv + float2(0.013, 0.006) * T).xyz * 2.0 - 1.0;
float3 b = Texture2DSample(Tex, TexSampler, uv * 1.7 + float2(-0.008, 0.011) * T).xyz * 2.0 - 1.0;
float3 n = normalize(float3(a.xy + b.xy, a.z * b.z));
return lerp(float3(0, 0, 1), n, 0.55);
"""
cn = custom(m, -700, -100, code_n, ["Tex", "W", "T"], desc="Normales d'eau")
link(ntex, "", cn, "Tex"); link(wp, "", cn, "W"); link(t, "", cn, "T")
fres = node(m, unreal.MaterialExpressionFresnel, -900, 350, exponent=4.0, base_reflect_fraction=0.05)
lerp = node(m, unreal.MaterialExpressionLinearInterpolate, -600, 350)
link(shallow, "", lerp, "A"); link(deep, "", lerp, "B"); link(fres, "", lerp, "Alpha")
glow = sparam(m, "Glow", 0.25, -900, 600)
eg = node(m, unreal.MaterialExpressionMultiply, -400, 550)
link(shallow, "", eg, "A"); link(glow, "", eg, "B")
to_prop(cn, "", unreal.MaterialProperty.MP_NORMAL)
to_prop(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
to_prop(eg, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
to_prop(node(m, unreal.MaterialExpressionConstant, -400, 700, r=0.06), "", unreal.MaterialProperty.MP_ROUGHNESS)
to_prop(node(m, unreal.MaterialExpressionConstant, -400, 800, r=0.9), "", unreal.MaterialProperty.MP_SPECULAR)
finish(m, ism=True)
m_water = m

# --- M_ZFall : cascade translucide non éclairée ---------------------------------------------------------------
log("M_ZFall")
m = new_material("M_ZFall")
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("two_sided", True)
uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1200, 0, u_tiling=3.0, v_tiling=1.0)
pan = node(m, unreal.MaterialExpressionPanner, -950, 0, speed_x=0.0, speed_y=0.9)
link(uv, "", pan, "Coordinate")
smp = node(m, unreal.MaterialExpressionTextureSample, -700, 0, texture=t_fall)
smp.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
link(pan, "", smp, "UVs")
col = vparam(m, "Color", (0.7, 0.93, 1.0, 1), -700, 250)
inten = sparam(m, "Emissive", 2.2, -700, 400)
e1 = node(m, unreal.MaterialExpressionMultiply, -450, 150)
link(smp, "R", e1, "A"); link(col, "", e1, "B")
e2 = node(m, unreal.MaterialExpressionMultiply, -250, 200)
link(e1, "", e2, "A"); link(inten, "", e2, "B")
op = node(m, unreal.MaterialExpressionMultiply, -450, 450)
link(smp, "R", op, "A")
link(sparam(m, "Opacity", 0.85, -700, 550), "", op, "B")
to_prop(e2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
to_prop(op, "", unreal.MaterialProperty.MP_OPACITY)
finish(m, ism=True)
m_fall = m

# --- M_ZGlow : glyphes et énergie (non éclairé) --------------------------------------------------------------
log("M_ZGlow")
m = new_material("M_ZGlow")
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
col = vparam(m, "Color", (0.25, 1.0, 0.92, 1), -600, 0)
inten = sparam(m, "Emissive", 6.0, -600, 200)
e = node(m, unreal.MaterialExpressionMultiply, -300, 100)
link(col, "", e, "A"); link(inten, "", e, "B")
to_prop(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(m, ism=True)
m_glow = m

# --- M_ZFoliage : herbe et lierre (feuillage deux faces, dégradé pied → pointe, vent) ---------------------------
log("M_ZFoliage")
m = new_material("M_ZFoliage")
m.set_editor_property("two_sided", True)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
uvn = node(m, unreal.MaterialExpressionTextureCoordinate, -1300, -200)
wp = node(m, unreal.MaterialExpressionWorldPosition, -1300, 0)
t = node(m, unreal.MaterialExpressionTime, -1300, 150)
pir = node(m, unreal.MaterialExpressionPerInstanceRandom, -1300, 300)
basec = vparam(m, "BaseColor", (0.10, 0.24, 0.06, 1), -1300, 450)
tipc = vparam(m, "TipColor", (0.45, 0.62, 0.22, 1), -1300, 600)
wind = sparam(m, "Wind", 1.0, -1300, 750)
code_c = """
float h = saturate(1.0 - UV.y);
float3 c = lerp(Base, Tip, pow(h, 0.8));
return c * lerp(0.78, 1.18, R);
"""
cc = custom(m, -800, -100, code_c, ["UV", "Base", "Tip", "R"], desc="Couleur du feuillage")
link(uvn, "", cc, "UV"); link(basec, "", cc, "Base"); link(tipc, "", cc, "Tip"); link(pir, "", cc, "R")
code_w = """
float h = saturate(1.0 - UV.y);
float ph = W.x * 0.011 + W.y * 0.017;
float s = sin(T * 1.6 + ph) * 0.6 + sin(T * 2.7 + ph * 1.7) * 0.4;
return float3(s * 4.0, s * 2.5, 0) * h * h * Wind;
"""
cw = custom(m, -800, 300, code_w, ["UV", "W", "T", "Wind"], desc="Vent")
link(uvn, "", cw, "UV"); link(wp, "", cw, "W"); link(t, "", cw, "T"); link(wind, "", cw, "Wind")
sss = node(m, unreal.MaterialExpressionMultiply, -500, 150)
link(cc, "", sss, "A"); link(node(m, unreal.MaterialExpressionConstant, -700, 200, r=0.6), "", sss, "B")
to_prop(cc, "", unreal.MaterialProperty.MP_BASE_COLOR)
to_prop(sss, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
to_prop(cw, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
to_prop(sparam(m, "Roughness", 0.7, -500, 450), "", unreal.MaterialProperty.MP_ROUGHNESS)
to_prop(node(m, unreal.MaterialExpressionConstant, -500, 550, r=0.2), "", unreal.MaterialProperty.MP_SPECULAR)
finish(m, ism=True)
m_foliage = m

# --- M_ZBanner : tissu imprimé, glyphe lumineux (alpha = masque d'émission) ---------------------------------------
log("M_ZBanner")
t_banner = import_texture("T_Banner_Water.png", "T_Banner_Water")
m = new_material("M_ZBanner")
m.set_editor_property("two_sided", True)
bt = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1000, 0, parameter_name="Tex", texture=t_banner)
glowp = sparam(m, "Glow", 5.0, -1000, 300)
e1 = node(m, unreal.MaterialExpressionMultiply, -700, 150)
link(bt, "RGB", e1, "A"); link(bt, "A", e1, "B")
e2 = node(m, unreal.MaterialExpressionMultiply, -500, 200)
link(e1, "", e2, "A"); link(glowp, "", e2, "B")
to_prop(bt, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
to_prop(e2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
to_prop(sparam(m, "Roughness", 0.85, -500, 400), "", unreal.MaterialProperty.MP_ROUGHNESS)
finish(m, ism=True)
m_banner = m

# --- Instances pour le kit de la citerne (lues par AZCistern::Kit) -------------------------------------------
log("instances du kit")
mi("MI_Kit_Floor", m_stone, {"Tint": (0.64, 0.61, 0.53, 1)}, {"TexScale": 300.0, "MossAmount": 0.35})
mi("MI_Kit_RoundFloor", m_stone, {"Tint": (0.70, 0.64, 0.52, 1)}, {"TexScale": 300.0, "MossAmount": 0.2})
mi("MI_Kit_Wall", m_stone, {"Tint": (0.56, 0.55, 0.50, 1)}, {"TexScale": 220.0, "MossAmount": 0.55})
mi("MI_Kit_Trim", m_stone, {"Tint": (0.76, 0.74, 0.67, 1)}, {"TexScale": 160.0, "MossAmount": 0.7})
mi("MI_Kit_RoundTrim", m_stone, {"Tint": (0.78, 0.74, 0.64, 1)}, {"TexScale": 160.0, "MossAmount": 0.3})
mi("MI_Kit_Pillar", m_stone, {"Tint": (0.73, 0.71, 0.64, 1)}, {"TexScale": 180.0, "MossAmount": 0.6})
mi("MI_Kit_Rock", m_stone, {"Tint": (0.52, 0.49, 0.43, 1)}, {"TexScale": 400.0, "MossAmount": 0.9})
mi("MI_Kit_Tower", m_stone, {"Tint": (0.66, 0.70, 0.76, 1)}, {"TexScale": 500.0, "MossAmount": 0.15})
mi("MI_Kit_Water", m_water, {"ShallowColor": (0.10, 0.60, 0.64, 1), "DeepColor": (0.02, 0.22, 0.30, 1)}, {"Glow": 0.1})
mi("MI_Kit_Fall", m_fall)
mi("MI_Kit_Foam", m_glow, {"Color": (0.85, 0.97, 1.0, 1)}, {"Emissive": 1.6})
mi("MI_Kit_Glyph", m_glow, {"Color": (0.25, 1.0, 0.92, 1)}, {"Emissive": 7.0})
mi("MI_Kit_RoundGlyph", m_glow, {"Color": (0.25, 1.0, 0.92, 1)}, {"Emissive": 4.0})
mi("MI_Kit_Banner", m_toon, {"Color": (0.04, 0.20, 0.24, 1)}, {"Roughness": 0.9, "RimStrength": 0.1})
mi("MI_Kit_Moss", m_toon, {"Color": (0.20, 0.38, 0.13, 1)}, {"Roughness": 0.95, "RimStrength": 0.2})
mi("MI_Kit_Grass", m_foliage, {"BaseColor": (0.08, 0.20, 0.05, 1), "TipColor": (0.42, 0.62, 0.20, 1)}, {"Wind": 1.0})
mi("MI_Kit_Leaf", m_foliage, {"BaseColor": (0.07, 0.19, 0.06, 1), "TipColor": (0.24, 0.45, 0.14, 1)}, {"Wind": 0.25})
mi("MI_Kit_Cloth", m_banner)
mi("MI_Kit_Metal", m_toon, {"Color": (0.85, 0.62, 0.22, 1)}, {"Roughness": 0.35, "RimStrength": 0.3})
mi("MI_Kit_Dark", m_stone, {"Tint": (0.26, 0.28, 0.28, 1)}, {"TexScale": 160.0, "MossAmount": 0.15})
mi("MI_Kit_Lily", m_toon, {"Color": (0.16, 0.42, 0.12, 1)}, {"Roughness": 0.6, "RimStrength": 0.3})
mi("MI_Kit_Arcade", m_stone, {"Tint": (0.74, 0.72, 0.65, 1)}, {"TexScale": 140.0, "MossAmount": 0.65})
mi("MI_Kit_ColumnBroken", m_stone, {"Tint": (0.70, 0.68, 0.61, 1)}, {"TexScale": 180.0, "MossAmount": 0.85})
mi("MI_Kit_Rubble", m_stone, {"Tint": (0.60, 0.58, 0.52, 1)}, {"TexScale": 200.0, "MossAmount": 0.9})
# Surcharges par maillage et emplacement (lues par ZVis::Tint : MI_<maillage>_<emplacement>)
mi("MI_SM_Golem_Body", m_stone_local, {"Tint": (0.66, 0.63, 0.55, 1)}, {"TexScale": 125.0, "MossAmount": 0.75})
mi("MI_SM_Golem_Dark", m_stone_local, {"Tint": (0.38, 0.38, 0.36, 1)}, {"TexScale": 95.0, "MossAmount": 0.35})
mi("MI_SM_Golem_Moss", m_foliage, {"BaseColor": (0.10, 0.25, 0.06, 1), "TipColor": (0.30, 0.52, 0.16, 1)}, {"Wind": 0.0})
log("terminé")
