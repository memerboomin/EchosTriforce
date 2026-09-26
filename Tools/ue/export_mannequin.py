# Exporte le mannequin UE5 (maillage + squelette) en FBX pour le reprendre dans Blender.
import os
import unreal
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(PROJ, "Tools", "art", "export")
for name, path in (("SKM_Manny_Simple", "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"), ("SKM_Quinn_Simple", "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple")):
    mesh = unreal.load_asset(path)
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = os.path.join(OUT, name + ".fbx")
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    task.exporter = unreal.SkeletalMeshExporterFBX()
    opts = unreal.FbxExportOption()
    opts.set_editor_property("ascii", False)
    opts.set_editor_property("collision", False)
    opts.set_editor_property("level_of_detail", False)
    opts.set_editor_property("export_morph_targets", False)
    task.options = opts
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log_warning("ECHOS export %s %s" % (name, ok))

unreal.SystemLibrary.quit_editor()
