# Importe des maillages squelettiques (Tools/art/skel/*.fbx) sur le squelette du mannequin UE5.
# Usage : -ExecutePythonScript=Tools/ue/import_skeletal.py  (fichiers listés dans Tools/art/skel)
import os
import unreal
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJ, "Tools", "art", "skel")
DST = "/Game/Art/Characters"
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX False")
skeleton = unreal.load_asset("/Game/Characters/Mannequins/Meshes/SK_Mannequin")
AT = unreal.AssetToolsHelpers.get_asset_tools()
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".fbx"):
        continue
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("skeleton", skeleton)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("create_physics_asset", False)
    sk = ui.get_editor_property("skeletal_mesh_import_data")
    sk.set_editor_property("import_morph_targets", False)
    sk.set_editor_property("update_skeleton_reference_pose", False)
    sk.set_editor_property("use_t0_as_ref_pose", False)
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f)
    t.destination_path = DST
    t.destination_name = f[:-4]
    t.replace_existing = True
    t.automated = True
    t.save = True
    t.options = ui
    AT.import_asset_tasks([t])
    m = unreal.load_asset(DST + "/" + f[:-4])
    unreal.log_warning("ECHOS skel %s -> %s skeleton=%s" % (f, type(m).__name__, m.skeleton.get_name() if m else None))
unreal.SystemLibrary.quit_editor()
