# Phase 3 gate — forms

Method (build/03_forms.py): the blockout body (Skin chain on the mannequin build pose, Subdivision 2, torso/head/
shoulders/hips subdivided once more) is projected along its normals on a Laplacian-smoothed copy of HIGH, part by
part (fingers and the hip zone under the tunic skirt stay modelled), smoothed, then fitted row by row to the front
sheet (legs, arms, torso: X or Z) and to the side panel (legs, torso, skirt: Y). The head (face, hair, ears, cap
crown and hood, collar) is cut from HIGH and decimated to 16k tris; the body under it is trimmed only where the head
covers it and stepped 4 mm in. Cuffs are wrapped on the finished legs; gear is built from the sheets.

World gate (pixel-registered, 785 px/m):

| view | IoU | ref only | model only | max band | tolerance |
| --- | --- | --- | --- | --- | --- |
| front (T-pose sheet) | 0.899 | 4.5 % | 6.3 % | 0.015 | 0.90 / 0.03 |
| back (T-pose sheet) | 0.892 | 4.0 % | 7.7 % | 0.023 | 0.90 / 0.03 |
| side panel, body only (shield erased, gear hidden) | 0.818 | 3.5 % | 17.9 % | 0.106 (head band) | per-view ceiling |
| HIGH itself, front / side | 0.837 / 0.714 | | | | |

compose_review (bounding-box aligned, front): IoU 0.885, bands within 0.016.

Brief proportions on LOW (tolerance 2 %): cap top 1.851 (1.852), fingertip span 1.884 (1.894, -0.5 %), hem 0.709
(0.715, -0.8 %), belt centre 1.052 (1.065, -1.2 %), cuff 0.344-0.446 (0.35-0.44), torso depth at z 1.10 0.307
(0.316, -2.8 % — the panel figure includes the pouch), boot length at the sole 0.280 (0.279-0.30), boot width at
z 0.25 0.106. Shoulder top 1.495 vs 1.470 (+1.7 %).

Remaining deviations, measured, with the reason they are not chased:

- Front 0.001 and back 0.008 under the IoU tolerance. The residue is the gear: the sheets bend the sword (grip 45 deg,
  sheath 29 deg) where a rigid sword is straight, and the front sheet draws the back gear 0.019 m further -X than the
  back sheet (the gear is placed halfway, 0.0096 m off each). These are per-view ceilings of the drawings.
- Side panel head band +0.106: the panel's head is drawn ~3 cm shorter front-to-back than the HIGH head (face 3 cm
  less forward). The HIGH head is kept (it matches both T-pose sheets); the panel is a smaller-scale illustration.
- Side panel shield: drawn smaller (bottom z 1.03) than on the back T-pose sheet (0.79); the T-pose sheet wins.
- Side panel shows no sheath; the sheath depth (y 0.175-0.205) is inferred.
- Mask orientation: the front sheet shows it in profile (facing +X); its face width along Y (0.10 m) is inferred.

Topology notes carried to Phase 4: body 22.3k tris (quads + a ring of triangles at mid upper arm and mid thigh where
the extra subdivision stops, away from bending joints); fingers 8x16 quad sections; head is a decimated triangle mesh
(no facial loops — no facial rig, as the shipped SKM_Link). LOD0 total 46.6k tris (hero tier 60-100k).
