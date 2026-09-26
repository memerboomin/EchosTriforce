# Phase 6 gate — articulation

build/06_rig.py on the game's skeleton (UE5 mannequin, 88 bones + weapon_r added at export as for SKM_Link).

- Weights: deforming parts (Body, TunicSkirt, Head) take the mannequin's production weights from its body posed in
  the same build pose (nearest surface, barycentric, top 4, normalised). Head restricted to head/neck_02/neck_01/
  spine_05 (hair locks near the shoulders must not follow the clavicles); vertices with no usable weight fall back to
  the part's root bone. Skirt: rig_model's rule (pelvis at the belt blending to the thighs at the hem, leg weights
  below). Rigid parts on one bone: Belt, Buckle, Pouch, Ocarina, Mask -> pelvis; cuffs -> calf_l/r; CapTail -> head;
  Shield, ShieldRim, Sheath, Hilt, Guard -> spine_05.
- Bind pose: meshes inverse-skinned from the sheets' T-pose build pose to the mannequin reference pose (rig_model.unpose),
  so every game animation plays on the bind pose it expects. Round trip checked: the build pose re-skinned forward
  gives the same front silhouette (world gate IoU 0.900 vs 0.900 before binding).
- Sockets (SOCKETS): SOCKET_weapon_r/l (between index_01 and pinky_01), SOCKET_back_shield, SOCKET_back_sword (spine_05),
  SOCKET_head. In UE the sword keeps using the weapon_r bone.
- validate.py on LOD0/LOD1/LOD2 + SOCKETS: FAIL 0 (no unweighted or over-influenced vertices, UVs present).

Extreme-pose sheet (pose_sheet.png; FK review actions POSE_rest, POSE_arms_up, POSE_crouch, POSE_lunge,
POSE_head_turn, never exported): shoulders stretch without collapsing with both arms overhead, elbows and knees
keep volume, the skirt follows the thighs in the lunge and drapes between them in the crouch, head and hair turn as
one piece, back gear stays on the back. Linear blend skinning (Preserve Volume off, as UE).

Fixed on the way: head vertices left unweighted when every mannequin weight near them fell under 0.01 (they stayed
behind in the crouch as vertical streaks); scale 100 on the bound meshes under the mannequin's 0.01 empty (identity
basis + parent inverse now).

Known limits: no facial rig (as SKM_Link); cap tail rigid on the head (no secondary chain, Phase 7 skipped: the brief
lists no simulated parts); skirt is skinned, not simulated — in deep crouches it drapes between the thighs rather than
over them.
