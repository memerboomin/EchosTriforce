"""Phase 5: UVs, bakes, delivery materials. Three texture sets (hero tier: 3-6 material groups):
  Head (2k): Head, CapTail            Body (4k, battle close-ups): Body, TunicSkirt, Belt, Buckle, cuffs, Pouch
  Gear (2k): Shield, ShieldRim, Sheath, Hilt, Guard, Ocarina, Mask
Head/Body colour, roughness, metallic and normal are baked from HIGH with its AI gear removed (HIGH_Bake) and the
de-shadowed, sheet-harmonised albedo of the shipped SKM_Link (out/link/T_Link_Albedo.png, same UVs as HIGH).
Gear colour is projected from the back sheet (front sheet for the ocarina and mask), gear roughness/metallic are
per-part constants, gear normals are flat. AO is baked on the whole LOD0 so parts occlude each other.
LOD1/LOD2 are rebuilt from the unwrapped LOD0 (same UV layout, same materials).
  blender -b --python build/05_materials.py -- --master CH_Link_master.blend [--res-scale 0.25]
"""
import bpy, bmesh, os, sys, math, re, time
import numpy as np
from mathutils import Vector
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _lib as L
L.PHASE = "05_materials"; L.OWNER_TAG = "phase:05_materials"
HERE = os.path.dirname(os.path.abspath(__file__))
ASSET = os.path.normpath(os.path.join(HERE, ".."))
ALBEDO = os.path.normpath(os.path.join(ASSET, "../out/link/T_Link_Albedo.png"))
TEX_OUT = os.path.join(ASSET, "textures")

SETS = {
    "Head": (2048, ["Head", "CapTail"]),
    "Body": (4096, ["Body", "TunicSkirt", "Belt", "Buckle", "BootCuff_L", "BootCuff_R", "Pouch"]),
    "Gear": (2048, ["Shield", "ShieldRim", "Sheath", "Hilt", "Guard", "Ocarina", "Mask"]),
}
EXTRUSION, RAY = 0.04, 0.08   # LOW fitted to the sheets sits up to ~3 cm off HIGH (arms 2.5 cm mean, notes 03)
MARGIN = 16

# gear surface: (sheet, roughness, metallic); sheet 'back'/'front' = planar projection of that T-pose sheet
GEAR = {
    "Shield": ("back", 0.45, 0.0), "ShieldRim": ("back", 0.35, 1.0), "Sheath": ("back", 0.4, 0.0),
    # the sheets draw the grip at 45 deg where the rigid sword is straight: projected colours land off the parts,
    # so hilt and guard take the sheet's colours as constants (ref/back.png: violet guard median (96, 89, 131),
    # blue grip wrap (72, 85, 117))
    "Hilt": ((0.0648, 0.0908, 0.1779), 0.55, 0.0), "Guard": ((0.117, 0.0999, 0.227), 0.35, 0.3), "Ocarina": ("front", 0.2, 0.0),
    "Mask": ("front", 0.3, 0.0),
}
# texels with no HIGH hit (body back under the shield, where the AI model fused the shield to the tunic; leg backs
# the side-panel fit moved off the HIGH) start as a sentinel colour and are filled from their baked neighbours
SENTINEL = (1.0, 0.0, 1.0, 1.0)
CAP_GREEN = (0.1022, 0.159, 0.0762)   # linear; median of the cap on ref/back.png (cols 975-1005, rows 120-200)
                                      # the tail follows the side panel, far from the HIGH hood: no bake hit
SHIELD_BACKFACE = (0.18, 0.11, 0.07)   # shield inner face: leather-brown (inferred, not drawn)
PPM, AXIS, GROUND = 785.0, 989.5, 1471.0      # registered ref/front.png and ref/back.png (1980 x 1493)
GEAR_DX = -0.0096                             # 03_forms.GEAR_DX: gear moved halfway between the sheets


def part_of(name):
    return re.sub(r"_LOD\d$", "", name).replace("CH_Link_", "")


def lod_objects(level):
    return {part_of(o.name): o for o in bpy.data.collections["LOD%d" % level].objects if o.type == "MESH"}


# UVS

def unwrap_set(objs):
    bpy.ops.object.mode_set(mode="OBJECT") if bpy.context.object and bpy.context.object.mode != "OBJECT" else None
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
        if not o.data.uv_layers:
            o.data.uv_layers.new(name="UVMap")
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.002, area_weight=1.0,
                             correct_aspect=True, scale_to_bounds=False)
    try:
        bpy.ops.uv.pack_islands(rotate=True, margin=0.003, shape_method="CONCAVE")
    except TypeError:
        bpy.ops.uv.pack_islands(rotate=True, margin=0.003)
    bpy.ops.object.mode_set(mode="OBJECT")


def uv_stats(objs, res):
    """Texel density (px per metre) and fill of a set, from mesh area vs UV area."""
    area3, area2 = 0.0, 0.0
    for o in objs:
        me = o.data
        uv = me.uv_layers.active.data
        for p in me.polygons:
            area3 += p.area
            pts = [uv[i].uv for i in p.loop_indices]
            a = 0.0
            for k in range(1, len(pts) - 1):
                a += abs((pts[k] - pts[0]).cross(pts[k + 1] - pts[0])) / 2
            area2 += a
    return res * math.sqrt(area2 / max(area3, 1e-9)), area2


# BAKE SOURCE: HIGH without its AI gear

def build_high_bake(body_lod0):
    src = bpy.data.objects["CH_Link_HIGH_AI"]
    ob = bpy.data.objects.new("CH_Link_HIGH_Bake", src.data.copy())
    L.col("HIGH").objects.link(ob)
    L.own(ob)
    # back surface of the finished body, per height (what lies clearly behind it is gear)
    co = np.array([v.co[:] for v in body_lod0.data.vertices])
    sel = np.abs(co[:, 0]) < 0.2
    zs = np.arange(0.5, 1.65, 0.01)
    yb = np.array([co[sel & (np.abs(co[:, 2] - z) < 0.01), 1].max() if (sel & (np.abs(co[:, 2] - z) < 0.01)).any() else 0.1
                   for z in zs])
    a, b = Vector((0.27, 0.0, 1.70)), Vector((-0.40, 0.0, 0.67))    # rig_model.SHEATH axis (HIGH frame, front view)
    def is_gear(p):
        if p.z > 1.52 and abs(p.x) < 0.16 and p.y < 0.2:
            return False                                             # head (rig_model.classify)
        back = float(np.interp(min(max(p.z, 0.5), 1.64), zs, yb))
        if 0.78 < p.z < 1.82 and p.y > back + 0.035:     # above the hem only: the HIGH thighs are deeper than LOW
            return True
        ab = Vector((b.x - a.x, 0, b.z - a.z)); ap = Vector((p.x - a.x, 0, p.z - a.z))
        t = max(0.0, min(1.0, ap.dot(ab) / ab.length_squared))
        return (ap - ab * t).length < 0.05 and p.y > back + 0.015 and 0.6 < p.z < 1.82   # outside the body only
    bm = bmesh.new(); bm.from_mesh(ob.data)
    gear = [v for v in bm.verts if is_gear(v.co)]
    bmesh.ops.delete(bm, geom=gear, context="VERTS")
    bm.to_mesh(ob.data); bm.free()
    ob.hide_set(True)
    L.log("HIGH_Bake: %d gear vertices removed" % len(gear))
    # swap its base colour to the de-shadowed albedo
    img = bpy.data.images.load(ALBEDO, check_existing=True)
    img.colorspace_settings.name = "sRGB"
    m = ob.data.materials[0].copy()
    m.name = "M_HIGH_Bake"
    for n in m.node_tree.nodes:
        if n.type == "TEX_IMAGE" and n.image and n.image.name == "texture_pbr_20250901.png":
            n.image = img
    ob.data.materials[0] = m
    return ob


# MATERIALS AND IMAGES

def new_image(name, res, data, fill):
    img = bpy.data.images.get(name)
    if img:
        bpy.data.images.remove(img)
    img = bpy.data.images.new(name, res, res, alpha=False, float_buffer=False)
    img.colorspace_settings.name = "Non-Color" if data else "sRGB"
    img.generated_color = fill
    return img


def bake_node(mat, img):
    nt = mat.node_tree
    for n in nt.nodes:
        n.select = False
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = img
    t.select = True
    nt.nodes.active = t
    return t


def emission_material(name, color=None, sheet=None, dx=0.0):
    """Emission shader: a constant colour, or the planar projection of a T-pose sheet at world position."""
    m = bpy.data.materials.new(name)
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    if sheet is None:
        em.inputs["Color"].default_value = (*color, 1.0)
        return m
    img = bpy.data.images.load(os.path.join(ASSET, "ref", sheet + ".png"), check_existing=True)
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sep.inputs[0])
    W, H = img.size
    sx = (-PPM if sheet == "back" else PPM) / W
    def affine(inp, scale, offset):
        mul = nt.nodes.new("ShaderNodeMath"); mul.operation = "MULTIPLY_ADD"
        nt.links.new(inp, mul.inputs[0]); mul.inputs[1].default_value = scale; mul.inputs[2].default_value = offset
        return mul.outputs[0]
    # gear was shifted by GEAR_DX from the back-sheet position: sample the sheet where the part was drawn
    u = affine(sep.outputs["X"], sx, (AXIS - (-PPM if sheet == "back" else PPM) * dx) / W)
    v = affine(sep.outputs["Z"], PPM / H, (H - GROUND) / H)
    comb = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(u, comb.inputs["X"]); nt.links.new(v, comb.inputs["Y"])
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = img; tex.extension = "EXTEND"
    nt.links.new(comb.outputs[0], tex.inputs["Vector"])
    nt.links.new(tex.outputs["Color"], em.inputs["Color"])
    return m


def high_emission(high, socket_name):
    """Temporarily route one Principled input of the HIGH material (its linked texture) to an Emission."""
    m = high.data.materials[0]
    nt = m.node_tree
    bsdf = [n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"][0]
    out = [n for n in nt.nodes if n.type == "OUTPUT_MATERIAL"][0]
    old = out.inputs["Surface"].links[0].from_socket
    em = nt.nodes.new("ShaderNodeEmission")
    inp = bsdf.inputs[socket_name]
    if inp.links:
        nt.links.new(inp.links[0].from_socket, em.inputs["Color"])
    else:
        v = inp.default_value
        em.inputs["Color"].default_value = (v, v, v, 1) if isinstance(v, float) else v
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    def restore():
        nt.links.new(old, out.inputs["Surface"])
        nt.nodes.remove(em)
    return restore


def setup_cycles(scene, samples, device="GPU"):
    scene.render.engine = "CYCLES"
    scene.cycles.samples = samples
    scene.cycles.use_denoising = False
    if device == "CPU":     # Metal baking threw NSInvalidArgumentException on this Mac (2026-09-26): CPU fallback
        scene.cycles.device = "CPU"
        bk = scene.render.bake
        bk.margin = MARGIN
        bk.use_clear = False
        return
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        prefs.compute_device_type = "METAL"
        prefs.get_devices()
        for d in prefs.devices:
            d.use = d.type == "METAL"
        scene.cycles.device = "GPU"
    except Exception:
        scene.cycles.device = "CPU"
    bk = scene.render.bake
    bk.margin = MARGIN
    bk.use_clear = False


def isolate(keep):
    for o in bpy.context.scene.objects:
        if o.type in ("MESH", "CURVE"):
            o.hide_render = o not in keep


def do_bake(low, img, kind, high=None, material_for_low=None):
    """Bake one LOW object into img (shared image; use_clear off). Returns seconds."""
    t0 = time.time()
    scene = bpy.context.scene
    saved = list(low.data.materials)
    tmp = material_for_low
    if tmp is not None:
        low.data.materials.clear(); low.data.materials.append(tmp)
    mats = [m for m in low.data.materials if m]
    nodes = [(m, bake_node(m, img)) for m in mats]
    bpy.ops.object.select_all(action="DESELECT")
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    bk = scene.render.bake
    if high is not None:
        high.hide_set(False); high.select_set(True)
        bk.use_selected_to_active = True
        bk.cage_extrusion = EXTRUSION
        bk.max_ray_distance = RAY
    else:
        bk.use_selected_to_active = False
    if kind == "NORMAL":
        bk.normal_space = "TANGENT"
        bpy.ops.object.bake(type="NORMAL")
    elif kind == "AO":
        bpy.ops.object.bake(type="AO")
    else:
        bpy.ops.object.bake(type="EMIT")
    for m, n in nodes:
        m.node_tree.nodes.remove(n)
    if tmp is not None:
        low.data.materials.clear()
        for m in saved:
            low.data.materials.append(m)
    if high is not None:
        high.select_set(False); high.hide_set(True)
    return time.time() - t0


sys.path.insert(0, os.path.normpath(os.path.join(HERE, "../../../blender")))


def fill_misses(img):
    """Sentinel texels (no bake hit, and the space between islands) get the push-pull average of baked texels:
    misses inside an island take their neighbours' colour, the gutters become padding."""
    from make_character import push_pull
    w, h = img.size
    a = np.empty(w * h * 4, dtype=np.float32); img.pixels.foreach_get(a); a = a.reshape(h, w, 4)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    miss = (r - g > 0.3) & (b - g > 0.3) & (np.abs(r - b) < 0.35)     # sentinel and its antialiased fringes
    miss = miss | np.roll(miss, 1, 0) | np.roll(miss, -1, 0) | np.roll(miss, 1, 1) | np.roll(miss, -1, 1)
    rgb = push_pull(a[..., :3].astype(np.float32), ~miss)
    a[..., :3] = np.where(miss[..., None], rgb, a[..., :3])
    img.pixels.foreach_set(a.ravel())
    img.update()
    L.log("  %s: %.1f %% sentinel texels filled" % (img.name, 100.0 * miss.mean()))


def save_png(img, path):
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()


def delivery_material(set_name, images):
    m = bpy.data.materials.new("M_Link_%s" % set_name)
    nt = m.node_tree
    bsdf = [n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"][0]
    def tex(key):
        t = nt.nodes.new("ShaderNodeTexImage"); t.image = images[key]; return t
    nt.links.new(tex("basecolor").outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(tex("roughness").outputs["Color"], bsdf.inputs["Roughness"])
    nt.links.new(tex("metallic").outputs["Color"], bsdf.inputs["Metallic"])
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(tex("normal").outputs["Color"], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    m["ao_map"] = images["ao"].name
    return m


def rebuild_lods(lod0):
    """LOD1/LOD2 from the unwrapped LOD0 (replaces Phase 4's copies): same UVs and material."""
    ratios = {1: 0.42, 2: 0.16}; head = {1: 0.40, 2: 0.14}; mins = {1: 60, 2: 24}
    for o in list(bpy.data.collections["LOD1"].objects) + list(bpy.data.collections["LOD2"].objects):
        bpy.data.objects.remove(o, do_unlink=True)
    tot = {1: 0, 2: 0}
    for part, o in lod0.items():
        t0 = len(o.data.polygons)
        for k in (1, 2):
            ob = L.duplicate(o, "CH_Link_%s_LOD%d" % (part, k), collection="LOD%d" % k)
            L.own(ob)
            r = head[k] if part == "Head" else ratios[k]
            r = max(r, min(1.0, mins[k] / max(1, t0)))
            if r < 1.0:
                L.decimate(ob, r)
                L.apply_all_modifiers(ob)
            tot[k] += len(ob.data.polygons)
    L.log("LODs rebuilt from the unwrapped LOD0:", tot)


def main():
    a = L.argv_after_dashes()
    master = a[a.index("--master") + 1]
    rs = float(a[a.index("--res-scale") + 1]) if "--res-scale" in a else 1.0
    L.open_master(master)
    L.clear_owned()
    os.makedirs(TEX_OUT, exist_ok=True)
    render_state = {o.name: o.hide_render for o in bpy.data.objects}
    scene = bpy.context.scene
    lod0 = lod_objects(0)
    for o in lod0.values():
        o.data.materials.clear()
        while o.data.uv_layers:
            o.data.uv_layers.remove(o.data.uv_layers[0])
    high = build_high_bake(lod0["Body"])
    setup_cycles(scene, 1, a[a.index("--device") + 1] if "--device" in a else "GPU")
    for set_name, (res, parts) in SETS.items():
        res = int(res * rs)
        objs = [lod0[p] for p in parts]
        unwrap_set(objs)
        dens, fill = uv_stats(objs, res)
        L.log("%s set: %d px, %.0f px/m, UV fill %.0f %%" % (set_name, res, dens, fill * 100))
        imgs = {
            "basecolor": new_image("T_Link_%s_BaseColor" % set_name, res, False,
                                   SENTINEL if set_name != "Gear" else (0.5, 0.5, 0.5, 1)),
            "normal": new_image("T_Link_%s_Normal" % set_name, res, True, (0.5, 0.5, 1.0, 1)),
            "roughness": new_image("T_Link_%s_Roughness" % set_name, res, True, (0.6, 0.6, 0.6, 1)),
            "metallic": new_image("T_Link_%s_Metallic" % set_name, res, True, (0, 0, 0, 1)),
            "ao": new_image("T_Link_%s_AO" % set_name, res, True, (1, 1, 1, 1)),
        }
        mat = delivery_material(set_name, imgs)
        for o in objs:
            o.data.materials.append(mat)
        t = 0.0
        for o in objs:
            part = part_of(o.name)
            if part == "CapTail":
                isolate([o])
                t += do_bake(o, imgs["basecolor"], "EMIT", material_for_low=emission_material("tmp_cap", color=CAP_GREEN))
                t += do_bake(o, imgs["roughness"], "EMIT", material_for_low=emission_material("tmp_capr", color=(0.85,) * 3))
                t += do_bake(o, imgs["metallic"], "EMIT", material_for_low=emission_material("tmp_capm", color=(0.0,) * 3))
            elif set_name != "Gear":
                isolate([o, high])
                for key, sock in (("basecolor", "Base Color"), ("roughness", "Roughness"), ("metallic", "Metallic")):
                    restore = high_emission(high, sock)
                    t += do_bake(o, imgs[key], "EMIT", high=high)
                    restore()
                t += do_bake(o, imgs["normal"], "NORMAL", high=high)
            else:
                sheet, rough, metal = GEAR[part]
                isolate([o])
                if isinstance(sheet, tuple):
                    m = emission_material("tmp_%s_col" % part, color=sheet)
                else:
                    m = emission_material("tmp_%s_col" % part, sheet=sheet, dx=GEAR_DX if part not in ("Ocarina", "Mask") else 0.0)
                t += do_bake(o, imgs["basecolor"], "EMIT", material_for_low=m)
                t += do_bake(o, imgs["roughness"], "EMIT", material_for_low=emission_material("tmp_r", color=(rough,) * 3))
                t += do_bake(o, imgs["metallic"], "EMIT", material_for_low=emission_material("tmp_m", color=(metal,) * 3))
            L.log("  %-12s baked (%.0f s cumulative)" % (part, t))
        if set_name != "Gear":
            fill_misses(imgs["basecolor"])
        # AO on the whole LOD0 (neighbours occlude), 64 samples
        isolate(list(lod0.values()))
        scene.cycles.samples = 64
        for o in objs:
            t += do_bake(o, imgs["ao"], "AO")
        scene.cycles.samples = 1
        for key, img in imgs.items():
            save_png(img, os.path.join(TEX_OUT, img.name + ".png"))
        L.log("%s set done in %.0f s" % (set_name, t))
    for o in bpy.data.objects:
        if o.name in render_state:
            o.hide_render = render_state[o.name]
    bpy.data.objects["CH_Link_HIGH_Bake"].hide_render = True
    for m in [m for m in bpy.data.materials if m.name.startswith("tmp_")]:
        bpy.data.materials.remove(m)
    rebuild_lods(lod0)
    for o in bpy.data.objects:
        if any(c.name in ("LOD0", "LOD1", "LOD2") for c in o.users_collection):
            o.hide_render = False
    L.save_master(master)

main()
