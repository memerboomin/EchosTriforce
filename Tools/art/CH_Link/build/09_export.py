"""Phase 9: FBX delivery for Unreal Engine 5.8 (EchosTriforce), on the game's UE5 mannequin skeleton.
Works on a throwaway copy of the master: rest pose (the meshes were inverse-skinned to it in Phase 6), then
Tools/blender/make_character.flatten_rig (drops the mannequin's 0.01 empty so UE adds no extra root bone, bakes
centimetres into vertices and bones, adds the weapon_r grip bone the sword animations use) and make_character.export
(cm, unit scale 0.01, FBX_SCALE_NONE — see ue_pipeline_gotchas). One file per LOD plus the hilt (hidden in game while
the sword is in hand, as SKM_Link_Hilt). Writes asset-manifest.json and animation-contract.json.
  blender -b --python build/09_export.py -- --master CH_Link_master.blend --name SKM_LinkHybrid
"""
import bpy, os, sys, json, math, datetime, re
from mathutils import Vector, Matrix
HERE = os.path.dirname(os.path.abspath(__file__))
ASSET = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "../../../blender")))
import make_character as mc

HILT_PARTS = ("Hilt", "Guard")
INFERRED = [
    "cap tail cross-section and exact path (side panel only, the HIGH has a flat hood)",
    "sheath depth between back and shield (y 0.11-0.135; the side panel draws no sheath)",
    "shield inner face and straps (not drawn), shield stand-off (+1.8 cm vs side panel to clear the sheath)",
    "mask face width along Y (0.10 m; drawn in profile only)",
    "hip and thigh volume under the tunic skirt (hidden)",
    "sole tread (generic flat sole)",
    "glove finger thickness (hands drawn as a flat paddle)",
    "top-down shapes (no top view)",
]


def part_of(name):
    return re.sub(r"_LOD\d$", "", name).replace("CH_Link_", "")


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def bounds(objs):
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    mn = [min(p[i] for p in pts) for i in range(3)]; mx = [max(p[i] for p in pts) for i in range(3)]
    return {"min": [round(v, 4) for v in mn], "max": [round(v, 4) for v in mx]}


def main():
    a = sys.argv[sys.argv.index("--") + 1:]
    master = a[a.index("--master") + 1]
    name = a[a.index("--name") + 1] if "--name" in a else "SKM_LinkHybrid"
    out = os.path.join(ASSET, "exports")
    os.makedirs(out, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=os.path.abspath(master))
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    empty = arm.parent
    if arm.animation_data:
        arm.animation_data.action = None
    mc.rest_pose(arm)
    lods = {k: [o for o in bpy.data.collections["LOD%d" % k].objects if o.type == "MESH"] for k in (0, 1, 2)}
    meshes = [o for k in lods for o in lods[k]]
    height = bounds(lods[0])
    # sockets and bounds before the centimetre flattening (metres, build frame)
    bpy.context.view_layer.update()
    sockets = [{"name": o.name, "parent_bone": o.parent_bone, "world_location_m_rest_pose": [round(v, 4) for v in o.matrix_world.translation],
                "axes": "local +Y forward of the attachment, +Z up"} for o in bpy.data.collections["SOCKETS"].objects]
    # everything that does not ship goes before the flattening (REF planes, HIGH, sources, sockets, blockout)
    keep = set(meshes) | {arm, empty}
    for o in list(bpy.data.objects):
        if o not in keep:
            bpy.data.objects.remove(o, do_unlink=True)
    mc.flatten_rig(empty, arm, meshes)
    files = {}
    for k, objs in lods.items():
        body = [o for o in objs if part_of(o.name) not in HILT_PARTS]
        p = os.path.join(out, "%s_LOD%d.fbx" % (name, k))
        mc.export([arm] + body, p)
        files["LOD%d" % k] = p
    hilt = [o for o in lods[0] if part_of(o.name) in HILT_PARTS]
    p = os.path.join(out, "%s_Hilt.fbx" % name)
    mc.export([arm] + hilt, p)
    files["Hilt"] = p
    tex_dir = os.path.join(ASSET, "textures")
    textures = sorted(f for f in os.listdir(tex_dir) if f.endswith(".png"))
    manifest = {
        "asset": name,
        "generated": datetime.datetime.now().isoformat(timespec="seconds"),
        "blender": bpy.app.version_string,
        "source": os.path.abspath(master),
        "target_engine": "Unreal Engine 5.8 (EchosTriforce), FBX importer (Interchange FBX disabled as in Tools/ue)",
        "units": {"file": "centimetres (scene unit scale 0.01, FBX_SCALE_NONE)", "authoring": "metres, Z up, facing -Y",
                  "engine": "UE X = Blender X, UE Y = -Blender Y (ue_pipeline_gotchas)"},
        "target_height_m": 1.851,
        "bounds_m_build_pose_lod0": height,
        "files": files,
        "lods": [{"level": k, "objects": [o.name for o in objs], "tris": sum(tris(o) for o in objs),
                  "tris_without_hilt": sum(tris(o) for o in objs if part_of(o.name) not in HILT_PARTS)} for k, objs in lods.items()],
        "materials": {"M_Link_Head": "Head, CapTail", "M_Link_Body": "Body, TunicSkirt, Belt, Buckle, BootCuff_L/R, Pouch",
                      "M_Link_Gear": "Shield, ShieldRim, Sheath, Hilt, Guard, Ocarina, Mask"},
        "textures": {"dir": tex_dir, "files": textures, "Head": "2048", "Body": "4096", "Gear": "2048"},
        "texture_contract": {"basecolor": "sRGB", "normal": "Non-Color, tangent space, OpenGL +Y: flip green on UE import",
                             "roughness": "Non-Color", "metallic": "Non-Color", "ao": "Non-Color",
                             "packed_channels": "none"},
        "skeleton": {"name": "SK_Mannequin (UE5 mannequin, /Game/Characters/Mannequins/Meshes/SK_Mannequin)",
                     "bones": len(arm.data.bones), "added": ["weapon_r (grip, child of hand_r, as SKM_Link)"],
                     "deform_bones": [b.name for b in arm.data.bones if b.use_deform],
                     "rest_pose": "mannequin reference pose (meshes inverse-skinned from the sheets' T-pose build pose)",
                     "max_influences": 4},
        "sockets": sockets,
        "rigid_parts": {"pelvis": ["Belt", "Buckle", "Pouch", "Ocarina", "Mask"], "calf_l/r": ["BootCuff_L", "BootCuff_R"],
                        "head": ["CapTail"], "spine_05": ["Shield", "ShieldRim", "Sheath", "Hilt", "Guard"]},
        "colliders": "none shipped: the physics asset is authored in UE",
        "animation_contract": "animation-contract.json",
        "inferred_from_no_reference": INFERRED,
    }
    with open(os.path.join(out, "asset-manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    with open(os.path.join(out, "animation-contract.json"), "w") as f:
        json.dump({"asset": name, "fps": 30, "clips": [],
                   "note": "no clips ship with the mesh; the game's A_Sword_*, A_Guard, A_Cast, A_Victory (Tools/art/anims) "
                           "play on the same skeleton. POSE_* actions in the master are review poses only."}, f, indent=2)
    print("EXPORT", json.dumps({k: v for k, v in files.items()}), [l["tris"] for l in manifest["lods"]])


main()
