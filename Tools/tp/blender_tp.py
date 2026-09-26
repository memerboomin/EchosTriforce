# Reconstruit un personnage de Twilight Princess dans Blender à partir de l'ISO de l'utilisateur, puis l'exporte pour
# Unreal : maillage squelettique (FBX), objets tenus (FBX, même squelette), animations (un FBX par animation),
# textures (PNG) et manifest.json (matériaux, animations) lu par Tools/ue/import_tp.py.
#   Blender -b -P Tools/tp/blender_tp.py -- --preset link --out Tools/art/tp/link [--anims all|none|a,b,c]
#       [--preview anim:frame,...] [--blend]
# Repère : J3D a Y en haut et le personnage regarde +Z ; Blender a Z en haut et le personnage regarde -Y (comme le
# mannequin importé, cf. Tools/blender/make_anims.py). Unités : cm (scale_length 0.01, export FBX à l'échelle 1).
import argparse
import json
import math
import os
import struct
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import presets  # noqa: E402
from tplib import bck, gxtex, j3d, rarc  # noqa: E402
from tplib.disc import Disc  # noqa: E402

FPS = 30
RC = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))  # J3D -> Blender


def log(*a):
    print("TP", *a, flush=True)


def M(rows):
    return Matrix([list(r) for r in rows])


# ------------------------------------------------------------------------------------------------ données sources
class Source:
    def __init__(self, iso):
        self.disc = Disc(iso)
        self._arcs = {}

    def arc(self, name):
        if name not in self._arcs:
            self._arcs[name] = rarc.parse(self.disc.read("res/Object/%s.arc" % name))
        return self._arcs[name]

    def file(self, arc, path):
        files = self.arc(arc)
        if path == "*":  # premier modèle de l'archive
            path = sorted(k for k in files if k.endswith((".bmd", ".bdl")))[0]
        return files[path]


class Skeleton:
    """Os de tous les modèles assemblés : monde de repos J3D, parent, nom unique."""

    def __init__(self):
        self.names = []
        self.parents = []
        self.world = []  # Matrix J3D
        self.local = []  # Matrix J3D relative au parent

    def add(self, name, parent, world):
        self.names.append(name)
        self.parents.append(parent)
        self.world.append(world)
        self.local.append(world if parent < 0 else self.world[parent].inverted() @ world)
        return len(self.names) - 1

    def index(self, name):
        return self.names.index(name)


# ------------------------------------------------------------------------------------------------ scène
def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.scale_length = 0.01
    sc.render.fps = FPS


def build_armature(name, skel):
    data = bpy.data.armatures.new(name)
    arm = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    ebs = []
    for i, n in enumerate(skel.names):
        eb = data.edit_bones.new(n)
        # longueur : distance au premier enfant, sinon 4 cm
        kids = [k for k, p in enumerate(skel.parents) if p == i]
        length = 4.0
        if kids:
            d = (skel.world[kids[0]].translation - skel.world[i].translation).length
            if d > 0.5:
                length = d
        eb.head = (0, 0, 0)
        eb.tail = (0, length, 0)
        eb.matrix = RC @ skel.world[i]
        ebs.append(eb)
    for i, p in enumerate(skel.parents):
        if p >= 0:
            ebs[i].parent = ebs[p]
            ebs[i].use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return arm


def save_texture(tex, out_dir, written):
    fname = "T_TP_%s.png" % tex.name.replace(".", "_")
    if fname not in written:
        tex.save_png(os.path.join(out_dir, fname))
        written[fname] = tex
    return fname


def alpha_binary(tex):
    """Alpha de découpe (presque tout à 0 ou à 255) ; un alpha intermédiaire sert souvent à doser un reflet (lame)."""
    px = tex.rgba
    n = len(px) // 4
    mid = sum(1 for i in range(3, len(px), 4) if 24 < px[i] < 232)
    return mid <= n * 0.08


def alpha_mode(mat, tex):
    if tex is None or not tex.has_alpha():
        return "opaque"
    if mat.blend:
        return "blend"
    if mat.alpha_test and alpha_binary(tex):
        return "mask"
    return "opaque"


def blender_material(name, tex_path, mode, two_sided):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Roughness"].default_value = 0.85
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    if tex_path:
        img = bpy.data.images.load(tex_path, check_existing=True)
        tn = nt.nodes.new("ShaderNodeTexImage")
        tn.image = img
        nt.links.new(tn.outputs["Color"], bsdf.inputs["Base Color"])
        if mode != "opaque":
            nt.links.new(tn.outputs["Alpha"], bsdf.inputs["Alpha"])
    mat.surface_render_method = "BLENDED" if mode == "blend" else "DITHERED"
    mat.use_backface_culling = not two_sided
    return mat


def build_mesh(obj_name, arm, skel, parts, out_dir, written, manifest_mats, mat_prefix=""):
    """parts : [(modèle j3d, matrice monde d'attache J3D, table os locaux -> os globaux)]"""
    verts, nrms, uvs, cols, faces, fmats, weights = [], [], [], [], [], [], []
    mat_slots = []
    has_color = False
    for model, attach, bone_map in parts:
        rot = attach.to_3x3()
        for shape in model.shapes:
            if not shape.triangles:
                continue
            mat = model.materials[shape.material] if 0 <= shape.material < len(model.materials) else None
            tex = model.textures[mat.main_texture] if mat and 0 <= mat.main_texture < len(model.textures) else None
            # préfixe : deux salles d'un décor peuvent avoir des matériaux du même nom avec d'autres textures
            mname = mat_prefix + (mat.name if mat else "TP_default")
            if mname not in mat_slots:
                mat_slots.append(mname)
                mode = alpha_mode(mat, tex) if mat else "opaque"
                tfile = save_texture(tex, out_dir, written) if tex else None
                two = (mat.cull == 0) if mat else False
                manifest_mats[mname] = {"texture": tfile, "alpha": mode, "two_sided": two,
                                        "env_map": bool(mat and mat.env_map),
                                        "vertex_color": bool(mat and mat.uses_vertex_color),
                                        "color": list(mat.mat_color) if mat else [255, 255, 255, 255],
                                        "wrap": [tex.wrap_s, tex.wrap_t] if tex else [0, 0]}
            slot = mat_slots.index(mname)
            base = len(verts)
            uvset = shape.uvs[mat.main_uv] if mat and shape.uvs[mat.main_uv] else shape.uvs[0]
            for k, p in enumerate(shape.positions):
                verts.append(RC @ (attach @ Vector(p)))
                n = shape.normals[k] if k < len(shape.normals) else (0.0, 1.0, 0.0)
                nrms.append((RC.to_3x3() @ (rot @ Vector(n))).normalized())
                uv = uvset[k] if k < len(uvset) else (0.0, 0.0)
                uvs.append((uv[0], 1.0 - uv[1]))
                if shape.colors:
                    has_color = True
                    c = shape.colors[k]
                    cols.append(tuple(x / 255.0 for x in c))
                else:
                    cols.append((1.0, 1.0, 1.0, 1.0))
                weights.append([(bone_map[j], w) for j, w in shape.weights[k]])
            # sens des triangles : celui qui s'accorde avec les normales des sommets
            agree = 0
            tris = []
            seen = set()
            for a, b, c in shape.triangles:
                if a == b or b == c or a == c:
                    continue
                key = tuple(sorted((a, b, c)))
                if key in seen:
                    continue
                seen.add(key)
                va, vb, vc = verts[base + a], verts[base + b], verts[base + c]
                g = (vb - va).cross(vc - va)
                s = nrms[base + a] + nrms[base + b] + nrms[base + c]
                agree += 1 if g.dot(s) >= 0 else -1
                tris.append((a, b, c))
            flip = agree < 0
            for a, b, c in tris:
                faces.append((base + a, base + c, base + b) if flip else (base + a, base + b, base + c))
                fmats.append(slot)
    me = bpy.data.meshes.new(obj_name)
    me.from_pydata([tuple(v) for v in verts], [], faces)
    me.polygons.foreach_set("material_index", fmats)
    uvl = me.uv_layers.new(name="UVMap")
    loop_vi = [0] * len(me.loops)
    me.loops.foreach_get("vertex_index", loop_vi)
    flat = []
    for vi in loop_vi:
        flat.extend(uvs[vi])
    uvl.data.foreach_set("uv", flat)
    if has_color:
        # octets d'origine à l'identique (color_srgb) : le FBX les exporte tels quels (colors_type SRGB)
        ca = me.color_attributes.new("Col", "BYTE_COLOR", "POINT")
        flatc = []
        for c in cols:
            flatc.extend(c)
        ca.data.foreach_set("color_srgb", flatc)
    me.normals_split_custom_set_from_vertices([tuple(n) for n in nrms])
    for mname in mat_slots:
        info = manifest_mats[mname]
        tpath = os.path.join(out_dir, info["texture"]) if info["texture"] else None
        me.materials.append(blender_material(mname, tpath, info["alpha"], info["two_sided"]))
    obj = bpy.data.objects.new(obj_name, me)
    bpy.context.scene.collection.objects.link(obj)
    groups = {}
    for vi, wl in enumerate(weights if arm is not None else []):
        for bi, w in wl:
            if w <= 0.0:
                continue
            g = groups.get(bi)
            if g is None:
                g = groups[bi] = obj.vertex_groups.new(name=skel.names[bi])
            g.add([vi], w, "REPLACE")
    if arm is None:  # maillage statique (décor)
        return obj
    obj.parent = arm
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    return obj


# ------------------------------------------------------------------------------------------------ animations
def bone_basis_frames(anim, skel, bone_map, frames):
    """{os global: [Matrix basis par image]} pour les os que l'animation pilote."""
    out = {}
    n = min(anim.joint_count, len(bone_map))
    for j in range(n):
        bi = bone_map[j]
        inv_rest = skel.local[bi].inverted()
        mats = []
        for f in frames:
            s, r, t = anim.sample(j, f)
            mats.append(inv_rest @ M(j3d.mat_srt(s, r, t)))
        out[bi] = mats
    return out


def offset_bases(skel, P):
    """Poses fixes d'os que le jeu anime par physique (bonnet) : rotation autour de l'axe latéral du personnage."""
    out = {}
    for bname, deg in P.get("pose_offsets", []):
        if bname not in skel.names:
            continue
        bi = skel.index(bname)
        axis = skel.world[bi].to_3x3().normalized().inverted() @ Vector((1, 0, 0))
        out[bi] = Matrix.Rotation(math.radians(deg), 4, axis)
    return out


def make_action(arm, skel, name, tracks, nframes):
    act = bpy.data.actions.new(name)
    slot = act.slots.new(id_type="OBJECT", name=arm.name)
    layer = act.layers.new("Layer")
    strip = layer.strips.new(type="KEYFRAME")
    cb = strip.channelbag(slot, ensure=True)
    frames = list(range(nframes))
    for bi, mats in tracks.items():
        bname = skel.names[bi]
        locs, quats, scales = [], [], []
        prev = None
        for mtx in mats:
            loc, q, sc = mtx.decompose()
            if prev is not None and prev.dot(q) < 0:
                q = -q
            prev = q
            locs.append(loc)
            quats.append(q)
            scales.append(sc)
        grp = cb.groups.new(bname)
        for prop, vals, count in (("location", locs, 3), ("rotation_quaternion", quats, 4), ("scale", scales, 3)):
            path = 'pose.bones["%s"].%s' % (bname, prop)
            for i in range(count):
                fc = cb.fcurves.new(path, index=i)
                fc.group = grp
                fc.keyframe_points.add(len(frames))
                co = []
                for f, v in zip(frames, vals):
                    co.extend((float(f), float(v[i])))
                fc.keyframe_points.foreach_set("co", co)
                fc.keyframe_points.foreach_set("interpolation", [1] * len(frames))  # LINEAR
                fc.update()
    return act, slot


def assign_action(arm, act, slot):
    if arm.animation_data is None:
        arm.animation_data_create()
    arm.animation_data.action = act
    arm.animation_data.action_slot = slot


def clear_pose(arm):
    for pb in arm.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)


# ------------------------------------------------------------------------------------------------ export
def select_only(objs):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]


def export_fbx(path, objs, anim=False):
    select_only(objs)
    types = {"ARMATURE", "MESH"} if not anim else {"ARMATURE"}
    kw = dict(filepath=path, use_selection=True, object_types=types, add_leaf_bones=False, apply_unit_scale=True,
              global_scale=1.0, apply_scale_options="FBX_SCALE_NONE", use_armature_deform_only=False,
              mesh_smooth_type="OFF", path_mode="STRIP")
    if anim:
        kw.update(bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
                  bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0)
    else:
        kw.update(bake_anim=False)
    bpy.ops.export_scene.fbx(**kw)


def export_static_fbx(path, obj):
    select_only([obj])
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             global_scale=1.0, apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="OFF",
                             colors_type="SRGB", bake_anim=False, path_mode="STRIP")


def export_stage(src, stage, name, rooms, out, preview, no_fbx, start_room=None):
    """Salles d'un décor (res/Stage/<stage>/Rxx_00.arc) : un maillage statique par modèle, en coordonnées monde."""
    written = {}
    manifest = {"kind": "stage", "name": name, "stage": stage, "materials": {}, "meshes": {}}
    objs = []
    keys = sorted(k for k in src.disc.files if k.startswith("res/Stage/%s/R" % stage) and k.endswith(".arc"))
    for key in keys:
        room = int(os.path.basename(key)[1:3])
        if rooms and room not in rooms:
            continue
        files = rarc.parse(src.disc.read(key))
        for path in sorted(files):
            if not path.endswith((".bmd", ".bdl")):
                continue
            base = os.path.basename(path).split(".")[0]  # model, model1, model2…
            layer = int(base[5:]) if base[5:].isdigit() else 0
            model = j3d.load(files[path])
            if not any(sh.triangles for sh in model.shapes):
                continue
            oname = "SM_%s_R%02d" % (name, room) + ("_L%d" % layer if layer else "")
            obj = build_mesh(oname, None, None, [(model, Matrix.Identity(4), [0] * len(model.joints))], out, written,
                             manifest["materials"], mat_prefix="R%02d_" % room)
            objs.append(obj)
            translucent = all(manifest["materials"][m.name]["alpha"] == "blend" for m in obj.data.materials)
            manifest["meshes"][oname] = {"file": oname + ".fbx", "room": room, "layer": layer,
                                         "materials": [m.name for m in obj.data.materials], "translucent": translucent,
                                         "anim": [k for k in files if k.startswith("btk/%s." % base)]}
            if not no_fbx:
                export_static_fbx(os.path.join(out, oname + ".fbx"), obj)
            log("salle", oname, len(obj.data.polygons), "triangles")
    for info in manifest["materials"].values():
        info["vertex_color"] = True
    # départ de Link : premier point PLYR de la première salle (dzr), converti en repère Unreal
    # (J3D (x, y, z) -> Blender (x, -z, y) -> FBX -> Unreal (x, z, y) ; lacet J3D autour de Y, 0 = vers +Z)
    for key in keys:
        room = int(os.path.basename(key)[1:3])
        if (rooms and room not in rooms) or (start_room is not None and room != start_room):
            continue
        files = rarc.parse(src.disc.read(key))
        dzr = files.get("dzr/room.dzr")
        if not dzr:
            continue
        n = struct.unpack(">I", dzr[:4])[0]
        for i in range(n):
            tag, cnt, off = struct.unpack(">4sII", dzr[4 + i * 12:16 + i * 12])
            if tag == b"PLYR" and cnt:
                x, y, z = struct.unpack(">3f", dzr[off + 12:off + 24])
                ay = struct.unpack(">h", dzr[off + 26:off + 28])[0] * 360.0 / 65536.0
                fx, fz = math.sin(math.radians(ay)), math.cos(math.radians(ay))
                manifest["start"] = {"room": room, "j3d": [x, y, z], "ue": [x, z, y],
                                     "yaw": math.degrees(math.atan2(fz, fx))}
                break
        if "start" in manifest:
            break
    with open(os.path.join(out, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1, ensure_ascii=False)
    if preview and objs:
        lo = Vector((min(v.co.x for o in objs for v in o.data.vertices), min(v.co.y for o in objs for v in o.data.vertices),
                     min(v.co.z for o in objs for v in o.data.vertices)))
        hi = Vector((max(v.co.x for o in objs for v in o.data.vertices), max(v.co.y for o in objs for v in o.data.vertices),
                     max(v.co.z for o in objs for v in o.data.vertices)))
        c = (lo + hi) / 2
        size = (hi - lo).length
        bpy.context.scene.display.shading.color_type = "TEXTURE"
        render_preview(os.path.join(out, "preview_overview.png"), None, objs, eye=tuple(c + Vector((size * 0.35, -size * 0.55, size * 0.45))),
                       target=tuple(c), lens=35, res=(1400, 900))
    log("décor terminé", len(objs), "maillages")


def render_preview(path, arm, meshes, eye=(0, -380, 90), target=(0, 0, 80), lens=50, res=(900, 900)):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "TEXTURE"
    sc.display.shading.show_backface_culling = False
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.film_transparent = False
    cam = bpy.data.objects.get("PreviewCam")
    if cam is None:
        cam = bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam"))
        sc.collection.objects.link(cam)
    cam.data.lens = lens
    cam.data.clip_start = 5.0
    cam.data.clip_end = 1.0e6
    cam.location = eye
    d = Vector(target) - Vector(eye)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


# ------------------------------------------------------------------------------------------------ programme
def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--preset", default="link")
    ap.add_argument("--out", required=True)
    ap.add_argument("--iso", default=presets.ISO)
    ap.add_argument("--anims", default="all")
    ap.add_argument("--preview", default="")
    ap.add_argument("--blend", action="store_true")
    ap.add_argument("--no-fbx", action="store_true")
    ap.add_argument("--stage", default="", help="décor : dossier res/Stage (ex. D_MN01)")
    ap.add_argument("--name", default="", help="décor : nom des assets (ex. TPLakebed)")
    ap.add_argument("--rooms", default="", help="décor : salles à convertir (ex. 0,1,2)")
    ap.add_argument("--start-room", type=int, default=None, help="décor : salle du départ de Link")
    a = ap.parse_args(argv)
    if a.stage:
        out = os.path.abspath(a.out)
        os.makedirs(out, exist_ok=True)
        reset_scene()
        rooms = [int(x) for x in a.rooms.split(",") if x]
        export_stage(Source(a.iso), a.stage, a.name or a.stage, rooms, out, bool(a.preview), a.no_fbx, a.start_room)
        return
    P = presets.PRESETS[a.preset]
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    os.makedirs(os.path.join(out, "anims"), exist_ok=True)
    src = Source(a.iso)
    reset_scene()

    # squelette assemblé
    skel = Skeleton()
    parts = []
    for spec in P["models"]:
        model = j3d.load(src.file(spec["arc"], spec["file"]))
        attach_bone = skel.index(spec["attach"]) if spec.get("attach") else -1
        attach = skel.world[attach_bone] if attach_bone >= 0 else Matrix.Identity(4)
        bone_map = []
        for j in model.joints:
            name = j.name if j.name not in skel.names else "%s_%s" % (os.path.basename(spec["file"]).split(".")[0], j.name)
            parent = bone_map[j.parent] if j.parent >= 0 else attach_bone
            bone_map.append(skel.add(name, parent, attach @ M(j.world)))
        parts.append((model, attach, bone_map, spec))
    name = P["name"]
    arm = build_armature(name, skel)
    written = {}
    manifest = {"name": name, "bones": skel.names, "materials": {}, "meshes": {}, "anims": {}}
    body = build_mesh("SKM_" + name, arm, skel, [(m, at, bm) for m, at, bm, _ in parts], out, written,
                      manifest["materials"])
    manifest["meshes"]["SKM_" + name] = {"file": "SKM_%s.fbx" % name, "materials": [s.name for s in body.data.materials]}
    log("maillage", body.name, len(body.data.vertices), "sommets", len(body.data.polygons), "triangles")

    props = []
    for spec in P.get("props", []):
        model = j3d.load(src.file(spec["arc"], spec["file"]))
        bi = skel.index(spec["bone"])
        bone_map = [bi] * len(model.joints)
        at = skel.world[bi]
        if spec.get("offset"):
            # décalage du jeu (transM puis XYZrotM, cf. d_a_alink.cpp setItemMatrix)
            t, r = spec["offset"]
            at = at @ M(j3d.mat_srt((1, 1, 1), tuple(math.radians(x) for x in r), t))
        obj = build_mesh("SKM_%s_%s" % (name, spec["name"]), arm, skel, [(model, at, bone_map)], out, written,
                         manifest["materials"])
        obj.hide_render = not any(k in obj.name for k in P.get("preview_props", []))
        props.append(obj)
        manifest["meshes"][obj.name] = {"file": obj.name + ".fbx", "bone": spec["bone"],
                                        "materials": [s.name for s in obj.data.materials]}

    if not a.no_fbx:
        clear_pose(arm)
        export_fbx(os.path.join(out, "SKM_%s.fbx" % name), [arm, body])
        for obj in props:
            export_fbx(os.path.join(out, obj.name + ".fbx"), [arm, obj])
        log("FBX maillages exportés")

    # animations
    old_manifest = os.path.join(out, "manifest.json")
    if os.path.exists(old_manifest):  # les animations déjà exportées restent listées
        with open(old_manifest) as f:
            manifest["anims"] = json.load(f).get("anims", {})
    face_map = {}
    fm_path = os.path.join(HERE, P.get("face_map", "")) if P.get("face_map") else ""
    if fm_path and os.path.exists(fm_path):
        with open(fm_path) as f:
            for anm, row in json.load(f).items():
                if row.get("under") and row.get("face"):
                    face_map.setdefault(row["under"], row["face"])
    if a.anims != "none" and P.get("anims"):
        files = src.arc(P["anims"]["arc"])
        wanted = None if a.anims == "all" else set(a.anims.split(","))
        body_map = parts[0][2]
        face_part = next((p for p in parts if "face" in p[3]["file"]), None)

        def load_anim(n):
            key = "bcks/%s.bck" % n
            return bck.Animation(files[key], n) if key in files else None

        # (nom, [(animation, os du corps pilotés ou None = tous)])
        jobs = []
        for path in sorted(files):
            if not path.endswith(".bck"):
                continue
            n = os.path.basename(path)[:-4]
            if wanted is not None and n not in wanted:
                continue
            an = bck.Animation(files[path], n)
            if an.joint_count == P["anims"]["joints"]:
                jobs.append((n, [(an, None)]))
        # le jeu joue une animation « bas du corps » et une « haut du corps » (m_anmDataTable) : haut = sous-arbre
        # de backbone1 ; les combinaisons utiles sont reconstituées
        upper = set()
        if "backbone1" in skel.names:
            root = skel.index("backbone1")
            for bi in range(len(skel.names)):
                k = bi
                while k >= 0 and k != root:
                    k = skel.parents[k]
                if k == root:
                    upper.add(bi)
        for cname, (under_n, upper_n) in P.get("combos", {}).items():
            if wanted is not None and cname not in wanted:
                continue
            au, ah = load_anim(under_n), load_anim(upper_n)
            if au and ah:
                lower_set = set(body_map) - upper
                jobs.append((cname, [(au, lower_set), (ah, upper)]))
        offsets = offset_bases(skel, P)
        preview_frames = {}
        for item in filter(None, a.preview.split(",")):
            if ":" not in item:
                continue
            an_name, fr = item.split(":")
            preview_frames.setdefault(an_name, []).append(float(fr))
        done = 0
        for n, layers in jobs:
            main_an = layers[0][0]
            nframes = max(1, main_an.frames) + 1
            frames = list(range(nframes))
            tracks = {}
            for an, subset in layers:
                fr_an = frames if an is main_an else [f % max(1, an.frames) if an.loop_mode in (2, 4) else
                                                      min(f, an.frames) for f in frames]
                for bi, mats in bone_basis_frames(an, skel, body_map, fr_an).items():
                    if subset is None or bi in subset:
                        tracks[bi] = mats
            face_name = face_map.get(main_an.name)
            if face_part and face_name and ("bcks/%s.bck" % face_name) in files:
                fa = bck.Animation(files["bcks/%s.bck" % face_name], face_name)
                # l'animation de visage peut être plus courte : on tient sa dernière image
                ff = [min(f, max(0, fa.frames)) for f in frames]
                tracks.update(bone_basis_frames(fa, skel, face_part[2], ff))
            for bi, basis in offsets.items():
                tracks.setdefault(bi, [basis] * nframes)
            act_name = "A_%s_%s" % (name, n)
            act, slot = make_action(arm, skel, act_name, tracks, nframes)
            assign_action(arm, act, slot)
            bpy.context.scene.frame_start = 0
            bpy.context.scene.frame_end = nframes - 1
            if not a.no_fbx:
                export_fbx(os.path.join(out, "anims", act_name + ".fbx"), [arm], anim=True)
            manifest["anims"][act_name] = {"bck": n, "frames": nframes, "loop": main_an.loop_mode in (2, 4),
                                           "face": face_name, "layers": [an.name for an, _ in layers]}
            for fr in preview_frames.get(n, []):
                bpy.context.scene.frame_set(int(fr))
                render_preview(os.path.join(out, "preview_%s_%03d.png" % (n, int(fr))), arm, [body])
            done += 1
            if done % 50 == 0:
                log("animations", done, "/", len(jobs))
            if not a.blend:
                bpy.data.actions.remove(act)
        log("animations exportées", done)
    if arm.animation_data:
        arm.animation_data.action = None
    clear_pose(arm)
    for obj in props:
        obj.hide_render = not any(k in obj.name for k in P.get("preview_props", []))
    if "rest" in a.preview.split(","):
        for bi, basis in offset_bases(skel, P).items():
            arm.pose.bones[skel.names[bi]].matrix_basis = basis
        bpy.context.scene.frame_set(0)
        render_preview(os.path.join(out, "preview_rest.png"), arm, [body])
        render_preview(os.path.join(out, "preview_rest_side.png"), arm, [body], eye=(380, 0, 90))
    with open(os.path.join(out, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1, ensure_ascii=False)
    if a.blend:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, name + ".blend"))
    log("terminé", out)


main()
