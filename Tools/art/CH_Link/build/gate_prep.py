"""Gate helper: save a copy of the master with the named LOW objects hidden from render (body-only gates).
  blender -b --python build/gate_prep.py -- --master CH_Link_master.blend --out review/x.blend --hide Shield,Sheath"""
import bpy, sys
a = sys.argv[sys.argv.index("--") + 1:]
bpy.ops.wm.open_mainfile(filepath=a[a.index("--master") + 1])
keys = a[a.index("--hide") + 1].split(",")
for o in bpy.data.objects:
    if any(k in o.name for k in keys):
        o.hide_render = True
bpy.ops.wm.save_as_mainfile(filepath=a[a.index("--out") + 1], copy=True)
