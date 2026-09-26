"""Phase 1b: HIGH = the image-to-3D model (Tools/art/source/link_model.fbx), placed at game scale.
Scale: cap top at 1.851 m (T-pose sheets, 1454 px / 785 px/m, same constant as Tools/blender/rig_model.py).
Depth: torso front at z 1.25, |x| < 0.05 put at y = -0.175, where the shipped SKM_Link has it (measured in
out/link/link_model.blend), so the mannequin skeleton registration of the game is kept.
  blender -b --python build/01_high.py -- --master CH_Link_master.blend
"""
import bpy, os, sys
import numpy as np
from mathutils import Matrix
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _lib as L
L.PHASE = "01_high"; L.OWNER_TAG = "phase:01_high"

HERE = os.path.dirname(os.path.abspath(__file__))
FBX = os.path.normpath(os.path.join(HERE, "../../source/link_model.fbx"))
CAP_TOP = 1.851        # T-pose front sheet row 17 -> (1471-17)/785
FRONT_Y_125 = -0.175   # shipped SKM_Link torso front at z 1.25 (link_model.blend)


def main():
    a = L.argv_after_dashes()
    master = a[a.index("--master") + 1]
    L.open_master(master)
    L.clear_owned()
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=FBX)
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    for o in meshes:
        o.data.transform(o.matrix_world); o.parent = None; o.matrix_world = Matrix.Identity(4)
    for o in new:
        if o.type != "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    if len(meshes) > 1:
        bpy.ops.object.select_all(action="DESELECT")
        for o in meshes: o.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        bpy.ops.object.join()
    o = meshes[0]
    for c in list(o.users_collection): c.objects.unlink(o)
    L.col("HIGH").objects.link(o)
    o.name = o.data.name = "CH_Link_HIGH_AI"
    co = np.empty(len(o.data.vertices) * 3); o.data.vertices.foreach_get("co", co); co = co.reshape(-1, 3)
    s = CAP_TOP / co[:, 2].max()
    co *= s
    m = (np.abs(co[:, 0]) < 0.05) & (np.abs(co[:, 2] - 1.25) < 0.02)
    dy = FRONT_Y_125 - co[m, 1].min()
    o.data.transform(Matrix.Translation((0, dy, 0)) @ Matrix.Scale(s, 4))
    L.own(o)
    for p in o.data.polygons: p.use_smooth = True
    L.log("HIGH", len(o.data.vertices), "verts", len(o.data.polygons), "faces, scale %.5f dy %.4f" % (s, dy))
    L.save_master(master)

main()
