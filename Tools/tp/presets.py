# Personnages et objets de Twilight Princess à convertir pour Unreal (Tools/tp/blender_tp.py).
# Chemins dans l'ISO (res/Object/<archive>.arc) puis dans l'archive ; « attach » = os du modèle principal auquel le
# sous-modèle est fixé (comme dans d_a_alink.cpp : visage et bonnet sur l'os 4 « head »).
import os

ISO = os.environ.get("TP_ISO", "/Users/memerboomin/Documents/Claude/Code/Zelda Mod/Legend of Zelda, The - Twilight Princess.iso")

PRESETS = {
    "link": {
        "name": "TPLink",
        "models": [
            {"arc": "Kmdl", "file": "bmwr/al.bmd"},
            {"arc": "Kmdl", "file": "bmwr/al_face.bmd", "attach": "head"},
            {"arc": "Kmdl", "file": "bmwr/al_head.bmd", "attach": "head"},
        ],
        # objets tenus : maillages squelettiques suiveurs (même squelette), rigides sur un os
        "props": [
            {"name": "OrdonSword", "arc": "Alink", "file": "bmwr/al_swa.bmd", "bone": "weaponL"},
            {"name": "MasterSword", "arc": "Alink", "file": "bmwe/al_swm.bmd", "bone": "weaponL"},
            {"name": "Sheath", "arc": "Kmdl", "file": "bmwr/al_swb.bmd", "bone": "pod"},
            {"name": "HylianShield", "arc": "HyShd", "file": "*", "bone": "weaponR"},
            {"name": "OrdonShield", "arc": "CWShd", "file": "*", "bone": "weaponR"},
            # rangés (d_a_alink.cpp setItemMatrix : os « pod », décalage (translation, rotation XYZ en degrés))
            {"name": "OrdonSwordBack", "arc": "Alink", "file": "bmwr/al_swa.bmd", "bone": "pod",
             "offset": [(-18.5, 0.14, 12.2), (0, 33.1, 0)]},
            {"name": "MasterSwordBack", "arc": "Alink", "file": "bmwe/al_swm.bmd", "bone": "pod",
             "offset": [(-18.5, 0.14, 12.2), (0, 33.1, 0)]},
            {"name": "HylianShieldBack", "arc": "HyShd", "file": "*", "bone": "pod",
             "offset": [(4.2, -4.4, -20.0), (91, 57, 180)]},
            {"name": "OrdonShieldBack", "arc": "CWShd", "file": "*", "bone": "pod",
             "offset": [(4.2, -4.4, -20.0), (91, 57, 180)]},
        ],
        # animations « bas du corps » + « haut du corps » (m_anmDataTable) reconstituées : nom -> (bas, haut)
        "combos": {"run": ("dashs", "dasha"), "battle_idle": ("atl", "at"), "cast": ("getawait", "holdout")},
        # animations : toutes celles de l'archive qui ont le nombre d'os du corps
        "anims": {"arc": "AlAnm", "joints": 35},
        # animations de visage (5 os) fusionnées dans les animations du corps (table m_anmDataTable de d_a_alink.cpp)
        "face_map": "link_anims.json",
        # le bonnet est animé par une physique dans le jeu : on le fait retomber dans les animations exportées
        "pose_offsets": [("z_cap1", -60), ("z_cap2", -18), ("z_cap3", -12), ("z_cap4", -8)],
        "preview_props": ["OrdonSwordBack", "HylianShieldBack", "Sheath"],
    },
}
