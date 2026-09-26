"""Phase 2a: the game's deformation skeleton (UE5 mannequin, Tools/art/export/SKM_Manny_Simple.fbx) in RIG_DEF,
posed in the sheets' T-pose (arms along +-X, 2 % down, as Tools/blender/make_character.t_pose). The mannequin
mesh is kept hidden in HIGH as the complete body under the costume (reference only, never exported).
  blender -b --python build/02a_skeleton.py -- --master CH_Link_master.blend
"""
import bpy, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../../blender")))
import _lib as L
import make_character as mc
L.PHASE = "02a_skeleton"; L.OWNER_TAG = "phase:02a_skeleton"
MANNY = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../export/SKM_Manny_Simple.fbx"))


SHOULDER_DROP = 0.035   # front sheet: arm centreline z 1.39 at x 0.3..0.88 (matte_front), mannequin joint line z 1.436
ARM_SLOPE = -0.01       # front sheet: arms horizontal to +-5 mm between x 0.3 and 0.88


def straighten_tip(arm, bone, target):
    """Last phalanx has no child: aim its own head->tail along target."""
    from mathutils import Matrix, Vector
    bpy.context.view_layer.update()
    pb = arm.pose.bones[bone]
    mw = arm.matrix_world
    h, t = mw @ pb.head, mw @ pb.tail
    q = (t - h).normalized().rotation_difference(Vector(target).normalized())
    M = mw @ pb.matrix
    loc = M.to_translation()
    pb.matrix = mw.inverted() @ (Matrix.Translation(loc) @ q.to_matrix().to_4x4() @ Matrix.Translation(-loc) @ M)
    bpy.context.view_layer.update()


def build_pose(arm):
    """Clavicles lowered so the upper arm joint sits on the drawn arm, arms along the drawn direction.
    Build pose only: Phase 6 binds in this pose and unposes to the mannequin reference pose for export."""
    from mathutils import Vector
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    mw = arm.matrix_world
    for s, sign in (("l", 1), ("r", -1)):
        bpy.context.view_layer.update()
        h = mw @ arm.pose.bones["clavicle_" + s].head
        c = mw @ arm.pose.bones["upperarm_" + s].head
        v = c - h
        mc.align(arm, "clavicle_" + s, "upperarm_" + s, (v.x, v.y, v.z - SHOULDER_DROP))
        for b, child in (("upperarm_", "lowerarm_"), ("lowerarm_", "hand_")):
            mc.align(arm, b + s, child + s, (sign, 0, ARM_SLOPE))
        mc.align(arm, "hand_" + s, "middle_01_" + s, (sign, 0, ARM_SLOPE - 0.02))
        # front sheet: flat hands, fingers together along the arm, thumb forward and down (fingerless gloves)
        for f in ("index", "middle", "ring", "pinky"):
            for a, b in (("_01_", "_02_"), ("_02_", "_03_")):
                mc.align(arm, f + a + s, f + b + s, (sign, 0, ARM_SLOPE - 0.03))
            straighten_tip(arm, f + "_03_" + s, (sign, 0, ARM_SLOPE - 0.03))
        for a, b in (("_01_", "_02_"), ("_02_", "_03_")):
            mc.align(arm, "thumb" + a + s, "thumb" + b + s, (sign * 0.7, -0.55, -0.45))
        straighten_tip(arm, "thumb_03_" + s, (sign * 0.7, -0.55, -0.45))
    bpy.ops.object.mode_set(mode="OBJECT")


def main():
    a = L.argv_after_dashes()
    master = a[a.index("--master") + 1]
    L.open_master(master)
    L.clear_owned()
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=MANNY)
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        for c in list(o.users_collection): c.objects.unlink(o)
        L.col("HIGH" if o.type == "MESH" else "RIG_DEF").objects.link(o)
        L.own(o)
    arm = [o for o in new if o.type == "ARMATURE"][0]
    body = [o for o in new if o.type == "MESH"][0]
    body.name = "CH_Link_HIGH_MannyBody"
    body.hide_render = True
    body.hide_set(True)
    mc.t_pose(arm)
    build_pose(arm)
    bpy.context.view_layer.update()
    mw = arm.matrix_world
    for b in ("pelvis", "spine_03", "spine_05", "neck_01", "head", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
              "middle_01_l", "thigh_l", "calf_l", "foot_l", "ball_l"):
        pb = arm.pose.bones[b]
        L.log(b, tuple(round(x, 3) for x in mw @ pb.head), tuple(round(x, 3) for x in mw @ pb.tail))
    L.save_master(master)

main()
