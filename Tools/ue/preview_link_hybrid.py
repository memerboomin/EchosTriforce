# Capture de contrôle du Link hybride dans le niveau du jeu (L_Cistern, son éclairage), à côté de SKM_Link :
# animations d'épée du jeu, épée SM_Sword_Hero sur l'os weapon_r (même décalage que ZVisuals.cpp), poignée dorsale en
# « leader pose ». Acteurs temporaires : le niveau n'est pas sauvegardé.
#   UnrealEditor EchosTriforce.uproject -ExecCmds="py <chemin absolu>/Tools/ue/preview_link_hybrid.py" -unattended -nosplash
# (pas -ExecutePythonScript : l'éditeur se fermerait avant le premier rendu ; le script le ferme lui-même)
import os, time
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(PROJ, "Tools", "art", "CH_Link", "review", "10_engine")
os.makedirs(OUT, exist_ok=True)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(msg):
    unreal.log_warning("ECHOS " + msg)


LES.load_level("/Game/Maps/L_Cistern")
world = unreal.EditorLevelLibrary.get_editor_world()
starts = [a for a in EAS.get_all_level_actors() if a.get_class().get_name() == "PlayerStart"]
base = starts[0].get_actor_location() if starts else unreal.Vector(0, 0, 100)
hit = unreal.SystemLibrary.line_trace_single(world, base, base - unreal.Vector(0, 0, 500), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                             False, [], unreal.DrawDebugTrace.NONE, True)
ground = base.z - 90.0
try:
    if hit and hit.to_tuple()[0]:
        ground = hit.to_tuple()[4].z
except Exception as e:
    log("trace sol : %s" % e)
log("départ %s sol z=%.1f" % (base, ground))

new_mesh = unreal.load_asset("/Game/Art/Characters/LinkHybrid/SKM_LinkHybrid")
new_hilt = unreal.load_asset("/Game/Art/Characters/LinkHybrid/SKM_LinkHybrid_Hilt")
old_mesh = unreal.load_asset("/Game/Art/Characters/Link/SKM_Link")
old_hilt = unreal.load_asset("/Game/Art/Characters/Link/SKM_Link_Hilt")
sword = unreal.load_asset("/Game/Art/Meshes/SM_Sword_Hero")
anims = {n: unreal.load_asset("/Game/Art/Anims/" + n) for n in ("A_Sword_Idle", "A_Guard", "A_Victory")}


def character(mesh, hilt, x, anim, t, with_sword):
    loc = unreal.Vector(base.x + x, base.y, ground)
    a = EAS.spawn_actor_from_class(unreal.SkeletalMeshActor, loc, unreal.Rotator(0, 0, 0))
    comp = a.skeletal_mesh_component
    comp.set_skeletal_mesh_asset(mesh) if hasattr(comp, "set_skeletal_mesh_asset") else comp.set_skeletal_mesh(mesh)
    comp.set_update_animation_in_editor(True)
    if anim:
        comp.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        comp.set_animation(anim)
        comp.play(False)
        comp.set_position(t, False)
    if with_sword and sword:
        s = EAS.spawn_actor_from_object(sword, loc)
        s.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)   # a static root cannot follow a bone
        s.attach_to_component(comp, "weapon_r", unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET,
                              unreal.AttachmentRule.KEEP_WORLD, False)
        s.set_actor_relative_rotation(unreal.Rotator(roll=-90, pitch=0, yaw=0), False, False)   # ZVisuals WeaponRotation
    elif hilt:
        h = EAS.spawn_actor_from_class(unreal.SkeletalMeshActor, loc, unreal.Rotator(0, 0, 0))
        hc = h.skeletal_mesh_component
        hc.set_skeletal_mesh_asset(hilt) if hasattr(hc, "set_skeletal_mesh_asset") else hc.set_skeletal_mesh(hilt)
        h.attach_to_component(comp, "", unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET,
                              unreal.AttachmentRule.KEEP_WORLD, False)
        hc.set_leader_pose_component(comp)
    return a


# anciens et nouveaux côte à côte (maillages tournés vers +Y : la caméra est en +Y et regarde vers -Y)
character(old_mesh, old_hilt, -180, anims["A_Sword_Idle"], 0.5, True)
character(new_mesh, new_hilt, -60, anims["A_Sword_Idle"], 0.5, True)
character(new_mesh, new_hilt, 60, anims["A_Guard"], 0.4, True)
character(new_mesh, new_hilt, 180, None, 0.0, False)

cams = []
for name, off, tgt in (("overview", unreal.Vector(0, 520, 120), unreal.Vector(0, 0, 95)),
                       ("closeup", unreal.Vector(-60, 170, 160), unreal.Vector(-60, 0, 150))):
    loc = unreal.Vector(base.x, base.y, ground) + off
    look = unreal.Vector(base.x, base.y, ground) + tgt
    rot = unreal.MathLibrary.find_look_at_rotation(loc, look)
    cam = EAS.spawn_actor_from_class(unreal.CameraActor, loc, rot)
    cam.camera_component.set_editor_property("field_of_view", 40.0)
    cams.append((name, cam))

state = {"t0": time.time(), "i": 0, "task": None}


def tick(dt):
    if time.time() - state["t0"] < 25.0:        # textures, shaders and animation settle
        return
    if state["task"] is not None and not state["task"].is_task_done():
        return
    if state["i"] >= len(cams):
        unreal.unregister_slate_post_tick_callback(handle)
        log("captures terminées")
        unreal.SystemLibrary.quit_editor()
        return
    name, cam = cams[state["i"]]
    path = os.path.join(OUT, "ue_%s.png" % name)
    state["task"] = unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, path, cam, False, False,
                                                                       unreal.ComparisonTolerance.LOW, "", 0.0, True)
    log("capture " + path)
    state["i"] += 1


handle = unreal.register_slate_post_tick_callback(tick)
