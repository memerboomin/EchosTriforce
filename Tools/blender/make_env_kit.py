# Kit d'habillage de la citerne (végétation, bannière, arcatures, ruines), généré par script puis exporté en FBX.
# Lancer : Blender -b -P Tools/blender/make_env_kit.py -- <dossier_sortie>
# Conventions : mètres dans Blender (→ cm dans Unreal), avant = +X (face visible des pièces murales), haut = +Z, pivot au sol.
# Emplacements de matériaux : Grass, Leaf, Cloth, Metal, Body (pierre claire), Dark (pierre sombre), Glyph, Lily.
import bpy, bmesh, math, os, sys, random
from mathutils import Vector, Matrix

OUT = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else os.path.join(os.path.dirname(__file__), "..", "art", "fbx")
os.makedirs(OUT, exist_ok=True)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


class Builder:
    """Accumule des faces dans un bmesh avec UV et emplacements de matériaux nommés."""

    def __init__(self):
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UV")
        self.slots = []
        self.closed = []  # faces de volumes fermés : normales recalculées à l'export

    def slot(self, name):
        if name not in self.slots:
            self.slots.append(name)
        return self.slots.index(name)

    def face(self, pts, uvs, mat, closed=False, want=None):
        """want : direction vers laquelle la face doit regarder (faces ouvertes)."""
        pts = [Vector(p) for p in pts]
        if want is not None:
            n = Vector((0, 0, 0))
            for i in range(len(pts)):
                a, c = pts[i], pts[(i + 1) % len(pts)]
                n += Vector(((a.y - c.y) * (a.z + c.z), (a.z - c.z) * (a.x + c.x), (a.x - c.x) * (a.y + c.y)))
            if n.dot(Vector(want)) < 0:
                pts, uvs = pts[::-1], list(uvs)[::-1]
        vs = [self.bm.verts.new(p) for p in pts]
        f = self.bm.faces.new(vs)
        f.material_index = self.slot(mat)
        for l, uv in zip(f.loops, uvs):
            l[self.uv].uv = uv
        if closed:
            self.closed.append(f)
        return f

    def box(self, center, size, mat="Body", rot=0.0):
        c = Vector(center)
        hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        R = Matrix.Rotation(rot, 3, "Z")
        corners = [c + R @ Vector((sx * hx, sy * hy, sz * hz)) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
        idx = [(0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)]
        for q in idx:
            self.face([corners[i] for i in q], [(0, 0), (1, 0), (1, 1), (0, 1)], mat, closed=True)

    def sweep(self, path, profile, mat="Body", closed_profile=True, caps=True):
        """Balayage d'un profil 2D (dans le plan normal au chemin) le long d'une polyligne."""
        rings = []
        n = len(path)
        for i, p in enumerate(path):
            p = Vector(p)
            d = (Vector(path[min(i + 1, n - 1)]) - Vector(path[max(i - 1, 0)])).normalized()
            side = Vector((0, 1, 0)) if abs(d.y) < 0.9 else Vector((1, 0, 0))
            a = d.cross(side).normalized()
            b = d.cross(a).normalized()
            rings.append([p + a * u + b * v for (u, v) in profile])
        m = len(profile)
        for i in range(n - 1):
            for k in range(m if closed_profile else m - 1):
                k2 = (k + 1) % m
                self.face([rings[i][k], rings[i][k2], rings[i + 1][k2], rings[i + 1][k]],
                          [(k / m, i / n), ((k + 1) / m, i / n), ((k + 1) / m, (i + 1) / n), (k / m, (i + 1) / n)], mat, closed=True)
        if caps and closed_profile:
            for r in (rings[0], rings[-1]):
                self.face(r, [(0.5, 0.5)] * m, mat, closed=True)

    def export(self, name, smooth=False):
        if self.closed:
            bmesh.ops.recalc_face_normals(self.bm, faces=self.closed)
        me = bpy.data.meshes.new(name)
        self.bm.to_mesh(me)
        self.bm.free()
        for s in self.slots:
            me.materials.append(bpy.data.materials.get(s) or bpy.data.materials.new(s))
        o = bpy.data.objects.new(name, me)
        bpy.context.collection.objects.link(o)
        if smooth:
            for p in me.polygons:
                p.use_smooth = True
        bpy.ops.object.select_all(action="DESELECT")
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        path = os.path.join(OUT, name + ".fbx")
        bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_scale_options="FBX_SCALE_UNITS",
                                 axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", add_leaf_bones=False,
                                 bake_space_transform=True)
        print("EXPORT", path, len(me.polygons), "faces", self.slots)


# ---------------------------------------------------------------------------------------------------------------
def blade(b, base, height, width, lean, yaw, rnd, mat="Grass", segs=4):
    """Brin effilé et courbé ; v = 0 au pied, 1 à la pointe (dégradé de couleur dans le matériau)."""
    pts_l, pts_r = [], []
    side = Vector((math.cos(yaw + math.pi / 2), math.sin(yaw + math.pi / 2), 0))
    out = Vector((math.cos(yaw), math.sin(yaw), 0))
    for i in range(segs + 1):
        t = i / segs
        c = Vector(base) + out * (lean * t * t) + Vector((0, 0, height * t))
        w = width * (1 - t) ** 0.8 * 0.5 + 0.0005
        pts_l.append(c - side * w)
        pts_r.append(c + side * w)
    for i in range(segs):
        v0, v1 = i / segs, (i + 1) / segs
        b.face([pts_l[i], pts_r[i], pts_r[i + 1], pts_l[i + 1]], [(0, v0), (1, v0), (1, v1), (0, v1)], mat)


def grass_tuft(name, radius, count, hmin, hmax, seed):
    reset()
    rnd = random.Random(seed)
    b = Builder()
    for _ in range(count):
        r = radius * math.sqrt(rnd.random())
        a = rnd.uniform(0, 2 * math.pi)
        base = (r * math.cos(a), r * math.sin(a), -0.02)
        h = rnd.uniform(hmin, hmax) * (1.0 - 0.35 * r / radius)
        blade(b, base, h, rnd.uniform(0.012, 0.022), rnd.uniform(0.05, 0.16) * h / hmax, a + rnd.uniform(-0.6, 0.6), rnd)
    b.export(name)


def leaf(b, center, size, normal_yaw, droop, rnd, mat="Leaf"):
    """Feuille en losange légèrement pliée (géométrie, sans transparence)."""
    c = Vector(center)
    f = Vector((math.cos(normal_yaw), math.sin(normal_yaw), 0))  # face vers l'avant
    s = Vector((-f.y, f.x, 0))
    down = Vector((0, 0, -1))
    tip = c + down * size * 0.9 + f * droop * size + s * rnd.uniform(-0.3, 0.3) * size
    top = c + down * -size * 0.25
    left = c + s * size * 0.45 + f * size * 0.12 + down * size * 0.3
    right = c - s * size * 0.45 + f * size * 0.12 + down * size * 0.3
    b.face([top, left, tip, right], [(0.5, 1), (0, 0.5), (0.5, 0), (1, 0.5)], mat)


def ivy(name, width, drop, seed):
    """Lierre qui retombe du haut d'un mur (pivot au sommet, face visible vers +X)."""
    reset()
    rnd = random.Random(seed)
    b = Builder()
    vines = int(width / 0.065)
    for i in range(vines):
        y = -width / 2 + width * (i + rnd.random()) / vines
        length = drop * rnd.uniform(0.25, 1.0) ** 0.7
        z = 0.0
        x = 0.02
        path = []
        while z > -length:
            path.append((x, y, z))
            z -= 0.042
            y += rnd.uniform(-0.02, 0.02)
            x = 0.02 + 0.03 * rnd.random()
        for k, p in enumerate(path):
            for _ in range(2 if rnd.random() < 0.5 else 1):
                sz = rnd.uniform(0.06, 0.1) * (1.0 - 0.4 * k / max(1, len(path)))
                leaf(b, (p[0] + rnd.uniform(0.0, 0.05), p[1] + rnd.uniform(-0.05, 0.05), p[2] + rnd.uniform(-0.02, 0.02)), sz, rnd.uniform(-0.6, 0.6), rnd.uniform(0.2, 0.7), rnd)
    # coussin de feuilles sur le couronnement
    for _ in range(int(width * 90)):
        y = rnd.uniform(-width / 2, width / 2)
        leaf(b, (rnd.uniform(-0.15, 0.08), y, rnd.uniform(0.0, 0.06)), rnd.uniform(0.05, 0.08), rnd.uniform(-1, 1), 0.8, rnd)
    b.export(name)


def banner():
    """Bannière de tissu à glyphe d'eau (1,5 m × 3,2 m), tringle dorée ; tissu tourné vers +X, pivot en bas."""
    reset()
    b = Builder()
    W, H, top = 1.5, 3.2, 3.55
    nx, nz = 12, 28
    def pos(u, v):
        y = (u - 0.5) * W
        z = top - (1 - v) * H
        if v < 0.15:  # pointe en bas (forme de fanion)
            z -= (1 - abs(u - 0.5) * 2) * 0.35 * (1 - v / 0.15)
        x = 0.025 * math.sin(u * math.pi * 3.0 + v * 1.7) * (0.4 + 0.6 * (1 - v)) + 0.01  # légers plis
        return Vector((x, y, z))
    grid = [[pos(i / nx, j / nz) for j in range(nz + 1)] for i in range(nx + 1)]
    for i in range(nx):
        for j in range(nz):
            q = [grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]]
            b.face(q, [(i / nx, j / nz), ((i + 1) / nx, j / nz), ((i + 1) / nx, (j + 1) / nz), (i / nx, (j + 1) / nz)], "Cloth")
    # tringle et embouts
    b.sweep([(0.03, -W / 2 - 0.12, top + 0.03), (0.03, W / 2 + 0.12, top + 0.03)],
            [(0.025 * math.cos(a), 0.025 * math.sin(a)) for a in [2 * math.pi * k / 8 for k in range(8)]], "Metal")
    for s in (-1, 1):
        b.box((0.03, s * (W / 2 + 0.15), top + 0.03), (0.07, 0.07, 0.07), "Metal")
    b.export("SM_Kit_BannerCloth")


def arch_profile(w, d):
    return [(-w / 2, 0), (w / 2, 0), (w / 2, d), (-w / 2, d)]


def arcade():
    """Arcature aveugle en relief (4 m de large) : pilastres, arc en plein cintre mouluré, fond sombre, glyphe."""
    reset()
    b = Builder()
    W, spring, R = 4.0, 3.3, 1.35
    depth = 0.22
    for s in (-1, 1):
        y = s * (R + 0.2)
        b.box((depth / 2, y, 0.15), (depth + 0.1, 0.62, 0.3), "Body")        # socle
        b.box((depth / 2, y, spring / 2 + 0.15), (depth, 0.4, spring - 0.3), "Body")  # pilastre
        b.box((depth / 2 + 0.03, y, spring), (depth + 0.12, 0.56, 0.18), "Body")      # imposte
    # arc : balayage d'un profil rectangulaire sur un demi-cercle (plan YZ)
    path = [(0.0, math.cos(a) * (R + 0.2), spring + math.sin(a) * (R + 0.2)) for a in [math.pi * k / 24 for k in range(25)]]
    prof = [(-0.2, 0.0), (0.2, 0.0), (0.2, depth + 0.06), (-0.2, depth + 0.06)]
    rings = []
    for p in path:
        p = Vector(p)
        radial = Vector((0, p.y, p.z - spring)).normalized()
        rings.append([p + radial * u + Vector((v, 0, 0)) for (u, v) in prof])
    for i in range(len(rings) - 1):
        for k in range(4):
            k2 = (k + 1) % 4
            b.face([rings[i][k], rings[i][k2], rings[i + 1][k2], rings[i + 1][k]], [(0, 0), (1, 0), (1, 1), (0, 1)], "Body", closed=True)
    for r in (rings[0], rings[-1]):
        b.face(r, [(0, 0), (1, 0), (1, 1), (0, 1)], "Body", closed=True)
    # clé de voûte
    b.box((depth / 2 + 0.05, 0, spring + R + 0.25), (depth + 0.18, 0.34, 0.5), "Body")
    # fond de niche sombre (légèrement en saillie pour éviter le z-fighting avec le mur)
    n = 24
    pts = [Vector((0.012, math.cos(math.pi * k / n) * R, spring + math.sin(math.pi * k / n) * R)) for k in range(n + 1)]
    pts = [Vector((0.012, R, 0.3))] + pts + [Vector((0.012, -R, 0.3))]
    b.face(pts, [((p.y + R) / (2 * R), (p.z) / (spring + R)) for p in pts], "Dark", want=(1, 0, 0))
    # glyphe d'eau stylisé : goutte + deux vagues
    drop = []
    for k in range(20):
        a = 2 * math.pi * k / 20
        r = 0.28
        y = math.sin(a) * r * (0.75 if math.cos(a) > 0 else 1.0)
        z = -math.cos(a) * r
        if math.cos(a) > 0.2:  # pointe vers le haut
            y *= (1 - (math.cos(a) - 0.2) / 0.8) * 0.9 + 0.1
            z *= 1.6
        drop.append(Vector((0.03, y, spring + 0.55 + z)))
    b.face(drop, [(0.5, 0.5)] * len(drop), "Glyph", want=(1, 0, 0))
    for i, zz in enumerate((spring - 0.25, spring - 0.5)):
        path = [(0.03, -0.45 + 0.9 * t / 16, zz + 0.05 * math.sin(t / 16 * 2 * math.pi * 1.5)) for t in range(17)]
        for t in range(16):
            p0, p1 = Vector(path[t]), Vector(path[t + 1])
            b.face([p0 + Vector((0, 0, -0.03)), p1 + Vector((0, 0, -0.03)), p1 + Vector((0, 0, 0.03)), p0 + Vector((0, 0, 0.03))],
                   [(0, 0), (1, 0), (1, 1), (0, 1)], "Glyph", want=(1, 0, 0))
    b.export("SM_Kit_Arcade")


def jag_top(b, radius, h0, seed, sides=16, mat="Body"):
    rnd = random.Random(seed)
    ring_lo = [Vector((math.cos(2 * math.pi * k / sides) * radius, math.sin(2 * math.pi * k / sides) * radius, 0)) for k in range(sides)]
    tops = [h0 + rnd.uniform(-0.35, 0.35) for _ in range(sides)]
    ring_hi = [Vector((p.x, p.y, t)) for p, t in zip(ring_lo, tops)]
    for k in range(sides):
        k2 = (k + 1) % sides
        b.face([ring_lo[k], ring_lo[k2], ring_hi[k2], ring_hi[k]], [(k / sides, 0), ((k + 1) / sides, 0), ((k + 1) / sides, 1), (k / sides, 1)], mat, closed=True)
    c = Vector((0, 0, h0 - 0.1))
    for k in range(sides):
        k2 = (k + 1) % sides
        b.face([ring_hi[k], ring_hi[k2], c], [(0, 0), (1, 0), (0.5, 1)], mat, closed=True)
    b.face(ring_lo[::-1], [(0.5, 0.5)] * sides, mat, closed=True)


def broken_column():
    reset()
    b = Builder()
    b.box((0, 0, 0.2), (1.5, 1.5, 0.4), "Body")
    b.box((0, 0, 0.47), (1.2, 1.2, 0.14), "Body")
    jag_top(b, 0.52, 1.7, 7)
    rnd = random.Random(3)
    for k in range(5):
        a = rnd.uniform(0, 2 * math.pi)
        d = rnd.uniform(1.0, 1.8)
        s = rnd.uniform(0.25, 0.55)
        b.box((math.cos(a) * d, math.sin(a) * d, s * 0.35), (s * 1.3, s, s * 0.7), "Body", rot=rnd.uniform(0, 3))
    # tambour tombé
    b.sweep([(1.3, -0.9, 0.5), (2.2, 0.3, 0.5)], [(0.5 * math.cos(2 * math.pi * k / 16), 0.5 * math.sin(2 * math.pi * k / 16)) for k in range(16)], "Body")
    b.export("SM_Kit_ColumnBroken")


def rubble():
    reset()
    b = Builder()
    rnd = random.Random(21)
    for k in range(9):
        a = rnd.uniform(0, 2 * math.pi)
        d = rnd.uniform(0.0, 1.1)
        s = rnd.uniform(0.18, 0.6)
        b.box((math.cos(a) * d, math.sin(a) * d, s * 0.3), (s * rnd.uniform(1.0, 1.8), s, s * 0.6), "Body", rot=rnd.uniform(0, 3))
    b.export("SM_Kit_Rubble")


def lily():
    """Nénuphars : trois feuilles échancrées et une fleur (pivot à la surface de l'eau)."""
    reset()
    b = Builder()
    rnd = random.Random(5)
    for k in range(3):
        cx, cy = rnd.uniform(-0.5, 0.5), rnd.uniform(-0.5, 0.5)
        r = rnd.uniform(0.18, 0.3)
        notch = rnd.uniform(0, 2 * math.pi)
        pts = []
        for i in range(19):
            a = notch + 0.35 + (2 * math.pi - 0.7) * i / 18
            pts.append(Vector((cx + math.cos(a) * r, cy + math.sin(a) * r, 0.01)))
        pts.append(Vector((cx, cy, 0.015)))
        b.face(pts, [((p.x - cx) / (2 * r) + 0.5, (p.y - cy) / (2 * r) + 0.5) for p in pts], "Lily", want=(0, 0, 1))
    # fleur
    for i in range(8):
        a = 2 * math.pi * i / 8
        c = Vector((0.05, 0.05, 0.03))
        tip = c + Vector((math.cos(a) * 0.09, math.sin(a) * 0.09, 0.06))
        l = c + Vector((math.cos(a + 0.35) * 0.04, math.sin(a + 0.35) * 0.04, 0.02))
        r = c + Vector((math.cos(a - 0.35) * 0.04, math.sin(a - 0.35) * 0.04, 0.02))
        b.face([c, r, tip, l], [(0.5, 0), (1, 0.5), (0.5, 1), (0, 0.5)], "Glyph", want=(0, 0, 1))
    b.export("SM_Kit_Lily")


grass_tuft("SM_Kit_GrassTuft", 0.28, 70, 0.16, 0.42, 1)
grass_tuft("SM_Kit_GrassPatch", 0.75, 220, 0.12, 0.5, 2)
ivy("SM_Kit_Ivy", 1.6, 2.6, 3)
banner()
arcade()
broken_column()
rubble()
lily()

# ---------------------------------------------------------------------------------------------------------------
# Objets interactifs (emplacements : Body pierre, Dark pierre sombre, Glyph lumineux, Water eau, Metal bronze)
def ring_solid(b, r_out, r_in, z0, z1, sides, mat="Body"):
    """Anneau plein (paroi de bassin) : faces extérieure, intérieure, dessus et dessous."""
    def pt(r, k, z):
        a = 2 * math.pi * k / sides
        return Vector((math.cos(a) * r, math.sin(a) * r, z))
    for k in range(sides):
        k2 = k + 1
        b.face([pt(r_out, k, z0), pt(r_out, k2, z0), pt(r_out, k2, z1), pt(r_out, k, z1)], [(0, 0), (1, 0), (1, 1), (0, 1)], mat, closed=True)
        b.face([pt(r_in, k2, z0), pt(r_in, k, z0), pt(r_in, k, z1), pt(r_in, k2, z1)], [(0, 0), (1, 0), (1, 1), (0, 1)], mat, closed=True)
        b.face([pt(r_out, k, z1), pt(r_out, k2, z1), pt(r_in, k2, z1), pt(r_in, k, z1)], [(0, 0), (1, 0), (1, 1), (0, 1)], mat, closed=True)
        b.face([pt(r_in, k, z0), pt(r_in, k2, z0), pt(r_out, k2, z0), pt(r_out, k, z0)], [(0, 0), (1, 0), (1, 1), (0, 1)], mat, closed=True)


def prism(b, r, z0, z1, sides, mat="Body", r_top=None, rot=0.0):
    r_top = r if r_top is None else r_top
    lo = [Vector((math.cos(rot + 2 * math.pi * k / sides) * r, math.sin(rot + 2 * math.pi * k / sides) * r, z0)) for k in range(sides)]
    hi = [Vector((math.cos(rot + 2 * math.pi * k / sides) * r_top, math.sin(rot + 2 * math.pi * k / sides) * r_top, z1)) for k in range(sides)]
    for k in range(sides):
        k2 = (k + 1) % sides
        b.face([lo[k], lo[k2], hi[k2], hi[k]], [(k / sides, 0), ((k + 1) / sides, 0), ((k + 1) / sides, 1), (k / sides, 1)], mat, closed=True)
    b.face(hi, [(0.5 + 0.5 * math.cos(2 * math.pi * k / sides), 0.5 + 0.5 * math.sin(2 * math.pi * k / sides)) for k in range(sides)], mat, closed=True)
    b.face(lo[::-1], [(0.5, 0.5)] * sides, mat, closed=True)


def disc(b, r, z, sides, mat, up=True):
    pts = [Vector((math.cos(2 * math.pi * k / sides) * r, math.sin(2 * math.pi * k / sides) * r, z)) for k in range(sides)]
    b.face(pts, [(0.5 + 0.5 * math.cos(2 * math.pi * k / sides), 0.5 + 0.5 * math.sin(2 * math.pi * k / sides)) for k in range(sides)], mat, want=(0, 0, 1 if up else -1))


def glyph_ring(b, r, z, width, sides=48, mat="Glyph"):
    for k in range(sides):
        a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * (k + 1) / sides
        pts = [Vector((math.cos(a0) * (r - width / 2), math.sin(a0) * (r - width / 2), z)), Vector((math.cos(a1) * (r - width / 2), math.sin(a1) * (r - width / 2), z)),
               Vector((math.cos(a1) * (r + width / 2), math.sin(a1) * (r + width / 2), z)), Vector((math.cos(a0) * (r + width / 2), math.sin(a0) * (r + width / 2), z))]
        b.face(pts, [(0, 0), (1, 0), (1, 1), (0, 1)], mat, want=(0, 0, 1))


def water_glyph(b, center, scale, axis="x", mat="Glyph"):
    """Goutte + deux vagues à plat sur une face (axis = normale de la face : 'x' ou 'z')."""
    c = Vector(center)
    def P(u, v):  # u horizontal, v vertical dans le plan de la face
        return c + (Vector((0, u, v)) if axis == "x" else Vector((u, v, 0))) * scale
    want = (1, 0, 0) if axis == "x" else (0, 0, 1)
    drop = []
    for k in range(20):
        a = 2 * math.pi * k / 20
        u, v = math.sin(a) * 0.28, -math.cos(a) * 0.28
        if v > 0:
            u *= max(0.0, 1 - v / 0.28) ** 1.2
            v *= 1.8
        drop.append(P(u, v + 0.3))
    b.face(drop, [(0.5, 0.5)] * 20, mat, want=want)
    for zz in (-0.12, -0.32):
        for t in range(12):
            u0, u1 = -0.4 + 0.8 * t / 12, -0.4 + 0.8 * (t + 1) / 12
            v0, v1 = zz + 0.05 * math.sin(t / 12 * 3 * math.pi), zz + 0.05 * math.sin((t + 1) / 12 * 3 * math.pi)
            b.face([P(u0, v0 - 0.03), P(u1, v1 - 0.03), P(u1, v1 + 0.03), P(u0, v0 + 0.03)], [(0, 0), (1, 0), (1, 1), (0, 1)], mat, want=want)


def prop_fountain():
    reset()
    b = Builder()
    ring_solid(b, 1.55, 1.32, 0.0, 0.5, 8)
    prism(b, 1.62, 0.0, 0.1, 8)                         # socle
    disc(b, 1.33, 0.36, 8, "Water")                     # eau du bassin
    prism(b, 0.28, 0.1, 1.0, 12, r_top=0.22)            # fût central
    prism(b, 0.4, 0.95, 1.05, 12)                        # bague
    ring_solid(b, 0.62, 0.52, 1.05, 1.3, 16)            # vasque
    prism(b, 0.55, 1.02, 1.08, 16)                       # fond de vasque
    disc(b, 0.53, 1.24, 16, "Water")
    glyph_ring(b, 0.95, 0.37, 0.05)
    for k in range(8):                                   # ornements sur le rebord
        a = 2 * math.pi * (k + 0.5) / 8
        b.box((math.cos(a) * 1.43, math.sin(a) * 1.43, 0.56), (0.16, 0.16, 0.12), "Body", rot=a)
    b.export("SM_Prop_Fountain")


def prop_save_crystal():
    reset()
    b = Builder()
    prism(b, 0.75, 0.0, 0.25, 6)
    prism(b, 0.55, 0.25, 0.42, 6, r_top=0.5)
    glyph_ring(b, 0.62, 0.252, 0.04, sides=36)
    b.export("SM_Prop_SavePlinth")
    # cristal : octaèdre étiré, centré sur l'origine (animé en jeu au-dessus du socle)
    reset()
    b = Builder()
    top, bot = Vector((0, 0, 0.65)), Vector((0, 0, -0.65))
    mid = [Vector((math.cos(2 * math.pi * k / 6) * 0.26, math.sin(2 * math.pi * k / 6) * 0.26, 0.05)) for k in range(6)]
    for k in range(6):
        k2 = (k + 1) % 6
        b.face([mid[k], mid[k2], top], [(0, 0), (1, 0), (0.5, 1)], "Glyph", closed=True)
        b.face([mid[k2], mid[k], bot], [(0, 0), (1, 0), (0.5, 1)], "Glyph", closed=True)
    b.export("SM_Prop_SaveGem")


def prop_valve():
    reset()
    b = Builder()
    b.box((0, 0, 0.1), (0.8, 0.8, 0.2), "Body")
    b.box((0, 0, 0.8), (0.5, 0.5, 1.3), "Body")
    b.box((0, 0, 1.5), (0.62, 0.62, 0.14), "Body")
    b.sweep([(0.2, 0, 1.2), (0.55, 0, 1.2)], [(0.1 * math.cos(2 * math.pi * k / 12), 0.1 * math.sin(2 * math.pi * k / 12)) for k in range(12)], "Metal")
    # volant (plan YZ, face vers +X)
    path = [(0.58, math.cos(2 * math.pi * k / 32) * 0.42, 1.2 + math.sin(2 * math.pi * k / 32) * 0.42) for k in range(33)]
    b.sweep(path, [(0.035 * math.cos(2 * math.pi * k / 8), 0.035 * math.sin(2 * math.pi * k / 8)) for k in range(8)], "Metal", caps=False)
    for k in range(6):
        a = 2 * math.pi * k / 6
        b.sweep([(0.58, 0, 1.2), (0.58, math.cos(a) * 0.42, 1.2 + math.sin(a) * 0.42)], [(0.02 * math.cos(2 * math.pi * j / 6), 0.02 * math.sin(2 * math.pi * j / 6)) for j in range(6)], "Metal")
    b.sweep([(0.55, 0, 1.2), (0.64, 0, 1.2)], [(0.07 * math.cos(2 * math.pi * k / 10), 0.07 * math.sin(2 * math.pi * k / 10)) for k in range(10)], "Metal")
    b.export("SM_Prop_Valve")


def prop_stele():
    reset()
    b = Builder()
    b.box((0, 0, 0.12), (0.6, 1.5, 0.24), "Body")
    # dalle à sommet cintré (profil extrudé en X)
    prof = [(-0.6, 0.24), (0.6, 0.24), (0.6, 1.55)] + [(math.cos(math.pi * k / 12) * 0.6, 1.55 + math.sin(math.pi * k / 12) * 0.45) for k in range(1, 12)] + [(-0.6, 1.55)]
    front = [Vector((0.16, y, z)) for y, z in prof]
    back = [Vector((-0.16, y, z)) for y, z in prof]
    b.face(front, [(0.5, 0.5)] * len(front), "Body", closed=True)
    b.face(back[::-1], [(0.5, 0.5)] * len(back), "Body", closed=True)
    n = len(prof)
    for k in range(n):
        k2 = (k + 1) % n
        b.face([front[k], back[k], back[k2], front[k2]], [(0, 0), (1, 0), (1, 1), (0, 1)], "Body", closed=True)
    # panneau sombre et glyphe
    panel = [Vector((0.165, y * 0.78, 0.45 + (z - 0.24) * 0.8)) for y, z in prof]
    b.face(panel, [(0.5, 0.5)] * len(panel), "Dark", want=(1, 0, 0))
    water_glyph(b, (0.17, 0, 1.15), 0.9, "x")
    b.export("SM_Prop_Stele")


def prop_pedestal():
    reset()
    b = Builder()
    b.box((0, 0, 0.08), (0.8, 0.8, 0.16), "Body")
    b.box((0, 0, 0.55), (0.52, 0.52, 0.8), "Body")
    b.box((0, 0, 1.0), (0.72, 0.72, 0.12), "Body")
    disc(b, 0.26, 1.062, 20, "Dark")
    glyph_ring(b, 0.3, 1.063, 0.03, sides=32)
    b.export("SM_Prop_Pedestal")


def prop_seal():
    """Sceau vertical du gardien (Ø 3,4 m), face vers +X, pivot au sol."""
    reset()
    b = Builder()
    zc, R = 2.0, 1.7
    path = [(0.0, math.cos(2 * math.pi * k / 48) * (R - 0.12), zc + math.sin(2 * math.pi * k / 48) * (R - 0.12)) for k in range(49)]
    b.sweep(path, [(-0.12, -0.18), (0.12, -0.18), (0.12, 0.18), (-0.12, 0.18)], "Body", caps=False)
    pts = [Vector((0.0, math.cos(2 * math.pi * k / 48) * (R - 0.2), zc + math.sin(2 * math.pi * k / 48) * (R - 0.2))) for k in range(48)]
    b.face(pts, [(0.5, 0.5)] * 48, "Dark", want=(1, 0, 0))
    b.face([p + Vector((-0.02, 0, 0)) for p in pts], [(0.5, 0.5)] * 48, "Dark", want=(-1, 0, 0))
    # anneau lumineux et trois gouttes
    for k in range(48):
        a0, a1 = 2 * math.pi * k / 48, 2 * math.pi * (k + 1) / 48
        r0, r1 = R - 0.42, R - 0.34
        q = [Vector((0.02, math.cos(a0) * r0, zc + math.sin(a0) * r0)), Vector((0.02, math.cos(a1) * r0, zc + math.sin(a1) * r0)),
             Vector((0.02, math.cos(a1) * r1, zc + math.sin(a1) * r1)), Vector((0.02, math.cos(a0) * r1, zc + math.sin(a0) * r1))]
        b.face(q, [(0, 0), (1, 0), (1, 1), (0, 1)], "Glyph", want=(1, 0, 0))
    for k in range(3):
        a = math.pi / 2 + 2 * math.pi * k / 3
        water_glyph(b, (0.03, math.cos(a) * 0.72, zc + math.sin(a) * 0.72 - 0.1), 0.75, "x")
    # socle
    b.box((0, 0, 0.15), (0.9, 2.6, 0.3), "Body")
    b.export("SM_Prop_Seal")


prop_fountain()
prop_save_crystal()
prop_valve()
prop_stele()
prop_pedestal()
prop_seal()
