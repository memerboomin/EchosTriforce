# Personnage texturé à partir de planches de face et de dos en pose T, riggé sur le squelette du mannequin UE5.
#   Blender -b -P Tools/blender/make_character.py -- <manny.fbx> <face.jpg> <dos.jpg> <dossier_sortie> [link]
# Étapes :
#  1. découpe des accessoires dans les planches (couleur + fermeture + suivi de contour) ;
#  2. deux atlas (face | dos) : « Gear » = dessin d'origine, « Body » = dessin où l'on a effacé ce qui est modélisé à part ;
#  3. mannequin remodelé aux proportions du dessin + bonnet, cheveux, oreilles, jupe de tunique ;
#  4. accessoires extrudés en volumes bombés à partir de leur silhouette dessinée ;
#  5. pose T temporaire → projection UV face/dos → pose de repos → export FBX sur le squelette de l'UE5.
import bpy, bmesh, math, os, sys
import numpy as np
from mathutils import Vector, Matrix
from mathutils.geometry import delaunay_2d_cdt

MANNY = FRONT = BACK = OUT = None
NAME, CAP = "link", "Link"


def configure(args):
    """Paramètres : <manny.fbx> <face.jpg> <dos.jpg> <dossier_sortie> [nom]. Aussi appelée par rig_model.py (module)."""
    global MANNY, FRONT, BACK, OUT, NAME, CAP
    MANNY, FRONT, BACK, OUT = args[:4]
    NAME = args[4] if len(args) > 4 else "link"
    CAP = NAME.capitalize()
    os.makedirs(OUT, exist_ok=True)

# Calibrage des planches (silhouette : pieds y=1471, axe x=1010, 785 px par mètre, face et dos alignés)
PPM, CX, SOLE = 785.0, 1010.0, 1471.0
ATLAS_W, ATLAS_H, HALF = 4096, 2048, 2048
FRONT_GEAR_DX = -15.0  # sur la planche de face, l'équipement dorsal est dessiné ~15 px plus à gauche que sur celle de dos

# Modes de projection par face (attribut « proj »)
P_BODY, P_FRONT_ONLY, P_GEAR, P_BACK_ONLY, P_EAR, P_HAIR = 0, 1, 2, 3, 4, 5
HEAD_WIDTHS = None   # (hauteurs, demi-largeurs dessinées) de la tête, calculées par hair_shell
SIDE_COLORS = None   # (blond des mèches, vert du bonnet) en linéaire, prélevés sur les planches
# Surfaces vues de biais : on lit la couleur jusqu'à INSET mètres à l'intérieur du dessin (évite d'étirer le trait de contour)
INSET = 0.022
HEAD_C = Vector((0.0, -0.03, 1.68))  # centre du crâne du mannequin après remodelage


def log(*a):
    print("CHAR", *a, flush=True)


# =============================================================================================================
# Images
def load_rgb(path):
    img = bpy.data.images.load(path)
    w, h = img.size
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1, :, :3]
    bpy.data.images.remove(img)
    return np.ascontiguousarray(a)


def save_rgb(a, path, name="tmp"):
    h, w = a.shape[:2]
    img = bpy.data.images.new(name, w, h, alpha=False)
    rgba = np.concatenate([a, np.ones((h, w, 1), np.float32)], axis=2)[::-1]
    img.pixels.foreach_set(np.ascontiguousarray(rgba).ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    return img


def hsv(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    mx, mn = a.max(-1), a.min(-1)
    d = mx - mn
    h = np.zeros_like(mx)
    ok = d > 1e-6
    rm = ok & (mx == r)
    gm = ok & (mx == g) & ~rm
    bm = ok & ~rm & ~gm
    h[rm] = ((g - b)[rm] / d[rm]) % 6
    h[gm] = (b - r)[gm] / d[gm] + 2
    h[bm] = (r - g)[bm] / d[bm] + 4
    h *= 60
    s = np.where(mx > 1e-6, d / np.maximum(mx, 1e-6), 0)
    return h, s, mx


def classes(a):
    h, s, v = hsv(a)
    return {
        "white": (a > 0.9).all(-1),
        "green": (h > 70) & (h < 170) & (s > 0.12),
        "purple": (h > 232) & (h < 310) & (s > 0.15) & (v > 0.12),
        "teal": (h >= 170) & (h < 200) & (s > 0.12),
        "blue": (h >= 195) & (h <= 232) & (s > 0.18),
        "gold": (h > 26) & (h < 58) & (s > 0.35) & (v > 0.42),
        "dark": v < 0.22,
        "grey": (s < 0.16) & (v > 0.22) & (v < 0.9),
        "brown": (h > 5) & (h < 32) & (s > 0.28) & (v < 0.5),
    }


def poly_mask(poly, shape):
    H, W = shape
    P = np.array(poly, float)
    x0, y0 = np.maximum(np.floor(P.min(0)).astype(int), 0)
    x1, y1 = np.minimum(np.ceil(P.max(0)).astype(int), [W - 1, H - 1])
    ys, xs = np.mgrid[y0:y1 + 1, x0:x1 + 1]
    inside = points_in_poly(np.stack([xs.ravel() + 0.5, ys.ravel() + 0.5], 1), P).reshape(xs.shape)
    m = np.zeros(shape, bool)
    m[y0:y1 + 1, x0:x1 + 1] = inside
    return m


def points_in_poly(G, P):
    px, py = G[:, 0], G[:, 1]
    inside = np.zeros(len(G), bool)
    n = len(P)
    for i in range(n):
        xa, ya = P[i]
        xb, yb = P[(i + 1) % n]
        if ya == yb:
            continue
        cond = (ya > py) != (yb > py)
        xint = xa + (py - ya) * (xb - xa) / (yb - ya)
        inside ^= cond & (px < xint)
    return inside


def dist_to_poly(G, P):
    A = P
    B = np.roll(P, -1, 0)
    AB = B - A
    L2 = np.maximum((AB * AB).sum(-1), 1e-9)
    out = np.full(len(G), np.inf)
    for s in range(0, len(G), 2048):
        g = G[s:s + 2048]
        AP = g[:, None, :] - A[None]
        t = np.clip((AP * AB[None]).sum(-1) / L2[None], 0, 1)
        proj = A[None] + t[..., None] * AB[None]
        out[s:s + 2048] = np.linalg.norm(g[:, None, :] - proj, axis=-1).min(1)
    return out


def dil(m, r=1):
    for _ in range(r):
        p = np.pad(m, 1)
        m = p[1:-1, 1:-1] | p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:] | p[:-2, :-2] | p[2:, 2:] | p[:-2, 2:] | p[2:, :-2]
    return m


def ero(m, r=1):
    return ~dil(~m, r)


def close(m, r):
    return ero(dil(m, r), r)


def fill_holes(m):
    ys, xs = np.nonzero(m)
    if not len(ys):
        return m
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    sub = np.pad(m[y0:y1, x0:x1], 1)
    out = np.zeros_like(sub)
    out[0, :] = out[-1, :] = out[:, 0] = out[:, -1] = True
    out &= ~sub
    while True:
        n = dil(out) & ~sub
        if (n == out).all():
            break
        out = n
    res = m.copy()
    res[y0:y1, x0:x1] |= ~out[1:-1, 1:-1]
    return res


def keep_components(m, min_px=150, largest=False):
    ys, xs = np.nonzero(m)
    if not len(ys):
        return m
    y0, x0 = ys.min(), xs.min()
    sub = m[y0:ys.max() + 1, x0:xs.max() + 1]
    H, W = sub.shape
    flat = sub.ravel().tolist()
    lab = [0] * (H * W)
    comps = []
    for start in np.flatnonzero(sub).tolist():
        if lab[start]:
            continue
        n = len(comps) + 1
        lab[start] = n
        stack, members = [start], []
        while stack:
            p = stack.pop()
            members.append(p)
            y, x = divmod(p, W)
            for yy in (y - 1, y, y + 1):
                if 0 <= yy < H:
                    for xx in (x - 1, x, x + 1):
                        if 0 <= xx < W:
                            q = yy * W + xx
                            if flat[q] and not lab[q]:
                                lab[q] = n
                                stack.append(q)
        comps.append(members)
    comps.sort(key=len, reverse=True)
    keep = comps[:1] if largest else [c for c in comps if len(c) >= min_px]
    out = np.zeros(H * W, bool)
    for c in keep:
        out[c] = True
    res = np.zeros_like(m)
    res[y0:y0 + H, x0:x0 + W] = out.reshape(H, W)
    return res


def contour(m):
    """Contour extérieur (coordonnées de coins de pixels) de la plus grande composante."""
    p = np.pad(m, 1)
    nxt = {}

    def add(a, b):
        nxt.setdefault(a, []).append(b)
    a, b = p[:-1, :], p[1:, :]
    for y, x in zip(*np.nonzero(b & ~a)):
        add((x, y + 1), (x + 1, y + 1))
    for y, x in zip(*np.nonzero(a & ~b)):
        add((x + 1, y + 1), (x, y + 1))
    l, r = p[:, :-1], p[:, 1:]
    for y, x in zip(*np.nonzero(r & ~l)):
        add((x + 1, y + 1), (x + 1, y))
    for y, x in zip(*np.nonzero(l & ~r)):
        add((x + 1, y), (x + 1, y + 1))
    loops = []
    while nxt:
        start = next(iter(nxt))
        loop, cur, prev = [start], start, None
        while True:
            outs = nxt[cur]
            pick = outs[0]
            if len(outs) > 1 and prev is not None:
                din = (cur[0] - prev[0], cur[1] - prev[1])
                right = (-din[1], din[0])
                for o in outs:
                    if (o[0] - cur[0], o[1] - cur[1]) == right:
                        pick = o
            outs.remove(pick)
            if not outs:
                del nxt[cur]
            if pick == start:
                break
            prev, cur = cur, pick
            loop.append(cur)
        loops.append(loop)
    best = max(loops, key=lambda L: abs(poly_area(np.array(L, float))))
    return np.array(best, float) - 1.0


def poly_area(P):
    x, y = P[:, 0], P[:, 1]
    return 0.5 * (np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1)))


def dp(P, eps):
    if len(P) < 3:
        return P
    a, b = P[0], P[-1]
    ab = b - a
    L = np.linalg.norm(ab)
    if L < 1e-9:
        d = np.linalg.norm(P - a, axis=1)
    else:
        d = np.abs(ab[0] * (P[:, 1] - a[1]) - ab[1] * (P[:, 0] - a[0])) / L
    i = int(np.argmax(d))
    if d[i] > eps:
        return np.vstack([dp(P[:i + 1], eps)[:-1], dp(P[i:], eps)])
    return np.vstack([a, b])


def simplify_closed(P, eps=1.1):
    far = int(np.argmax(np.linalg.norm(P - P[0], axis=1)))
    A = dp(P[:far + 1], eps)
    B = dp(np.vstack([P[far:], P[:1]]), eps)
    return np.vstack([A[:-1], B[:-1]])


def push_pull(rgb, known):
    levels = []
    c = rgb * known[..., None]
    a = known.astype(np.float32)
    while min(a.shape) > 4:
        levels.append((c, a))
        h, w = a.shape
        h2, w2 = h // 2 * 2, w // 2 * 2
        c = c[:h2, :w2].reshape(h2 // 2, 2, w2 // 2, 2, 3).sum((1, 3))
        a = a[:h2, :w2].reshape(h2 // 2, 2, w2 // 2, 2).sum((1, 3))
    col = c / np.maximum(a, 1e-6)[..., None]
    for cl, al in reversed(levels):
        h, w = al.shape
        up = np.repeat(np.repeat(col, 2, 0), 2, 1)
        up = np.pad(up, ((0, max(0, h - up.shape[0])), (0, max(0, w - up.shape[1])), (0, 0)), mode="edge")[:h, :w]
        for _ in range(2):  # lisse l'agrandissement (sinon blocs visibles aux jointures)
            up = (up + np.roll(up, 1, 0) + np.roll(up, -1, 0) + np.roll(up, 1, 1) + np.roll(up, -1, 1)) / 5.0
        col = np.where((al > 0)[..., None], cl / np.maximum(al, 1e-6)[..., None], up)
    return col


def background(A):
    """Fond blanc relié aux bords de la planche (les blancs entourés d'un trait, yeux ou masque, sont conservés)."""
    h, s, v = hsv(A)
    nearw = (A.min(-1) > 0.8) & (s < 0.12)
    small = nearw[::2, ::2]
    bg = np.zeros_like(small)
    bg[0, :], bg[-1, :], bg[:, 0], bg[:, -1] = small[0, :], small[-1, :], small[:, 0], small[:, -1]
    while True:
        n = dil(bg, 4) & small
        if (n == bg).all():
            break
        bg = n
    return np.repeat(np.repeat(bg, 2, 0), 2, 1)[:A.shape[0], :A.shape[1]] & nearw


def run_extent(sil, y, x0, half=350):
    """Pixels extrêmes de la silhouette sur la ligne y, dans une fenêtre centrée sur x0."""
    xs = np.nonzero(sil[y, x0 - half:x0 + half])[0]
    if not len(xs):
        return x0 - 1, x0 + 1
    return x0 - half + xs.min(), x0 - half + xs.max() + 1


def drawn_extents(sil, zs, back=False, margin_px=2.0):
    """Pour chaque hauteur z : (X_min, X_max) du dessin en coordonnées monde (mètres)."""
    out = []
    for z in zs:
        y = int(round(SOLE - z * PPM))
        xl, xr = run_extent(sil, y, int(CX))
        xl, xr = xl + margin_px, xr - margin_px
        if back:
            out.append(((CX - xr) / PPM, (CX - xl) / PPM))
        else:
            out.append(((xl - CX) / PPM, (xr - CX) / PPM))
    return np.array(out)


# =============================================================================================================
# Découpe des accessoires de Link (coordonnées en pixels des planches 2000 x 1493)
SHIELD_BACK = [(882, 294), (1101, 275), (1135, 315), (1165, 380), (1182, 440), (1187, 505), (1185, 560), (1175, 620),
               (1157, 670), (1140, 690), (1120, 696), (1060, 680), (1000, 655), (940, 620), (890, 585), (840, 545),
               (800, 505), (773, 465)]
HILT_ROI_BACK = [(735, 85), (975, 85), (975, 385), (735, 385)]
SHEATH_ROI_BACK = [(1105, 700), (1122, 698), (1150, 680), (1165, 625), (1198, 622), (1348, 955), (1268, 968), (1092, 716)]
SHEATH_UNDER_SHIELD = [(1110, 702), (1170, 628), (1120, 553), (1060, 627)]
HILT_ROI_FRONT = [(1040, 85), (1290, 85), (1290, 335), (1040, 335)]
SHEATH_ROI_FRONT = [(835, 600), (892, 600), (892, 640), (852, 800), (762, 962), (660, 962), (660, 900), (782, 700)]
PROPS_ROI_FRONT = [(1050, 662), (1140, 656), (1146, 634), (1238, 634), (1238, 808), (1050, 808)]
EAR_ROIS_FRONT = [[(880, 112), (948, 112), (948, 206), (880, 206)], [(1072, 112), (1140, 112), (1140, 206), (1072, 206)]]
EAR_ROIS_BACK = [[(884, 118), (940, 118), (940, 222), (884, 222)], [(1074, 118), (1130, 118), (1130, 222), (1074, 222)]]


def skin(a):
    h, s, v = hsv(a)
    return (h > 8) & (h < 36) & (s > 0.15) & (s < 0.6) & (v > 0.5)


SHIELD_FRONT_PARTS = [
    [(798, 434), (870, 434), (888, 470), (888, 586), (864, 610), (828, 594), (800, 522)],
    [(893, 248), (944, 258), (944, 306), (925, 306), (890, 292)],
    [(1133, 440), (1180, 426), (1252, 460), (1212, 542), (1167, 612), (1138, 592)],
]


def densify(P, step):
    P = np.array(P, float)
    out = []
    for a, b in zip(P, np.roll(P, -1, 0)):
        k = max(1, int(np.ceil(np.linalg.norm(b - a) / step)))
        out += [a + (b - a) * t / k for t in range(k)]
    return np.array(out)


def snap_outward(poly, ok, reach=(-10, 40), step=6.0):
    """Recale un contour approximatif sur le trait du dessin : chaque point glisse le long de sa normale
    jusqu'au dernier pixel « ok » (appartenant à l'objet, trait noir compris)."""
    P = densify(poly, step)
    n = len(P)
    T = np.roll(P, -1, 0) - np.roll(P, 1, 0)
    N = np.stack([T[:, 1], -T[:, 0]], 1)
    N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
    if points_in_poly(P + N * 2.0, np.array(poly, float)).mean() > 0.5:
        N = -N
    H, W = ok.shape
    off = np.zeros(n)
    for i in range(n):
        last, seen, miss = reach[0], False, 0
        for t in range(reach[0], reach[1]):
            x, y = (P[i] + N[i] * t).astype(int)
            if 0 <= x < W and 0 <= y < H and ok[y, x]:
                last, seen, miss = t, True, 0
            elif seen:
                miss += 1
                if miss > 2:
                    break
        off[i] = last if seen else 0
    sm = np.array([np.median(np.take(off, range(i - 3, i + 4), mode="wrap")) for i in range(n)])
    return P + N * sm[:, None]


def segment(F, B, dbg):
    cf, cb = classes(F), classes(B)
    shape = B.shape[:2]
    masks = {}
    # --- dos
    roi = poly_mask(HILT_ROI_BACK, shape)
    m = roi & (cb["purple"] | cb["teal"])
    m = keep_components(fill_holes(close(m, 3)), 300)
    masks["hilt"] = keep_components((dil(m, 2) & roi & ~cb["white"]) | m, 300, largest=True)
    m = poly_mask(SHEATH_ROI_BACK, shape) & (cb["blue"] | cb["gold"] | cb["dark"])
    sheath_vis = keep_components(fill_holes(close(m, 3)), 300, largest=True)
    masks["sheath"] = fill_holes(close(sheath_vis | poly_mask(SHEATH_UNDER_SHIELD, shape), 2))
    ok = (cb["grey"] | cb["dark"] | cb["blue"]) & ~cb["white"] & ~sheath_vis & ~masks["hilt"]
    masks["shield"] = fill_holes(poly_mask(snap_outward(SHIELD_BACK, ok), shape))
    # --- face
    roi = poly_mask(PROPS_ROI_FRONT, shape)
    bg = roi & cf["white"]
    bgflood = roi & ~ero(roi, 1) & bg
    while True:
        n = dil(bgflood) & bg
        if (n == bgflood).all():
            break
        bgflood = n
    m = roi & ~cf["green"] & ~bgflood & ~cf["brown"]
    masks["props"] = keep_components(fill_holes(close(ero(m, 1), 2)), 400)
    hilt_f = poly_mask(HILT_ROI_FRONT, shape) & (cf["purple"] | cf["teal"])
    hilt_f = keep_components(fill_holes(close(hilt_f, 3)), 300)
    sheath_f = poly_mask(SHEATH_ROI_FRONT, shape) & (cf["blue"] | cf["gold"])
    sheath_f = keep_components(fill_holes(close(sheath_f, 3)), 300)
    shield_f = np.zeros(shape, bool)
    for p in SHIELD_FRONT_PARTS:
        shield_f |= poly_mask(p, shape)
    shield_f &= (cf["grey"] | cf["dark"]) & ~cf["white"]
    ears_f = np.zeros(shape, bool)
    ears_b = np.zeros(shape, bool)
    for roi in EAR_ROIS_FRONT:
        ears_f |= poly_mask(roi, shape) & skin(F)
    for roi in EAR_ROIS_BACK:
        ears_b |= poly_mask(roi, shape) & skin(B)
    ears_f = dil(keep_components(close(ears_f, 2), 60), 3)
    ears_b = dil(keep_components(close(ears_b, 2), 60), 3)
    clean_front = dil(masks["props"], 3) | dil(hilt_f, 5) | dil(sheath_f, 5) | dil(shield_f, 3) | ears_f
    clean_back = dil(masks["shield"], 6) | dil(masks["hilt"], 4) | dil(masks["sheath"], 4) | ears_b
    # zones effacées à reboucher avec le vert de la tunique uniquement
    masks["green_f"] = dil(masks["props"], 3) | dil(sheath_f, 5) | dil(shield_f, 3)
    masks["green_b"] = clean_back
    # silhouettes du corps seul (fond et accessoires retirés) pour caler le torse et la jupe
    masks["bg_f"], masks["bg_b"] = background(F), background(B)
    acc_f = cf["blue"] | cf["gold"] | cf["purple"] | cf["teal"] | masks["props"] | shield_f
    acc_b = cb["blue"] | cb["gold"] | cb["purple"] | cb["teal"] | masks["shield"]
    masks["sil_f"] = dil(ero(~dil(masks["bg_f"], 1) & ~acc_f, 3), 3)
    masks["sil_b"] = dil(ero(~dil(masks["bg_b"], 1) & ~acc_b, 3), 3)
    # --- contrôle visuel
    for tag, img, layers in (("back", B, [("shield", (1, 0, 0)), ("hilt", (1, 0, 1)), ("sheath", (1, 1, 0))]),
                             ("front", F, [("props", (0, 1, 1))])):
        o = img.copy()
        for k, col in layers:
            mk = masks[k]
            o[mk] = o[mk] * 0.45 + np.array(col) * 0.55
            o[mk & ~ero(mk, 2)] = col
        cm = clean_back if tag == "back" else clean_front
        o[cm & ~ero(cm, 2)] = (0, 0.3, 1)
        save_rgb(np.ascontiguousarray(o[:1100, 560:1460]), os.path.join(dbg, "seg_%s.png" % tag), "seg_" + tag)
    return masks, clean_front, clean_back


def build_atlas(F, B, known_f, known_b, name, green_f=None, green_b=None):
    f = push_pull(F, known_f)
    b = push_pull(B, known_b)
    if green_f is not None:  # rebouchage au vert de tunique là où l'on a effacé un accessoire
        f = np.where(green_f[..., None], push_pull(F, known_f & classes(F)["green"]), f)
        b = np.where(green_b[..., None], push_pull(B, known_b & classes(B)["green"]), b)
    h, w = f.shape[:2]
    atlas = np.zeros((ATLAS_H, ATLAS_W, 3), dtype=np.float32)
    atlas[:h, :w] = f
    atlas[:h, HALF:HALF + w] = b
    atlas[:h, w:HALF] = f[:, -1:]
    atlas[:h, HALF + w:] = b[:, -1:]
    atlas[h:] = atlas[h - 1:h]
    path = os.path.join(OUT, "T_%s_%s.png" % (CAP, name))
    img = save_rgb(atlas, path, "T_%s_%s" % (CAP, name))
    log("atlas", path)
    return img


# =============================================================================================================
# Géométrie
def piecewise(z, table):
    if z <= table[0][0]:
        return table[0][1]
    for (z0, v0), (z1, v1) in zip(table, table[1:]):
        if z <= z1:
            return v0 + (v1 - v0) * (z - z0) / (z1 - z0)
    return table[-1][1]


def category(name):
    n = name.lower()
    if n.startswith(("head", "neck_02")):
        return "head"
    if n.startswith("neck"):
        return "neck"
    if n.startswith(("upperarm", "lowerarm", "hand", "thumb", "index", "middle", "ring", "pinky")):
        return "arm"
    if n.startswith(("thigh", "calf", "foot", "ball")):
        return "leg"
    return "torso"


def shape_body(body):
    """Proportions du dessin : tunique plus large, tête plus grosse (yeux gardés à la même hauteur)."""
    mw = body.matrix_world
    mwi = mw.inverted()
    cats = [category(g.name) for g in body.vertex_groups]
    fx_table = [(0.80, 1.0), (0.90, 1.12), (1.02, 1.28), (1.18, 1.34), (1.36, 1.30), (1.46, 1.16), (1.54, 1.0)]
    for v in body.data.vertices:
        acc = {"head": 0.0, "neck": 0.0, "arm": 0.0, "leg": 0.0, "torso": 0.0}
        tot = 0.0
        for g in v.groups:
            acc[cats[g.group]] += g.weight
            tot += g.weight
        if tot <= 0:
            continue
        for k in acc:
            acc[k] /= tot
        p = mw @ v.co
        wt = acc["torso"] + 0.5 * acc["neck"]
        if wt > 0.01 and acc["arm"] < 0.6:
            fx = 1.0 + (piecewise(p.z, fx_table) - 1.0) * wt * (1.0 - acc["arm"])
            fy = 1.0 + (fx - 1.0) * 0.45
            p = Vector((p.x * fx, (p.y + 0.01) * fy - 0.01, p.z))
        if acc["head"] > 0.05:
            c = Vector((0.0, 0.0, 1.66))
            p = c + (p - c) * (1.0 + 0.16 * acc["head"])
        v.co = mwi @ p


def fit_torso(body, sil_f):
    """Recale la largeur du torse (entre la taille et les aisselles) sur la silhouette de face du dessin."""
    mw = body.matrix_world
    mwi = mw.inverted()
    cats = [category(g.name) for g in body.vertex_groups]
    zs = np.arange(1.06, 1.325, 0.01)
    target = drawn_extents(sil_f, zs)
    P, W = [], []
    for v in body.data.vertices:
        acc = {}
        for g in v.groups:
            acc[cats[g.group]] = acc.get(cats[g.group], 0) + g.weight
        tot = sum(acc.values()) or 1
        P.append(tuple(mw @ v.co))
        W.append((acc.get("torso", 0) + 0.5 * acc.get("neck", 0)) / tot if acc.get("arm", 0) / tot < 0.3 else 0.0)
    P, W = np.array(P), np.array(W)
    cur = []
    for z in zs:
        m = (np.abs(P[:, 2] - z) < 0.006) & (W > 0.5)
        cur.append((P[m, 0].min(), P[m, 0].max()) if m.any() else (-0.2, 0.2))
    cur = np.array(cur)
    fl = np.clip(target[:, 0] / cur[:, 0], 0.75, 1.35)
    fr = np.clip(target[:, 1] / cur[:, 1], 0.75, 1.35)
    k = np.ones(5) / 5
    fl = np.convolve(np.pad(fl, 2, mode="edge"), k, "valid")
    fr = np.convolve(np.pad(fr, 2, mode="edge"), k, "valid")
    for z, a, b in list(zip(zs, fl, fr))[::5]:
        log("torse z=%.2f facteur g=%.2f d=%.2f" % (z, a, b))
    for i, v in enumerate(body.data.vertices):
        if W[i] <= 0:
            continue
        p = Vector(P[i])
        blend = np.clip((p.z - 0.93) / 0.05, 0, 1) * np.clip((1.40 - p.z) / 0.08, 0, 1)
        if blend <= 0:
            continue
        f = np.interp(p.z, zs, fr if p.x > 0 else fl)
        p.x *= 1 + (f - 1) * W[i] * blend
        v.co = mwi @ p


def fit_skirt(sil_f, sil_b):
    """Rayons gauche/droite de la jupe d'après les silhouettes de face et de dos (le plus serré des deux)."""
    global SKIRT
    zs = [s[0] for s in SKIRT]
    ef = drawn_extents(sil_f, [max(z, 0.715) for z in zs])
    eb = drawn_extents(sil_b, [max(z, 0.715) for z in zs], back=True)
    new = []
    for (z, rl, rr, ry), (fl, fr), (bl, br) in zip(SKIRT, ef, eb):
        # jupe symétrique : l'ocarina, le masque ou le fourreau masquent parfois un bord → on écarte les mesures trop courtes
        vals = np.array([-fl, fr] + ([-bl, br] if z < 0.98 else []))
        vals = vals[vals > np.median(vals) - 0.02]
        r = float(np.clip(vals.mean() + 0.004, 0.17, 0.3))
        new.append((z, r, r, ry))
        log("jupe z=%.2f rx=%.3f (avant %.3f) mesures %s" % (z, r, rr, np.round(vals, 3)))
    SKIRT = new


def back_surface(body):
    mw = body.matrix_world
    ys = []
    for v in body.data.vertices:
        p = mw @ v.co
        if 1.0 < p.z < 1.5 and abs(p.x) < 0.22:
            ys.append(p.y)
    return max(ys)


def new_mesh_obj(name, bm, proj=P_BODY):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    at = me.attributes.new("proj", "INT", "FACE")
    at.data.foreach_set("value", [proj] * len(me.polygons))
    return o


def assign_groups(obj, weight_fn):
    for v in obj.data.vertices:
        for bone, w in weight_fn(obj.matrix_world @ v.co).items():
            if w > 0:
                g = obj.vertex_groups.get(bone) or obj.vertex_groups.new(name=bone)
                g.add([v.index], w, "REPLACE")


def orient_outward(bm, center):
    for f in bm.faces:
        if f.normal.dot(f.calc_center_median() - center) < 0:
            f.normal_flip()


def ring(bm, center, rx, ry, n):
    """Anneau elliptique ; rx peut être un couple (gauche, droite) pour une silhouette asymétrique."""
    rl, rr = rx if isinstance(rx, tuple) else (rx, rx)
    pts = []
    for k in range(n):
        c, s = math.cos(2 * math.pi * k / n), math.sin(2 * math.pi * k / n)
        pts.append(bm.verts.new((center.x + (rr if c > 0 else rl) * c, center.y + ry * s, center.z)))
    return pts


def bridge(bm, r0, r1):
    n = len(r0)
    for k in range(n):
        bm.faces.new((r0[k], r0[(k + 1) % n], r1[(k + 1) % n], r1[k]))


# (z, rayon gauche, rayon droit, profondeur) ; les rayons sont recalés sur le dessin par fit_skirt
SKIRT = [(1.06, 0.205, 0.205, 0.150), (0.98, 0.215, 0.215, 0.160), (0.90, 0.228, 0.228, 0.170), (0.82, 0.240, 0.240, 0.180),
         (0.74, 0.250, 0.250, 0.190), (0.69, 0.254, 0.254, 0.194)]


def skirt_front(X, Z):
    """Surface avant de la jupe de tunique (pour poser les objets pendus à la ceinture)."""
    zs = [s[0] for s in SKIRT][::-1]
    rl = np.interp(Z, zs, [s[1] for s in SKIRT][::-1])
    rr = np.interp(Z, zs, [s[2] for s in SKIRT][::-1])
    ry = np.interp(Z, zs, [s[-1] for s in SKIRT][::-1])
    rx = np.where(X > 0, rr, rl)
    return -0.005 - ry * np.sqrt(np.clip(1 - (X / rx) ** 2, 0, 1))


def skirt():
    bm = bmesh.new()
    rings = [ring(bm, Vector((0, -0.005, e[0])), (e[1], e[2]), e[-1], 40) for e in SKIRT]
    for r0, r1 in zip(rings, rings[1:]):
        bridge(bm, r0, r1)
    last = SKIRT[-1]
    inner = ring(bm, Vector((0, -0.005, last[0])), (last[1] - 0.02, last[2] - 0.02), last[-1] - 0.015, 40)
    bridge(bm, rings[-1], inner)
    bm.normal_update()
    for f in bm.faces:
        c = f.calc_center_median()
        if c.z > 0.695:
            if f.normal.dot(Vector((c.x, c.y + 0.005, 0))) < 0:
                f.normal_flip()
        elif f.normal.z > 0:
            f.normal_flip()
    o = new_mesh_obj("Skirt", bm)

    def w(p):
        t = max(0.0, min(1.0, (1.06 - p.z) / 0.37))
        leg = 0.55 * t * min(1.0, abs(p.x) / 0.06)
        return {"pelvis": 1.0 - leg, ("thigh_l" if p.x > 0 else "thigh_r"): leg}
    assign_groups(o, w)
    return o


def tube(points, radii, sides=14, squash=(1.0, 1.0), name="Tube", proj=P_BODY):
    bm = bmesh.new()
    rings = []
    for i, (p, r) in enumerate(zip(points, radii)):
        p = Vector(p)
        d = (Vector(points[min(i + 1, len(points) - 1)]) - Vector(points[max(i - 1, 0)])).normalized()
        up = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((0, 1, 0))
        a = d.cross(up).normalized()
        b = d.cross(a).normalized()
        rings.append([bm.verts.new(p + a * math.cos(2 * math.pi * k / sides) * r * squash[0] + b * math.sin(2 * math.pi * k / sides) * r * squash[1]) for k in range(sides)])
    for r0, r1 in zip(rings, rings[1:]):
        bridge(bm, r0, r1)
    bm.faces.new(tuple(reversed(rings[0])))
    bm.faces.new(tuple(rings[-1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_mesh_obj(name, bm, proj)


def dome(center, radii, name, rings=10, seg=32, back_extend=0.0, keep=None):
    """Calotte ellipsoïdale (cheveux, bonnet), prolongée vers la nuque ; keep(centre_face) filtre les faces."""
    bm = bmesh.new()
    top = bm.verts.new((center.x, center.y, center.z + radii[2]))
    rows = []
    for i in range(1, rings + 1):
        phi = (math.pi / 2) * (1 - i / rings)
        row = []
        for k in range(seg):
            th = 2 * math.pi * k / seg
            x, y, z = math.cos(phi) * math.cos(th), math.cos(phi) * math.sin(th), math.sin(phi)
            ext = back_extend * max(0.0, y) ** 1.5 * (i / rings)
            row.append(bm.verts.new((center.x + x * radii[0], center.y + y * radii[1], center.z + z * radii[2] - ext)))
        rows.append(row)
    for k in range(seg):
        bm.faces.new((top, rows[0][(k + 1) % seg], rows[0][k]))
    for r0, r1 in zip(rows, rows[1:]):
        for k in range(seg):
            bm.faces.new((r0[k], r0[(k + 1) % seg], r1[(k + 1) % seg], r1[k]))
    bm.normal_update()
    orient_outward(bm, center)
    if keep:
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if not keep(f.calc_center_median())], context="FACES")
    return new_mesh_obj(name, bm)


def smoothstep(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def head_weight(v, dl, cats):
    tot = sum(v[dl].values())
    if tot <= 0:
        return 0.0
    return sum(w for g, w in v[dl].items() if g < len(cats) and cats[g] == "head") / tot


def hair_shell(body, bg_f):
    """Cheveux et bonnet : la calotte du mannequin (sans la fenêtre du visage) dupliquée puis gonflée le long des normales
    jusqu'à la silhouette dessinée (largeur par hauteur, sommet par colonne), refermée sur le crâne. Elle garde le skinning."""
    mw = body.matrix_world
    mwi = mw.inverted()
    cats = [category(g.name) for g in body.vertex_groups]
    # silhouette dessinée de face : côté gauche de la planche (la poignée d'épée encombre le côté droit)
    def left_half(z):
        y = int(round(SOLE - z * PPM))
        xs = np.nonzero(~bg_f[y, int(CX) - 350:int(CX)])[0]
        return (350 - xs.min()) / PPM if len(xs) else 0.0
    global HEAD_WIDTHS
    zs = np.arange(1.56, 1.87, 0.01)
    ws = np.array([left_half(z) for z in zs])
    ws = np.array([np.median(ws[max(0, k - 2):k + 3]) for k in range(len(ws))])  # retire la pointe de l'oreille
    HEAD_WIDTHS = (zs, ws)
    def width_at(z):
        return float(np.interp(z, zs, ws))
    def top_at(x):
        col = ~bg_f[:500, int(round(CX - min(abs(x), 0.2) * PPM))]
        ys = np.nonzero(col)[0]
        return (SOLE - ys.min()) / PPM if len(ys) else 0.0
    bm = bmesh.new()
    bm.from_mesh(body.data)
    dl = bm.verts.layers.deform.active
    for v in bm.verts:
        v.co = mw @ v.co
    bm.normal_update()
    keep = []
    for f in bm.faces:
        if any(head_weight(v, dl, cats) < 0.55 for v in f.verts):
            continue
        c = f.calc_center_median()
        if c.z < 1.60:
            continue
        if c.y < HEAD_C.y - 0.035 and c.z < 1.705 and abs(c.x) < 0.078:
            continue  # fenêtre du visage
        keep.append(f)
    keep_set = set(keep)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f not in keep_set], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.normal_update()
    # décalage le long de la normale : côtés jusqu'à la largeur dessinée, dessus jusqu'au sommet, arrière = bonnet
    off = {}
    for v in bm.verts:
        n, p = v.normal, v.co
        W = width_at(p.z)
        d_side = min(0.06, max(0.0, (W - abs(p.x)) / max(abs(n.x), 0.35))) if W > 0 else 0.02
        d_top = min(0.05, max(0.0, (top_at(p.x) + 0.004 - p.z) / max(n.z, 0.35))) if n.z > 0 else 0.0
        d = n.x * n.x * d_side + max(n.z, 0.0) ** 2 * d_top + max(n.y, 0.0) ** 2 * 0.032 + max(-n.y, 0.0) ** 2 * 0.012 + min(n.z, 0.0) ** 2 * 0.015
        off[v] = max(d, 0.008)
    for _ in range(10):  # lissage du champ de décalage
        new = {}
        for v in bm.verts:
            nb = [e.other_vert(v) for e in v.link_edges]
            new[v] = 0.5 * off[v] + 0.5 * (sum(off[u] for u in nb) / len(nb) if nb else off[v])
        off = new
    orig = {v: v.co.copy() for v in bm.verts}
    for v in bm.verts:
        v.co = v.co + v.normal * off[v]
    # rebord : referme la coque sur le crâne le long de la fenêtre du visage et du bas
    boundary = [e for e in bm.edges if e.is_boundary]
    inner = {}
    for e in boundary:
        for v in e.verts:
            if v not in inner:
                nv = bm.verts.new(orig[v] + (v.co - orig[v]) * 0.15)
                for g, w in v[dl].items():
                    nv[dl][g] = w
                inner[v] = nv
    for e in boundary:
        a, b = e.verts
        try:
            bm.faces.new((a, b, inner[b], inner[a]))
        except ValueError:
            pass
    for v in bm.verts:
        v.co = mwi @ v.co
    me = bpy.data.meshes.new("HairShell")
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new("HairShell", me)
    bpy.context.collection.objects.link(o)
    o.matrix_world = body.matrix_world.copy()
    for g in body.vertex_groups:
        o.vertex_groups.new(name=g.name)
    at = me.attributes.get("proj") or me.attributes.new("proj", "INT", "FACE")
    at.data.foreach_set("value", [P_HAIR] * len(me.polygons))
    log("coque de cheveux", len(me.vertices), "sommets ; largeurs dessinées", np.round(ws[::5], 3))
    return o


def sample_side_colors(F, B):
    """Blond des mèches (planche de face, de part et d'autre du visage) et vert du bonnet (planche de dos), en linéaire."""
    global SIDE_COLORS
    def med(img, x0, x1, y0, y1):
        a = img[y0:y1, x0:x1].reshape(-1, 3)
        h, s_, v = hsv(a[None])[0][0], hsv(a[None])[1][0], hsv(a[None])[2][0]
        return a, h, s_, v
    a, h, s_, v = med(F, 900, 950, 70, 150)
    hair = a[(h > 30) & (h < 60) & (s_ > 0.3) & (v > 0.35)]
    b, h2, s2, v2 = med(B, 950, 1070, 40, 170)
    cap = b[(h2 > 70) & (h2 < 170) & (s2 > 0.15)]
    to_lin = lambda c: np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
    def bright_median(px, q):
        lum = px @ np.array([0.3, 0.59, 0.11])
        sel = px[lum >= np.percentile(lum, q)]
        return np.median(sel, 0)
    hc = to_lin(bright_median(hair, 55)) if len(hair) else np.array([0.55, 0.38, 0.12])
    cc = to_lin(np.median(cap, 0)) if len(cap) else np.array([0.10, 0.20, 0.06])
    SIDE_COLORS = (hc, cc)
    import json
    with open(os.path.join(OUT, "side_colors.json"), "w") as f:
        json.dump({"HairSide": [float(x) for x in hc], "CapSide": [float(x) for x in cc]}, f)
    log("couleurs des côtés de la tête : blond", np.round(hc, 3), "bonnet", np.round(cc, 3))


def side_color(p, n):
    """Couleur peinte des côtés de la tête : bonnet vert au-dessus d'une ligne qui descend vers l'arrière, mèches blondes dessous."""
    hc, cc = SIDE_COLORS
    t = min(1.0, max(0.0, (p.y + 0.13) / 0.26))
    zcap = 1.79 - 0.13 * t
    k = smoothstep(zcap - 0.006, zcap + 0.006, p.z)
    streak = 0.86 + 0.14 * math.sin(p.y * 240.0 + 10.0 * math.sin(p.z * 45.0)) * (0.6 + 0.4 * math.sin(p.z * 23.0 + p.y * 31.0))
    light = 0.82 + 0.3 * min(1.0, max(0.0, (p.z - 1.58) / 0.26)) + 0.08 * n.z
    bright = streak * light * (1 - k) + (0.9 + 0.15 * max(n.z, 0.0) + 0.05 * math.sin(p.y * 90.0)) * k
    return k, bright


def back_profile(body):
    """Profondeur du dos (x ≈ 0) par hauteur, pour poser le pan du bonnet sur la nuque."""
    mw = body.matrix_world
    pts = [mw @ v.co for v in body.data.vertices]
    prof = {}
    for p in pts:
        if abs(p.x) < 0.03 and 1.38 < p.z < 1.70:
            k = round(p.z, 2)
            prof[k] = max(prof.get(k, -1.0), p.y)
    ks = sorted(prof)
    return lambda z: float(np.interp(z, ks, [prof[k] for k in ks]))


def cap_flap(body, B):
    """Pan du bonnet qui retombe de l'arrière de la tête sur la nuque (texturé par la planche de dos)."""
    green = classes(B)["green"]
    def half(z):
        y = int(round(SOLE - z * PPM))
        row = green[y]
        if not row[int(CX) - 3:int(CX) + 4].any():
            return 0.0
        xl = xr = int(CX)
        while xl > int(CX) - 200 and row[max(0, xl - 6):xl].any():
            xl -= 1
        while xr < int(CX) + 200 and row[xr + 1:xr + 7].any():
            xr += 1
        return (xr - xl) * 0.5 / PPM
    back = back_profile(body)
    zs = [1.665, 1.62, 1.58, 1.54, 1.50, 1.47, 1.447]
    hw = []
    for z in zs:
        h = min(0.11, half(z))
        hw.append(min(h, hw[-1]) if hw else h)
    hw[-1] = 0.004
    pts = [(0.0, max(back(z) + 0.016, 0.10), z) for z in zs]
    o = tube(pts, [max(h, 0.004) for h in hw], 16, (1.0, 0.22), "CapFlap", P_BACK_ONLY)
    def w(p):
        t = min(1.0, max(0.0, (p.z - 1.52) / 0.1))
        return {"head": t, "neck_01": (1 - t) * 0.6, "spine_05": (1 - t) * 0.4}
    assign_groups(o, w)
    log("pan du bonnet : demi-largeurs", [round(h, 3) for h in hw])
    return o


def head_parts():
    """Mèches latérales et oreilles pointues, posées sur le crâne réel du mannequin."""
    parts = []
    for s in (-1, 1):
        parts.append(tube([(s * 0.098, -0.07, 1.705), (s * 0.108, -0.078, 1.625), (s * 0.104, -0.068, 1.545)],
                          [0.022, 0.019, 0.005], 10, (0.75, 1.0), "Lock%d" % s))
        parts.append(tube([(s * 0.092, -0.035, 1.645), (s * 0.126, -0.02, 1.666), (s * 0.162, 0.006, 1.693)],
                          [0.024, 0.016, 0.003], 8, (0.35, 1.0), "Ear%d" % s, P_EAR))
    for o in parts:
        assign_groups(o, lambda p: {"head": 1.0} if p.z > 1.58 else {"head": 0.75, "neck_02": 0.25})
    return parts


def slab(name, mask, src, profile, spacing=14.0, proj=P_GEAR, eps=1.1):
    """Volume bombé extrudé depuis une silhouette dessinée.
    src : 'back' ou 'front' (sens de lecture X de la planche) ; profile(X, Z, d_m) -> (y_avant, y_arriere)."""
    P = simplify_closed(contour(mask), eps)
    n = len(P)
    mn, mx = P.min(0), P.max(0)
    gx, gy = np.meshgrid(np.arange(mn[0] + spacing / 2, mx[0], spacing), np.arange(mn[1] + spacing / 2, mx[1], spacing))
    G = np.stack([gx.ravel(), gy.ravel()], 1)
    if len(G):
        G = G[points_in_poly(G, P)]
        G = G[dist_to_poly(G, P) > spacing * 0.6]
    pts = [Vector((float(x), float(y))) for x, y in P] + [Vector((float(x), float(y))) for x, y in G]
    res = delaunay_2d_cdt(pts, [(i, (i + 1) % n) for i in range(n)], [list(range(n))], 1, 1e-5)
    vco, tris = res[0], res[2]
    V = np.array([[v.x, v.y] for v in vco])
    D = dist_to_poly(V, P) / PPM
    X = (V[:, 0] - CX) / PPM if src == "front" else (CX - V[:, 0]) / PPM
    Z = (SOLE - V[:, 1]) / PPM
    yf, yb = profile(X, Z, D)
    bm = bmesh.new()
    fv = [bm.verts.new((X[i], yf[i], Z[i])) for i in range(len(V))]
    bv = [bm.verts.new((X[i], yb[i], Z[i])) for i in range(len(V))]
    edge_count = {}
    for t in tris:
        if len(t) != 3:
            continue
        a, b, c = t
        try:
            bm.faces.new((fv[a], fv[b], fv[c]))
            bm.faces.new((bv[c], bv[b], bv[a]))
        except ValueError:
            continue
        for e in ((a, b), (b, c), (c, a)):
            k = (min(e), max(e))
            edge_count[k] = edge_count.get(k, 0) + 1
    for (a, b), cnt in edge_count.items():
        if cnt == 1:
            try:
                bm.faces.new((fv[a], fv[b], bv[b], bv[a]))
            except ValueError:
                pass
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    log("volume", name, "contour", n, "sommets", len(bm.verts))
    return new_mesh_obj(name, bm, proj)


def pillow(d, half, edge=0.35, d0=0.012):
    return half * (edge + (1 - edge) * np.sqrt(np.clip(d / d0, 0, 1)))


def gear(masks, yb):
    """Bouclier, fourreau et poignée dans le dos, découpés dans la planche de dos."""
    ax = yb + 0.035  # axe de l'épée
    sy = yb + 0.092  # centre du bouclier
    Pm = np.argwhere(masks["shield"])
    cy_px, cx_px = Pm.mean(0)
    scx, scz = (CX - cx_px) / PPM, (SOLE - cy_px) / PPM

    def shield_prof(X, Z, D):
        mid = sy - 0.42 * ((X - scx) ** 2 + (Z - scz) ** 2)
        h = pillow(D, 0.011, 0.45, 0.01)
        return mid - h, mid + h

    def sheath_prof(X, Z, D):
        h = pillow(D, 0.021, 0.3, 0.02)
        return ax - h, ax + h

    def hilt_prof(X, Z, D):
        h = pillow(D, 0.017, 0.3, 0.012)
        return ax - h, ax + h
    shield = slab("Shield", masks["shield"], "back", shield_prof, 16)
    sheath = slab("Sheath", masks["sheath"], "back", sheath_prof, 12)
    hilt = slab("Hilt", masks["hilt"], "back", hilt_prof, 10)
    for o in (shield, sheath, hilt):
        assign_groups(o, lambda p: {"spine_05": 1.0})
    return [shield, sheath], hilt


def hip_props(masks):
    """Ocarina et masque Zora pendus à la ceinture (découpés dans la planche de face)."""
    def prof(X, Z, D):
        h = pillow(D, 0.016, 0.3, 0.015)
        mid = skirt_front(X, Z) - 0.012 - 0.016
        return mid - h, mid + h
    o = slab("HipProps", masks["props"], "front", prof, 10, P_FRONT_ONLY)
    assign_groups(o, lambda p: {"pelvis": 0.7, "thigh_l": 0.3})
    return o


# =============================================================================================================
# Pose T et projection
def align(arm, bone, child, target):
    pb = arm.pose.bones[bone]
    bpy.context.view_layer.update()
    mw = arm.matrix_world
    h = mw @ pb.head
    c = mw @ arm.pose.bones[child].head
    q = (c - h).normalized().rotation_difference(Vector(target))
    M = mw @ pb.matrix
    loc = M.to_translation()
    pb.matrix = mw.inverted() @ (Matrix.Translation(loc) @ q.to_matrix().to_4x4() @ Matrix.Translation(-loc) @ M)
    bpy.context.view_layer.update()


def t_pose(arm):
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    for s, sign in (("l", 1), ("r", -1)):
        align(arm, "upperarm_" + s, "lowerarm_" + s, (sign, 0, -0.02))
        align(arm, "lowerarm_" + s, "hand_" + s, (sign, 0, -0.02))
        align(arm, "hand_" + s, "middle_01_" + s, (sign, 0, -0.03))
    bpy.ops.object.mode_set(mode="OBJECT")


def rest_pose(arm):
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.context.view_layer.update()


def project_uvs(obj):
    """Deux projections par sommet (UVFront : planche de face, UVBack : planche de dos) et un poids « Blend » (couleur de
    sommet, R = part de la face) : le matériau les fond sur les flancs au lieu de basculer d'une planche à l'autre.
    Les surfaces vues de biais lisent la couleur un peu à l'intérieur du dessin (INSET) plutôt que sur le contour."""
    dg = bpy.context.evaluated_depsgraph_get()
    ev = obj.evaluated_get(dg)
    me = ev.to_mesh()
    mw = obj.matrix_world
    nm = mw.to_3x3().inverted().transposed()
    pos = [mw @ v.co for v in me.vertices]
    nrm = [(nm @ v.normal).normalized() for v in me.vertices]
    modes = [0] * len(me.polygons)
    if "proj" in me.attributes:
        me.attributes["proj"].data.foreach_get("value", modes)
    vmode = [None] * len(me.vertices)
    for poly in me.polygons:
        for vi in poly.vertices:
            if vmode[vi] is None:
                vmode[vi] = modes[poly.index]
    uvf, uvb, wgt, sides, scol = [], [], [], [], []
    for i, (p, n) in enumerate(zip(pos, nrm)):
        mode = vmode[i] or 0
        g = 1.0 - abs(n.y)
        # tête : cheveux et oreilles étroits (le visage est juste à côté) → décalage réduit ; les côtés de la tête
        # montrent les mèches blondes de la planche de face, la planche de dos ne sert qu'à l'arrière (bonnet)
        head = mode in (P_BODY, P_EAR, P_HAIR) and p.z > 1.56 and abs(p.x) < 0.25
        ps = p - Vector((n.x, 0.0, n.z)) * ((0.008 if head else INSET) * smoothstep(0.45, 0.95, g))
        # côtés des cheveux : couleur peinte (aucune planche ne les montre)
        side_w = smoothstep(0.5, 0.85, abs(n.x)) if mode == P_HAIR and SIDE_COLORS is not None else 0.0
        sides.append(side_w)
        # couleur définie sur toute la coque (sinon l'interpolation entre sommets tirerait vers une couleur neutre)
        scol.append(side_color(p, n) if mode == P_HAIR and SIDE_COLORS is not None else (0.0, 1.0))
        dx = FRONT_GEAR_DX if mode == P_GEAR else 0.0
        v = 1.0 - (SOLE - ps.z * PPM) / ATLAS_H
        uvf.append(((CX + ps.x * PPM + dx) / ATLAS_W, v))
        uvb.append(((HALF + CX - ps.x * PPM) / ATLAS_W, v))
        if mode == P_FRONT_ONLY:
            wgt.append(1.0)
        elif mode == P_BACK_ONLY:
            wgt.append(0.0)
        else:
            wgt.append(smoothstep(-0.6, -0.05, -n.y) if head else smoothstep(-0.28, 0.28, -n.y))
    loop_v = [l.vertex_index for l in me.loops]
    ev.to_mesh_clear()
    data = obj.data
    for other in list(data.uv_layers):
        data.uv_layers.remove(other)
    lf = data.uv_layers.new(name="UVFront")
    lb = data.uv_layers.new(name="UVBack")
    # Données dans des canaux UV (pas de souci de gamma) ; y stocké en 1 - valeur car l'import UE retourne V (V = 1 - V)
    ld = data.uv_layers.new(name="UVData")  # x = part de la planche de face, y = part de la couleur peinte (côtés)
    ls = data.uv_layers.new(name="UVSide")  # x = part du vert du bonnet (sinon blond), y = luminosité
    for li, vi in enumerate(loop_v):
        lf.data[li].uv = uvf[vi]
        lb.data[li].uv = uvb[vi]
        ld.data[li].uv = (wgt[vi], 1.0 - sides[vi])
        ls.data[li].uv = (scol[vi][0], 1.0 - scol[vi][1])
    data.uv_layers.active = lf
    for ca in list(data.color_attributes):
        data.color_attributes.remove(ca)
    log("projection", obj.name, "sommets", len(pos))


def textured_material(name, img):
    """Aperçu Blender (même logique que M_ZToonTex dans l'UE) : fondu face/dos, puis couleur peinte des côtés."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    L = nt.links
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    emi = nt.nodes.new("ShaderNodeEmission")
    def uvmap(name):
        u = nt.nodes.new("ShaderNodeUVMap")
        u.uv_map = name
        return u
    def mixc(fac, a, b):
        mx = nt.nodes.new("ShaderNodeMix")
        mx.data_type = "RGBA"
        L.new(fac, mx.inputs["Factor"])
        L.new(a, mx.inputs["A"])
        L.new(b, mx.inputs["B"])
        return mx.outputs["Result"]
    def one_minus(v):
        n = nt.nodes.new("ShaderNodeMath")
        n.operation = "SUBTRACT"
        n.inputs[0].default_value = 1.0
        L.new(v, n.inputs[1])
        return n.outputs[0]
    texs = []
    for uvn in ("UVBack", "UVFront"):
        t = nt.nodes.new("ShaderNodeTexImage")
        t.image = img
        L.new(uvmap(uvn).outputs["UV"], t.inputs["Vector"])
        texs.append(t.outputs["Color"])
    sd = nt.nodes.new("ShaderNodeSeparateXYZ")
    L.new(uvmap("UVData").outputs["UV"], sd.inputs["Vector"])
    ss = nt.nodes.new("ShaderNodeSeparateXYZ")
    L.new(uvmap("UVSide").outputs["UV"], ss.inputs["Vector"])
    hc, cc = SIDE_COLORS if SIDE_COLORS is not None else ((0.5, 0.35, 0.1), (0.1, 0.2, 0.06))
    hair = nt.nodes.new("ShaderNodeRGB")
    hair.outputs[0].default_value = (float(hc[0]), float(hc[1]), float(hc[2]), 1.0)
    cap = nt.nodes.new("ShaderNodeRGB")
    cap.outputs[0].default_value = (float(cc[0]), float(cc[1]), float(cc[2]), 1.0)
    painted = mixc(ss.outputs["X"], hair.outputs[0], cap.outputs[0])
    shade = nt.nodes.new("ShaderNodeMix")
    shade.data_type = "RGBA"
    shade.blend_type = "MULTIPLY"
    shade.inputs["Factor"].default_value = 1.0
    L.new(painted, shade.inputs["A"])
    comb = nt.nodes.new("ShaderNodeCombineXYZ")
    br = one_minus(ss.outputs["Y"])
    for k in ("X", "Y", "Z"):
        L.new(br, comb.inputs[k])
    L.new(comb.outputs[0], shade.inputs["B"])
    drawn = mixc(sd.outputs["X"], texs[0], texs[1])
    final = mixc(one_minus(sd.outputs["Y"]), drawn, shade.outputs["Result"])
    L.new(final, emi.inputs["Color"])
    L.new(emi.outputs["Emission"], out.inputs["Surface"])
    return m


def set_material(o, mat):
    o.data.materials.clear()
    o.data.materials.append(mat)


def join(target, others):
    bpy.ops.object.select_all(action="DESELECT")
    for o in others:
        o.select_set(True)
    target.select_set(True)
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.join()
    return target


def rig(o, arm):
    o.parent = arm
    o.matrix_parent_inverse = arm.matrix_world.inverted()
    mod = o.modifiers.new("Armature", "ARMATURE")
    mod.object = arm


def flatten_rig(empty, arm, meshes):
    """Supprime le nœud vide « SKM_Manny_Simple » (échelle 0,01) au-dessus de l'armature : sinon l'UE en fait un os
    racine supplémentaire et le maillage devient incompatible avec les Animation Blueprints du mannequin.
    Tout est converti en centimètres bruts (export sans conversion d'unités) : l'os « root » reste à l'échelle 1."""
    CM = Matrix.Scale(100.0, 4)
    for m in meshes:
        m.data.transform(CM @ m.matrix_world)
        m.parent = None
        m.matrix_world = Matrix.Identity(4)
    M = CM @ arm.matrix_world
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    for eb in arm.data.edit_bones:
        eb.transform(M, scale=True, roll=True)
    # os d'arme partagé avec Tools/blender/make_anims.py (même calcul) : l'épée suit la prise en main
    ebs = arm.data.edit_bones
    knuckle = (ebs["index_01_r"].head - ebs["pinky_01_r"].head).normalized()
    finger = (ebs["middle_01_r"].head - ebs["hand_r"].head).normalized()
    grip = (ebs["index_01_r"].head + ebs["pinky_01_r"].head) * 0.5 - finger * 1.5
    w = ebs.new("weapon_r")
    w.head, w.tail, w.roll = grip, grip + knuckle * 12.0, 0.0
    w.parent = ebs["hand_r"]
    w.use_deform = False
    bpy.ops.object.mode_set(mode="OBJECT")
    arm.parent = None
    arm.matrix_world = Matrix.Identity(4)
    for m in meshes:
        m.parent = arm
        m.matrix_parent_inverse = Matrix.Identity(4)
        for mod in m.modifiers:
            if mod.type == "ARMATURE":
                mod.object = arm
    bpy.data.objects.remove(empty)
    bpy.context.view_layer.update()


def export(objs, path):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.context.scene.unit_settings.scale_length = 0.01  # valeurs en cm : fichier FBX en cm, racine à l'échelle 1
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, add_leaf_bones=False, bake_anim=False, apply_unit_scale=True,
                             global_scale=1.0, apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE", colors_type="LINEAR",
                             use_armature_deform_only=False, path_mode="STRIP")
    log("export", path)


def preview(path):
    sc = bpy.context.scene
    for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            sc.render.engine = eng
            break
        except TypeError:
            continue
    sc.view_settings.view_transform = "Standard"
    sc.render.resolution_x = sc.render.resolution_y = 1000
    if not sc.world:
        sc.world = bpy.data.worlds.new("W")
    sc.world.color = (0.82, 0.86, 0.9)
    cam = bpy.data.objects.get("Cam")
    if not cam:
        cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
        sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type = "ORTHO"
    views = [((0, -5, 0.93), (90, 0, 0), 2.05), ((0, 5, 0.93), (90, 0, 180), 2.05), ((5, 0, 0.93), (90, 0, 90), 2.05), ((3.5, -3.5, 1.1), (84, 0, 45), 2.05),
             ((0, -5, 1.62), (90, 0, 0), 0.62), ((0, 5, 1.62), (90, 0, 180), 0.62), ((5, 0, 1.62), (90, 0, 90), 0.62), ((3.5, 3.5, 1.2), (80, 0, 135), 1.3)]
    for i, (loc, rot, scale) in enumerate(views):
        cam.data.ortho_scale = scale
        cam.location = loc
        cam.rotation_euler = [math.radians(a) for a in rot]
        sc.render.filepath = path.replace(".png", "_%d.png" % i)
        bpy.ops.render.render(write_still=True)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    F, B = load_rgb(FRONT), load_rgb(BACK)
    masks, clean_f, clean_b = segment(F, B, OUT)
    known_f, known_b = ~dil(masks["bg_f"], 2), ~dil(masks["bg_b"], 2)
    body_img = build_atlas(F, B, known_f & ~clean_f, known_b & ~clean_b, "Body", masks["green_f"], masks["green_b"])
    gear_img = build_atlas(F, B, known_f, known_b, "Gear")
    sample_side_colors(F, B)
    m_body = textured_material("M_%s_Body" % CAP, body_img)
    m_gear = textured_material("M_%s_Gear" % CAP, gear_img)

    bpy.ops.import_scene.fbx(filepath=MANNY)
    empty = [o for o in bpy.context.scene.objects if o.type == "EMPTY"][0]
    arm = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"][0]
    body = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
    shape_body(body)
    fit_torso(body, masks["sil_f"])
    fit_skirt(masks["sil_f"], masks["sil_b"])
    yb = back_surface(body)
    log("dos du torse y =", round(yb, 3))
    shell = hair_shell(body, masks["bg_f"])
    flap = cap_flap(body, B)
    body.data.attributes.new("proj", "INT", "FACE")
    set_material(body, m_body)
    parts = [skirt(), shell, flap] + head_parts()
    for p in parts:
        set_material(p, m_gear if p.name.startswith("Ear") else m_body)
    props = hip_props(masks)
    set_material(props, m_gear)
    body = join(body, parts + [props])
    back_gear, hilt = gear(masks, yb)
    gear_obj = join(back_gear[0], back_gear[1:])
    for o in (gear_obj, hilt):
        set_material(o, m_gear)
        rig(o, arm)
    for o in (body, gear_obj, hilt):
        bpy.ops.object.select_all(action="DESELECT")
        bpy.context.view_layer.objects.active = o
        o.select_set(True)
        bpy.ops.object.shade_smooth()
    t_pose(arm)
    for o in (body, gear_obj, hilt):
        project_uvs(o)
    preview(os.path.join(OUT, "preview_%s_tpose.png" % NAME))
    rest_pose(arm)
    preview(os.path.join(OUT, "preview_%s_rest.png" % NAME))
    body.name, gear_obj.name, hilt.name = "SKM_%s" % CAP, "SKM_%s_Gear" % CAP, "SKM_%s_Hilt" % CAP
    flatten_rig(empty, arm, [body, gear_obj, hilt])
    for o in (body, gear_obj, hilt):
        export([arm, o], os.path.join(OUT, o.name + ".fbx"))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "%s.blend" % NAME))
    log("TERMINE")


if __name__ == "__main__":
    configure(sys.argv[sys.argv.index("--") + 1:])
    main()
