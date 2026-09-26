"""Phase 4: topology. Phase-3 objects become the quad sources (renamed *_SRC, hidden collection LOW_SRC); the
delivery LOD0 is their applied copy (modifiers, transforms), cleaned and triangulated (triangulation locked before
baking); LOD1 and LOD2 are decimated from LOD0. Each LOD level has its own child collection of LOW.
  blender -b --python build/04_topology.py -- --master CH_Link_master.blend
"""
import bpy, bmesh, os, sys, re
from mathutils import Matrix, Vector
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _lib as L
L.PHASE = "04_topology"; L.OWNER_TAG = "phase:04_topology"

# hero tier: LOD0 60-100k, LOD1 15-30k, LOD2 5-10k (delivery-and-acceptance.md)
LOD_RATIOS = {1: 0.42, 2: 0.16}
MIN_TRIS = {1: 60, 2: 24}            # small props keep a readable silhouette
KEEP_FULL = {1: ("Head",), 2: ()}    # the face reads at LOD1 distance: decimated less (see HEAD_LOD1)
HEAD_LOD = {1: 0.40, 2: 0.14}


def child_col(name, parent="LOW"):
    c = bpy.data.collections.get(name)
    if c is None:
        c = bpy.data.collections.new(name)
        bpy.data.collections[parent].children.link(c)
    return c


def hide_col(name):
    lc = bpy.context.view_layer.layer_collection
    def find(l):
        if l.name == name:
            return l
        for ch in l.children:
            r = find(ch)
            if r:
                return r
    l = find(lc)
    if l:
        l.exclude = True


def show_col(name):
    lc = bpy.context.view_layer.layer_collection
    def find(l):
        if l.name == name:
            return l
        for ch in l.children:
            r = find(ch)
            if r:
                return r
    l = find(lc)
    if l:
        l.exclude = False


def part_name(src_name):
    return re.sub(r"_(LOD0|SRC)$", "", src_name).replace("CH_Link_", "")


def applied_copy(src, name, col):
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(src.evaluated_get(dg))
    me.transform(src.matrix_world)
    me.name = name
    ob = bpy.data.objects.new(name, me)
    col.objects.link(ob)
    return L.own(ob)


def clean_and_triangulate(ob, open_band=False):
    bm = bmesh.new(); bm.from_mesh(ob.data)
    n0 = len(bm.verts)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=1e-6)
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    # non-manifold fans (edges with 3+ faces, from decimating the welded AI surface): drop the smallest extra faces
    for e in [e for e in bm.edges if len(e.link_faces) > 2]:
        if e.is_valid and len(e.link_faces) > 2:
            extra = sorted(e.link_faces, key=lambda f: f.calc_area())[:len(e.link_faces) - 2]
            bmesh.ops.delete(bm, geom=extra, context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if open_band:
        # recalc cannot tell inside from outside on an open band (the belt): face away from its vertical axis
        c = sum((v.co for v in bm.verts), Vector()) / len(bm.verts)
        inward = sum(1 for f in bm.faces if Vector((f.calc_center_median().x - c.x, f.calc_center_median().y - c.y, 0)).dot(f.normal) < 0)
        if inward > len(bm.faces) / 2:
            bmesh.ops.reverse_faces(bm, faces=bm.faces)
    bmesh.ops.triangulate(bm, faces=bm.faces, quad_method="BEAUTY", ngon_method="BEAUTY")
    bm.to_mesh(ob.data); bm.free()
    for p in ob.data.polygons:
        p.use_smooth = True
    return n0 - len(ob.data.vertices)


def main():
    a = L.argv_after_dashes()
    master = a[a.index("--master") + 1]
    L.open_master(master)
    L.clear_owned()
    show_col("LOW_SRC")
    src_col = L.col("LOW_SRC")
    lods = {k: child_col("LOD%d" % k) for k in (0, 1, 2)}
    sources = [o for o in bpy.data.objects if o.get("owner") == "phase:03_forms" and o.type == "MESH"
               and any(c.name in ("LOW", "LOW_SRC") for c in o.users_collection)]
    report = {0: 0, 1: 0, 2: 0}
    for src in sorted(sources, key=lambda o: o.name):
        part = part_name(src.name)
        src.name = src.data.name = "CH_Link_%s_SRC" % part
        for c in list(src.users_collection):
            c.objects.unlink(src)
        src_col.objects.link(src)
        src.hide_render = True
        lod0 = applied_copy(src, "CH_Link_%s_LOD0" % part, lods[0])
        merged = clean_and_triangulate(lod0, open_band=(part == "Belt"))
        t0 = len(lod0.data.polygons)
        report[0] += t0
        for k in (1, 2):
            ob = L.duplicate(lod0, "CH_Link_%s_LOD%d" % (part, k), collection=lods[k].name)
            L.own(ob)
            ratio = HEAD_LOD[k] if part == "Head" else LOD_RATIOS[k]
            ratio = max(ratio, min(1.0, MIN_TRIS[k] / max(1, t0)))
            if ratio < 1.0:
                L.decimate(ob, ratio)
                L.apply_all_modifiers(ob)
            clean_and_triangulate(ob, open_band=(part == "Belt"))
            report[k] += len(ob.data.polygons)
        L.log("%-14s LOD0 %6d tris (%d doubles merged)" % (part, t0, merged))
    hide_col("LOW_SRC")
    L.log("totals", report)
    L.save_master(master)

main()
