# Importe les FBX générés par Tools/blender/make_models.py dans /Game/Art/Meshes.
import os
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJ, "Tools", "art", "fbx")
DST = "/Game/Art/Meshes"
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

ONLY = [n for n in os.environ.get("ECHOS_ONLY", "").split(",") if n]  # ex. ECHOS_ONLY=SM_Sword_Hero
tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".fbx") or (ONLY and f[:-4] not in ONLY):
        continue
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f)
    t.destination_path = DST
    t.destination_name = f[:-4]
    t.replace_existing = True
    t.automated = True
    t.save = True
    tasks.append(t)
AT.import_asset_tasks(tasks)
for t in tasks:
    path = DST + "/" + t.destination_name
    m = unreal.load_asset(path)
    if not isinstance(m, unreal.StaticMesh):
        unreal.log_warning("ECHOS import manqué " + path + " -> " + str(type(m)))
        continue
    b = m.get_bounds()
    slots = [str(s.material_slot_name) for s in m.static_materials]
    unreal.log_warning("ECHOS %s origin=(%.1f,%.1f,%.1f) extent=(%.1f,%.1f,%.1f) slots=%s" % (t.destination_name, b.origin.x, b.origin.y, b.origin.z, b.box_extent.x, b.box_extent.y, b.box_extent.z, slots))
    EAL.save_loaded_asset(m)
