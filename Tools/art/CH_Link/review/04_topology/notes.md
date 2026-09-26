# Phase 4 gate — topology

build/04_topology.py: Phase-3 objects kept as quad sources (*_SRC, hidden LOW_SRC); LOD0 = applied copy (modifiers,
transforms), cleaned (doubles, degenerate faces, loose verts, 3 non-manifold fans in the decimated head removed),
triangulated with BEAUTY (locked before baking). LOD1/LOD2 decimated from LOD0 (head 0.40/0.14, rest 0.42/0.16).

| level | tris | hero tier | front world-gate IoU |
| --- | --- | --- | --- |
| LOD0 | 46 601 | 60-100k (under) | 0.899 |
| LOD1 | 19 280 | 15-30k | 0.899 |
| LOD2 | 7 144 | 5-10k | 0.900 |

validate.py on LOD0: FAIL 0; warnings only "no UV layer / no material" (Phase 5) and "root: multiple root bones"
(the UE mannequin keeps ik_foot_root, ik_hand_root, interaction, center_of_mass beside pelvis under its armature
object `root` — the game's skeleton, kept as is).

Wire renders: wire_front_white.png, wire_threequarter_white.png. The radiating lines around the head are the
review Wireframe modifier's miter spikes on thin triangles of the decimated head, not geometry (LOD0 head max edge
0.062 m). Rigid gear has long edges by design (sheath 0.41 m segments, rigid on one bone).
