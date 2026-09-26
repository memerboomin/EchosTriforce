# CH_Link — asset brief (Phase 0)

Decisions (user, 2026-09-26): hybrid build (coded LOW parts measured on the sheets, the existing
image-to-3D model `Tools/art/source/link_model.fbx` as HIGH for normal/AO bakes), game scale kept
(785 px/m on the T-pose sheets, not the sheet's "approx 175 cm"), autonomous run to acceptance.

```text
ASSET      CH_Link
CATEGORY   character, biped (humanoid, stylised "Twilight Princess" proportions), costume + rigid back gear
VIEWS      ref/front.png  (from tpose_front.jpg) front, orthographic, T-pose, az 0 el 0 lens 0; 1980x1493, subject
                          rows 17-1471 (97.4 % of the height), ground row 1471, axis column 990, 785 px/m.
           ref/back.png   (from tpose_back.jpg) back, orthographic, same framing as the front (axis 990, ground 1471).
           ref/side.png   side panel of sheet_model.webp (x 560-696, y 62-626), relaxed pose (arms down), faces right
                          on the sheet = left view; flipped to a right view, rescaled x2.6485 to 785 px/m
                          (cap top -> ground = 1454 px as on the T-poses). Near-orthographic; silhouette + depth only.
           sheet_model.webp other panels: front/back relaxed views (x 365-560 / 700-900), detail callouts
                          (face, profile, eye, hair, tunic/chainmail, strap, glove/bracer, belt/accessories),
                          weapons & gear (Master Sword, Hylian Shield, ocarina, mask), combat keyframes, expressions,
                          colour palette. Perspective-free illustrations: surface detail and colour only.
SCALE      Game scale: cap top 1.852 m, top of hair 1.83 m (T-pose sheets 1454 px / 785 px/m); matches the UE5
           mannequin skeleton the game already uses (skull top 1.83 m). The sheet's ruler says ~1.75 m to the hair
           (311 px/m on the sheet); rejected by the user for skeleton/animation compatibility (-4 %).
           Width: fingertip span 1.894 m in the T-pose (row 376, z 1.395).
PROPORTIONS measured on ref/front.png (z = (1471-row)/785) and ref/side.png, blockout 5 %, forms 2 %:
           P1  cap top                       1.852 m
           P2  eye line                      1.655 m (row 172)
           P3  chin                          1.520 m (row 278)  -> head (hair top to chin) 0.31 m, 5.9 heads tall
           P4  shoulder top (sleeve)         1.470 m (row 317)
           P5  belt centre                   1.065 m (row 635)
           P6  tunic + chainmail hem         0.715 m (row ~910)
           P7  crotch                        0.704 m (row 918)
           P8  boot cuff top                 0.440 m (row ~1125); cuff bottom 0.350 m (row ~1195)
           P9  fingertip span                1.894 m
           P10 boot width (front)            0.12-0.15 m per boot; stance: legs centred at x = +-0.11 m
           P11 torso depth at z 1.10 (side)  0.316 m incl. pouch; tunic hem depth 0.29 m (flared)
           P12 head depth at z 1.62 (side)   0.32 m incl. cap tail; cap tail tip hangs to z ~1.40 behind the head
           P13 boot length (side, z 0.10)    0.28 m
PARTS      construction and overlap order (separate object / deform type / attaches to / pivot):
           Body (skin: face, neck, forearms under gloves)  yes / deforming / skeleton / —
           Eyes                                             yes / rigid / head / eye centres
           Ears (pointed)                                   part of Body / deforming / head
           Hair (shell + fringe + side locks)               yes / deforming / head (+ neck for locks)
           Cap (crown band + long tail)                     yes / deforming / head; tail = secondary chain
           Undershirt (white collar, long sleeves)          yes / deforming / spine, arms
           Chainmail (neck V, sleeve rims, hem)             yes / deforming / spine, arms, pelvis
           Tunic (short sleeves, skirt to mid-thigh)        yes / deforming / spine, arms, pelvis, thighs
           Trousers                                         yes / deforming / pelvis, legs
           Boots (shaft + folded cuff + sole)               yes / deforming / calves, feet
           Gloves + bracers (fingerless, blue band)         yes / deforming / forearms, hands
           Belt + buckle + side pouch                       yes / rigid-ish / pelvis
           Cross strap (shoulder to hip, 2 buckles)         yes / deforming / spine
           Shield (Hylian, on the back)                     yes / rigid / spine_03 (one bone)
           Sheath + Master Sword hilt                       yes / rigid / spine_03; hilt kept separate (drawn in play)
           Ocarina + mask (right hip)                       yes / rigid / pelvis
SILHOUETTE 1 long green cap tail hanging behind the head (side, back);
           2 pointed ears through the blond fringe;
           3 tunic skirt flaring to mid-thigh over chainmail;
           4 shield + sword hilt over the left shoulder, sheath tip at the right hip (back);
           5 tall brown boots with folded cuffs.
MATERIALS  skin (warm peach, rough 0.55-0.65), hair (golden blond, 0.45-0.6, anisotropic look painted),
           cap and tunic (green woven wool, 0.8-0.9), undershirt and trousers (cream/beige linen, 0.8-0.9),
           chainmail (grey iron, metallic 1, rough 0.45-0.6, modelled as texture not geometry),
           leather (belt, straps, boots, gloves, pouch: brown worn leather 0.55-0.75, edge wear at folds),
           bracer band (dark blue cloth), shield (blue painted steel face, silver rim metallic, red crest paint),
           sheath (blue lacquer with gold fittings), hilt (purple guard, blue grip wrap), ocarina (blue glaze, 0.2),
           mask (white glazed ceramic). Palette from sheet_model: tunic, cloth, leather, metal, hair, skin, eyes,
           accents (violet). Toon-shaded in game (M_ZToonModel), so base colour must be de-lit.
ARTICULATION rig family humanoid biped = the UE5 mannequin skeleton SK_Mannequin (the game's AnimBP, sword animations
           and weapon_r / weapon_l bones already use it). Bone names must stay the mannequin's (not DEF- prefixed):
           the skill convention is mapped, not imposed. Built in T-pose like the sheets, bound, then unposed to the
           mannequin reference pose for export (as Tools/blender/rig_model.py does). Sockets: weapon_r (hand sword),
           weapon_l (shield in hand), back mounts for shield and sheath, head. Animation: none new (existing
           A_Sword_*, A_Guard, A_Cast, A_Victory must play on it).
INFERRED   top-down shapes (no top view); sole tread (generic); inside of the cap and sleeves; underside of the
           tunic skirt; the shield's inner face and straps (only its outer face and rim are drawn; the AI model invents
           a golden crest — ignored); the sheath throat under the shield; side-view hands and arm depth in a T-pose
           (the side panel is in a relaxed pose); fingers of the gloves between drawn knuckles.
TARGET     Unreal Engine 5.8, FBX (Blender -Y forward, Z up, cm via unit scale 0.01, see ue_pipeline_gotchas),
           hero character tier: LOD0 60-100k tris, LOD1 15-30k, LOD2 5-10k, 2k texture sets (4k only for the face
           set if the battle close-up proves it), 3-6 material groups. Game cameras: exploration third-person
           (~el 25, 50 mm, subject ~160 px at 1080p) and battle (three-quarter, subject ~300-400 px).
           Gates use --gameplay-px 160 and --ref-cam 30 15 50.
```

## Source conflicts

- The sheet's relaxed front view draws the cap as a flat beanie; the T-pose front hides the tail behind
  the head. The side and back views show the long tail — the side/back win for the cap.
- Sheet ruler (~1.75 m) vs game scale (1.83 m to the hair): game scale wins (user decision).
- The AI model (HIGH) re-draws the costume with 1-3 cm offsets and invents content (golden shield crest).
  The orthographic sheets win for every dimension and all colour; HIGH is used for normal/AO detail only
  where it agrees with the sheet silhouettes.
