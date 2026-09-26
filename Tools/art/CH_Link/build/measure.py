"""Brief proportions measured on the LOW collection (world metres, build pose).
  blender -b CH_Link_master.blend --python build/measure.py"""
import bpy, json, sys
import numpy as np
dg = bpy.context.evaluated_depsgraph_get()
pts = {}
for o in bpy.data.collections["LOW"].all_objects:
    if o.type not in ("MESH", "CURVE") or o.hide_render:
        continue
    m = o.evaluated_get(dg).to_mesh()
    co = np.array([o.matrix_world @ v.co for v in m.vertices])
    pts[o.name] = co
allp = np.vstack(list(pts.values()))
body = pts["CH_Link_Body_LOD0"]
def width_at(co, z, tol=0.006, xmax=9):
    s = co[(np.abs(co[:, 2] - z) < tol) & (np.abs(co[:, 0]) < xmax)]
    return float(np.ptp(s[:, 0])) if len(s) else None
res = {
    "P1 top (cap) z": float(allp[:, 2].max()),
    "sole min z": float(allp[:, 2].min()),
    "P9 fingertip span": float(np.ptp(body[:, 0])),
    "P4 shoulder top z (|x| 0.25-0.30)": float(body[(np.abs(body[:, 0]) > 0.25) & (np.abs(body[:, 0]) < 0.30), 2].max()),
    "P6 skirt hem z": float(pts["CH_Link_TunicSkirt_LOD0"][:, 2].min()),
    "P5 belt centre z": float(pts["CH_Link_Belt_LOD0"][:, 2].mean()),
    "P8 cuff top z": float(pts["CH_Link_BootCuff_L_LOD0"][:, 2].max()),
    "P8 cuff bottom z": float(pts["CH_Link_BootCuff_L_LOD0"][:, 2].min()),
    "P10 boot width at z 0.25": width_at(body[body[:, 0] > 0], 0.25, tol=0.015),
    "P11 torso depth z 1.10": float(np.ptp(body[np.abs(body[:, 2] - 1.10) < 0.006, 1])),
    "P13 boot length z 0.03": float(np.ptp(body[(np.abs(body[:, 2] - 0.03) < 0.01) & (body[:, 0] > 0), 1])),
    "head top z": float(pts["CH_Link_Head_LOD0"][:, 2].max()),
    "tris": {k: None for k in pts},
}
for o in bpy.data.collections["LOW"].all_objects:
    if o.type == "MESH" and not o.hide_render:
        m = o.evaluated_get(dg).to_mesh()
        res["tris"][o.name] = sum(len(p.vertices) - 2 for p in m.polygons)
res["tris_total"] = sum(v for v in res["tris"].values() if v)
print("MEASURE " + json.dumps(res, indent=1))
