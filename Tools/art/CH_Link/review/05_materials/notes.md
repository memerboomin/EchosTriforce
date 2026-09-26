# Phase 5 gate — UVs, bakes, materials

build/05_materials.py (CPU bake: Metal baking threw NSInvalidArgumentException twice on this Mac; `--device CPU`).

| set | res | texel density | UV fill | parts |
| --- | --- | --- | --- | --- |
| Head | 2048 | 2007 px/m | 53 % | Head, CapTail |
| Body | 4096 | 1302 px/m | 41 % | Body, TunicSkirt, Belt, Buckle, cuffs, Pouch |
| Gear | 2048 | 1390 px/m | 48 % | Shield, ShieldRim, Sheath, Hilt, Guard, Ocarina, Mask |

Maps per set (textures/): BaseColor (sRGB), Normal (tangent, OpenGL +Y), Roughness, Metallic, AO (Non-Color).
Materials M_Link_Head / M_Link_Body / M_Link_Gear (3 groups, hero tier 3-6). LOD1/LOD2 rebuilt from the unwrapped LOD0
(same UVs and materials).

Sources: Head/Body colour, roughness, metallic, normal from HIGH_Bake (HIGH minus the AI gear, 171k vertices) with the
de-shadowed, sheet-harmonised albedo of the shipped SKM_Link (out/link/T_Link_Albedo.png). Cage 0.04 m, ray 0.08 m
(the LOW was fitted to the sheets up to ~3 cm off the HIGH). Texels without a hit (body back under the shield, where
the AI fused shield and tunic; leg backs moved by the side-panel fit) start as a magenta sentinel and are push-pull
filled from their baked neighbours (Head 7.4 %, Body 48.7 % of the image incl. the gutters between islands).
Gear colour: planar projection of the back T-pose sheet (shield, rim, sheath; sampled at the drawn position, the gear
being shifted 0.0096 m), front sheet for ocarina and mask; hilt and guard take the sheet's colours as constants
(the sheets draw the grip at 45 deg, the rigid sword is straight). Cap tail: constant cap green sampled on the sheet.

Gate: material turntable (12 frames, turntable_sheet.png) reads correctly under the moving light; greyscale compare at
160 px (compare_front_grey.png) reads as the sheet; checker (checker_*.png) shows even density across parts (head
denser by design). Fixed on the way: belt normals facing inward (open band; black belt bake), sword above the shield
(the back sheet shows it under the shield: sheath now between back and shield), nape hole in the head cut.
Known: Smart UV gives many small islands (seams are hidden by the toon shading at gameplay size, but a hand-placed
seam layout would pack better than 41-53 %).
