"""Phase 6: articulation on the game's skeleton (UE5 mannequin, RIG_DEF, from 02a_skeleton.py).
Weights: deforming parts take the mannequin body's production weights (nearest surface of the mannequin mesh posed
in the same build pose, barycentric, top 4, normalised) with rules per part; rigid parts follow one bone.
The meshes were modelled in the build pose (the sheets' T-pose): they are inverse-skinned to the mannequin reference
pose, so the exported bind pose is the one every game animation expects (Tools/blender/rig_model.unpose). Rerunning
re-skins them forward first (flag "unposed"), so the phase is idempotent.
Sockets (SOCKETS): weapon grips, back mounts, head. Pose actions for the extreme-pose gate (RIG_CTRL-free: plain
FK keys on the deformation skeleton, never exported).
  blender -b --python build/06_rig.py -- --master CH_Link_master.blend
"""
import bpy, os, sys, math
import numpy as np
from mathutils import Vector, Matrix, Euler, Quaternion
from mathutils.bvhtree import BVHTree
from mathutils.interpolate import poly_3d_calc
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _lib as L
L.PHASE = "06_rig"; L.OWNER_TAG = "phase:06_rig"

RIGID = {   # part -> bone (rigid armour/props follow one bone; delivery-and-acceptance, rigging-animation.md 2)
    "Belt": "pelvis", "Buckle": "pelvis", "Pouch": "pelvis", "Ocarina": "pelvis", "Mask": "pelvis",
    "BootCuff_L": "calf_l", "BootCuff_R": "calf_r", "CapTail": "head",
    "Shield": "spine_05", "ShieldRim": "spine_05", "Sheath": "spine_05", "Hilt": "spine_05", "Guard": "spine_05",
}
HEAD_BONES = {"head", "neck_02", "neck_01", "spine_05"}   # hair locks must not follow the clavicles
SKIRT_TOP, HEM = 1.06, 0.715


def smoothstep(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def part_of(name):
    import re
    return re.sub(r"_LOD\d$", "", name).replace("CH_Link_", "")


def skin_matrices(arm):
    amw = arm.matrix_world
    amwi = amw.inverted()
    return {pb.name: amw @ pb.matrix @ pb.bone.matrix_local.inverted() @ amwi for pb in arm.pose.bones}


def skin(ob, B, inverse):
    """Linear blend skinning of the mesh's own coordinates with its vertex groups: forward (rest -> pose) or inverse."""
    names = [g.name for g in ob.vertex_groups]
    mw, mwi = ob.matrix_world, ob.matrix_world.inverted()
    for v in ob.data.vertices:
        M = Matrix(((0.0,) * 4,) * 4); tot = 0.0
        for g in v.groups:
            b = B.get(names[g.group])
            if b is not None:
                M = M + b * g.weight; tot += g.weight
        if tot < 1e-6:
            continue
        M = M * (1.0 / tot)
        p = mw @ v.co
        v.co = mwi @ ((M.inverted_safe() if inverse else M) @ p)
    ob.data.update()


def manny_source():
    body = bpy.data.objects["CH_Link_HIGH_MannyBody"]
    dg = bpy.context.evaluated_depsgraph_get()
    ev = body.evaluated_get(dg)
    me = ev.to_mesh()
    mw = body.matrix_world
    verts = [mw @ v.co for v in me.vertices]
    polys = [tuple(p.vertices) for p in me.polygons]
    ev.to_mesh_clear()
    names = [g.name for g in body.vertex_groups]
    vw = [dict((names[g.group], g.weight) for g in v.groups) for v in body.data.vertices]
    return BVHTree.FromPolygons(verts, polys), verts, polys, vw


def weights_for(part, p, src):
    if part in RIGID:
        return {RIGID[part]: 1.0}
    tree, verts, polys, vw = src
    loc, nrm, fi, d = tree.find_nearest(p)
    acc = {}
    if fi is not None:
        ids = polys[fi]
        bw = poly_3d_calc([verts[i] for i in ids], loc)
        for vi, b in zip(ids, bw):
            for n, w in vw[vi].items():
                acc[n] = acc.get(n, 0.0) + w * b
    if part == "Head":
        acc = {k: w for k, w in acc.items() if k in HEAD_BONES} or {"head": 1.0}
    if part == "TunicSkirt":
        # rig_model.transfer_weights skirt rule: pelvis at the belt, blending to the thighs at the hem, then back to
        # the leg weights below it; keeps the skirt from splitting between the legs
        t = max(0.0, min(1.0, (SKIRT_TOP - p.z) / (SKIRT_TOP - HEM + 0.02)))
        leg = 0.55 * t * min(1.0, abs(p.x) / 0.06)
        sk = {"pelvis": 1.0 - leg, ("thigh_l" if p.x > 0 else "thigh_r"): leg}
        blend = smoothstep(0.68, 0.76, p.z)
        mixed = {}
        for k, w in sk.items():
            mixed[k] = mixed.get(k, 0.0) + w * blend
        for k, w in acc.items():
            mixed[k] = mixed.get(k, 0.0) + w * (1 - blend)
        acc = mixed
    ranked = sorted(acc.items(), key=lambda kv: -kv[1])
    top = [(k, w) for k, w in ranked[:4] if w > 0.01]
    tot = sum(w for _, w in top)
    if tot < 1e-6:     # no usable mannequin weight (hair tips far from its head): the part's root bone
        top, tot = [("head" if part == "Head" else "pelvis", 1.0)], 1.0
    return {k: w / tot for k, w in top}


def bind(ob, arm, src):
    part = part_of(ob.name)
    ob.vertex_groups.clear()
    mw = ob.matrix_world
    groups = {}
    for v in ob.data.vertices:
        for bone, w in weights_for(part, mw @ v.co, src).items():
            g = groups.get(bone) or ob.vertex_groups.new(name=bone)
            groups[bone] = g
            g.add([v.index], w, "REPLACE")
    for m in [m for m in ob.modifiers if m.type == "ARMATURE"]:
        ob.modifiers.remove(m)
    ob.parent = arm                                        # mesh data is in world metres: identity basis under
    ob.matrix_parent_inverse = arm.matrix_world.inverted()  # the mannequin's 0.01 empty (scale stays 1)
    ob.matrix_basis = Matrix.Identity(4)
    mod = ob.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    mod.use_deform_preserve_volume = False   # UE skins linearly: review what the engine will show


# SOCKETS (SOCKET_<role>, +Y forward of the attachment, +Z up; rigging-animation.md 4)

SOCKETS = {
    "SOCKET_weapon_r": "hand_r", "SOCKET_weapon_l": "hand_l", "SOCKET_back_shield": "spine_05",
    "SOCKET_back_sword": "spine_05", "SOCKET_head": "head",
}


def make_sockets(arm):
    P = {pb.name: arm.matrix_world @ pb.head for pb in arm.pose.bones}
    shield = bpy.data.objects.get("CH_Link_Shield_LOD0")
    hilt = bpy.data.objects.get("CH_Link_Hilt_LOD0")
    loc = {
        "SOCKET_weapon_r": (P["index_01_r"] + P["pinky_01_r"]) * 0.5,
        "SOCKET_weapon_l": (P["index_01_l"] + P["pinky_01_l"]) * 0.5,
        "SOCKET_back_shield": sum((shield.matrix_world @ v.co for v in shield.data.vertices), Vector()) / len(shield.data.vertices),
        "SOCKET_back_sword": sum((hilt.matrix_world @ v.co for v in hilt.data.vertices), Vector()) / len(hilt.data.vertices),
        "SOCKET_head": P["head"],
    }
    for name, bone in SOCKETS.items():
        e = bpy.data.objects.new(name, None)
        e.empty_display_type = "ARROWS"; e.empty_display_size = 0.08
        L.col("SOCKETS").objects.link(e)
        e.parent = arm; e.parent_type = "BONE"; e.parent_bone = bone
        e.matrix_world = Matrix.Translation(loc[name])
        L.own(e)


# EXTREME POSES: FK keys on top of the rest pose, one action per pose (review_render --action X --frame 1)

POSES = {
    "POSE_rest": {},
    # local -Y raises both arms (hands at z 1.93 from the rest pose, measured)
    "POSE_arms_up": {"upperarm_l": (0, -150, 0), "upperarm_r": (0, -150, 0), "clavicle_l": (0, -20, 0), "clavicle_r": (0, -20, 0)},
    "POSE_crouch": {"thigh_l": (0, 0, 100), "thigh_r": (0, 0, 100), "calf_l": (0, 0, -125), "calf_r": (0, 0, -125),
                    "foot_l": (0, 0, 30), "foot_r": (0, 0, 30), "spine_02": (0, 0, 20)},
    "POSE_lunge": {"spine_01": (35, 0, 0), "spine_03": (15, 0, 0), "upperarm_r": (0, -70, -60), "lowerarm_r": (0, 0, -30),
                   "thigh_l": (0, 0, 70), "calf_l": (0, 0, -70), "thigh_r": (0, 0, -25)},
    "POSE_head_turn": {"neck_01": (30, 0, 0), "head": (35, 0, 20)},
}


PELVIS_DROP = {"POSE_crouch": 0.36, "POSE_lunge": 0.12}   # metres


def make_pose_actions(arm):
    for n in list(POSES):
        a = bpy.data.actions.get(n)
        if a:
            bpy.data.actions.remove(a)
    arm.animation_data_create()
    saved = {pb.name: pb.matrix_basis.copy() for pb in arm.pose.bones}
    for name, rot in POSES.items():
        act = bpy.data.actions.new(name)
        act.use_fake_user = True
        arm.animation_data.action = act
        for pb in arm.pose.bones:
            pb.rotation_mode = "QUATERNION"
            pb.matrix_basis = Matrix.Identity(4)
            e = rot.get(pb.name)
            if e:
                pb.rotation_quaternion = Euler(tuple(math.radians(x) for x in e), "XYZ").to_quaternion()
            pb.keyframe_insert("rotation_quaternion", frame=1)   # every bone keyed: the pose is fully defined
            pb.keyframe_insert("location", frame=1)
        if name in PELVIS_DROP:
            # lower the pelvis in armature space so the feet stay on the ground (armature units are cm)
            pb = arm.pose.bones["pelvis"]
            bpy.context.view_layer.update()
            pb.matrix = Matrix.Translation((0, 0, -PELVIS_DROP[name] / arm.matrix_world.to_scale().z)) @ pb.matrix
            pb.keyframe_insert("location", frame=1)
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.matrix_basis = saved[pb.name]
    L.log("pose actions:", ", ".join(POSES))


def main():
    a = L.argv_after_dashes()
    master = a[a.index("--master") + 1]
    L.open_master(master)
    L.clear_owned()
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    if arm.animation_data:
        arm.animation_data.action = None
    bpy.context.view_layer.update()
    B = skin_matrices(arm)          # build pose (02a) relative to the mannequin reference pose
    meshes = [o for c in ("LOD0", "LOD1", "LOD2") for o in bpy.data.collections[c].objects if o.type == "MESH"]
    for o in meshes:                # rerun: back to build-pose coordinates with the previous weights
        if o.get("unposed"):
            for m in [m for m in o.modifiers if m.type == "ARMATURE"]:
                o.modifiers.remove(m)
            skin(o, B, inverse=False)
            o["unposed"] = False
    src = manny_source()
    for o in meshes:
        bind(o, arm, src)
    for o in meshes:
        skin(o, B, inverse=True)    # mesh data now in the mannequin reference pose; the armature shows the build pose
        o["unposed"] = True
    make_sockets(arm)
    make_pose_actions(arm)
    L.log("bound %d meshes" % len(meshes))
    L.save_master(master)

main()
