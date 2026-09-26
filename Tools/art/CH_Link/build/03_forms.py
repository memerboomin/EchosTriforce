"""Phase 3: forms. The blockout body (Skin chain, Subdivision 2, all quads, loops at every joint) is projected on
the HIGH surface along its normals, part by part (fingers and the hip/thigh zone under the tunic skirt stay as
modelled); gear is rebuilt from the sheet measurements with real thickness. Blockout objects are moved to the
hidden BLOCKOUT collection.
  blender -b --python build/03_forms.py -- --master CH_Link_master.blend
"""
import bpy, bmesh, os, sys, math
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
from mathutils.geometry import intersect_point_line
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _lib as L
import importlib.util
_spec = importlib.util.spec_from_file_location("blk", os.path.join(os.path.dirname(os.path.abspath(__file__)), "02_blockout.py"))
L.PHASE = "03_forms"; L.OWNER_TAG = "phase:03_forms"

# reuse the blockout's measurement helpers without running its main()
src = open(_spec.origin).read().replace("\nmain()\n", "\n")
BLK = {"__file__": _spec.origin, "__name__": "blk"}
exec(compile(src, _spec.origin, "exec"), BLK)
L.PHASE = "03_forms"; L.OWNER_TAG = "phase:03_forms"
radial, despike, perp_frame, high_bvh = BLK["radial"], BLK["despike"], BLK["perp_frame"], BLK["high_bvh"]
sheet_front, sheet_back = BLK["sheet_front"], BLK["sheet_back"]

BAND = 0.06          # projection search band along the normal (m); HIGH clothes sit within 6 cm of the blockout
BANDS = {"head": 0.09, "foot": 0.035,   # hair, fringe and cap crown stand up to 9 cm off the skull blob
         "leg": 0.09}    # drawn boots sit ~5 cm behind the shin bone (calf, heel): blockout legs are centred on it
NORMAL_MIN = 0.25    # accept a HIGH hit only if its face normal agrees with the vertex normal (ears, props rejected)
SKIRT_TOP = 1.02     # front sheet belt bottom (row 670)
HEM = 0.715          # front sheet hem (row ~910)


# BLOCKOUT HAND-OFF

def retire_blockout():
    blk = L.col("BLOCKOUT")
    for o in [o for o in bpy.data.objects if o.get("owner") == "phase:02_blockout"]:
        for c in list(o.users_collection):
            c.objects.unlink(o)
        blk.objects.link(o)
        o.hide_render = True
        o.hide_set(True)
    lc = bpy.context.view_layer.layer_collection.children.get("BLOCKOUT")
    if lc:
        lc.exclude = True


def restore_blockout():
    """Rerun safety: bring phase-02 objects back where the script can read them."""
    lc = bpy.context.view_layer.layer_collection.children.get("BLOCKOUT")
    if lc:
        lc.exclude = False


# PART LABELS FROM THE SKELETON

def bone_segments(arm):
    mw = arm.matrix_world
    segs = []
    skip = ("ik_", "interaction", "center_of_mass", "root")
    for pb in arm.pose.bones:
        if pb.name.startswith(skip) or "twist" in pb.name:
            continue
        segs.append((pb.name, mw @ pb.head, mw @ pb.tail))
    return segs


def nearest_bone(p, segs):
    best, bn = 1e9, None
    for n, h, t in segs:
        q, f = intersect_point_line(p, h, t)
        f = min(1.0, max(0.0, f))
        d = (p - (h + (t - h) * f)).length
        if d < best:
            best, bn = d, n
    return bn


def part_of(bone):
    if "metacarpal" in bone:
        return "hand"
    if bone.startswith(("index", "middle", "ring", "pinky", "thumb")):
        return "finger"
    if bone in ("head", "neck_02"):
        return "head"
    if bone.startswith("neck"):
        return "neck"
    if bone.startswith("hand"):
        return "hand"
    if bone.startswith(("upperarm", "lowerarm", "clavicle")):
        return "arm"
    if bone.startswith(("thigh",)):
        return "thigh"
    if bone.startswith(("foot", "ball")):
        return "foot"
    if bone.startswith(("calf",)):
        return "leg"
    return "torso"


# PROJECTION

def high_normal_ok(bvh, loc, fidx, n):
    return True


def project_point(bvh, p, n, band):
    """Hit on HIGH closest to p along +-n within band, with the face normal agreeing with n."""
    best = None
    for d in (n, -n):
        o = p + d * band
        hit = bvh.ray_cast(o, -d, band * 2)
        if hit[0] is not None and hit[1].dot(n) > NORMAL_MIN:
            dd = (hit[0] - p).length
            if dd <= band and (best is None or dd < best[1]):
                best = (hit[0], dd)
    return best


def relax(bm, fixed, iters=2, amount=0.5):
    for _ in range(iters):
        new = {}
        for v in bm.verts:
            if v.index in fixed or not v.link_edges:
                continue
            c = sum((e.other_vert(v).co for e in v.link_edges), Vector()) / len(v.link_edges)
            d = c - v.co
            d -= v.normal * d.dot(v.normal)       # tangential only: keep the projected surface
            new[v] = v.co + d * amount
        for v, co in new.items():
            v.co = co


def densify_core(bm, parts, arm):
    """One more subdivision (16 -> 32 around) on the torso, neck, head, shoulders to mid upper arm and hips to
    mid thigh. The transition rings (tris/ngons) sit at mid upper arm and mid thigh, away from bending joints."""
    P = {b.name: arm.matrix_world @ b.head for b in arm.pose.bones}
    def core(v):
        part = parts[v.index]
        if part in ("torso", "neck", "head"):
            return True
        if part == "arm":
            return abs(v.co.x) < abs((P["upperarm_l"].x + P["lowerarm_l"].x) / 2)
        if part == "thigh":
            return v.co.z > (P["thigh_l"].z + P["calf_l"].z) / 2
        return False
    faces = [f for f in bm.faces if all(core(v) for v in f.verts)]
    edges = list({e for f in faces for e in f.edges})
    bmesh.ops.subdivide_edges(bm, edges=edges, cuts=1, use_grid_fill=True, smooth=0.0)
    # remaining ngons at the transition rings are split so the delivery mesh is quads and triangles only
    ngons = [f for f in bm.faces if len(f.verts) > 4]
    bmesh.ops.triangulate(bm, faces=ngons, quad_method="BEAUTY", ngon_method="BEAUTY")
    L.log("core densified: %d faces subdivided, %d transition ngons split" % (len(faces), len(ngons)))


SMOOTH_ITERS = 25   # Laplacian iterations on the HIGH copy used as the projection target (AI triangle noise ~3 mm)
SBVH = None


def smooth_high():
    """Projection target: HIGH with its surface noise smoothed (volume preserved). The bake still uses HIGH."""
    global SBVH
    src = bpy.data.objects["CH_Link_HIGH_AI"]
    ob = bpy.data.objects.new("CH_Link_HIGH_Smooth", src.data.copy())
    L.col("HIGH").objects.link(ob)
    L.own(ob)
    m = ob.modifiers.new("Smooth", "LAPLACIANSMOOTH")
    m.lambda_factor = 1.0
    m.iterations = SMOOTH_ITERS
    m.use_volume_preserve = True
    m.use_normalized = True
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    ob.modifiers.clear()
    old = ob.data; ob.data = me; bpy.data.meshes.remove(old)
    ob.hide_render = True
    ob.hide_set(True)
    n = len(me.vertices)
    co = np.empty(n * 3); me.vertices.foreach_get("co", co); co = co.reshape(-1, 3)
    me.calc_loop_triangles()
    tri = np.empty(len(me.loop_triangles) * 3, dtype=np.int64); me.loop_triangles.foreach_get("vertices", tri)
    SBVH = BVHTree.FromPolygons([tuple(v) for v in co], [tuple(t) for t in tri.reshape(-1, 3)], all_triangles=True)
    L.log("smoothed HIGH target ready (%d iterations)" % SMOOTH_ITERS)
    return SBVH


# SILHOUETTE FIT: the sheets win for dimensions (the AI HIGH is 1-2 cm fat on legs and arms)
MATTE = None
M_AXIS, M_GROUND, M_PPM = 989.5, 1471.0, 785.0      # ref/matte_front.png


def matte():
    global MATTE
    if MATTE is None:
        img = bpy.data.images.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../ref/matte_front.png"))
        w, h = img.size
        a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1, :, 3]
        MATTE = a > 0.5
        bpy.data.images.remove(img)
    return MATTE


def runs(mask_line):
    xs = np.nonzero(mask_line)[0]
    if len(xs) == 0:
        return []
    cut = np.nonzero(np.diff(xs) > 2)[0]
    starts = np.r_[xs[0], xs[cut + 1]]; ends = np.r_[xs[cut], xs[-1]]
    return list(zip(starts, ends))


def drawn_leg(z, sign):
    row = int(round(M_GROUND - z * M_PPM))
    rs = [(a, b) for a, b in runs(matte()[row]) if b - a > 20]
    rs = [((a - M_AXIS) / M_PPM, (b - M_AXIS) / M_PPM) for a, b in rs]
    rs = [r for r in rs if (r[0] + r[1]) / 2 * sign > 0.02]
    return min(rs, key=lambda r: abs((r[0] + r[1]) / 2 - sign * 0.11)) if rs else None


def drawn_arm(x):
    col = int(round(M_AXIS + x * M_PPM))
    column = matte()[:, col]
    r0, r1 = int(M_GROUND - 1.56 * M_PPM), int(M_GROUND - 1.22 * M_PPM)
    ys = np.nonzero(column[r0:r1])[0]
    if len(ys) == 0:
        return None
    cut = np.nonzero(np.diff(ys) > 2)[0]
    ends = np.r_[ys[cut], ys[-1]]; starts = np.r_[ys[0], ys[cut + 1]]
    k = int(np.argmax(ends - starts))
    return (M_GROUND - (r0 + ends[k])) / M_PPM, (M_GROUND - (r0 + starts[k])) / M_PPM   # (bottom z, top z)


def drawn_torso(z):
    row = int(round(M_GROUND - z * M_PPM))
    rs = runs(matte()[row])
    rs = [((a - M_AXIS) / M_PPM, (b - M_AXIS) / M_PPM) for a, b in rs]
    rs = [r for r in rs if r[0] < 0 < r[1]]
    return rs[0] if rs else None


def affine_fit(samples, model_ext, drawn_ext, smooth=5, sc_range=(0.7, 1.3)):
    """Per sample (lo, hi) model -> drawn maps, smoothed along the scan axis; missing samples interpolated."""
    s, a, b = [], [], []
    for t in samples:
        m, d = model_ext(t), drawn_ext(t)
        if m is None or d is None or m[1] - m[0] < 0.01:
            continue
        sc = (d[1] - d[0]) / (m[1] - m[0])
        if not sc_range[0] < sc < sc_range[1]:
            continue
        s.append(t); a.append(d[0] - m[0] * sc); b.append(sc)
    if len(s) < 3:
        return None
    k = np.ones(smooth) / smooth
    a = np.convolve(np.pad(a, smooth // 2, mode="edge"), k, "valid")
    b = np.convolve(np.pad(b, smooth // 2, mode="edge"), k, "valid")
    return np.array(s), a, b


def fit_to_sheet(objs_parts, arm):
    """objs_parts: list of (object, per-vertex part list or None). Legs and cuffs: X per z; arms: Z per x;
    torso above the belt: X per z. Depth (Y) keeps the HIGH."""
    P = {b.name: arm.matrix_world @ b.head for b in arm.pose.bones}
    allv = []
    for ob, parts in objs_parts:
        for v in ob.data.vertices:
            allv.append((ob, v, parts[v.index] if parts else "leg"))
    def ext(sel, axis, key, t, tol=0.01):
        vals = [v.co[axis] for ob, v, p in sel if abs(v.co[key] - t) < tol]
        return (min(vals), max(vals)) if len(vals) >= 4 else None
    report = {}
    for sign, side in ((1, "l"), (-1, "r")):
        legs = [(ob, v, p) for ob, v, p in allv if p in ("leg", "thigh", "foot") and v.co.x * sign > 0 and 0.02 < v.co.z < HEM]
        zs = np.arange(0.03, HEM - 0.01, 0.02)
        f = affine_fit(zs, lambda t: ext(legs, 0, 2, t), lambda t: drawn_leg(t, sign))
        if f:
            s, a, b = f
            d = []
            for ob, v, p in legs:
                A, Bv = np.interp(v.co.z, s, a), np.interp(v.co.z, s, b)
                nx = A + v.co.x * Bv
                d.append(abs(nx - v.co.x)); v.co.x = nx
            report["leg_" + side] = round(float(np.mean(d)), 4)
        arms = [(ob, v, p) for ob, v, p in allv if p in ("arm", "hand") and v.co.x * sign > 0.22]
        xs = np.arange(0.26, 0.74, 0.02) * sign
        f = affine_fit(xs if sign > 0 else xs[::-1], lambda t: ext(arms, 2, 0, t), drawn_arm)
        if f:
            s, a, b = f
            d = []
            for ob, v, p in arms:
                w = min(1.0, max(0.0, (abs(v.co.x) - 0.22) / 0.06))
                A, Bv = np.interp(v.co.x, s, a), np.interp(v.co.x, s, b)
                nz = A + v.co.z * Bv
                nz = v.co.z + (nz - v.co.z) * w
                d.append(abs(nz - v.co.z)); v.co.z = nz
            report["arm_" + side] = round(float(np.mean(d)), 4)
    tor = [(ob, v, p) for ob, v, p in allv if p in ("torso",) and 1.05 < v.co.z < 1.32]
    zs = np.arange(1.10, 1.30, 0.02)
    for sign in (1, -1):
        half = [(ob, v, p) for ob, v, p in tor if v.co.x * sign > 0]
        def mext(t):
            e = ext(half, 0, 2, t)
            return (0.0, e[1]) if e and sign > 0 else ((e[0], 0.0) if e else None)
        def dext(t):
            e = drawn_torso(t)
            return (0.0, e[1]) if e and sign > 0 else ((e[0], 0.0) if e else None)
        f = affine_fit(zs, mext, dext)
        if f:
            s, a, b = f
            d = []
            for ob, v, p in half:
                w = min(1.0, max(0.0, (v.co.z - 1.05) / 0.05)) * min(1.0, max(0.0, (1.32 - v.co.z) / 0.03))
                nx = np.interp(v.co.z, s, a) + v.co.x * np.interp(v.co.z, s, b)
                nx = v.co.x + (nx - v.co.x) * w
                d.append(abs(nx - v.co.x)); v.co.x = nx
            report["torso_" + ("l" if sign > 0 else "r")] = round(float(np.mean(d)), 4)
    for ob, _ in objs_parts:
        ob.data.update()
    L.log("sheet fit, mean |correction| (m):", report)


# HEAD: cut from HIGH (face, hair, ears, cap crown and hood, collar) and decimated. Rigid on head/neck bones,
# no facial rig (as the shipped SKM_Link); a blob projected on the sculpted hair locks cannot keep them.
HEAD_CUT = 1.49          # collar base on the front sheet (row 1455 -> z 1.49 at the neckline V top)
HEAD_TRIS = 16000


def in_head_box(p):
    x, y, z = p
    if z < HEAD_CUT or abs(x) > 0.24 or y > 0.16:
        return False
    if z < 1.56 and abs(x) > 0.16:                 # shoulders and straps below the ears
        return False
    if x > 0.06 and y > 0.15 and z < 1.66:         # sword hilt behind the left shoulder (HIGH slice x 0.12: hair
                                                   # reaches y 0.12 at z 1.60, the guard starts at 0.19)
        return False
    if abs(x) > 0.07 and y > 0.13 and z < 1.58:    # shield top edge
        return False
    return True


def build_head():
    src = bpy.data.objects["CH_Link_HIGH_AI"].data
    n = len(src.vertices)
    co = np.empty(n * 3); src.vertices.foreach_get("co", co); co = co.reshape(-1, 3)
    keep = np.array([in_head_box(p) for p in co])
    src.calc_loop_triangles()
    tri = np.empty(len(src.loop_triangles) * 3, dtype=np.int64); src.loop_triangles.foreach_get("vertices", tri)
    tri = tri.reshape(-1, 3)
    tri = tri[keep[tri].all(1)]
    used = np.unique(tri)
    remap = -np.ones(n, dtype=np.int64); remap[used] = np.arange(len(used))
    bm = bmesh.new()
    vs = [bm.verts.new(tuple(co[i])) for i in used]
    for t in remap[tri]:
        try:
            bm.faces.new((vs[t[0]], vs[t[1]], vs[t[2]]))
        except ValueError:
            pass
    # largest connected piece only (drops strap and hilt crumbs caught by the box)
    bm.verts.ensure_lookup_table()
    seen, comps = set(), []
    for v in bm.verts:
        if v.index in seen:
            continue
        comp, st = [], [v]; seen.add(v.index)
        while st:
            x = st.pop(); comp.append(x)
            for e in x.link_edges:
                o = e.other_vert(x)
                if o.index not in seen:
                    seen.add(o.index); st.append(o)
        comps.append(comp)
    comps.sort(key=len, reverse=True)
    for comp in comps[1:]:
        bmesh.ops.delete(bm, geom=comp, context="VERTS")
    ob = L.own(L.new_mesh_object("CH_Link_Head_LOD0", bm, "LOW"))
    tris = len(ob.data.polygons)
    L.decimate(ob, min(1.0, HEAD_TRIS / max(1, tris)))
    L.apply_all_modifiers(ob)
    for p in ob.data.polygons:
        p.use_smooth = True
    L.log("head: %d HIGH tris cut, %d pieces dropped, %d tris after decimation" % (tris, len(comps) - 1, len(ob.data.polygons)))
    return ob


def trim_body_under_head(body, head):
    """Body faces above the collar go (the head piece owns them); the neck ring left under the collar is pulled
    4 mm inward so the two surfaces never z-fight."""
    bm = bmesh.new(); bm.from_mesh(body.data)
    parts = list(body["parts"])
    hb = BVHTree.FromObject(head, bpy.context.evaluated_depsgraph_get())
    def covered(v):
        loc, nrm, idx, d = hb.find_nearest(v.co)
        return loc is not None and d < 0.012
    kill = [f for f in bm.faces if all(v.co.z > HEAD_CUT + 0.01 and parts[v.index] in ("head", "neck", "torso") and covered(v)
                                         for v in f.verts)]
    bm.normal_update()
    for v in bm.verts:
        if v.co.z > HEAD_CUT - 0.015 and abs(v.co.x) < 0.17 and parts[v.index] in ("head", "neck", "torso"):
            v.co -= v.normal * 0.004
    bmesh.ops.delete(bm, geom=kill, context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    keep_idx = [v.index for v in bm.verts if v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bm.to_mesh(body.data); bm.free()
    body["parts"] = [parts[i] for i in keep_idx]
    L.log("body trimmed under the head: %d faces removed" % len(kill))


SIDE = None
S_AXIS = 990.2   # ref/matte_side.png column of y = 0 (image right = +Y, back)


def side_matte():
    global SIDE
    if SIDE is None:
        img = bpy.data.images.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../ref/matte_side.png"))
        w, h = img.size
        SIDE = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1, :, 3] > 0.5
        bpy.data.images.remove(img)
    return SIDE


def drawn_depth(z, near):
    """Side panel run at height z that overlaps `near` (model y extents) the most, as world (front y, back y)."""
    row = int(round(M_GROUND - z * M_PPM))
    rs = [((a - S_AXIS) / M_PPM, (b - S_AXIS) / M_PPM) for a, b in runs(side_matte()[row]) if b - a > 15]
    if not rs or near is None:
        return None
    ov = lambda r: min(r[1], near[1]) - max(r[0], near[0])
    best = max(rs, key=ov)
    return best if ov(best) > 0 else None


def fit_depth(objs_parts, rules):
    """rules: list of (label, part set or None, z0, z1, keep_back): Y per z mapped to the side panel. keep_back
    leaves the back edge where the HIGH has it (only the front is fitted) where the panel's back is hidden gear."""
    report = {}
    for label, pset, z0, z1, blend in rules:
        sel = []
        for ob, parts in objs_parts:
            for v in ob.data.vertices:
                p = parts[v.index] if parts else None
                if (pset is None or p in pset) and z0 <= v.co.z <= z1:
                    sel.append(v)
        if not sel:
            continue
        zs = np.arange(z0 + 0.01, z1 - 0.005, 0.02)
        def mext(t):
            vals = [v.co.y for v in sel if abs(v.co.z - t) < 0.012]
            return (min(vals), max(vals)) if len(vals) >= 4 else None
        f = affine_fit(zs, mext, lambda t: drawn_depth(t, mext(t)), sc_range=(0.6, 1.5))
        if not f:
            continue
        s, a, b = f
        d = []
        for v in sel:
            w = min(1.0, (v.co.z - z0) / blend, (z1 - v.co.z) / blend) if blend else 1.0
            ny = np.interp(v.co.z, s, a) + v.co.y * np.interp(v.co.z, s, b)
            ny = v.co.y + (ny - v.co.y) * max(0.0, w)
            d.append(abs(ny - v.co.y)); v.co.y = ny
        report[label] = round(float(np.mean(d)), 4)
    for ob, _ in objs_parts:
        ob.data.update()
    L.log("side panel depth fit, mean |correction| (m):", report)


CUFF_LO, CUFF_HI = 0.325, 0.475   # leg zone hidden by the separate boot cuff (0.345-0.445) plus margins


def bridge_under_cuffs(bm, parts, arm):
    """Leg radius under the cuff = linear blend, per angle around the calf axis, of the projected rings just
    below and just above the cuff (the HIGH cuff flare would otherwise pull spikes out of the shaft)."""
    P = {b.name: arm.matrix_world @ b.head for b in arm.pose.bones}
    for s in ("l", "r"):
        ca, ft = P["calf_" + s], P["foot_" + s]
        def axis(z):
            t = (ca.z - z) / (ca.z - ft.z)
            c = ca + (ft - ca) * t
            return Vector((c.x, c.y, z))
        sign = 1 if s == "l" else -1
        legv = [v for v in bm.verts if parts[v.index] == "leg" and v.co.x * sign > 0]
        def profile(z0, z1):
            bins = [[] for _ in range(24)]
            for v in legv:
                if z0 < v.co.z < z1:
                    d = v.co - axis(v.co.z)
                    a = math.atan2(d.y, d.x)
                    bins[int((a + math.pi) / (2 * math.pi) * 24) % 24].append(math.hypot(d.x, d.y))
            return [float(np.median(b)) if b else None for b in bins]
        lo, hi = profile(CUFF_LO - 0.04, CUFF_LO), profile(CUFF_HI, CUFF_HI + 0.04)
        for v in legv:
            if not CUFF_LO < v.co.z < CUFF_HI:
                continue
            c = axis(v.co.z)
            d = v.co - c
            a = math.atan2(d.y, d.x)
            k = int((a + math.pi) / (2 * math.pi) * 24) % 24
            if lo[k] is None or hi[k] is None:
                continue
            t = (v.co.z - CUFF_LO) / (CUFF_HI - CUFF_LO)
            r = lo[k] + (hi[k] - lo[k]) * t - 0.003
            v.co.x, v.co.y = c.x + math.cos(a) * r, c.y + math.sin(a) * r


def smooth_full(bm, fixed, parts, per_part):
    """Plain Laplacian smoothing (normal direction included) with per-part iterations and strength."""
    it_max = max(i for i, _ in per_part.values())
    for k in range(it_max):
        new = {}
        for v in bm.verts:
            cfg = per_part.get(parts[v.index])
            if v.index in fixed or not cfg or k >= cfg[0] or not v.link_edges or v.co.z < 0.002:
                continue
            c = sum((e.other_vert(v).co for e in v.link_edges), Vector()) / len(v.link_edges)
            new[v] = v.co + (c - v.co) * cfg[1]
        for v, co in new.items():
            v.co = co


def build_body(arm):
    blk = bpy.data.objects["CH_Link_Body_LOD0_BLK"]
    blk.modifiers["Subdivision"].levels = 2       # 8.4k quads: rings every 2-3 cm on the limbs
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(blk.evaluated_get(dg))
    blk.modifiers["Subdivision"].levels = 1
    me.name = "CH_Link_Body_LOD0"
    ob = bpy.data.objects.new("CH_Link_Body_LOD0", me)
    L.col("LOW").objects.link(ob)
    L.own(ob)
    segs = bone_segments(arm)
    bvh = SBVH or smooth_high()
    bm = bmesh.new(); bm.from_mesh(me)
    bm.verts.ensure_lookup_table(); bm.normal_update()
    parts = [part_of(nearest_bone(v.co, segs)) for v in bm.verts]
    densify_core(bm, parts, arm)
    bm.verts.ensure_lookup_table(); bm.normal_update()
    parts = [part_of(nearest_bone(v.co, segs)) for v in bm.verts]
    fixed, moved, missed = set(), 0, 0
    # two passes: project, relax tangentially, project again
    for it in range(2):
        bm.normal_update()
        for v in bm.verts:
            part = parts[v.index]
            z = v.co.z
            if part == "finger":
                fixed.add(v.index); continue
            if (part in ("torso", "thigh") and HEM - 0.02 < z < SKIRT_TOP + 0.01):
                # under the tunic skirt: keep inside it (inferred trousers/hips), no HIGH surface to follow
                fixed.add(v.index); continue
            band = BANDS.get(part, BAND)
            if part == "leg" and CUFF_LO < z < CUFF_HI:
                continue              # bridged below from the rings above and under the cuff
            hit = project_point(bvh, v.co.copy(), v.normal.copy(), band)
            if hit:
                v.co = hit[0]
                moved += (it == 0)
            else:
                missed += (it == 0)
        if it == 0:
            relax(bm, fixed, iters=2)
    bridge_under_cuffs(bm, parts, arm)
    # low-frequency surface: wrinkles and seams of the AI sculpt go to the normal map, not the LOW
    smooth_full(bm, fixed, parts, {"foot": (4, 0.4), "leg": (2, 0.35), "torso": (2, 0.3), "arm": (2, 0.3),
                                   "hand": (2, 0.3), "thigh": (1, 0.3), "neck": (1, 0.3), "head": (1, 0.3)})
    bm.to_mesh(me); bm.free()
    for p in me.polygons:
        p.use_smooth = True
    ob["parts"] = parts
    L.log("body projected: %d moved, %d without a HIGH hit, %d kept (fingers, under skirt)" % (moved, missed, len(fixed)))
    counts = {}
    for p in parts:
        counts[p] = counts.get(p, 0) + 1
    L.log("parts", counts)
    return ob


# SKIRT: robust low-order Fourier fit of each HIGH section, props rejected as positive outliers

def orient_outward(ob):
    """Open lofted rings (belt, skirt, cuffs): face normals away from the ring's vertical axis. recalc_face_normals
    cannot decide inside from outside on an open band, and baking needs the cage on the outside."""
    me = ob.data
    c = sum((v.co for v in me.vertices), Vector()) / len(me.vertices)
    bm = bmesh.new(); bm.from_mesh(me)
    flip = [f for f in bm.faces if Vector((f.calc_center_median().x - c.x, f.calc_center_median().y - c.y, 0)).dot(f.normal) < 0]
    bmesh.ops.reverse_faces(bm, faces=flip)
    bm.to_mesh(me); bm.free()
    return ob


def smooth_ring(r, harmonics=5, iters=4, tol=0.012):
    n = len(r)
    t = np.linspace(0, 2 * np.pi, n, endpoint=False)
    A = np.column_stack([np.ones(n)] + [f(k * t) for k in range(1, harmonics + 1) for f in (np.cos, np.sin)])
    ok = ~np.isnan(r)
    for _ in range(iters):
        coef, *_ = np.linalg.lstsq(A[ok], r[ok], rcond=None)
        fit = A @ coef
        ok = ~np.isnan(r) & (r < fit + tol)
    return fit


SKIRT_Z = [SKIRT_TOP - i * 0.025 for i in range(12)] + [HEM + 0.012, HEM]


def build_skirt():
    secs = []
    for z in SKIRT_Z:
        c = Vector((0.0, 0.0, z))
        r, u, w = radial(c, (0, 0, 1), n=48, reach=0.6, inward=True)
        r = smooth_ring(r) + 0.003
        secs.append([tuple(c + (u * math.cos(2 * math.pi * i / 48) + w * math.sin(2 * math.pi * i / 48)) * float(r[i]))
                     for i in range(48)])
    ob = orient_outward(L.own(L.loft("CH_Link_TunicSkirt_LOD0", secs, cap_ends=False)))
    L.solidify(ob, thickness=0.006, offset=-1.0)
    return ob


def build_belt():
    """Belt ring over the tunic: front sheet rows 620-670 (z 1.084-1.020), 5 mm proud of the HIGH tunic."""
    secs = []
    for z in (1.020, 1.030, 1.074, 1.084):
        c = Vector((0.0, 0.0, z))
        r, u, w = radial(c, (0, 0, 1), n=40, reach=0.6, inward=True)
        r = smooth_ring(r, harmonics=4) + (0.004 if 1.025 < z < 1.08 else 0.0)
        secs.append([tuple(c + (u * math.cos(2 * math.pi * i / 40) + w * math.sin(2 * math.pi * i / 40)) * float(r[i]))
                     for i in range(40)])
    ob = orient_outward(L.own(L.loft("CH_Link_Belt_LOD0", secs, cap_ends=False)))
    # buckle: front sheet cols 997-1062, rows 622-668 (0.083 x 0.059 m), on the front centre
    x0, z0 = sheet_front(997, 668); x1, z1 = sheet_front(1062, 622)
    ymin = min(v.co.y for v in ob.data.vertices if abs(v.co.x) < 0.03)
    bk = L.own(L.box("CH_Link_Buckle_LOD0", (x1 - x0, 0.008, z1 - z0), location=((x0 + x1) / 2, ymin - 0.004, (z0 + z1) / 2)))
    L.bevel(bk, width=0.002, segments=2)
    return [ob, bk]


CUFF_Z = (0.345, 0.36, 0.40, 0.43, 0.445)


def build_cuffs(arm, body):
    """Folded boot cuffs wrapped around the finished leg: per angle around the calf axis, the leg radius at that
    height + 7 mm (4 mm more at the bottom edge, front sheet: the cuff flares), 5 mm thick."""
    P = {b.name: arm.matrix_world @ b.head for b in arm.pose.bones}
    parts = list(body["parts"])
    obs = []
    n = 36
    for s in ("l", "r"):
        sign = 1 if s == "l" else -1
        ca, ft = P["calf_" + s], P["foot_" + s]
        legv = [v.co.copy() for v in body.data.vertices if parts[v.index] == "leg" and v.co.x * sign > 0]
        secs = []
        for z in CUFF_Z:
            t = (ca.z - z) / (ca.z - ft.z)
            c = ca + (ft - ca) * t
            c = Vector((c.x, c.y, z))
            bins = [0.0] * n
            for p in legv:
                if abs(p.z - z) < 0.02:
                    d = p - c
                    k = int((math.atan2(d.y, d.x) + math.pi) / (2 * math.pi) * n) % n
                    bins[k] = max(bins[k], math.hypot(d.x, d.y))
            r = np.array(bins); good = r > 0
            idx = np.arange(n)
            r = np.interp(idx, idx[good], r[good], period=n)
            r = np.maximum(r, np.maximum(np.roll(r, 1), np.roll(r, -1)))      # no leg vertex between two bins pokes out
            off = 0.004 + (0.003 if z < CUFF_Z[1] + 0.001 else 0.0)
            secs.append([tuple(c + Vector((math.cos(-math.pi + (k + 0.5) * 2 * math.pi / n),
                                           math.sin(-math.pi + (k + 0.5) * 2 * math.pi / n), 0)) * (r[k] + off))
                         for k in range(n)])
        ob = orient_outward(L.own(L.loft("CH_Link_BootCuff_%s_LOD0" % s.upper(), secs, cap_ends=False)))
        L.solidify(ob, thickness=0.004, offset=-1.0)
        obs.append(ob)
        # the boot shaft under the cuff steps 6 mm in, toward the calf axis, so the cuff never z-fights it
        for v in body.data.vertices:
            if parts[v.index] == "leg" and v.co.x * sign > 0 and CUFF_Z[0] - 0.004 < v.co.z < CUFF_Z[-1] + 0.002:
                t = (ca.z - v.co.z) / (ca.z - ft.z)
                c = ca + (ft - ca) * t
                d = Vector((v.co.x - c.x, v.co.y - c.y, 0))
                if d.length > 0.01:
                    v.co -= d.normalized() * 0.006
    body.data.update()
    return obs


# CAP TAIL: centreline from the side panel, width from the back sheet (0.13 m at the crown row 150,
# 0.09 at row 260, 0.05 at row 330, point at row ~395 -> z 1.37 on the T-pose back sheet)
CAP_TAIL = [((0.0, 0.045, 1.765), 0.070, 0.050), ((0.0, 0.09, 1.765), 0.068, 0.050), ((0.0, 0.13, 1.735), 0.062, 0.040),
            ((0.0, 0.165, 1.68), 0.052, 0.032), ((0.0, 0.185, 1.62), 0.040, 0.026), ((0.0, 0.198, 1.56), 0.028, 0.018),
            ((0.0, 0.205, 1.50), 0.016, 0.011), ((0.0, 0.207, 1.45), 0.006, 0.005), ((0.0, 0.207, 1.42), 0.001, 0.001)]


def build_cap():
    pts = [Vector(p) for p, _, _ in CAP_TAIL]
    secs = []
    n = 16
    for i, (p, rx, ry) in enumerate(CAP_TAIL):
        d = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        side = Vector((1, 0, 0))
        up = d.cross(side).normalized()
        secs.append([tuple(Vector(p) + side * math.cos(2 * math.pi * k / n) * rx + up * math.sin(2 * math.pi * k / n) * ry * 0.7)
                     for k in range(n)])
    ob = L.own(L.loft("CH_Link_CapTail_LOD0", secs))
    return ob


# SHIELD: domed plate inside the back-sheet outline + rim
SHIELD_BACK = BLK["SHIELD_BACK"]
SHIELD_FACE_Y = 0.136   # back sheet: the sword passes UNDER the shield (hilt over the shoulder, sheath out of its
                        # lower right edge), so the shield stands 1.8 cm further off than the side panel's 0.118
SHIELD_DOME = 0.035     # side panel: outer face reaches y 0.170 at the centre (z 1.30-1.40)
RIM_W, RIM_T = 0.022, 0.014   # back sheet: silver rim 28 px wide; thickness inferred


def densify(poly, step):
    out = []
    for a, b in zip(poly, poly[1:] + poly[:1]):
        a, b = Vector(a), Vector(b)
        k = max(1, int((b - a).length / step))
        out += [tuple(a + (b - a) * (i / k)) for i in range(k)]
    return out


def build_shield():
    outline = densify([sheet_back(c, r) for c, r in SHIELD_BACK], 0.02)
    xs = [p[0] for p in outline]; zs = [p[1] for p in outline]
    cx, cz = (min(xs) + max(xs)) / 2, (min(zs) + max(zs)) / 2
    hx, hz = (max(xs) - min(xs)) / 2, (max(zs) - min(zs)) / 2
    bm = bmesh.new()
    vs = [bm.verts.new((x, 0, z)) for x, z in outline]
    f = bm.faces.new(vs)
    # fill with rings shrinking toward the centre, then dome them
    rings = [vs]
    for k in range(1, 7):
        s = 1 - k / 7
        rings.append([bm.verts.new((cx + (v.co.x - cx) * s, 0, cz + (v.co.z - cz) * s)) for v in vs])
    bm.faces.remove(f)
    for ra, rb in zip(rings, rings[1:]):
        for i in range(len(ra)):
            bm.faces.new((ra[i], ra[(i + 1) % len(ra)], rb[(i + 1) % len(rb)], rb[i]))
    centre = bm.verts.new((cx, 0, cz))
    last = rings[-1]
    for i in range(len(last)):
        bm.faces.new((last[i], last[(i + 1) % len(last)], centre))
    for v in bm.verts:
        u = ((v.co.x - cx) / hx) ** 2 + ((v.co.z - cz) / hz) ** 2
        v.co.y = SHIELD_FACE_Y + RIM_T + SHIELD_DOME * max(0.0, 1 - u) ** 0.8
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    plate = L.own(L.new_mesh_object("CH_Link_Shield_LOD0", bm, "LOW"))
    for p in plate.data.polygons:
        if p.normal.y < 0:
            break
    L.solidify(plate, thickness=0.006, offset=-1.0)
    L.shade_smooth(plate, math.radians(40))
    # rim: square section swept along the outline
    secs = []
    n_out = len(outline)
    rim_pts = []
    for i in range(n_out):
        a = Vector((outline[i - 1][0], 0, outline[i - 1][1])); b = Vector((outline[(i + 1) % n_out][0], 0, outline[(i + 1) % n_out][1]))
        p = Vector((outline[i][0], 0, outline[i][1]))
        t = (b - a).normalized()
        inward = Vector((-t.z, 0, t.x))
        if inward.dot(Vector((cx, 0, cz)) - p) < 0:
            inward = -inward
        rim_pts.append((p, inward))
    bm = bmesh.new()
    loops = []
    for (p, inn) in rim_pts:
        y0, y1 = SHIELD_FACE_Y, SHIELD_FACE_Y + RIM_T + 0.006
        loops.append([bm.verts.new(p + Vector((0, y0, 0))), bm.verts.new(p + Vector((0, y1, 0))),
                      bm.verts.new(p + inn * RIM_W + Vector((0, y1, 0))), bm.verts.new(p + inn * RIM_W + Vector((0, y0, 0)))])
    for i in range(n_out):
        a, b = loops[i], loops[(i + 1) % n_out]
        for k in range(4):
            bm.faces.new((a[k], a[(k + 1) % 4], b[(k + 1) % 4], b[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    rim = L.own(L.new_mesh_object("CH_Link_ShieldRim_LOD0", bm, "LOW"))
    L.shade_smooth(rim, math.radians(40))
    return [plate, rim]


# SWORD: straight axis pommel -> tip (back sheet; the drawings bend it, see review/02_blockout/notes.md)
POMMEL = Vector((0.331, 0.110, 1.746))   # behind the left shoulder, over the back
TIP = Vector((-0.390, 0.135, 0.664))      # sheath between the back (y ~0.10) and the shield (0.136); depth inferred
GUARD_T = 0.20          # guard at 20 % of the way from pommel to tip (back sheet: guard row ~215 -> z 1.60)
SHEATH_W, SHEATH_D = 0.036, 0.016   # back sheet: sheath 0.07 m wide at z 0.93; depth inferred


def sword_frame():
    d = (TIP - POMMEL).normalized()
    side = d.cross(Vector((0, 1, 0))).normalized()     # in the back plane, across the blade
    depth = side.cross(d).normalized()
    return d, side, depth


def section(c, side, depth, w, t, n=12):
    return [tuple(c + side * math.cos(2 * math.pi * k / n) * w + depth * math.sin(2 * math.pi * k / n) * t) for k in range(n)]


def build_sword():
    d, side, depth = sword_frame()
    L_all = (TIP - POMMEL).length
    guard = POMMEL + d * (L_all * GUARD_T)
    # sheath from just below the guard to the tip, tapering, pointed
    secs = []
    for f, w in ((0.0, 1.0), (0.1, 1.0), (0.5, 0.95), (0.85, 0.88), (0.95, 0.6), (1.0, 0.08)):
        c = guard + d * 0.015 + (TIP - guard - d * 0.015) * f
        secs.append(section(c, side, depth, SHEATH_W * w, SHEATH_D * max(w, 0.3)))
    sheath = L.own(L.loft("CH_Link_Sheath_LOD0", secs))
    # hilt: pommel, grip, guard wings (purple, back sheet: guard 0.17 m across)
    secs = []
    grip = [(0.0, 0.022, 0.022), (0.02, 0.026, 0.026), (0.04, 0.016, 0.016), (0.05, 0.015, 0.015),
            (0.95, 0.017, 0.017), (1.0, 0.02, 0.02)]
    for f, w, t in grip:
        c = POMMEL + (guard - POMMEL) * f
        secs.append(section(c, side, depth, w, t, n=10))
    hilt = L.own(L.loft("CH_Link_Hilt_LOD0", secs))
    wings = [(-0.085, 0.012), (-0.03, 0.03), (0.0, 0.035), (0.03, 0.03), (0.085, 0.012)]
    bm = bmesh.new()
    prof = [guard + side * s + d * (-h * 0.3) for s, h in wings] + [guard + side * s + d * (h) for s, h in reversed(wings)]
    vs = [bm.verts.new(p - depth * 0.012) for p in prof]
    f = bm.faces.new(vs)
    res = bmesh.ops.extrude_face_region(bm, geom=[f])
    top = [g for g in res["geom"] if isinstance(g, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, vec=depth * 0.024, verts=top)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    g_ob = L.own(L.new_mesh_object("CH_Link_Guard_LOD0", bm, "LOW"))
    L.bevel(g_ob, width=0.003, segments=2)
    return [sheath, hilt, g_ob]


# EARS: front sheet EAR_ROIS_FRONT (x 0.079-0.166, z 1.611-1.731); leaf profile, 1.4 cm thick (inferred)
EAR = [(0.0, 0.0), (0.03, 0.022), (0.065, 0.045), (0.095, 0.075), (0.105, 0.098), (0.085, 0.07), (0.05, 0.025), (0.02, -0.02), (0.0, -0.028)]


def build_ears():
    obs = []
    for s, sg in (("L", 1), ("R", -1)):
        root = Vector((sg * 0.074, 0.015, 1.625))
        out = Vector((sg * 0.72, 0.18, 0.67)).normalized()    # ear axis: out, up and slightly back (front + side sheets)
        up = Vector((0, 0, 1)); up = (up - out * up.dot(out)).normalized()
        nrm = out.cross(up).normalized()
        bm = bmesh.new()
        vs = [bm.verts.new(root + out * a + up * b - nrm * 0.007) for a, b in EAR]
        f = bm.faces.new(vs)
        res = bmesh.ops.extrude_face_region(bm, geom=[f])
        top = [g for g in res["geom"] if isinstance(g, bmesh.types.BMVert)]
        bmesh.ops.translate(bm, vec=nrm * 0.014, verts=top)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        ob = L.own(L.new_mesh_object("CH_Link_Ear_%s_LOD0" % s, bm, "LOW"))
        L.subsurf(ob, levels=1)
        obs.append(ob)
    return obs


GEAR_DX = -0.0096   # front sheet draws the back gear 15 px (0.019 m) further -X than the back sheet
                    # (make_character.FRONT_GEAR_DX); the gear is built from the back sheet and shifted halfway


def shift_gear(obs):
    for o in obs:
        o.location.x += GEAR_DX


def build_props():
    obs = []
    # pouch on the right hip, hanging from the belt: front sheet cols 835-892 (x), rows 600-700 (z); side panel shows
    # it at the hip side behind the hip line (y ~0.02); pushed against the belt/skirt surface
    x0, z0 = sheet_front(835, 700); x1, z1 = sheet_front(892, 600)
    p = L.own(L.box("CH_Link_Pouch_LOD0", (0.05, 0.075, z1 - z0), location=(x0 + 0.03, 0.02, (z0 + z1) / 2)))
    L.bevel(p, width=0.008, segments=2)
    obs.append(p)
    # ocarina: ellipsoid 0.13 x 0.05 x 0.07 (front sheet cols 1055-1180, rows 660-745), tilted 20 deg
    x0, z0 = sheet_front(1055, 745); x1, z1 = sheet_front(1180, 660)
    oc = L.own(L.sphere("CH_Link_Ocarina_LOD0", 1.0, location=((x0 + x1) / 2, -0.20, (z0 + z1) / 2), u=20, v=10,
                        scale=((x1 - x0) / 2, 0.022, (z1 - z0) / 2)))
    oc.rotation_euler = (0, math.radians(-20), 0)
    obs.append(oc)
    # mask: seen in profile on the front sheet, so it faces +X (cols 1165-1235, rows 640-800): thin along X,
    # its face width (0.10 m, inferred from the sheet's accessory callout proportions) along Y
    x0, z0 = sheet_front(1165, 800); x1, z1 = sheet_front(1235, 640)
    mk = L.own(L.sphere("CH_Link_Mask_LOD0", 1.0, location=((x0 + x1) / 2, -0.15, (z0 + z1) / 2), u=20, v=12,
                        scale=((x1 - x0) / 2, 0.05, (z1 - z0) / 2)))
    obs.append(mk)
    return obs


def main():
    a = L.argv_after_dashes()
    master = a[a.index("--master") + 1]
    L.open_master(master)
    L.clear_owned()
    L.clear_owned("phase:04_topology")   # downstream delivery copies are stale once the forms are rebuilt
    restore_blockout()
    for o in bpy.data.objects:
        if o.get("owner") == "phase:02_blockout" and not o.name.endswith("_BLK"):
            o.name = o.name + "_BLK"
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    bpy.context.view_layer.update()
    body = build_body(arm)
    build_skirt()
    build_belt()
    fit_to_sheet([(body, list(body["parts"]))], arm)
    parts = list(body["parts"])
    fit_depth([(body, parts)], [("legs", {"leg", "thigh", "foot"}, 0.0, HEM - 0.01, 0.0)])
    fit_depth([(body, parts)], [("torso", {"torso"}, SKIRT_TOP + 0.03, 1.45, 0.04)])
    fit_depth([(bpy.data.objects["CH_Link_TunicSkirt_LOD0"], None), (bpy.data.objects["CH_Link_Belt_LOD0"], None)],
              [("skirt", None, HEM - 0.005, SKIRT_TOP + 0.07, 0.0)])
    belt, buckle = bpy.data.objects["CH_Link_Belt_LOD0"], bpy.data.objects["CH_Link_Buckle_LOD0"]
    buckle.location.y = min(v.co.y for v in belt.data.vertices if abs(v.co.x) < 0.03) - 0.004   # re-seat after the fit
    for v in body.data.vertices:            # soles flat on the ground plane, nothing below z = 0
        if v.co.z < 0.008:
            v.co.z = 0.0
    body.data.update()
    build_cuffs(arm, body)
    head = build_head()
    trim_body_under_head(body, head)
    build_cap()
    shift_gear(build_shield() + build_sword())
    build_props()
    retire_blockout()
    L.save_master(master)

main()
