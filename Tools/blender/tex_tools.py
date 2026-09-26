# Outils numpy pour la texture des personnages (sans OpenCV) : tramage de triangles en z-buffer, échantillonnage bilinéaire,
# flous, lecture/écriture d'images Blender. Utilisé par rig_model.py.
import numpy as np


def rasterize(p2, depth, tris, W, H, chunk=16000):
    if depth is None:
        return rasterize_flat(p2, tris, W, H, chunk)
    return _rasterize_z(p2, depth, tris, W, H, chunk)


def rasterize_flat(p2, tris, W, H, chunk=16000):
    """Tramage sans profondeur (espace UV : îlots sans recouvrement), mémoire bornée."""
    P = p2[tris].astype(np.float64)
    x0 = np.floor(P[:, :, 0].min(1) - 0.5).astype(np.int64)
    y0 = np.floor(P[:, :, 1].min(1) - 0.5).astype(np.int64)
    x1 = np.ceil(P[:, :, 0].max(1)).astype(np.int64)
    y1 = np.ceil(P[:, :, 1].max(1)).astype(np.int64)
    size = np.maximum(x1 - x0, y1 - y0) + 1
    tid = np.full(H * W, -1, np.int64)
    bary = np.zeros((H * W, 3), np.float32)
    prev = 0
    for k in (2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096):
        sel = np.nonzero((size > prev) & (size <= k))[0]
        prev = k
        if not len(sel):
            continue
        oy, ox = np.mgrid[0:k, 0:k]
        ox, oy = ox.ravel(), oy.ravel()
        step = max(1, chunk * 64 // (k * k))
        for s in range(0, len(sel), step):
            c = sel[s:s + step]
            px = x0[c][:, None] + ox[None, :]
            py = y0[c][:, None] + oy[None, :]
            cx, cy = px + 0.5, py + 0.5
            v0, v1, v2 = P[c, 0], P[c, 1], P[c, 2]
            den = (v1[:, 1] - v2[:, 1]) * (v0[:, 0] - v2[:, 0]) + (v2[:, 0] - v1[:, 0]) * (v0[:, 1] - v2[:, 1])
            good = np.abs(den) > 1e-12
            den = np.where(good, den, 1.0)
            l0 = ((v1[:, 1] - v2[:, 1])[:, None] * (cx - v2[:, 0][:, None]) + (v2[:, 0] - v1[:, 0])[:, None] * (cy - v2[:, 1][:, None])) / den[:, None]
            l1 = ((v2[:, 1] - v0[:, 1])[:, None] * (cx - v2[:, 0][:, None]) + (v0[:, 0] - v2[:, 0])[:, None] * (cy - v2[:, 1][:, None])) / den[:, None]
            l2 = 1.0 - l0 - l1
            inside = (l0 >= -1e-7) & (l1 >= -1e-7) & (l2 >= -1e-7) & (px >= 0) & (px < W) & (py >= 0) & (py < H) & good[:, None]
            if not inside.any():
                continue
            idx = (py * W + px)[inside]
            tid[idx] = np.broadcast_to(c[:, None], px.shape)[inside]
            bary[idx] = np.stack([l0[inside], l1[inside], l2[inside]], 1)
    return tid.reshape(H, W), bary.reshape(H, W, 3)


def _rasterize_z(p2, depth, tris, W, H, chunk=16000):
    """Tramage en z-buffer. p2 (N,2) coordonnées pixel (x vers la droite, y vers le bas), depth (N,) : plus petit = plus
    proche, tris (T,3). Renvoie tid (H,W) indice de triangle (-1 = vide) et bary (H,W,3)."""
    P = p2[tris].astype(np.float64)
    D = depth[tris].astype(np.float64)
    x0 = np.floor(P[:, :, 0].min(1) - 0.5).astype(np.int64)
    y0 = np.floor(P[:, :, 1].min(1) - 0.5).astype(np.int64)
    x1 = np.ceil(P[:, :, 0].max(1)).astype(np.int64)
    y1 = np.ceil(P[:, :, 1].max(1)).astype(np.int64)
    size = np.maximum(x1 - x0, y1 - y0) + 1
    ok = (x1 >= 0) & (y1 >= 0) & (x0 < W) & (y0 < H)
    cand_i, cand_z, cand_t, cand_b = [], [], [], []
    prev = 0
    for k in (2, 4, 8, 16, 32, 64, 128, 256, 512, 1024):
        sel = np.nonzero(ok & (size > prev) & (size <= k))[0]
        prev = k
        if not len(sel):
            continue
        oy, ox = np.mgrid[0:k, 0:k]
        ox, oy = ox.ravel(), oy.ravel()
        step = max(1, chunk * 64 // (k * k))
        for s in range(0, len(sel), step):
            c = sel[s:s + step]
            px = x0[c][:, None] + ox[None, :]
            py = y0[c][:, None] + oy[None, :]
            cx, cy = px + 0.5, py + 0.5
            v0, v1, v2 = P[c, 0], P[c, 1], P[c, 2]
            den = (v1[:, 1] - v2[:, 1]) * (v0[:, 0] - v2[:, 0]) + (v2[:, 0] - v1[:, 0]) * (v0[:, 1] - v2[:, 1])
            good = np.abs(den) > 1e-12
            den = np.where(good, den, 1.0)
            l0 = ((v1[:, 1] - v2[:, 1])[:, None] * (cx - v2[:, 0][:, None]) + (v2[:, 0] - v1[:, 0])[:, None] * (cy - v2[:, 1][:, None])) / den[:, None]
            l1 = ((v2[:, 1] - v0[:, 1])[:, None] * (cx - v2[:, 0][:, None]) + (v0[:, 0] - v2[:, 0])[:, None] * (cy - v2[:, 1][:, None])) / den[:, None]
            l2 = 1.0 - l0 - l1
            inside = (l0 >= -1e-7) & (l1 >= -1e-7) & (l2 >= -1e-7) & (px >= 0) & (px < W) & (py >= 0) & (py < H) & good[:, None]
            if not inside.any():
                continue
            z = l0 * D[c, 0][:, None] + l1 * D[c, 1][:, None] + l2 * D[c, 2][:, None]
            cand_i.append((py * W + px)[inside])
            cand_z.append(z[inside])
            cand_t.append(np.broadcast_to(c[:, None], px.shape)[inside])
            cand_b.append(np.stack([l0[inside], l1[inside], l2[inside]], 1).astype(np.float32))
    tid = np.full(H * W, -1, np.int64)
    bary = np.zeros((H * W, 3), np.float32)
    if cand_i:
        ci, cz, ct, cb = (np.concatenate(a) for a in (cand_i, cand_z, cand_t, cand_b))
        order = np.lexsort((cz, ci))
        ci, ct, cb = ci[order], ct[order], cb[order]
        first = np.ones(len(ci), bool)
        first[1:] = ci[1:] != ci[:-1]
        tid[ci[first]] = ct[first]
        bary[ci[first]] = cb[first]
    return tid.reshape(H, W), bary.reshape(H, W, 3)


def sample(img, x, y):
    """Échantillonnage bilinéaire d'une image (H,W,C) aux coordonnées pixel x, y (centres à +0.5), bords étendus."""
    H, W = img.shape[:2]
    fx = np.clip(x - 0.5, 0, W - 1.0001)
    fy = np.clip(y - 0.5, 0, H - 1.0001)
    ix, iy = np.floor(fx).astype(np.int64), np.floor(fy).astype(np.int64)
    ax, ay = (fx - ix)[..., None], (fy - iy)[..., None]
    a = img[iy, ix] * (1 - ax) + img[iy, ix + 1] * ax
    b = img[iy + 1, ix] * (1 - ax) + img[iy + 1, ix + 1] * ax
    return a * (1 - ay) + b * ay


def box_blur(a, r):
    """Flou boîte séparable de rayon r (sommes cumulées), bords répliqués ; a (H,W) ou (H,W,C)."""
    if r <= 0:
        return a
    for axis in (0, 1):
        pad = [(0, 0)] * a.ndim
        pad[axis] = (r + 1, r)
        p = np.pad(a, pad, mode="edge")
        c = np.cumsum(p, axis=axis, dtype=np.float64)
        n = a.shape[axis]
        hi = np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis)
        lo = np.take(c, np.arange(0, n), axis=axis)
        a = ((hi - lo) / (2 * r + 1)).astype(np.float32)
    return a


def gauss(a, sigma):
    """Approximation gaussienne (trois flous boîte)."""
    r = max(1, int(round(sigma * 0.8)))
    for _ in range(3):
        a = box_blur(a, r)
    return a


def masked_blur(a, m, sigma):
    """Flou normalisé qui ignore les pixels hors masque (a (H,W,C), m (H,W) booléen ou poids)."""
    w = m.astype(np.float32)
    num = gauss(a * w[..., None], sigma) if a.ndim == 3 else gauss(a * w, sigma)
    den = gauss(w, sigma)
    den = np.maximum(den, 1e-6)
    return num / (den[..., None] if a.ndim == 3 else den), den


def image_to_array(img):
    """Image Blender → tableau (H,W,3) float32, lignes de HAUT en BAS."""
    w, h = img.size
    a = np.zeros(w * h * 4, np.float32)
    img.pixels.foreach_get(a)
    return np.ascontiguousarray(a.reshape(h, w, 4)[::-1, :, :3])


def array_to_image(bpy, a, name, path=None):
    h, w = a.shape[:2]
    img = bpy.data.images.get(name)
    if img is None or tuple(img.size) != (w, h):
        if img is not None:
            bpy.data.images.remove(img)
        img = bpy.data.images.new(name, w, h, alpha=False)
    rgba = np.concatenate([np.clip(a, 0, 1), np.ones((h, w, 1), np.float32)], axis=2)[::-1]
    img.pixels.foreach_set(np.ascontiguousarray(rgba, dtype=np.float32).ravel())
    if path:
        img.filepath_raw = path
        img.file_format = "PNG"
        img.save()
    return img


# ---------------------------------------------------------------------------------------------------------------
# Recalage dense (flot optique pyramidal de type Lucas-Kanade, champ lissé) : sert à caler les planches dessinées sur
# la texture du modèle 3D, qui les reproduit avec des décalages (visage, sangles, ceinture).
def _down(a):
    h, w = a.shape[:2]
    a = a[:h - h % 2, :w - w % 2]
    return 0.25 * (a[0::2, 0::2] + a[1::2, 0::2] + a[0::2, 1::2] + a[1::2, 1::2])


def _up_flow(f, shape):
    h, w = shape
    g = np.repeat(np.repeat(f, 2, axis=0), 2, axis=1) * 2.0
    out = np.zeros((h, w, 2), np.float32)
    hh, ww = min(h, g.shape[0]), min(w, g.shape[1])
    out[:hh, :ww] = g[:hh, :ww]
    if hh < h:
        out[hh:] = out[hh - 1:hh]
    if ww < w:
        out[:, ww:] = out[:, ww - 1:ww]
    return out


def normalize_local(a, m, sigma):
    """Contraste local normalisé par canal dans le masque (atténue les écarts de style entre dessin et texture)."""
    mu, _ = masked_blur(a, m, sigma)
    d = (a - mu) * m[..., None]
    var, _ = masked_blur(d * d, m, sigma)
    return d / np.sqrt(var + 1e-3), mu


def flow_lk(A, B, mA, mB, levels=5, iters=8, win=2.0, smooth=2.5, norm_sigma=12.0, max_flow=40.0, robust=2.0, log=None):
    """Flot u (H,W,2) tel que B(p + u(p)) ≈ A(p) sur le masque mA. A, B (H,W,C) ; mA, mB (H,W) booléens."""
    An, _ = normalize_local(A, mA, norm_sigma)
    Bn, _ = normalize_local(B, mB, norm_sigma)
    pyrA, pyrB, pyrM = [An], [Bn], [mA.astype(np.float32)]
    for _ in range(levels - 1):
        pyrA.append(_down(pyrA[-1]))
        pyrB.append(_down(pyrB[-1]))
        pyrM.append(_down(pyrM[-1]))
    flow = None
    for lvl in range(levels - 1, -1, -1):
        a, b, m = pyrA[lvl], pyrB[lvl], pyrM[lvl]
        h, w = a.shape[:2]
        flow = np.zeros((h, w, 2), np.float32) if flow is None else _up_flow(flow, (h, w))
        yy, xx = np.mgrid[0:h, 0:w].astype(np.float32) + 0.5
        for _ in range(iters):
            bw = sample(b, xx + flow[..., 0], yy + flow[..., 1])
            gx = np.zeros_like(bw); gy = np.zeros_like(bw)
            gx[:, 1:-1] = 0.5 * (bw[:, 2:] - bw[:, :-2])
            gy[1:-1] = 0.5 * (bw[2:] - bw[:-2])
            it = (bw - a) * m[..., None]
            # pondération robuste : là où dessin et texture diffèrent vraiment (autre contenu), on laisse le lissage décider
            r2 = (it * it).sum(-1)
            k2 = max(float(np.median(r2[m > 0.5])) * robust, 1e-6) if (m > 0.5).any() else 1.0
            rho = 1.0 / (1.0 + r2 / k2)
            conf = m * rho
            sw = np.sqrt(rho)[..., None]
            it *= sw
            gx *= m[..., None] * sw; gy *= m[..., None] * sw
            sxx = gauss((gx * gx).sum(-1), win); syy = gauss((gy * gy).sum(-1), win); sxy = gauss((gx * gy).sum(-1), win)
            sxt = gauss((gx * it).sum(-1), win); syt = gauss((gy * it).sum(-1), win)
            lam = 1e-2 + 0.05 * (sxx + syy).mean()
            det = (sxx + lam) * (syy + lam) - sxy * sxy
            du = -((syy + lam) * sxt - sxy * syt) / det
            dv = -((sxx + lam) * syt - sxy * sxt) / det
            flow[..., 0] += np.clip(du, -1.0, 1.0)
            flow[..., 1] += np.clip(dv, -1.0, 1.0)
            # régularisation : lissage normalisé sur le masque, extrapolation douce hors masque
            sm, den = masked_blur(flow, np.maximum(conf, 1e-3), smooth)
            flow = sm
            lim = max_flow / (2 ** lvl)
            mag = np.sqrt((flow ** 2).sum(-1, keepdims=True))
            flow = flow * np.minimum(1.0, lim / np.maximum(mag, 1e-6))
        if log:
            bw = sample(b, xx + flow[..., 0], yy + flow[..., 1])
            err = (np.abs(bw - a).sum(-1) * m).sum() / max(m.sum(), 1)
            err0 = (np.abs(b - a).sum(-1) * m).sum() / max(m.sum(), 1)
            log("niveau %d (%dx%d) : écart %.3f → %.3f, flot moyen %.1f px" % (lvl, w, h, err0, err, float(np.sqrt((flow ** 2).sum(-1))[m > 0.5].mean()) * 2 ** lvl))
    return flow


def erode(m, r):
    for _ in range(r):
        p = np.pad(m, 1, constant_values=False)
        m = p[1:-1, 1:-1] & p[:-2, 1:-1] & p[2:, 1:-1] & p[1:-1, :-2] & p[1:-1, 2:]
    return m


def dilate(m, r):
    for _ in range(r):
        p = np.pad(m, 1, constant_values=False)
        m = p[1:-1, 1:-1] | p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:]
    return m
