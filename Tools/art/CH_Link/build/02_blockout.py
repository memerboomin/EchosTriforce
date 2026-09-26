"""Phase 2: blockout. Primary masses only, joints at the mannequin skeleton's build pose (02a_skeleton.py).
Body = Skin-modifier chain over the pose bones, radii measured on HIGH by rays perpendicular to each segment.
Gear = separate rough objects placed from the sheets (front/back T-pose, side panel) — see asset-brief.md.
  blender -b --python build/02_blockout.py -- --master CH_Link_master.blend
"""
import bpy, bmesh, os, sys, math
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _lib as L
L.PHASE = "02_blockout"; L.OWNER_TAG = "phase:02_blockout"

PPM, AXIS_COL, GROUND_ROW = 785.0, 1010.0, 1471.0   # original T-pose sheets (tpose_front.jpg / tpose_back.jpg)


def sheet_front(col, row):
    """Front T-pose sheet pixel -> world (x, z)."""
    return (col - AXIS_COL) / PPM, (GROUND_ROW - row) / PPM


def sheet_back(col, row):
    """Back T-pose sheet pixel -> world (x, z); image right is world -X."""
    return -(col - AXIS_COL) / PPM, (GROUND_ROW - row) / PPM


# HIGH MEASUREMENT

BVH = None


def high_bvh():
    global BVH
    if BVH is None:
        o = bpy.data.objects["CH_Link_HIGH_AI"]
        n = len(o.data.vertices)
        co = np.empty(n * 3, dtype=np.float64); o.data.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3)
        o.data.calc_loop_triangles()
        tri = np.empty(len(o.data.loop_triangles) * 3, dtype=np.int64)
        o.data.loop_triangles.foreach_get("vertices", tri)
        BVH = BVHTree.FromPolygons([tuple(v) for v in co], [tuple(t) for t in tri.reshape(-1, 3)], all_triangles=True)
    return BVH


def perp_frame(d):
    d = Vector(d).normalized()
    a = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))
    u = d.cross(a).normalized()
    return u, d.cross(u).normalized()


def radial(p, d, n=24, reach=0.5, inward=True):
    """Distances from p to the HIGH surface along n rays perpendicular to d. inward=True casts from
    `reach` outside toward p and keeps the first (outermost) hit; inward=False casts outward from p."""
    bvh = high_bvh()
    u, w = perp_frame(d)
    out = []
    for i in range(n):
        t = 2 * math.pi * i / n
        r = u * math.cos(t) + w * math.sin(t)
        if inward:
            hit = bvh.ray_cast(Vector(p) + r * reach, -r, reach)
            out.append(reach - hit[3] if hit[0] is not None else np.nan)
        else:
            hit = bvh.ray_cast(Vector(p), r, reach)
            out.append(hit[3] if hit[0] is not None else np.nan)
    return np.array(out), u, w


def despike(r, tol=0.03):
    """Replace ray distances that jump out of their angular neighbourhood (props, straps crossing a ray)."""
    r = r.copy()
    n = len(r)
    med = np.array([np.nanmedian([r[(i + k) % n] for k in range(-3, 4)]) for i in range(n)])
    bad = np.isnan(r) | (np.abs(r - med) > tol)
    r[bad] = med[bad]
    return r


# BODY

def pose_points(arm):
    mw = arm.matrix_world
    P = {b.name: mw @ b.head for b in arm.pose.bones}
    T = {b.name: mw @ b.tail for b in arm.pose.bones}
    return P, T


def lerp(a, b, t):
    return a + (b - a) * t


FINGER_EXTRA = 0.015   # front sheet span 1.894 m vs mannequin fingertips 1.855 m: glove-less fingertips 1.5 cm longer
TOE_Y, HEEL_Y = -0.174, 0.126   # side panel boot at z 0.03 (matte_side run)
BOOT_R = {"ball": 0.052, "toe": 0.042, "heel": 0.048}   # front sheet boot foot 0.11-0.12 m wide; side panel toe cap


def body_chains(arm):
    P, T = pose_points(arm)
    ch = {}
    spine = [P["pelvis"], P["spine_01"], P["spine_02"], P["spine_03"], P["spine_04"], P["spine_05"],
             P["neck_01"], P["neck_02"], P["head"], P["head"] + Vector((0, 0.01, 0.075))]
    ch["spine"] = spine
    for s in ("l", "r"):
        ua, la, h = P["upperarm_" + s], P["lowerarm_" + s], P["hand_" + s]
        ch["arm_" + s] = [P["spine_05"], P["clavicle_" + s].lerp(ua, 0.55), ua, lerp(ua, la, 0.5), la,
                          lerp(la, h, 0.5), h]
        for f in ("index", "middle", "ring", "pinky"):
            tip = T[f + "_03_" + s] + (T[f + "_03_" + s] - P[f + "_03_" + s]).normalized() * FINGER_EXTRA
            ch[f + "_" + s] = [h, P[f + "_01_" + s], P[f + "_02_" + s], P[f + "_03_" + s], tip]
        ch["thumb_" + s] = [h, P["thumb_01_" + s], P["thumb_02_" + s], P["thumb_03_" + s], T["thumb_03_" + s]]
        th, ca, ft, ba = P["thigh_" + s], P["calf_" + s], P["foot_" + s], P["ball_" + s]
        ball = Vector((ba.x, ba.y + 0.01, BOOT_R["ball"]))                 # node kept above the ground
        toe = Vector((ba.x, TOE_Y + BOOT_R["toe"], BOOT_R["toe"]))
        ch["leg_" + s] = [P["pelvis"], th, lerp(th, ca, 0.5), ca, lerp(ca, ft, 0.5), ft, ball, toe]
        ch["heel_" + s] = [ft, Vector((ft.x, HEEL_Y - BOOT_R["heel"], BOOT_R["heel"]))]
    return ch


# radii that the HIGH cannot give (hidden under the skirt, fingers drawn closed) — all inferred or sheet-measured
FIXED = {
    "finger": 0.0115,     # front sheet: flat hand 0.042 m thick at the fingers (matte_front x 0.88); fingers 23 mm
    "thumb": 0.012,       # inferred
    "thigh_top": 0.085,   # front sheet: each leg 0.167 m wide just below the hem (row 925)
    "pelvis": 0.14,       # inferred: hidden under the tunic skirt
}


def measure_xy(p):
    """Spine sections: half width along X and half depth along Y (4 ray pairs each), for elliptical skin nodes."""
    r, u, w = radial(p, (0, 0, 1), n=16, reach=0.35, inward=False)
    ry = np.nanmedian([r[15], r[0], r[1], r[7], r[8], r[9]])      # rays near +-Y
    rx = np.nanmedian([r[3], r[4], r[5], r[11], r[12], r[13]])     # rays near +-X
    return float(rx), float(ry)


def measure_radius(p, d):
    r, _, _ = radial(p, d, n=16, reach=0.35, inward=False)
    r = r[~np.isnan(r)]
    return float(np.median(r)) if len(r) >= 6 else None


def build_body(arm):
    ch = body_chains(arm)
    joints = {}
    for name, pts in ch.items():
        seq = []
        for i, p in enumerate(pts):
            d = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)])
            if name.startswith(("index", "middle", "ring", "pinky")):
                r = FIXED["finger"] * (0.8 if i == len(pts) - 1 else 1.0)
                if i == 0:
                    r = 0.035
            elif name.startswith("thumb"):
                r = FIXED["thumb"] * (0.8 if i == len(pts) - 1 else 1.0)
                if i == 0:
                    r = 0.035
            elif name.startswith("leg") and i == 0:
                r = FIXED["pelvis"]
            elif name.startswith("leg") and p.z > 0.70:
                r = FIXED["thigh_top"]
            elif name == "spine" and i == 0:
                r = FIXED["pelvis"]
            else:
                r = measure_radius(p, d) or 0.05
                # head: inward rays from the skull centre also hit the cap, ears and fringe; keep the skull
                if name == "spine" and i >= 8:
                    r = min(r, 0.115)
                if name.startswith("leg") and i == 6:
                    r = BOOT_R["ball"]
                if name.startswith("leg") and i == 7:
                    r = BOOT_R["toe"]
                if name.startswith("heel"):
                    r = BOOT_R["heel"] if i == 1 else 0.06
                if name.startswith("arm") and i == 0:
                    r = min(r, 0.12)
            if name == "spine" and 1 <= i <= 5:
                r = measure_xy(p)
            if name == "spine" and i >= 8:
                r = (0.125, 0.13)   # head + hair volume: front sheet hair 0.25 m wide at the ears (row 150)
            seq.append((p.x, p.y, p.z, r))
        joints[name] = seq
    flat = {k: [(x, y, z, (r if isinstance(r, float) else max(r))) for x, y, z, r in v] for k, v in joints.items()}
    ob = L.skin_chain("CH_Link_Body_LOD0", flat, collection="LOW", subdiv=1, root_index=0)
    # elliptical spine/head nodes: skin radius is (x, y) in the node frame, which for the vertical spine is world X/Y
    layer = ob.data.skin_vertices[0].data
    for v in ob.data.vertices:
        for x, y, z, r in joints["spine"]:
            if not isinstance(r, float) and (Vector((x, y, z)) - v.co).length < 1e-4:
                layer[v.index].radius = r
    L.log("body chain", sum(len(v) for v in joints.values()), "points")
    for k in ("spine", "arm_l", "leg_l"):
        L.log(k, [j[3] if isinstance(j[3], float) else tuple(round(t, 3) for t in j[3]) for j in joints[k]])
    return ob


# GEAR

def ring_section(center, n, z, reach=0.6, tol=0.03, extra=0.0):
    """Horizontal cross-section of the outermost HIGH surface around `center` at height z."""
    c = Vector((center[0], center[1], z))
    r, u, w = radial(c, (0, 0, 1), n=n, reach=reach, inward=True)
    r = despike(r, tol) + extra
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        v = c + (u * math.cos(t) + w * math.sin(t)) * float(r[i])
        pts.append(tuple(v))
    return pts


SKIRT_Z = (1.02, 0.96, 0.90, 0.84, 0.78, 0.74, 0.715)   # belt bottom (front sheet row 670) to hem (row ~910)


def build_skirt():
    secs = [ring_section((0.0, 0.0), 32, z, extra=0.004) for z in SKIRT_Z]
    return L.own(L.loft("CH_Link_TunicSkirt_LOD0", secs, cap_ends=False))


CUFF_Z = (0.35, 0.39, 0.44)   # front sheet: boot cuff bottom row ~1195, top row ~1125


def build_cuffs(arm):
    P, _ = pose_points(arm)
    obs = []
    for s in ("l", "r"):
        ca, ft = P["calf_" + s], P["foot_" + s]
        secs = []
        for z in CUFF_Z:
            t = (ca.z - z) / (ca.z - ft.z)
            c = lerp(ca, ft, t)
            secs.append(ring_section((c.x, c.y), 16, z, reach=0.16, extra=0.003))
        obs.append(L.own(L.loft("CH_Link_BootCuff_%s_LOD0" % s.upper(), secs, cap_ends=False)))
    return obs


# cap tail centreline from the side panel (matte_side runs: crown back y 0.10 at z 1.80, 0.17 at 1.70,
# separate tip run y 0.184-0.212 at z 1.50); width from the back sheet (tail 0.13 m wide at the crown, row 150)
CAP_TAIL = [((0.0, 0.03, 1.80), 0.085), ((0.0, 0.12, 1.74), 0.075), ((0.0, 0.17, 1.66), 0.055),
            ((0.0, 0.195, 1.57), 0.035), ((0.0, 0.205, 1.50), 0.008)]


def build_cap():
    pts = [Vector(p) for p, _ in CAP_TAIL]
    secs = []
    for i, (p, r) in enumerate(CAP_TAIL):
        d = pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]
        u, w = perp_frame(d)
        secs.append([tuple(Vector(p) + (u * math.cos(2 * math.pi * k / 12) * 1.25 + w * math.sin(2 * math.pi * k / 12) * 0.6) * r)
                     for k in range(12)])
    return L.own(L.loft("CH_Link_Cap_LOD0", secs))


# shield outline on the back sheet (make_character.SHIELD_BACK, px); depth from the side panel: front face
# y 0.115, outer face y 0.173 between z 1.30 and 1.40
SHIELD_BACK = [(882, 294), (1101, 275), (1135, 315), (1165, 380), (1182, 440), (1187, 505), (1185, 560), (1175, 620),
               (1150, 690), (1105, 760), (1040, 820), (985, 850), (930, 820), (870, 760), (830, 690), (805, 620),
               (795, 560), (793, 505), (800, 440), (815, 380), (845, 315)]
SHIELD_Y = (0.118, 0.170)


def build_shield():
    pts = [sheet_back(c, r) for c, r in SHIELD_BACK]
    bm = bmesh.new()
    vs = [bm.verts.new((x, SHIELD_Y[0], z)) for x, z in pts]
    f = bm.faces.new(vs)
    res = bmesh.ops.extrude_face_region(bm, geom=[f])
    top = [g for g in res["geom"] if isinstance(g, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, vec=(0, SHIELD_Y[1] - SHIELD_Y[0], 0), verts=top)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return L.own(L.new_mesh_object("CH_Link_Shield_LOD0", bm, "LOW"))


# sword axis on the front sheet: pommel top (1290, 85) px, sheath tip (700, 962) px; guard at the shoulder
# (1175, 300) px (HILT_ROI_FRONT / SHEATH_ROI_FRONT of make_character). Depth inferred: behind the shield.
POMMEL = sheet_front(1290, 85)
GUARD = sheet_front(1175, 300)
TIP = sheet_front(700, 962)
SWORD_Y = 0.19   # inferred: sheath rides 2 cm behind the shield's outer face


def build_sword():
    g, t, p = Vector((GUARD[0], SWORD_Y, GUARD[1])), Vector((TIP[0], SWORD_Y + 0.02, TIP[1])), Vector((POMMEL[0], SWORD_Y - 0.01, POMMEL[1]))
    sheath = L.curve_tube("CH_Link_Sheath_LOD0", [tuple(g), tuple(t)], 0.035, collection="LOW", resolution=2, bevel_res=2)
    hilt = L.curve_tube("CH_Link_Hilt_LOD0", [tuple(g), tuple(p)], 0.02, collection="LOW", resolution=2, bevel_res=2)
    return [L.own(sheath), L.own(hilt)]


def build_props():
    obs = []
    # pouch on the right hip: front sheet x 835-892 px, rows 600-700 (PROPS / SHEATH ROIs)
    x0, z0 = sheet_front(835, 700); x1, z1 = sheet_front(892, 600)
    obs.append(L.own(L.box("CH_Link_Pouch_LOD0", (x1 - x0, 0.06, z1 - z0), location=((x0 + x1) / 2, -0.10, (z0 + z1) / 2))))
    # ocarina + mask on the left front hip: PROPS_ROI_FRONT x 1050-1238, rows 634-808
    x0, z0 = sheet_front(1055, 745); x1, z1 = sheet_front(1180, 660)      # ocarina body
    obs.append(L.own(L.box("CH_Link_Ocarina_LOD0", (x1 - x0, 0.045, z1 - z0), location=((x0 + x1) / 2, -0.215, (z0 + z1) / 2))))
    x0, z0 = sheet_front(1165, 800); x1, z1 = sheet_front(1235, 640)      # mask
    obs.append(L.own(L.box("CH_Link_Mask_LOD0", (x1 - x0, 0.05, z1 - z0), location=((x0 + x1) / 2, -0.20, (z0 + z1) / 2))))
    # ears: EAR_ROIS_FRONT, tips at x -+0.165, z 1.72 (rows 112-206)
    for s, sg in (("L", 1), ("R", -1)):
        e = L.curve_tube("CH_Link_Ear_%s_LOD0" % s, [(sg * 0.085, 0.01, 1.635), (sg * 0.165, 0.035, 1.715)], 0.018,
                         collection="LOW", resolution=2, bevel_res=2, taper_to=0.1)
        obs.append(L.own(e))
    return obs


def main():
    a = L.argv_after_dashes()
    master = a[a.index("--master") + 1]
    L.open_master(master)
    L.clear_owned()
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    bpy.context.view_layer.update()
    build_body(arm)
    build_skirt()
    build_cuffs(arm)
    build_cap()
    build_shield()
    build_sword()
    build_props()
    # blockout objects carry a _BLK suffix so Phase 3 can build the final names next to them
    for o in bpy.data.objects:
        if o.get("owner") == L.OWNER_TAG and not o.name.endswith("_BLK"):
            import re
            o.name = re.sub(r"\.\d+$", "", o.name) + "_BLK"   # a Phase 3 object may already hold the plain name
            if o.data is not None:
                o.data.name = o.name
    L.save_master(master)

main()
