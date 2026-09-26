# Modèles stylisés générés par script (Blender 5.x, mode arrière-plan) puis exportés en FBX.
# Lancer : Blender -b -P Tools/blender/make_models.py -- <dossier_sortie> [sword_hero chest ...]
# Conventions : mètres dans Blender (→ centimètres dans Unreal), avant = +X, haut = +Z, pivot au sol.
# Emplacements de matériaux reconnus par le jeu : Body, Dark, Accent, Glow, Metal, Red, White, Moss, Hilt, Eye.
import bpy, bmesh, math, os, sys, random
from mathutils import Vector, Matrix, Euler

OUT = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else os.path.join(os.path.dirname(__file__), "..", "art", "fbx")
os.makedirs(OUT, exist_ok=True)
rnd = random.Random(12)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name):
    m = bpy.data.materials.get(name)
    if not m:
        m = bpy.data.materials.new(name)
    return m


def finish_obj(o, mat_name, bevel=0.0, segments=2, smooth=False):
    if o.data.materials:
        o.data.materials.clear()
    o.data.materials.append(material(mat_name))
    bpy.context.view_layer.objects.active = o
    o.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    if bevel > 0:
        b = o.modifiers.new("Bevel", "BEVEL")
        b.width = bevel
        b.segments = segments
        b.limit_method = "ANGLE"
        bpy.ops.object.modifier_apply(modifier=b.name)
    if smooth:
        bpy.ops.object.shade_smooth()
    o.select_set(False)
    return o


def box(size, loc, rot=(0, 0, 0), mat="Body", bevel=0.03):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=rot)
    o = bpy.context.active_object
    o.scale = size
    return finish_obj(o, mat, bevel)


def cyl(radius, depth, loc, rot=(0, 0, 0), mat="Body", verts=16, bevel=0.0, r2=None):
    if r2 is None:
        bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=depth, location=loc, rotation=rot, vertices=verts)
    else:
        bpy.ops.mesh.primitive_cone_add(radius1=radius, radius2=r2, depth=depth, location=loc, rotation=rot, vertices=verts)
    o = bpy.context.active_object
    return finish_obj(o, mat, bevel, smooth=verts >= 12)


def sphere(radius, loc, scale=(1, 1, 1), mat="Body", seg=24, rings=12):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=radius, location=loc, segments=seg, ring_count=rings)
    o = bpy.context.active_object
    o.scale = scale
    return finish_obj(o, mat, smooth=True)


def torus(major, minor, loc, rot=(0, 0, 0), mat="Glow"):
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, location=loc, rotation=rot, major_segments=32, minor_segments=8)
    o = bpy.context.active_object
    return finish_obj(o, mat, smooth=True)


def rock(size, loc, rot=(0, 0, 0), mat="Body", jitter=0.18):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.5, location=(0, 0, 0))
    o = bpy.context.active_object
    for v in o.data.vertices:
        v.co *= 1.0 + rnd.uniform(-jitter, jitter)
    o.scale = size
    o.location = loc
    o.rotation_euler = rot
    return finish_obj(o, mat)


def tapered_tube(points, radii, mat="Body", sides=12, name="Tube"):
    """Tube effilé le long d'une polyligne 3D (bonnet, nageoires, tentacules)."""
    bm = bmesh.new()
    rings = []
    for i, (p, r) in enumerate(zip(points, radii)):
        p = Vector(p)
        if i < len(points) - 1:
            d = (Vector(points[i + 1]) - p).normalized()
        else:
            d = (p - Vector(points[i - 1])).normalized()
        up = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))
        a = d.cross(up).normalized()
        b = d.cross(a).normalized()
        ring = []
        for k in range(sides):
            t = 2 * math.pi * k / sides
            ring.append(bm.verts.new(p + (a * math.cos(t) + b * math.sin(t)) * max(r, 0.0005)))
        rings.append(ring)
    for i in range(len(rings) - 1):
        for k in range(sides):
            bm.faces.new((rings[i][k], rings[i][(k + 1) % sides], rings[i + 1][(k + 1) % sides], rings[i + 1][k]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    return finish_obj(o, mat, smooth=True)


def join_and_export(name, yaw_fix=0.0):
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    o = bpy.context.active_object
    o.name = name
    if yaw_fix:
        o.rotation_euler = (0, 0, math.radians(yaw_fix))
        bpy.ops.object.transform_apply(rotation=True)
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    path = os.path.join(OUT, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_scale_options="FBX_SCALE_UNITS",
                             axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", add_leaf_bones=False,
                             bake_space_transform=True, use_mesh_modifiers=True)
    print("EXPORT", path, len(o.data.polygons), "faces")


# ---------------------------------------------------------------------------------------------------------------
def golem():
    """Gardien Néréide : colosse de pierre aux circuits d'eau (≈ 2,15 m à l'échelle 1, ×2,4 en jeu)."""
    reset()
    # Jambes et pieds
    for s in (-1, 1):
        box((0.42, 0.36, 0.55), (0, s * 0.36, 0.42), (0, 0, s * 0.05), "Body", 0.05)
        box((0.56, 0.42, 0.2), (0.07, s * 0.37, 0.1), (0, 0, 0), "Dark", 0.05)
        box((0.36, 0.3, 0.2), (0.02, s * 0.36, 0.75), (0, 0, 0), "Dark", 0.03)
    # Bassin et torse
    box((0.62, 0.95, 0.3), (0, 0, 0.9), (0, 0, 0), "Dark", 0.05)
    box((0.82, 1.25, 0.72), (0, 0, 1.38), (0, math.radians(-4), 0), "Body", 0.07)
    box((0.3, 1.05, 0.32), (0.3, 0, 1.52), (0, math.radians(-8), 0), "Body", 0.05)
    # Plaques de poitrine et cœur
    for s in (-1, 1):
        box((0.1, 0.36, 0.42), (0.42, s * 0.34, 1.33), (0, 0, s * 0.12), "Body", 0.03)
    cyl(0.24, 0.12, (0.43, 0, 1.35), (0, math.radians(90), 0), "Dark", 24)
    sphere(0.19, (0.48, 0, 1.35), (0.7, 1, 1), "Glow")
    torus(0.27, 0.025, (0.46, 0, 1.35), (0, math.radians(90), 0), "Glow")
    # Runes (lignes lumineuses)
    for k in range(3):
        box((0.03, 0.05, 0.3 - k * 0.06), (0.41, -0.52 + k * 0.52, 1.02), (0, 0, 0), "Glow", 0.0)
    # Tête : bloc avec visière
    box((0.42, 0.44, 0.34), (0.08, 0, 1.92), (0, math.radians(6), 0), "Body", 0.05)
    box((0.08, 0.36, 0.06), (0.3, 0, 1.94), (0, 0, 0), "Glow", 0.0)
    box((0.5, 0.52, 0.08), (0.06, 0, 2.1), (0, 0, 0), "Dark", 0.03)
    # Épaules et bras
    for s in (-1, 1):
        rock((0.55, 0.5, 0.48), (0, s * 0.82, 1.62), (0.2, 0.1, 0.3), "Body", 0.12)
        cyl(0.17, 0.52, (0.06, s * 0.9, 1.2), (math.radians(-s * 12), math.radians(-18), 0), "Dark", 12, 0.02)
        box((0.46, 0.44, 0.62), (0.18, s * 0.96, 0.72), (0, math.radians(-10), s * 0.06), "Body", 0.06)
        torus(0.3, 0.035, (0.18, s * 0.96, 0.93), (0, 0, 0), "Glow")
        box((0.38, 0.4, 0.3), (0.26, s * 0.98, 0.3), (0, 0, 0), "Dark", 0.05)
        # Mousse sur les épaules
        box((0.4, 0.34, 0.05), (0, s * 0.82, 1.9), (0.1, 0, s * 0.2), "Moss", 0.02)
    # Pierres flottantes autour de la tête
    for k in range(4):
        a = k * math.pi / 2 + 0.3
        rock((0.14, 0.12, 0.12), (math.cos(a) * 0.55, math.sin(a) * 0.55, 2.3), (a, a, 0), "Body", 0.25)
    join_and_export("SM_Golem")


def octorok():
    reset()
    sphere(0.42, (0, 0, 0.62), (1.0, 1.0, 1.08), "Body", 32, 16)
    for k in range(9):
        a = rnd.uniform(0, 2 * math.pi)
        z = rnd.uniform(0.45, 0.95)
        r = math.sqrt(max(0.0, 0.42 ** 2 - (z - 0.62) ** 2)) * 0.98
        sphere(0.06, (math.cos(a) * r, math.sin(a) * r, z), (1, 1, 0.5), "Dark", 12, 6)
    cyl(0.14, 0.3, (0.45, 0, 0.6), (0, math.radians(90), 0), "Body", 20, 0.0, r2=0.17)
    cyl(0.12, 0.05, (0.61, 0, 0.6), (0, math.radians(90), 0), "Dark", 20)
    for s in (-1, 1):
        sphere(0.1, (0.28, s * 0.2, 0.88), (1, 1, 1.1), "White", 16, 8)
        sphere(0.05, (0.36, s * 0.2, 0.88), (0.6, 1, 1.3), "Dark", 12, 6)
    for k in range(4):
        a = math.radians(45 + 90 * k)
        pts = [(math.cos(a) * 0.2, math.sin(a) * 0.2, 0.32), (math.cos(a) * 0.32, math.sin(a) * 0.32, 0.12), (math.cos(a) * 0.4, math.sin(a) * 0.4, 0.02)]
        tapered_tube(pts, [0.09, 0.07, 0.04], "Body", 10, "Leg%d" % k)
    join_and_export("SM_Octorok")


def chuchu():
    reset()
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.5, location=(0, 0, 0.45), segments=32, ring_count=16)
    o = bpy.context.active_object
    for v in o.data.vertices:
        z = v.co.z
        if z < 0:
            v.co.z = z * 0.55
            v.co.x *= 1.15
            v.co.y *= 1.15
        else:
            v.co.x *= 1.0 - z * 0.25
            v.co.y *= 1.0 - z * 0.25
    finish_obj(o, "Body", smooth=True)
    sphere(0.16, (0.05, 0, 0.88), (1, 1, 0.8), "Body", 16, 8)
    for s in (-1, 1):
        sphere(0.07, (0.4, s * 0.15, 0.62), (0.5, 1, 1.5), "Dark", 12, 8)
    join_and_export("SM_Chuchu")


def sentinel():
    reset()
    cyl(0.55, 0.45, (0, 0, 1.05), (0, 0, 0), "Body", 8, 0.04)
    sphere(0.5, (0, 0, 1.3), (1, 1, 0.7), "Body", 24, 12)
    for k in range(8):
        a = k * math.pi / 4
        box((0.08, 0.3, 0.5), (math.cos(a) * 0.52, math.sin(a) * 0.52, 1.05), (0, 0, a), "Dark", 0.02)
    cyl(0.2, 0.12, (0.47, 0, 1.45), (0, math.radians(70), 0), "Dark", 20)
    sphere(0.14, (0.52, 0, 1.47), (0.5, 1, 1), "Glow", 16, 8)
    torus(0.36, 0.03, (0, 0, 1.62), (0, 0, 0), "Glow")
    for k in range(4):
        a = math.radians(45 + 90 * k)
        pts = [(math.cos(a) * 0.45, math.sin(a) * 0.45, 0.95), (math.cos(a) * 0.9, math.sin(a) * 0.9, 0.9), (math.cos(a) * 1.05, math.sin(a) * 1.05, 0.0)]
        tapered_tube(pts, [0.09, 0.07, 0.04], "Dark", 8, "SLeg%d" % k)
        sphere(0.1, (math.cos(a) * 0.9, math.sin(a) * 0.9, 0.9), (1, 1, 1), "Body", 12, 6)
    join_and_export("SM_Sentinel")


def hat_link():
    """Bonnet de Link : monte puis retombe dans le dos (+Y Blender = arrière de la tête en jeu). ~50 cm."""
    reset()
    pts, radii = [], []
    for i in range(18):
        t = i / 17.0
        pts.append((0.0, 0.36 * t ** 1.25, 0.2 * math.sin(t * math.pi * 0.85) - 0.08 * t))
        radii.append(0.115 * (1 - t) ** 1.15 + 0.004)
    tapered_tube(pts, radii, "Body", 18, "Hat")
    join_and_export("SM_Hat_Link")


def sword_hero():
    """Épée de légende, lame vers +Z, pivot au milieu de la poignée (~95 cm) : lame à arête centrale et gorge, garde en ailes
    relevées, pierre jaune, poignée filetée, pommeau en losange."""
    reset()
    bm = bmesh.new()
    # section en losange aplati (arête centrale, tranchants biseautés) : (hauteur, demi-largeur, demi-épaisseur)
    prof = [(0.085, 0.021, 0.0048), (0.12, 0.0225, 0.005), (0.40, 0.021, 0.0047), (0.66, 0.0185, 0.0042), (0.74, 0.0145, 0.0034)]
    rings = []
    for (z, w, t) in prof:
        e = 0.0011
        pts = [(t, 0.0), (e, w * 0.9), (0.0, w), (-e, w * 0.9), (-t, 0.0), (-e, -w * 0.9), (0.0, -w), (e, -w * 0.9)]
        rings.append([bm.verts.new((x, y, z)) for (x, y) in pts])
    tip = bm.verts.new((0.0, 0.0, 0.83))
    n = len(rings[0])
    for i in range(len(rings) - 1):
        for k in range(n):
            bm.faces.new((rings[i][k], rings[i][(k + 1) % n], rings[i + 1][(k + 1) % n], rings[i + 1][k]))
    for k in range(n):
        bm.faces.new((rings[-1][k], rings[-1][(k + 1) % n], tip))
    bm.faces.new(list(reversed(rings[0])))
    me = bpy.data.meshes.new("Blade")
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new("Blade", me)
    bpy.context.collection.objects.link(o)
    finish_obj(o, "Metal")
    # gorge : filet sombre le long de l'arête, sur les deux faces
    for sx in (-1, 1):
        box((0.0012, 0.0055, 0.27), (sx * 0.0049, 0, 0.26), (0, 0, 0), "Dark", 0.0)
    # garde : bloc central, ailes relevées aplaties, pointe en V sur la lame, pierre
    box((0.022, 0.05, 0.028), (0, 0, 0.078), (0, 0, 0), "Hilt", 0.004)
    for s in (-1, 1):
        w = tapered_tube([(0, s * 0.018, 0.074), (0, s * 0.06, 0.08), (0, s * 0.092, 0.1), (0, s * 0.108, 0.13), (0, s * 0.104, 0.15)],
                         [0.013, 0.011, 0.009, 0.006, 0.0015], "Hilt", 10, "Wing%d" % s)
        w.scale = (0.55, 1, 1)
        finish_obj(w, "Hilt", smooth=True)
    for sx in (-1, 1):
        bmv = bmesh.new()
        tri = [(sx * 0.0062, -0.02, 0.088), (sx * 0.0062, 0.02, 0.088), (sx * 0.0062, 0.0, 0.14)]
        vs = [bmv.verts.new(p) for p in tri]
        bmv.faces.new(vs if sx > 0 else list(reversed(vs)))
        mev = bpy.data.meshes.new("V%d" % sx)
        bmv.to_mesh(mev)
        bmv.free()
        ov = bpy.data.objects.new("V%d" % sx, mev)
        bpy.context.collection.objects.link(ov)
        sol = ov.modifiers.new("S", "SOLIDIFY")
        sol.thickness = 0.0015
        bpy.context.view_layer.objects.active = ov
        bpy.ops.object.modifier_apply(modifier=sol.name)
        finish_obj(ov, "Hilt")
        sphere(0.0075, (sx * 0.011, 0, 0.078), (0.45, 0.8, 1.2), "Accent", 12, 6)
    # poignée filetée et pommeau
    cyl(0.0135, 0.15, (0, 0, -0.003), (0, 0, 0), "Hilt", 12)
    for i in range(7):
        torus_ring = bpy.ops.mesh.primitive_torus_add(major_radius=0.0138, minor_radius=0.0022, location=(0, 0, -0.066 + i * 0.02),
                                                      major_segments=16, minor_segments=6)
        finish_obj(bpy.context.active_object, "Hilt", smooth=True)
    cyl(0.016, 0.012, (0, 0, -0.082), (0, 0, 0), "Accent", 12)
    sphere(0.022, (0, 0, -0.105), (0.65, 1.25, 1.0), "Hilt", 16, 8)
    sphere(0.0065, (0, 0, -0.128), (1, 1, 1), "Accent", 10, 5)
    join_and_export("SM_Sword_Hero")


def shield_hylian():
    """Bouclier en écu, face vers +Z (normale), sommet vers +Y. ~60 cm."""
    reset()
    bm = bmesh.new()
    outline = []
    n = 28
    for i in range(n):
        t = i / (n - 1)
        # contour : haut plat arrondi, pointe en bas
        ang = math.pi * t
        x = 0.22 * math.cos(ang)
        y = 0.12 + 0.1 * math.sin(ang)
        outline.append((x, y))
    outline += [(-0.2, -0.05), (-0.12, -0.2), (0.0, -0.3), (0.12, -0.2), (0.2, -0.05)]
    top = [bm.verts.new((x, y, 0.03 - 0.25 * (x * x))) for (x, y) in outline]
    bot = [bm.verts.new((x, y, -0.0 - 0.25 * (x * x))) for (x, y) in outline]
    bm.faces.new(top)
    bm.faces.new(list(reversed(bot)))
    for i in range(len(outline)):
        j = (i + 1) % len(outline)
        bm.faces.new((top[i], top[j], bot[j], bot[i]))
    me = bpy.data.meshes.new("Shield")
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new("Shield", me)
    bpy.context.collection.objects.link(o)
    finish_obj(o, "Body", 0.008)
    # Bordure métallique et emblème
    pts = [(x * 1.02, y * 1.02 + 0.0, 0.035 - 0.25 * (x * x)) for (x, y) in outline] + [(outline[0][0] * 1.02, outline[0][1] * 1.02, 0.035)]
    tapered_tube(pts, [0.012] * len(pts), "Metal", 6, "Rim")
    tapered_tube([(0, -0.08, 0.04), (0, 0.1, 0.045)], [0.01, 0.01], "Red", 6, "Emb1")
    tapered_tube([(-0.09, 0.02, 0.043), (0, 0.06, 0.047), (0.09, 0.02, 0.043)], [0.012, 0.016, 0.012], "Red", 6, "Emb2")
    tapered_tube([(-0.05, 0.14, 0.045), (0, 0.19, 0.046), (0.05, 0.14, 0.045)], [0.01, 0.01, 0.01], "Accent", 6, "Tri")
    join_and_export("SM_Shield_Hylian")


def trident():
    reset()
    cyl(0.018, 1.5, (0, 0, 0.1), (0, 0, 0), "Accent", 10)
    cyl(0.03, 0.08, (0, 0, 0.87), (0, 0, 0), "Red", 10)
    for s in (-1, 0, 1):
        pts = [(0, s * 0.02, 0.9), (0, s * 0.09, 0.98), (0, s * 0.1, 1.12)] if s else [(0, 0, 0.9), (0, 0, 1.2)]
        tapered_tube(pts, [0.02, 0.015, 0.003] if s else [0.02, 0.003], "Accent", 8, "Prong%d" % s)
    sphere(0.03, (0, 0, 0.84), (1, 1, 1), "Glow", 10, 5)
    join_and_export("SM_Trident")


def fin_head():
    """Nageoire de tête (Mipha, forme Zora) : lame recourbée vers l'arrière, base à l'origine."""
    reset()
    pts, radii = [], []
    for i in range(12):
        t = i / 11.0
        pts.append((0.0, 0.42 * t, 0.1 * math.sin(t * math.pi) - 0.28 * t * t))
        radii.append(0.1 * (1 - t) + 0.01)
    o = tapered_tube(pts, radii, "Body", 12, "Fin")
    o.scale = (0.45, 1, 1)
    bpy.context.view_layer.objects.active = o
    o.select_set(True)
    bpy.ops.object.transform_apply(scale=True)
    join_and_export("SM_Fin_Head")


def chest():
    reset()
    box((0.9, 0.6, 0.38), (0, 0, 0.19), (0, 0, 0), "Body", 0.02)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.3, depth=0.9, location=(0, 0, 0.38), rotation=(0, math.radians(90), 0), vertices=24)
    lid = bpy.context.active_object
    bm = bmesh.new()
    bm.from_mesh(lid.data)
    for v in [v for v in bm.verts if v.co.x < 0 or True]:
        pass
    bm.free()
    lid.scale = (1, 1, 0.55)
    finish_obj(lid, "Body")
    for x in (-0.36, 0, 0.36):
        box((0.06, 0.63, 0.4), (x, 0, 0.2), (0, 0, 0), "Accent", 0.005)
    box((0.94, 0.05, 0.05), (0, 0.3, 0.38), (0, 0, 0), "Accent", 0.005)
    box((0.1, 0.04, 0.12), (0, 0.32, 0.36), (0, 0, 0), "Accent", 0.005)
    join_and_export("SM_Chest", yaw_fix=90)


def kit_column():
    """Fût cannelé centré, 1 m de haut, rayon 0,5 m (même convention que le cylindre de base)."""
    reset()
    bpy.ops.mesh.primitive_cylinder_add(radius=0.5, depth=1.0, location=(0, 0, 0), vertices=48)
    o = bpy.context.active_object
    for v in o.data.vertices:
        a = math.atan2(v.co.y, v.co.x)
        r = 0.5 - 0.03 * (0.5 + 0.5 * math.cos(a * 16))
        l = math.hypot(v.co.x, v.co.y)
        if l > 0.01:
            v.co.x *= r / l
            v.co.y *= r / l
    finish_obj(o, "Body", smooth=True)
    join_and_export("SM_Kit_Column")


def kit_rock():
    reset()
    rock((1, 1, 1), (0, 0, 0), (0, 0, 0), "Body", 0.22)
    join_and_export("SM_Kit_Rock")


ONLY = sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else []
for fn in (golem, octorok, chuchu, sentinel, hat_link, sword_hero, shield_hylian, trident, fin_head, chest, kit_column, kit_rock):
    if ONLY and fn.__name__ not in ONLY:
        continue
    try:
        fn()
    except Exception as e:
        import traceback
        print("ECHEC", fn.__name__, e)
        traceback.print_exc()
print("TERMINE")
