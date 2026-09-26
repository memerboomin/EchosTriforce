# Modèle 3D d'un personnage (généré « image → 3D » ou modélisé, en pose T, sans squelette) riggé sur le squelette du
# mannequin UE5 et texturé par les planches de face et de dos (plus nettes et justes que la texture générée).
#   Blender -b -P Tools/blender/rig_model.py -- <modele.fbx> <manny.fbx> <face.jpg> <dos.jpg> <dossier_sortie> [link]
# Étapes : mise à l'échelle sur la planche ; allègement ; bras ramenés aux proportions du squelette ; poids transférés depuis
# le mannequin ajusté à la silhouette (jupe et équipement dorsal traités à part) ; projection des planches avec test de
# visibilité ; skinning inverse de la pose T vers la pose de repos ; poignée d'épée séparée ; export FBX (même squelette).
import bpy, bmesh, math, os, sys
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
from mathutils.interpolate import poly_3d_calc

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_character as mc
import tex_tools as tt

MODEL = None
TARGET_TRIS = 90000
CAP_TOP = 1.851          # sommet du bonnet sur les planches (m)
SHOULDER_X = 0.19        # articulation de l'épaule du mannequin
SHEATH = (Vector((0.27, 0.0, 1.70)), Vector((-0.40, 0.0, 0.67)))  # axe du fourreau (vu de face, cf. make_character)


def configure(args):
    global MODEL
    MODEL = args[0]
    mc.configure(args[1:])


def log(*a):
    print("RIG", *a, flush=True)


def smoothstep(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def world_verts(obj, evaluated=True):
    if evaluated:
        dg = bpy.context.evaluated_depsgraph_get()
        ev = obj.evaluated_get(dg)
        me = ev.to_mesh()
    else:
        me = obj.data
    mw = obj.matrix_world
    co = np.zeros(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3) @ np.array(mw.to_3x3()).T + np.array(mw.translation)
    if evaluated:
        obj.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh_clear()
    return co


def arm_profile(co, side, xs):
    """Centre (z, y) de la section du bras aux abscisses xs (bras le long de X en pose T), None si vide."""
    out = []
    for x in xs:
        m = (np.abs(co[:, 0] - side * x) < 0.012) & (co[:, 2] > 1.15) & (co[:, 2] < 1.75)
        if m.sum() < 8:
            out.append(None)
            continue
        z, y = co[m, 2], co[m, 1]
        out.append(((z.min() + z.max()) * 0.5, (y.min() + y.max()) * 0.5))
    return out


# ---------------------------------------------------------------------------------------------------------------
def load_model():
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=MODEL)
    objs = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    o.parent = None
    o.data.transform(o.matrix_world)
    o.matrix_world = Matrix.Identity(4)
    # images de la texture générée (couleur et normales)
    albedo = normal = None
    for img in bpy.data.images:
        n = img.name.lower()
        if "normal" in n:
            normal = img
        elif "rough" not in n and "metal" not in n and img.size[0] > 0:
            albedo = img
    log("modèle", o.name, len(o.data.vertices), "sommets", len(o.data.polygons), "faces ; textures", albedo and albedo.name, normal and normal.name)
    return o, albedo, normal


def place_model(o, body):
    """Échelle (sommet du bonnet) et profondeur (devant du torse aligné sur le mannequin ajusté)."""
    co = world_verts(o, False)
    s = CAP_TOP / co[:, 2].max()
    o.data.transform(Matrix.Scale(s, 4))
    co = world_verts(o, False)
    bc = world_verts(body)
    def front(c):
        m = (np.abs(c[:, 0]) < 0.05) & (np.abs(c[:, 2] - 1.25) < 0.02)
        return c[m, 1].min()
    dy = front(bc) - front(co) + 0.004
    dx = -0.5 * (co[:, 0].min() + co[:, 0].max()) * 0.0
    o.data.transform(Matrix.Translation((dx, dy, 0.0)))
    log("échelle", round(s, 4), "décalage en profondeur", round(dy, 4))


def decimate(o):
    tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
    if tris <= TARGET_TRIS:
        return
    mod = o.modifiers.new("Decimate", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = TARGET_TRIS / tris
    mod.use_collapse_triangulate = True
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier=mod.name)
    log("allègement", tris, "→", len(o.data.polygons), "triangles")


def fit_arms(o, manny_co):
    """Bras du modèle ramenés à la longueur et à l'axe de ceux du mannequin en pose T (articulations au bon endroit)."""
    co = world_verts(o, False)
    xs = np.arange(0.40, 0.86, 0.02)
    for side in (1, -1):
        arm_m = co[:, 0] * side > SHOULDER_X
        tip_model = (co[arm_m & (co[:, 2] > 1.2) & (co[:, 2] < 1.7), 0] * side).max()
        tip_manny = (manny_co[(manny_co[:, 0] * side > SHOULDER_X) & (manny_co[:, 2] > 1.2), 0] * side).max()
        k = (tip_manny - SHOULDER_X) / (tip_model - SHOULDER_X)
        pm = arm_profile(co, side, xs)
        pn = arm_profile(manny_co, side, xs)
        ok = [i for i in range(len(xs)) if pm[i] and pn[i]]
        dz = np.array([pn[i][0] - pm[i][0] for i in ok])
        dy = np.array([pn[i][1] - pm[i][1] for i in ok])
        dz_m, dy_m = float(np.median(dz)), float(np.median(dy))
        for v in o.data.vertices:
            x = v.co.x * side
            if x <= SHOULDER_X or v.co.y > 0.13:
                continue
            w = smoothstep(0.20, 0.38, x)
            nx = SHOULDER_X + (x - SHOULDER_X) * (1 + (k - 1) * smoothstep(0.19, 0.30, x))
            v.co.x = side * nx
            v.co.z += dz_m * w
            v.co.y += dy_m * w
        log("bras", "gauche" if side > 0 else "droit", "longueur ×%.3f" % k, "hauteur %+.3f" % dz_m, "profondeur %+.3f" % dy_m)


# ---------------------------------------------------------------------------------------------------------------
def back_surface_profile(manny_co):
    prof = {}
    for p in manny_co:
        if abs(p[0]) < 0.2 and 0.55 < p[2] < 1.6:
            k = round(float(p[2]), 2)
            prof[k] = max(prof.get(k, -1.0), float(p[1]))
    ks = sorted(prof)
    return lambda z: float(np.interp(z, ks, [prof[k] for k in ks]))


def dist_to_sheath(p):
    a, b = SHEATH
    ab = Vector((b.x - a.x, 0.0, b.z - a.z))
    ap = Vector((p.x - a.x, 0.0, p.z - a.z))
    t = max(0.0, min(1.0, ap.dot(ab) / ab.length_squared))
    return (ap - ab * t).length


def classify(p, yb):
    """'hilt', 'gear' (rigide sur le haut du dos), 'skirt' (jupe de tunique) ou 'body'."""
    head = p.z > 1.52 and abs(p.x) < 0.16 and p.y < 0.2
    behind = p.y > yb(min(max(p.z, 0.56), 1.59)) + 0.02
    if not head and 0.60 < p.z < 1.82 and (behind or (dist_to_sheath(p) < 0.055 and p.y > yb(min(max(p.z, 0.56), 1.59)) - 0.05)):
        return "hilt" if p.z > 1.47 and p.x > 0.05 else "gear"
    if 0.64 < p.z < 1.04 and abs(p.x) < 0.33:
        return "skirt"
    return "body"


def classify_all(o, yb):
    """Classe de chaque sommet ; les petits îlots collés à la poignée (morceaux de garde isolés par l'allègement)
    rejoignent la poignée, sinon ils restent visibles près de la nuque quand l'épée est en main."""
    kinds = [classify(v.co, yb) for v in o.data.vertices]
    hilt = [i for i, k in enumerate(kinds) if k == "hilt"]
    if not hilt:
        return kinds
    adj = [[] for _ in kinds]
    for e in o.data.edges:
        a, b = e.vertices
        if kinds[a] != "hilt" and kinds[b] != "hilt":
            adj[a].append(b)
            adj[b].append(a)
    hp = np.array([o.data.vertices[i].co[:] for i in hilt])
    seen = [k == "hilt" for k in kinds]
    moved = 0
    for start in range(len(kinds)):
        if seen[start]:
            continue
        comp, stack = [], [start]
        seen[start] = True
        while stack:
            x = stack.pop()
            comp.append(x)
            for y in adj[x]:
                if not seen[y]:
                    seen[y] = True
                    stack.append(y)
        if len(comp) > 0.02 * len(kinds):
            continue
        cp = np.array([o.data.vertices[i].co[:] for i in comp])
        d = np.sqrt(((hp[None, :, :] - cp[::max(1, len(cp) // 40), None, :]) ** 2).sum(-1)).min()
        if d < 0.03:
            for i in comp:
                kinds[i] = "hilt"
            moved += len(comp)
    log("poignée : %d sommets d'îlots voisins rattachés" % moved)
    return kinds


def transfer_weights(o, manny, kinds):
    """Poids de skinning : surface la plus proche du mannequin (pose T), barycentrique ; règles pour jupe et équipement."""
    dg = bpy.context.evaluated_depsgraph_get()
    ev = manny.evaluated_get(dg)
    me = ev.to_mesh()
    mw = manny.matrix_world
    verts = [mw @ v.co for v in me.vertices]
    polys = [tuple(p.vertices) for p in me.polygons]
    tree = BVHTree.FromPolygons(verts, polys)
    names = [g.name for g in manny.vertex_groups]
    vw = [dict((g.group, g.weight) for g in v.groups) for v in manny.data.vertices]
    ev.to_mesh_clear()
    out = []
    for v in o.data.vertices:
        p = v.co.copy()
        kind = kinds[v.index]
        if kind in ("gear", "hilt"):
            out.append({"spine_05": 1.0})
            continue
        loc, nrm, fi, d = tree.find_nearest(p)
        acc = {}
        if fi is not None:
            ids = polys[fi]
            bw = poly_3d_calc([verts[i] for i in ids], loc)
            for vi, b in zip(ids, bw):
                for g, w in vw[vi].items():
                    acc[names[g]] = acc.get(names[g], 0.0) + w * b
        if kind == "skirt":
            t = max(0.0, min(1.0, (1.06 - p.z) / 0.37))
            leg = 0.55 * t * min(1.0, abs(p.x) / 0.06)
            sk = {"pelvis": 1.0 - leg, ("thigh_l" if p.x > 0 else "thigh_r"): leg}
            blend = smoothstep(0.68, 0.76, p.z)  # sous l'ourlet : retour aux poids des jambes
            mixed = {}
            for kname, w in sk.items():
                mixed[kname] = mixed.get(kname, 0.0) + w * blend
            for kname, w in acc.items():
                mixed[kname] = mixed.get(kname, 0.0) + w * (1 - blend)
            acc = mixed
        top = sorted(acc.items(), key=lambda kv: -kv[1])[:4]
        tot = sum(w for _, w in top) or 1.0
        out.append({k: w / tot for k, w in top if w > 1e-4})
    for name in names:
        o.vertex_groups.new(name=name)
    for i, ws in enumerate(out):
        for bone, w in ws.items():
            g = o.vertex_groups.get(bone) or o.vertex_groups.new(name=bone)
            g.add([i], w, "REPLACE")
    counts = {k: kinds.count(k) for k in set(kinds)}
    log("poids transférés", counts)


def unpose(o, arm, weights_obj):
    """Skinning inverse : sommets en pose T → pose de repos du squelette (p_repos = (Σ w·B)⁻¹ p_T)."""
    amw = arm.matrix_world
    amwi = amw.inverted()
    B = {}
    for pb in arm.pose.bones:
        B[pb.name] = amw @ pb.matrix @ pb.bone.matrix_local.inverted() @ amwi
    gnames = [g.name for g in weights_obj.vertex_groups]
    for v in weights_obj.data.vertices:
        M = Matrix(((0.0,) * 4,) * 4)
        tot = 0.0
        for g in v.groups:
            b = B.get(gnames[g.group])
            if b is None:
                continue
            M = M + b * g.weight
            tot += g.weight
        if tot < 1e-6:
            continue
        M = M * (1.0 / tot)
        v.co = M.inverted_safe() @ v.co


def color_class(c):
    """Famille de couleur (valeurs sRGB 0-1) pour comparer le dessin et la texture du modèle."""
    import colorsys
    h, sat, v = colorsys.rgb_to_hsv(float(c[0]), float(c[1]), float(c[2]))
    h *= 360.0
    if sat < 0.16:
        return "grey"
    if 70 <= h < 170:
        return "green"
    if 185 <= h < 245:
        return "blue"
    if 245 <= h < 320:
        return "purple"
    if h >= 340 or h < 12:
        return "red" if v > 0.35 else "warm"
    return "warm"


def image_array(img):
    w, h = img.size
    a = np.zeros(w * h * 4, np.float32)
    img.pixels.foreach_get(a)
    return a.reshape(h, w, 4)[:, :, :3]  # lignes de bas en haut (convention Blender)


def model_arrays(o):
    """Sommets, triangles (sommets et coins) et UV propres du modèle (avant projection)."""
    me = o.data
    me.calc_loop_triangles()
    co = world_verts(o, False)
    lt = me.loop_triangles
    tv = np.zeros(len(lt) * 3, np.int64)
    lt.foreach_get("vertices", tv)
    tl = np.zeros(len(lt) * 3, np.int64)
    lt.foreach_get("loops", tl)
    uv = np.zeros(len(me.loops) * 2, np.float32)
    me.uv_layers[0].data.foreach_get("uv", uv)
    return co, tv.reshape(-1, 3), tl.reshape(-1, 3), uv.reshape(-1, 2)


def render_model(co, tv, tl, uv, alb, W, H, sign):
    """Vue orthographique de la texture du modèle au cadrage des planches (sign 1 = face, -1 = dos, en miroir)."""
    px = mc.CX + sign * co[:, 0] * mc.PPM
    py = mc.SOLE - co[:, 2] * mc.PPM
    tid, bary = tt.rasterize(np.stack([px, py], 1), co[:, 1] * sign, tv, W, H)
    m = tid >= 0
    uvs = (uv[tl[tid[m]]] * bary[m][:, :, None]).sum(1)
    AH, AW = alb.shape[:2]
    img = np.full((H, W, 3), 0.5, np.float32)
    img[m] = tt.sample(alb, uvs[:, 0] * AW, (1 - uvs[:, 1]) * AH)
    # orientation de la surface vue (normale de face du triangle)
    P = co[tv]
    fn = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    fn /= np.maximum(np.linalg.norm(fn, axis=1, keepdims=True), 1e-12)
    facing = np.zeros((H, W), np.float32)
    facing[m] = -sign * fn[tid[m], 1]
    return img, m, facing


def kmeans(x, k, iters=25, seed=3):
    rng = np.random.default_rng(seed)
    c = x[rng.choice(len(x), k, replace=False)]
    for _ in range(iters):
        d = ((x[:, None, :] - c[None, :, :]) ** 2).sum(-1)
        lab = d.argmin(1)
        for j in range(k):
            m = lab == j
            if m.any():
                c[j] = x[m].mean(0)
    return lab, c


def deshadow_albedo(o, albedo, strength=0.9, k=14, iters=25):
    """Ombres cuites de la texture générée (flancs, dessous des bras, taches sombres) : luminance lissée sur la surface
    du modèle à l'intérieur de chaque famille de couleur (chromaticité, insensible à l'éclairage), ramenée vers la teinte
    éclairée de la famille. Les couleurs neutres (blanc, gris, métal) ne sont pas touchées : ombre et teinte s'y confondent."""
    co, tv, tl, uv = model_arrays(o)
    me = o.data
    alb = tt.image_to_array(albedo)
    AH, AW = alb.shape[:2]
    lv = np.zeros(len(me.loops), np.int64)
    me.loops.foreach_get("vertex_index", lv)
    N = len(me.vertices)
    lc = tt.sample(alb, uv[:, 0] * AW, (1 - uv[:, 1]) * AH)
    vc = np.zeros((N, 3))
    np.add.at(vc, lv, lc)
    vc /= np.maximum(np.bincount(lv, minlength=N), 1)[:, None]
    L = vc @ np.array([0.3, 0.59, 0.11])
    mx, mn = vc.max(1), vc.min(1)
    sat = (mx - mn) / np.maximum(mx, 1e-3)
    chroma = vc / np.maximum(vc.sum(1, keepdims=True), 1e-3)
    lab, _ = kmeans(np.concatenate([chroma * 4.0, sat[:, None]], 1), k)
    ev = np.zeros(len(me.edges) * 2, np.int64)
    me.edges.foreach_get("vertices", ev)
    ev = ev.reshape(-1, 2)
    e = ev[lab[ev[:, 0]] == lab[ev[:, 1]]]
    deg = np.bincount(e[:, 0], minlength=N) + np.bincount(e[:, 1], minlength=N)
    Ls = L.copy()
    for _ in range(iters):
        acc = np.bincount(e[:, 0], weights=Ls[e[:, 1]], minlength=N) + np.bincount(e[:, 1], weights=Ls[e[:, 0]], minlength=N)
        Ls = (0.05 * L + acc) / (0.05 + deg)
    target = np.array([np.percentile(L[lab == j], 70) if (lab == j).any() else 0.5 for j in range(k)])
    g = np.clip((target[lab] / np.maximum(Ls, 1e-3)) ** strength, 0.75, 1.6)
    g = np.where(sat > 0.22, g, 1.0)
    # application dans la texture (dépliage d'origine) : facteur interpolé au texel
    tid, bary = tt.rasterize(np.stack([uv[:, 0] * AW, (1 - uv[:, 1]) * AH], 1), None, tl, AW, AH)
    cov = tid >= 0
    gt = (g[lv[tl[tid[cov]]]] * bary[cov]).sum(1)
    out = alb.copy()
    out[cov] = np.clip(alb[cov] * gt[:, None], 0, 1)
    tt.array_to_image(bpy, out.astype(np.float32), albedo.name)
    log("ombres cuites : %d familles, gain médian %.2f, %.0f %% des sommets éclaircis de plus de 10 %%"
        % (k, float(np.median(g)), 100.0 * float((g > 1.1).mean())))


def clear_green_blotches(albedo, radius=3):
    """Taches vert foncé peintes par le générateur sur la tunique et le bonnet (surtout aux flancs) : les aplats sombres
    assez larges sont ramenés vers le vert moyen ; les traits fins (plis, coutures) survivent à l'ouverture morphologique."""
    a = tt.image_to_array(albedo)
    mx, mn = a.max(-1), a.min(-1)
    sat = (mx - mn) / np.maximum(mx, 1e-3)
    green = (a[..., 1] >= a[..., 0]) & (a[..., 1] >= a[..., 2]) & (sat > 0.25)
    L = a @ np.array([0.3, 0.59, 0.11], np.float32)
    ref = float(np.median(L[green]))
    dark = green & (L < 0.72 * ref)
    blob = tt.dilate(tt.erode(dark, radius), radius) & green
    t = tt.gauss(blob.astype(np.float32), 1.5) * green
    target = 0.9 * ref
    gain = 1.0 + t * (np.maximum(target / np.maximum(L, 1e-3), 1.0) - 1.0) * 0.85
    out = np.clip(a * gain[..., None], 0, 1)
    tt.array_to_image(bpy, out.astype(np.float32), albedo.name)
    log("taches vertes : %.2f %% de la texture éclaircis (vert de référence %.2f)" % (100.0 * float((t > 0.5).mean()), ref))


def align_drawings(o, F, B, masks, alb):
    """Les planches et la texture du modèle ne se superposent pas exactement (visage, sangles, ceinture et bouclier
    décalés de 1 à 3 cm) : flot dense robuste dessin → modèle, vue de face et de dos. Sans recalage, les zones de fondu
    montrent deux images décalées (sangles fantômes, mèches doublées)."""
    co, tv, tl, uv = model_arrays(o)
    H, W = F.shape[:2]
    views = {}
    for key, D, sign, bg in (("front", F, 1, masks["bg_f"]), ("back", B, -1, masks["bg_b"])):
        M, m, facing = render_model(co, tv, tl, uv, alb, W, H, sign)
        fl = tt.flow_lk(M, D, m, ~bg, log=lambda *a: log("recalage", key, *a))
        yy, xx = np.mgrid[0:H, 0:W].astype(np.float32) + 0.5
        Dw = tt.sample(D, xx + fl[..., 0], yy + fl[..., 1])
        fg = tt.sample((~bg).astype(np.float32)[..., None], xx + fl[..., 0], yy + fl[..., 1])[..., 0] > 0.5
        views[key] = dict(flow=fl, M=M, m=m, facing=facing, Dw=Dw, fg=fg)
    return views


def warped_coords(co, views):
    """Coordonnées pixel (recalées) de chaque sommet dans les planches de face et de dos."""
    out = {}
    for key, sign in (("front", 1), ("back", -1)):
        px = mc.CX + sign * co[:, 0] * mc.PPM
        py = mc.SOLE - co[:, 2] * mc.PPM
        d = tt.sample(views[key]["flow"], px, py)
        out[key] = (np.clip(px + d[:, 0], 0.0, mc.HALF - 0.5), np.clip(py + d[:, 1], 0.0, mc.ATLAS_H - 0.5))
    return out


def harmonize_albedo(albedo, views, strength=0.85, bins=20):
    """Palette des planches appliquée à la texture du modèle (surtout visible sur les flancs) : table de correspondance
    couleur → couleur apprise là où les deux se superposent après recalage (surfaces bien de face ou de dos), en écartant
    les zones au contenu différent (blason inventé par le modèle, bout du bonnet)."""
    src, dst = [], []
    for key in ("front", "back"):
        v = views[key]
        An, _ = tt.normalize_local(v["M"], v["m"], 12.0)
        Bn, _ = tt.normalize_local(v["Dw"], v["fg"], 12.0)
        r = np.sqrt(((An - Bn) ** 2).sum(-1))
        ok = v["m"] & v["fg"] & (v["facing"] > 0.6)
        ok &= r < np.median(r[ok]) * 1.5
        src.append(v["M"][ok])
        dst.append(v["Dw"][ok])
    src, dst = np.concatenate(src), np.concatenate(dst)
    idx = np.clip((src * bins).astype(np.int64), 0, bins - 1)
    key = (idx[:, 0] * bins + idx[:, 1]) * bins + idx[:, 2]
    sums = np.zeros((bins ** 3, 3), np.float64)
    cnt = np.zeros(bins ** 3, np.float64)
    np.add.at(sums, key, dst - src)
    np.add.at(cnt, key, 1.0)
    sums, cnt = sums.reshape(bins, bins, bins, 3), cnt.reshape(bins, bins, bins)
    # cases vides : diffusion des cases voisines (convolution normalisée), puis léger lissage de la table
    def blur3(a):
        for ax in range(3):
            p = np.concatenate([np.take(a, [0], axis=ax), a, np.take(a, [-1], axis=ax)], axis=ax)
            a = (np.take(p, range(0, bins), axis=ax) + np.take(p, range(1, bins + 1), axis=ax) + np.take(p, range(2, bins + 2), axis=ax)) / 3.0
        return a
    lut = sums / np.maximum(cnt, 1e-9)[..., None]
    known = cnt > 3
    filled = np.where(known[..., None], lut, 0.0)
    w = known.astype(np.float64)
    fs, ws = filled.copy(), w.copy()
    for _ in range(12):
        fs, ws = blur3(fs), blur3(ws)
        filled = np.where(known[..., None], lut, fs / np.maximum(ws, 1e-9)[..., None])
    lut = 0.5 * filled + 0.5 * blur3(filled)
    a = tt.image_to_array(albedo)
    h, w_ = a.shape[:2]
    c = a.reshape(-1, 3)
    out = c.copy()
    step = 1 << 20
    for s0 in range(0, len(c), step):
        x = np.clip(c[s0:s0 + step] * bins - 0.5, 0, bins - 1.0001)
        i0 = np.floor(x).astype(np.int64)
        f = x - i0
        acc = np.zeros((len(x), 3), np.float64)
        for dx in (0, 1):
            for dy in (0, 1):
                for dz in (0, 1):
                    wgt = (f[:, 0] if dx else 1 - f[:, 0]) * (f[:, 1] if dy else 1 - f[:, 1]) * (f[:, 2] if dz else 1 - f[:, 2])
                    acc += lut[np.minimum(i0[:, 0] + dx, bins - 1), np.minimum(i0[:, 1] + dy, bins - 1), np.minimum(i0[:, 2] + dz, bins - 1)] * wgt[:, None]
        out[s0:s0 + step] = c[s0:s0 + step] + strength * acc
    tt.array_to_image(bpy, np.clip(out.reshape(h, w_, 3), 0, 1).astype(np.float32), albedo.name)
    log("harmonisation : %d paires, %d cases apprises sur %d, correction moyenne %.3f"
        % (len(src), int(known.sum()), bins ** 3, float(np.abs(out - c).mean())))


def project(o, kinds, F, B, albedo, views=None):
    """UV0 = planche de face, UV1 = planche de dos, UV2 = texture du modèle, UV3 = (part face, 1 - part dos).
    Les parts suivent l'orientation de la surface et la visibilité (rayon vers la vue) ; le reste = texture du modèle."""
    me = o.data
    me.calc_loop_triangles()
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.normal_update()
    tree = BVHTree.FromBMesh(bm)
    nrm = [v.normal.copy() for v in bm.verts]
    pos = [v.co.copy() for v in bm.verts]
    bm.free()
    # couleur de la texture du modèle au sommet (premier coin de face rencontré)
    alb = image_array(albedo) if albedo else None
    vuv = [None] * len(pos)
    for li, loop in enumerate(me.loops):
        if vuv[loop.vertex_index] is None:
            vuv[loop.vertex_index] = tuple(me.uv_layers[0].data[li].uv)
    H, W = F.shape[:2]
    cof = np.array([p[:] for p in pos], np.float32)
    if views:
        wc = warped_coords(cof, views)
    else:
        py0 = mc.SOLE - cof[:, 2] * mc.PPM
        wc = {"front": (mc.CX + cof[:, 0] * mc.PPM, py0), "back": (mc.CX - cof[:, 0] * mc.PPM, py0)}
    drawn_f = tt.sample(F, wc["front"][0], wc["front"][1])
    drawn_b = tt.sample(B, wc["back"][0], wc["back"][1])
    rejected = [0, 0]
    wf, wb = [], []
    hard = []  # sommets écartés pour incohérence : le lissage ne doit pas y ramener le dessin
    for vi, (p, n) in enumerate(zip(pos, nrm)):
        if kinds[vi] in ("gear", "hilt"):
            # bouclier et fourreau : le blason dessiné couvre toute la face (celui du modèle est inventé)
            f = smoothstep(0.10, 0.45, -n.y)
            b = smoothstep(0.10, 0.45, n.y)
        else:
            f = smoothstep(0.30, 0.75, -n.y)
            b = smoothstep(0.30, 0.75, n.y)
        if f > 0:
            hit = tree.ray_cast(p + n * 0.003 + Vector((0, -0.002, 0)), Vector((0, -1, 0)), 1.5)
            if hit[0] is not None:
                f = 0.0
        if b > 0:
            hit = tree.ray_cast(p + n * 0.003 + Vector((0, 0.002, 0)), Vector((0, 1, 0)), 1.5)
            if hit[0] is not None:
                b = 0.0
        # cohérence : le dessin ne doit pas peindre un accessoire sur le corps (ni le tissu sur un accessoire)
        if alb is not None and (f > 0 or b > 0) and vuv[vi] is not None:
            u, v = vuv[vi]
            mcol = color_class(alb[min(int(v * alb.shape[0]), alb.shape[0] - 1), min(int(u * alb.shape[1]), alb.shape[1] - 1)])
            gear = kinds[vi] in ("gear", "hilt")
            for idx, (w_, dc) in enumerate(((f, drawn_f[vi]), (b, drawn_b[vi]))):
                if w_ <= 0:
                    continue
                dcol = color_class(dc)
                bad = (gear and dcol in ("green", "purple") and dcol != mcol) or \
                      (not gear and dcol in ("blue", "purple") and mcol in ("green", "warm")) or \
                      (not gear and dcol == "grey" and mcol == "green")
                if bad:
                    rejected[idx] += 1
                    hard.append((vi, idx))
                    if idx == 0:
                        f = 0.0
                    else:
                        b = 0.0
        wf.append(f)
        wb.append(b)
    # adoucit les bords des zones visibles (moyenne avec les voisins)
    adj = [[] for _ in pos]
    for e in me.edges:
        a, c = e.vertices
        adj[a].append(c)
        adj[c].append(a)
    for _ in range(2):
        wf = [0.5 * wf[i] + 0.5 * (sum(wf[j] for j in adj[i]) / len(adj[i]) if adj[i] else wf[i]) for i in range(len(pos))]
        wb = [0.5 * wb[i] + 0.5 * (sum(wb[j] for j in adj[i]) / len(adj[i]) if adj[i] else wb[i]) for i in range(len(pos))]
    for vi, idx in hard:
        if idx == 0:
            wf[vi] = 0.0
        else:
            wb[vi] = 0.0
    model_uv = [tuple(me.uv_layers[0].data[li].uv) for li in range(len(me.loops))]
    for l in list(me.uv_layers):
        me.uv_layers.remove(l)
    lf = me.uv_layers.new(name="UVFront")
    lb = me.uv_layers.new(name="UVBack")
    lm = me.uv_layers.new(name="UVModel")
    ld = me.uv_layers.new(name="UVData")
    (fx, fy), (bx, by) = wc["front"], wc["back"]
    for li, loop in enumerate(me.loops):
        vi = loop.vertex_index
        lf.data[li].uv = (fx[vi] / mc.ATLAS_W, 1.0 - fy[vi] / mc.ATLAS_H)
        lb.data[li].uv = ((mc.HALF + bx[vi]) / mc.ATLAS_W, 1.0 - by[vi] / mc.ATLAS_H)
        lm.data[li].uv = model_uv[li]
        ld.data[li].uv = (wf[vi], 1.0 - wb[vi])  # y en 1 - valeur : l'import UE retourne V
    me.uv_layers.active = lf
    log("projection : part face moyenne %.2f, part dos moyenne %.2f ; incohérences écartées face %d, dos %d"
        % (sum(wf) / len(wf), sum(wb) / len(wb), rejected[0], rejected[1]))


def preview_material(name, draw_img, albedo):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    L = nt.links
    def uvmap(n):
        u = nt.nodes.new("ShaderNodeUVMap")
        u.uv_map = n
        return u.outputs["UV"]
    def tex(img, uvn):
        t = nt.nodes.new("ShaderNodeTexImage")
        t.image = img
        L.new(uvmap(uvn), t.inputs["Vector"])
        return t.outputs["Color"]
    def mix(fac, a, b):
        mx = nt.nodes.new("ShaderNodeMix")
        mx.data_type = "RGBA"
        L.new(fac, mx.inputs["Factor"])
        L.new(a, mx.inputs["A"])
        L.new(b, mx.inputs["B"])
        return mx.outputs["Result"]
    sd = nt.nodes.new("ShaderNodeSeparateXYZ")
    L.new(uvmap("UVData"), sd.inputs["Vector"])
    inv = nt.nodes.new("ShaderNodeMath")
    inv.operation = "SUBTRACT"
    inv.inputs[0].default_value = 1.0
    L.new(sd.outputs["Y"], inv.inputs[1])
    col = mix(sd.outputs["X"], tex(albedo, "UVModel"), tex(draw_img, "UVFront"))
    col = mix(inv.outputs[0], col, tex(draw_img, "UVBack"))
    emi = nt.nodes.new("ShaderNodeEmission")
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    L.new(col, emi.inputs["Color"])
    L.new(emi.outputs["Emission"], out.inputs["Surface"])
    return m


def split_hilt(o, kinds):
    """Sépare la poignée d'épée (masquée en combat quand l'épée est en main)."""
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = o
    o.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.ops.object.mode_set(mode="OBJECT")
    for v in o.data.vertices:
        v.select = kinds[v.index] == "hilt"
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_mode(type="VERT")
    bpy.ops.mesh.separate(type="SELECTED")
    bpy.ops.object.mode_set(mode="OBJECT")
    parts = [x for x in bpy.context.selected_objects if x != o]
    return parts[0] if parts else None


def save_image(img, path):
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    log("texture", path)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    F, B = mc.load_rgb(mc.FRONT), mc.load_rgb(mc.BACK)
    masks, clean_f, clean_b = mc.segment(F, B, mc.OUT)
    known_f, known_b = ~mc.dil(masks["bg_f"], 2), ~mc.dil(masks["bg_b"], 2)
    draw_img = mc.build_atlas(F, B, known_f, known_b, "Draw")

    bpy.ops.import_scene.fbx(filepath=mc.MANNY)
    empty = [x for x in bpy.context.scene.objects if x.type == "EMPTY"][0]
    arm = [x for x in bpy.context.scene.objects if x.type == "ARMATURE"][0]
    manny = [x for x in bpy.context.scene.objects if x.type == "MESH"][0]
    mc.shape_body(manny)
    mc.fit_torso(manny, masks["sil_f"])
    yb = back_surface_profile(world_verts(manny))

    model, albedo, normal = load_model()
    place_model(model, manny)
    decimate(model)
    mc.t_pose(arm)
    fit_arms(model, world_verts(manny))
    kinds = classify_all(model, yb)
    transfer_weights(model, manny, kinds)
    if albedo:
        deshadow_albedo(model, albedo)
        clear_green_blotches(albedo)
    views = align_drawings(model, F, B, masks, tt.image_to_array(albedo)) if albedo else None
    project(model, kinds, F, B, albedo, views)
    if views:
        harmonize_albedo(albedo, views)
    mat = preview_material("M_%s_Model" % mc.CAP, draw_img, albedo)
    model.data.materials.clear()
    model.data.materials.append(mat)
    for p in model.data.polygons:
        p.use_smooth = True
    unpose(model, arm, model)
    # le modèle suit désormais le squelette ; le mannequin ne sert plus
    bpy.data.objects.remove(manny)
    model.parent = arm
    model.matrix_parent_inverse = arm.matrix_world.inverted()
    mod = model.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    hilt = split_hilt(model, kinds)
    mc.preview(os.path.join(mc.OUT, "preview_%s_tpose.png" % mc.NAME))
    mc.rest_pose(arm)
    mc.preview(os.path.join(mc.OUT, "preview_%s_rest.png" % mc.NAME))
    model.name = "SKM_%s" % mc.CAP
    objs = [model]
    if hilt:
        hilt.name = "SKM_%s_Hilt" % mc.CAP
        objs.append(hilt)
    mc.flatten_rig(empty, arm, objs)
    for x in objs:
        mc.export([arm, x], os.path.join(mc.OUT, x.name + ".fbx"))
    old = os.path.join(mc.OUT, "SKM_%s_Gear.fbx" % mc.CAP)
    if os.path.exists(old):
        os.remove(old)  # bouclier et fourreau font partie du modèle
    if albedo:
        save_image(albedo, os.path.join(mc.OUT, "T_%s_Albedo.png" % mc.CAP))
    if normal:
        save_image(normal, os.path.join(mc.OUT, "T_%s_Normal.png" % mc.CAP))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(mc.OUT, "%s_model.blend" % mc.NAME))
    log("TERMINE")


if __name__ == "__main__":
    configure(sys.argv[sys.argv.index("--") + 1:])
    main()
