# Animations d'épée pour le squelette du mannequin UE5 (garde de combat, taille, estoc, attaque tournoyante,
# incantation, parade, victoire), posées par directions de segments puis exportées en FBX d'animation.
#   Blender -b -P Tools/blender/make_anims.py -- <manny.fbx> <dossier_sortie> [aperçu]
# Repère du personnage (Blender, mannequin importé) : avant = -Y, droite = -X, haut = +Z. Unités : cm (rig aplati).
# Un os « weapon_r » (enfant de hand_r, axe Y le long de la lame) porte l'épée : Link l'a aussi dans son maillage.
import bpy, math, os, sys
from mathutils import Vector, Matrix, Quaternion

ARGS = sys.argv[sys.argv.index("--") + 1:]
MANNY, OUT = ARGS[0], ARGS[1]
PREVIEW = len(ARGS) > 2
os.makedirs(OUT, exist_ok=True)
FPS = 30


def log(*a):
    print("ANIM", *a, flush=True)


def C(f, r, u):
    """Direction dans le repère du personnage (avant, droite, haut) → monde Blender."""
    return Vector((-r, -f, u)).normalized()


# ---------------------------------------------------------------------------------------------------------------
def load_rig():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=MANNY)
    empty = [o for o in bpy.context.scene.objects if o.type == "EMPTY"][0]
    arm = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"][0]
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    # rig aplati en cm (même traitement que make_character.py) : racine « root » à l'échelle 1
    CM = Matrix.Scale(100.0, 4)
    for m in meshes:
        m.data.transform(CM @ m.matrix_world)
        m.parent = None
        m.matrix_world = Matrix.Identity(4)
    M = CM @ arm.matrix_world
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    for eb in arm.data.edit_bones:
        eb.transform(M, scale=True, roll=True)
    add_weapon_bone(arm)
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
    bpy.context.scene.unit_settings.scale_length = 0.01
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return arm, meshes


def add_weapon_bone(arm):
    """weapon_r : point de prise au creux du poing, axe le long de la lame (côté pouce/index)."""
    eb = arm.data.edit_bones
    hand, idx, pinky, mid = eb["hand_r"], eb["index_01_r"], eb["pinky_01_r"], eb["middle_01_r"]
    knuckle = (idx.head - pinky.head).normalized()
    finger = (mid.head - hand.head).normalized()
    grip = (idx.head + pinky.head) * 0.5 - finger * 1.5
    w = eb.new("weapon_r")
    w.head = grip
    w.tail = grip + knuckle * 12.0
    w.roll = 0.0
    w.parent = hand
    w.use_connect = False
    w.use_deform = False


# ---------------------------------------------------------------------------------------------------------------
# Outils de pose (espace armature = monde, l'armature est à l'identité)
def upd():
    bpy.context.view_layer.update()


def rotate_about(pb, q, pivot):
    M = pb.matrix
    pb.matrix = Matrix.Translation(pivot) @ q.to_matrix().to_4x4() @ Matrix.Translation(-pivot) @ M
    upd()


def head(arm, name):
    return arm.pose.bones[name].head.copy()


def aim(arm, bone, child, target):
    """Oriente le segment bone→child vers la direction monde target (rotation minimale)."""
    pb = arm.pose.bones[bone]
    d = (arm.pose.bones[child].head - pb.head)
    if d.length < 1e-6:
        return
    q = d.normalized().rotation_difference(target)
    rotate_about(pb, q, pb.head.copy())


def twist(arm, bone, child, angle_deg):
    pb = arm.pose.bones[bone]
    axis = (arm.pose.bones[child].head - pb.head).normalized()
    rotate_about(pb, Quaternion(axis, math.radians(angle_deg)), pb.head.copy())


def aim_blade(arm, blade_dir):
    """Tourne la main autour de l'axe de ses doigts pour que la lame (Y de weapon_r) vise au mieux blade_dir."""
    hand = arm.pose.bones["hand_r"]
    axis = (arm.pose.bones["middle_01_r"].head - hand.head).normalized()
    wy = (arm.pose.bones["weapon_r"].matrix.to_3x3() @ Vector((0, 1, 0))).normalized()
    a = wy - axis * wy.dot(axis)
    b = blade_dir - axis * blade_dir.dot(axis)
    if a.length < 1e-4 or b.length < 1e-4:
        return
    a.normalize()
    b.normalize()
    ang = math.atan2(axis.dot(a.cross(b)), a.dot(b))
    rotate_about(hand, Quaternion(axis, ang), hand.head.copy())


FINGERS = ["index", "middle", "ring", "pinky"]


def fist(arm, side, amount=1.0):
    """Ferme la main (doigts enroulés vers la paume, pouce replié)."""
    hand = arm.pose.bones["hand_" + side]
    knuckle = (arm.pose.bones["index_01_" + side].head - arm.pose.bones["pinky_01_" + side].head).normalized()
    thumb_ref = arm.pose.bones["thumb_01_" + side].head.copy()
    for f in FINGERS:
        for seg, ang in (("01", 70), ("02", 80), ("03", 55)):
            name = "%s_%s_%s" % (f, seg, side)
            if name not in arm.pose.bones:
                continue
            pb = arm.pose.bones[name]
            nxt = "%s_%02d_%s" % (f, int(seg) + 1, side)
            tip_ref = arm.pose.bones[nxt].head.copy() if nxt in arm.pose.bones else pb.tail.copy()
            best = None
            for sgn in (1, -1):
                q = Quaternion(knuckle, math.radians(ang * amount * sgn))
                p = pb.head + q @ (tip_ref - pb.head)
                dist = (p - thumb_ref).length
                if best is None or dist < best[0]:
                    best = (dist, q)
            rotate_about(pb, best[1], pb.head.copy())
    for seg in ("01", "02"):
        name = "thumb_%s_%s" % (seg, side)
        pb = arm.pose.bones[name]
        rotate_about(pb, Quaternion(knuckle, math.radians(-25 * amount if side == "r" else 25 * amount)), pb.head.copy())


SPINE = ["spine_01", "spine_02", "spine_03", "spine_04", "spine_05"]
KEYED = ["pelvis"] + SPINE + ["neck_01", "neck_02", "head", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "clavicle_l", "upperarm_l",
         "lowerarm_l", "hand_l", "thigh_r", "calf_r", "foot_r", "ball_r", "thigh_l", "calf_l", "foot_l", "ball_l"]


def rest(arm):
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    upd()


def apply_pose(arm, P):
    """P : dictionnaire de pose. Directions en repère du personnage, tournées avec le bassin (yaw)."""
    rest(arm)
    yaw = P.get("yaw", 0.0)            # rotation du corps entier (attaque tournoyante), + = vers la droite
    Ry = Quaternion(Vector((0, 0, 1)), math.radians(-yaw))
    def D(v):
        return Ry @ C(*v)
    pel = arm.pose.bones["pelvis"]
    offset = Ry @ Vector((-P.get("pel_r", 0.0), -P.get("pel_f", 0.0), 0.0)) + Vector((0, 0, -P.get("drop", 0.0)))
    pel.matrix = Matrix.Translation(offset) @ pel.matrix
    upd()
    if yaw:
        rotate_about(pel, Ry, pel.head.copy())
    # colonne : torsion (yaw), inclinaison avant (pitch), inclinaison latérale (roll) réparties
    for name in SPINE:
        pb = arm.pose.bones[name]
        if P.get("twist"):
            rotate_about(pb, Quaternion(Vector((0, 0, 1)), math.radians(-P["twist"] / len(SPINE))), pb.head.copy())
        if P.get("lean"):
            rotate_about(pb, Quaternion(Ry @ Vector((1, 0, 0)), math.radians(P["lean"] / len(SPINE))), pb.head.copy())
        if P.get("side"):
            rotate_about(pb, Quaternion(Ry @ Vector((0, -1, 0)), math.radians(P["side"] / len(SPINE))), pb.head.copy())
    if P.get("look"):
        for name in ("neck_01", "head"):
            pb = arm.pose.bones[name]
            rotate_about(pb, Quaternion(Vector((0, 0, 1)), math.radians(-P["look"] * 0.5)), pb.head.copy())
    if P.get("nod"):
        pb = arm.pose.bones["head"]
        rotate_about(pb, Quaternion(Ry @ Vector((1, 0, 0)), math.radians(P["nod"])), pb.head.copy())
    # jambes
    for s in ("l", "r"):
        if ("thigh_" + s) in P:
            aim(arm, "thigh_" + s, "calf_" + s, D(P["thigh_" + s]))
        if ("calf_" + s) in P:
            aim(arm, "calf_" + s, "foot_" + s, D(P["calf_" + s]))
        aim(arm, "foot_" + s, "ball_" + s, D(P.get("foot_" + s, (1.0, 0.0, -0.45))))
    # bras
    for s in ("r", "l"):
        if ("upper_" + s) in P:
            aim(arm, "upperarm_" + s, "lowerarm_" + s, D(P["upper_" + s]))
        if ("lower_" + s) in P:
            aim(arm, "lowerarm_" + s, "hand_" + s, D(P["lower_" + s]))
        if ("hand_" + s) in P:
            aim(arm, "hand_" + s, "middle_01_" + s, D(P["hand_" + s]))
    if "blade" in P:
        aim_blade(arm, D(P["blade"]))
    if "palm_l" in P:  # rotation du poignet gauche (bouclier face à l'ennemi)
        twist(arm, "hand_l", "middle_01_l", P["palm_l"])
    fist(arm, "r")
    fist(arm, "l", 0.8)


def key(arm, frame):
    for name in KEYED + ["%s_%s_%s" % (f, s, side) for f in FINGERS for s in ("01", "02", "03") for side in ("l", "r")] + \
            ["thumb_01_r", "thumb_02_r", "thumb_01_l", "thumb_02_l"]:
        if name not in arm.pose.bones:
            continue
        pb = arm.pose.bones[name]
        pb.keyframe_insert("rotation_quaternion", frame=frame)
        if name == "pelvis":
            pb.keyframe_insert("location", frame=frame)


def make_action(arm, name, keys):
    """keys : liste (image, pose). Crée l'action et la pose image par image."""
    act = bpy.data.actions.new(name)
    arm.animation_data_create()
    arm.animation_data.action = act
    for frame, P in keys:
        apply_pose(arm, P)
        key(arm, frame)
    act.frame_range = (keys[0][0], keys[-1][0])
    log("action", name, "images", keys[-1][0])
    return act


# ---------------------------------------------------------------------------------------------------------------
# Poses (avant, droite, haut)
STANCE = {
    "drop": 6.0, "lean": 6.0, "twist": 14.0, "look": -10.0,
    "thigh_l": (0.35, -0.12, -1.0), "calf_l": (0.02, -0.05, -1.0),
    "thigh_r": (-0.25, 0.18, -1.0), "calf_r": (-0.45, 0.1, -1.0), "foot_r": (0.9, 0.35, -0.4),
    "upper_r": (0.25, 0.3, -0.9), "lower_r": (0.85, 0.05, -0.35), "hand_r": (0.8, -0.05, -0.2), "blade": (0.55, -0.25, 0.8),
    "upper_l": (0.35, -0.35, -0.85), "lower_l": (0.8, 0.45, -0.15), "hand_l": (0.7, 0.6, 0.1), "palm_l": 0.0,
}


def pose(**kw):
    P = dict(STANCE)
    P.update(kw)
    return P


def breathe(d):
    return pose(lean=STANCE["lean"] + d, drop=STANCE["drop"] + d * 0.6)


ACTIONS = {
    "A_Sword_Idle": [(0, breathe(0)), (30, breathe(1.5)), (60, breathe(0))],
    "A_Sword_Slash": [
        (0, pose()),
        (8, pose(twist=40, lean=2, drop=8, upper_r=(-0.15, 0.55, 0.8), lower_r=(0.05, 0.25, 1.0), hand_r=(0.2, 0.15, 0.95), blade=(-0.7, 0.35, 0.45))),
        (12, pose(twist=-5, lean=14, drop=12, thigh_l=(0.55, -0.1, -0.85), calf_l=(-0.05, -0.05, -1.0),
                  upper_r=(0.9, 0.1, 0.25), lower_r=(1.0, -0.15, -0.05), hand_r=(1.0, 0.0, -0.1), blade=(0.1, -1.0, 0.0))),
        (16, pose(twist=-38, lean=16, drop=12, thigh_l=(0.55, -0.1, -0.85), calf_l=(-0.05, -0.05, -1.0),
                  upper_r=(0.55, -0.55, -0.55), lower_r=(0.35, -0.85, -0.4), hand_r=(0.25, -0.9, -0.35), blade=(-0.25, -0.6, -0.8))),
        (22, pose(twist=-20, lean=10, drop=9, upper_r=(0.5, -0.2, -0.8), lower_r=(0.8, -0.3, -0.5), hand_r=(0.8, -0.35, -0.3), blade=(0.4, -0.5, 0.2))),
        (32, pose()),
    ],
    "A_Sword_Thrust": [
        (0, pose()),
        (8, pose(twist=30, lean=4, drop=10, upper_r=(-0.55, 0.45, -0.65), lower_r=(0.9, 0.05, 0.15), hand_r=(0.5, 0.0, -0.85), blade=(0.85, 0.0, 0.5))),
        (12, pose(twist=-12, lean=18, drop=16, thigh_l=(0.75, -0.1, -0.7), calf_l=(-0.1, -0.05, -1.0), thigh_r=(-0.55, 0.15, -0.85), calf_r=(-0.75, 0.1, -0.65),
                  upper_r=(1.0, 0.05, 0.05), lower_r=(1.0, 0.0, 0.02), hand_r=(0.45, 0.0, -0.9), blade=(0.9, 0.0, 0.45))),
        (18, pose(twist=-8, lean=16, drop=14, thigh_l=(0.75, -0.1, -0.7), calf_l=(-0.1, -0.05, -1.0), thigh_r=(-0.55, 0.15, -0.85), calf_r=(-0.75, 0.1, -0.65),
                  upper_r=(1.0, 0.05, 0.0), lower_r=(1.0, 0.0, 0.0), hand_r=(0.45, 0.0, -0.9), blade=(0.9, 0.0, 0.45))),
        (30, pose()),
    ],
    "A_Sword_Spin": [
        (0, pose()),
        (8, pose(twist=35, lean=12, drop=18, upper_r=(-0.3, 0.9, -0.2), lower_r=(-0.4, 0.9, -0.1), hand_r=(-0.4, 0.9, 0.0), blade=(-0.9, 0.3, -0.2))),
        (11, pose(yaw=-90, twist=0, lean=10, drop=16, upper_r=(0.0, 1.0, -0.1), lower_r=(0.0, 1.0, 0.0), hand_r=(0.0, 1.0, 0.0), blade=(0.95, 0.2, 0.1))),
        (14, pose(yaw=-180, twist=0, lean=10, drop=16, upper_r=(0.0, 1.0, -0.1), lower_r=(0.0, 1.0, 0.0), hand_r=(0.0, 1.0, 0.0), blade=(0.95, 0.2, 0.1))),
        (17, pose(yaw=-270, twist=0, lean=10, drop=16, upper_r=(0.0, 1.0, -0.1), lower_r=(0.0, 1.0, 0.0), hand_r=(0.0, 1.0, 0.0), blade=(0.95, 0.2, 0.1))),
        (20, pose(yaw=-360, twist=0, lean=10, drop=16, upper_r=(0.0, 1.0, -0.1), lower_r=(0.0, 1.0, 0.0), hand_r=(0.0, 1.0, 0.0), blade=(0.95, 0.2, 0.1))),
        (24, pose(yaw=-385, twist=-20, lean=12, drop=14, upper_r=(0.6, -0.4, -0.5), lower_r=(0.5, -0.7, -0.4), hand_r=(0.4, -0.8, -0.3), blade=(0.2, -0.7, -0.6))),
        (36, pose(yaw=-360)),
    ],
    "A_Cast": [
        (0, pose()),
        (10, pose(twist=0, lean=-4, drop=4, nod=-10, upper_r=(0.75, 0.25, 0.55), lower_r=(0.6, 0.05, 0.8), hand_r=(0.4, 0.0, 0.95), blade=(0.2, 0.0, 1.0),
                  upper_l=(0.75, -0.25, 0.55), lower_l=(0.7, -0.05, 0.7), hand_l=(0.6, 0.0, 0.8))),
        (22, pose(twist=0, lean=2, drop=6, nod=-6, upper_r=(0.9, 0.2, 0.35), lower_r=(0.95, 0.0, 0.3), hand_r=(0.8, 0.0, 0.6), blade=(0.3, 0.0, 1.0),
                  upper_l=(0.9, -0.2, 0.35), lower_l=(0.95, 0.0, 0.3), hand_l=(0.9, 0.1, 0.4))),
        (34, pose()),
    ],
    "A_Guard": [
        (0, pose()),
        (8, pose(drop=14, lean=10, twist=5, upper_l=(0.7, -0.45, 0.05), lower_l=(0.35, 0.85, 0.35), hand_l=(0.2, 0.95, 0.3),
                 upper_r=(0.1, 0.45, -0.85), lower_r=(0.6, 0.2, -0.7), hand_r=(0.6, 0.15, -0.6), blade=(0.8, -0.2, 0.5))),
        (26, pose(drop=14, lean=10, twist=5, upper_l=(0.7, -0.45, 0.05), lower_l=(0.35, 0.85, 0.35), hand_l=(0.2, 0.95, 0.3),
                  upper_r=(0.1, 0.45, -0.85), lower_r=(0.6, 0.2, -0.7), hand_r=(0.6, 0.15, -0.6), blade=(0.8, -0.2, 0.5))),
        (34, pose()),
    ],
    "A_Victory": [
        (0, pose()),
        (10, pose(drop=2, lean=-6, twist=-5, nod=-12, look=0, upper_r=(0.15, 0.3, 0.95), lower_r=(0.05, 0.05, 1.0), hand_r=(0.1, -0.95, 0.3), blade=(0.0, 0.05, 1.0),
                  upper_l=(0.1, -0.5, -0.85), lower_l=(0.2, -0.3, -0.9), hand_l=(0.2, -0.2, -0.95))),
        (40, pose(drop=1, lean=-5, twist=-5, nod=-12, look=0, upper_r=(0.15, 0.3, 0.95), lower_r=(0.05, 0.05, 1.0), hand_r=(0.1, -0.95, 0.3), blade=(0.0, 0.05, 1.0),
                  upper_l=(0.1, -0.5, -0.85), lower_l=(0.2, -0.3, -0.9), hand_l=(0.2, -0.2, -0.95))),
    ],
}


def export_action(arm, act):
    arm.animation_data.action = act
    bpy.context.scene.frame_start = int(act.frame_range[0])
    bpy.context.scene.frame_end = int(act.frame_range[1])
    bpy.context.scene.render.fps = FPS
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    path = os.path.join(OUT, act.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE"}, add_leaf_bones=False, apply_unit_scale=True,
                             global_scale=1.0, apply_scale_options="FBX_SCALE_NONE", use_armature_deform_only=False,
                             bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                             bake_anim_simplify_factor=0.0, path_mode="STRIP")
    log("export", path)


def preview(arm, meshes, acts):
    """Planche de contrôle : quelques images de chaque action, épée témoin sur weapon_r."""
    sc = bpy.context.scene
    bpy.ops.mesh.primitive_cube_add(size=1)
    sword = bpy.context.active_object
    sword.scale = (2.0, 95.0, 5.0)
    bpy.ops.object.transform_apply(scale=True)
    for v in sword.data.vertices:
        v.co.y += 47.5
    sword.parent = arm
    sword.parent_type = "BONE"
    sword.parent_bone = "weapon_r"
    sword.matrix_parent_inverse = Matrix.Identity(4)
    # parent osseux : l'origine est au bout de l'os → recaler à la tête
    sword.location = (0, -arm.data.bones["weapon_r"].length, 0)
    mat = bpy.data.materials.new("Blade")
    mat.diffuse_color = (0.85, 0.9, 1.0, 1)
    sword.data.materials.append(mat)
    for m in meshes:
        mm = bpy.data.materials.new("Body")
        mm.diffuse_color = (0.55, 0.62, 0.5, 1)
        m.data.materials.clear()
        m.data.materials.append(mm)
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "MATERIAL"
    sc.render.resolution_x, sc.render.resolution_y = 420, 520
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 260
    # vue 3/4 avant droite
    cam.location = Vector((-380, -380, 150))
    cam.rotation_euler = (math.radians(88), 0, math.radians(-45))
    shots = []
    for act in acts:
        arm.animation_data.action = act
        f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])
        frames = [f for f, _ in ACTIONS[act.name]][:6]
        for f in frames:
            sc.frame_set(f)
            p = os.path.join(OUT, "prev_%s_%02d.png" % (act.name, f))
            sc.render.filepath = p
            bpy.ops.render.render(write_still=True)
            shots.append(p)
    log("aperçus", len(shots))


def main():
    arm, meshes = load_rig()
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    acts = [make_action(arm, n, k) for n, k in ACTIONS.items()]
    bpy.ops.object.mode_set(mode="OBJECT")
    rest(arm)
    if PREVIEW:
        preview(arm, meshes, acts)
    for a in acts:
        export_action(arm, a)
    log("TERMINE")


main()
