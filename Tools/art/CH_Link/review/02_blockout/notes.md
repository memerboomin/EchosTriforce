# Phase 2 gate — blockout

World gate (LOW vs cleaned sheet mattes, 785 px/m, pixel-registered):

| view | IoU | ref only | model only | worst bands (fraction of height) |
| --- | --- | --- | --- | --- |
| front (T-pose sheet) | 0.800 | 13.0 % | 8.8 % | band 0 +0.118 (hilt), band 2 -0.099 (arm undersides, fingers), band 6 +0.107 (sheath angle) |
| back (T-pose sheet) | 0.805 | 11.9 % | 9.4 % | same pattern |
| side (sheet panel) | 0.611 | 17.8 % | 34.5 % | gear: the side panel draws no sheath and a smaller shield |
| HIGH (AI model) for comparison | front 0.837 / side 0.714 | | | its gear is oversized |

Below the 0.85 blockout tolerance. What the numbers say, as measurements:

- Skeleton and proportions pass: arm axis z 1.40 (sheet 1.39), wrist x 0.732 (sheet glove end ~0.75),
  crotch 0.70 (sheet 0.704), eye/chin heights from the head node within 1 cm.
- Body sections are Skin-modifier ellipses: tunic sides narrow by up to 0.03 m per side at z 1.0-1.2;
  arm undersides 1-2 cm high (sleeve flare not modelled); legs centred on the bones while the drawn boots sit
  ~5 cm behind the shin bone (calf and heel mass). Assigned to Phase 3 (projection on HIGH).
- Head is a 0.25 x 0.26 m blob; hair, fringe and cap crown volumes come from the HIGH projection in Phase 3.
- Sword: the sheets bend it (back sheet: grip 45 deg, sheath 29 deg; the front sheet's hilt and sheath are not
  colinear). A rigid sword must be straight: axis pommel (0.33, 1.75) -> tip (-0.39, 0.66), 3-4 cm off the
  drawn grip and mid-sheath. Recorded deviation, not chased.
- Shield: back T-pose sheet outline (bottom tip z 0.79) conflicts with the side panel (bottom z 1.03, drawn
  smaller). The orthographic T-pose sheet wins for the outline; the side panel gives only its distance from
  the back (front face y 0.115, outer face 0.173).
- Feet: mannequin toes 4 cm ahead of the drawn boot toe (y -0.174) and heels 7 cm short of the drawn heel
  (+0.126). Bones stay (animations); the boot mesh follows the drawing in Phase 3.

Decision: continue to Phase 3; the forms gate (0.90 IoU, bands 0.03) is held on the body, with the gear
conflicts above listed as per-view ceilings.
