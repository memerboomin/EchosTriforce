#!/usr/bin/env python3
"""
Construit les données de jeu (Content/Data/*.json) à partir :
  - du catalogue de préproduction incorporé au dossier (Docs/catalogue_zelda_rpg.json, 1 257 références)
  - des tables de règles du dossier « Les Échos de la Triforce » (chapitres 5 à 20)

Relancer après toute modification :   python3 Tools/build_game_data.py
Le jeu relit ces fichiers au démarrage (UZGameData). Les champs suivent les USTRUCT de Source/EchosTriforce/Core/ZTypes.h.
"""
import json, os, re, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CATALOG = os.path.join(ROOT, "Docs", "catalogue_zelda_rpg.json")
OUT = os.path.join(ROOT, "Content", "Data")

WEAPON_POWER = [0, 12, 24, 38, 54, 74]
DEF_HEAD_LEGS = [0, 2, 4, 6, 9, 12]
DEF_TORSO = [0, 4, 8, 13, 19, 26]
SHIELD_DEF = [0, 4, 8, 13, 19, 26]

# Chapitre d'ouverture des archives (annexe A)
ARCHIVE_CHAPTER = {
    "Z1": 1, "OOT": 1, "Z2": 2, "BOTW": 2, "ALTTP": 3, "OOS": 3, "OOA": 3, "OOS/OOA": 3, "MC": 3,
    "MM": 4, "FS": 4, "FSA": 4, "TFH": 4, "TP": 5, "ALBW": 5, "LA": 6, "WW": 6, "PH": 6, "ST": 6, "EOW": 6,
    "SS": 7, "TOTK": 7,
}
GAME_NAMES = {
    "Z1": "The Legend of Zelda (1986)", "Z2": "Zelda II: The Adventure of Link (1987)",
    "ALTTP": "A Link to the Past (1991)", "LA": "Link's Awakening (1993)", "OOT": "Ocarina of Time (1998)",
    "MM": "Majora's Mask (2000)", "OOS": "Oracle of Seasons (2001)", "OOA": "Oracle of Ages (2001)",
    "OOS/OOA": "Oracle of Seasons / Ages (2001)", "FS": "Four Swords (2002)", "WW": "The Wind Waker (2002)",
    "FSA": "Four Swords Adventures (2004)", "MC": "The Minish Cap (2004)", "TP": "Twilight Princess (2006)",
    "PH": "Phantom Hourglass (2007)", "ST": "Spirit Tracks (2009)", "SS": "Skyward Sword (2011)",
    "ALBW": "A Link Between Worlds (2013)", "TFH": "Tri Force Heroes (2015)", "BOTW": "Breath of the Wild (2017)",
    "TOTK": "Tears of the Kingdom (2023)", "EOW": "Echoes of Wisdom (2024)",
}

# Noms français sûrs (les autres restent en anglais : clés de recherche du catalogue, cf. chapitre 26)
NAME_FR = {
    "Master Sword": "Épée de légende", "Kokiri Sword": "Épée Kokiri", "Biggoron's Sword": "Épée de Biggoron",
    "Biggoron’s Sword": "Épée de Biggoron", "Giant's Knife": "Lame des géants", "Hylian Shield": "Bouclier hylien",
    "Mirror Shield": "Bouclier miroir", "Deku Shield": "Bouclier Mojo", "Kokiri Tunic": "Tunique Kokiri",
    "Goron Tunic": "Tunique Goron", "Zora Tunic": "Tunique Zora", "Zora Armor": "Armure Zora",
    "Magic Armor": "Armure magique", "Iron Boots": "Bottes de plomb", "Hover Boots": "Bottes des airs",
    "Hookshot": "Grappin", "Longshot": "Super grappin", "Clawshot": "Grappin", "Double Clawshots": "Double grappin",
    "Boomerang": "Boomerang", "Gale Boomerang": "Boomerang tornade", "Bow": "Arc", "Fairy Bow": "Arc des fées",
    "Hero's Bow": "Arc du héros", "Fire Arrow": "Flèche de feu", "Ice Arrow": "Flèche de glace",
    "Light Arrow": "Flèche de lumière", "Silver Arrow": "Flèche d'argent", "Bombs": "Bombes", "Bomb": "Bombe",
    "Bomb Bag": "Sac de bombes", "Megaton Hammer": "Masse des titans", "Lens of Truth": "Monocle de vérité",
    "Ocarina of Time": "Ocarina du temps", "Fairy Ocarina": "Ocarina des fées", "Din's Fire": "Feu de Din",
    "Farore's Wind": "Vent de Farore", "Nayru's Love": "Amour de Nayru", "Deku Mask": "Masque Mojo",
    "Goron Mask": "Masque Goron", "Zora Mask": "Masque Zora", "Fierce Deity’s Mask": "Masque du Dieu démon",
    "Fierce Deity's Mask": "Masque du Dieu démon", "Great Fairy’s Mask": "Masque de la Grande Fée",
    "Kafei’s Mask": "Masque de Kafei", "Bremen Mask": "Masque de Brême", "Kamaro’s Mask": "Masque de Kamaro",
    "Blast Mask": "Masque d'explosion", "Bunny Hood": "Masque du lapin", "Keaton Mask": "Masque de Keaton",
    "Mask of Truth": "Masque de vérité", "Don Gero’s Mask": "Masque de Don Gero", "Romani’s Mask": "Masque de Romani",
    "Garo’s Mask": "Masque de Garo", "Stone Mask": "Masque de pierre", "Giant’s Mask": "Masque du géant",
    "Gibdo Mask": "Masque de Gibdo", "Couple’s Mask": "Masque des époux", "Four Sword": "Épée de Quatre",
    "Great Fairy's Sword": "Épée de la Grande Fée", "Great Fairy’s Sword": "Épée de la Grande Fée",
    "Sword": "Épée", "White Sword": "Épée blanche", "Magical Sword": "Épée magique", "Wooden Sword": "Épée de bois",
    "Ordon Sword": "Épée d'Ordinn", "Hero's Clothes": "Tenue du héros", "Green Tunic": "Tunique verte",
    "Blue Mail": "Cotte bleue", "Red Mail": "Cotte rouge", "Blue Ring": "Anneau bleu", "Red Ring": "Anneau rouge",
    "Tree Branch": "Branche d'arbre", "Pegasus Boots": "Bottes de Pégase", "Roc's Feather": "Plume de Roc",
    "Power Bracelet": "Bracelet de force", "Zora's Flippers": "Palmes de Zora", "Zora’s Flippers": "Palmes de Zora",
    "Fire Rod": "Baguette de feu", "Ice Rod": "Baguette de glace", "Deku Stick": "Bâton Mojo",
    "Slingshot": "Lance-pierre", "Fairy Slingshot": "Lance-pierre des fées", "Beetle": "Scarabée",
}

GARMENT_WORDS = {
    "helm", "helmet", "armor", "greaves", "cap", "tunic", "trousers", "hood", "mask", "boots", "gaiters",
    "headdress", "pants", "top", "wrap", "veil", "sirwal", "shirt", "headband", "earrings", "circlet", "bandanna",
    "headpiece", "shoes", "leggings", "vest", "tights", "cloak", "gloves", "shorts", "robe", "doublet", "garb",
    "hat", "mail", "outfit", "crown", "bottoms", "clothes", "headgear", "sandals", "skirt", "cape", "guards",
    "brace", "wraps", "legwraps", "spaulder", "bracers", "hat", "tabard", "sash",
}

ELEMENT_WORDS = {"feu": "Fire", "eau": "Water", "glace": "Ice", "foudre": "Thunder", "vent": "Wind",
                 "terre": "Earth", "lumière": "Light", "ombre": "Shadow"}
STATUS_WORDS = {"brûlure": "Burn", "gel": "Freeze", "choc": "Shock", "poison": "Poison", "sommeil": "Sleep",
                "silence": "Silence", "cécité": "Blind", "enlisement": "Mire", "désarmement": "Disarm",
                "recul": "Knockback"}


def slot_for(item):
    cat, prof = item["category"], item["profile"]
    if prof == "MUN":
        return "Ammo", 0, ""
    if cat in ("Une main", "Arme 1 main", "Arme"):
        return "Weapon", 1, {"FEU": "Rod", "GLACE": "Rod", "FOUDRE": "Rod"}.get(prof, "Blade") if cat == "Arme" else "Blade"
    if cat in ("Deux mains", "Arme 2 mains"):
        if prof == "BATON":
            return "Weapon", 2, "Staff"
        if prof == "LANCE":
            return "Weapon", 2, "Spear"
        if prof == "ARC":
            return "Weapon", 2, "Bow"
        return "Weapon", 2, "Heavy"
    if cat == "Lance":
        return "Weapon", 2, "Spear"
    if cat == "Arc":
        return "Weapon", 2, "Bow"
    if cat in ("Bouclier", "Main secondaire"):
        return "Offhand", 0, "Shield"
    if cat == "Tête":
        return "Head", 0, ""
    if cat == "Torse":
        return "Torso", 0, ""
    if cat == "Jambes":
        return "Legs", 0, ""
    if cat in ("Tenue complète", "Tenue / pièce", "Tenue / accessoire"):
        return "Outfit", 0, ""
    if cat in ("Accessoire", "Anneau Oracle"):
        return "Accessory", 0, ""
    if cat in ("Outil de combat", "Outil / munition"):
        return "Tool", 0, ""
    if cat == "Exploration":
        return "Traversal", 0, ""
    if cat == "Sort / relique":
        return "Spell", 0, ""
    if cat == "Instrument / magie":
        return "Relic", 0, "Instrument"
    if cat == "Masque MM":
        return "Relic", 0, "Mask"
    if cat == "Amélioration permanente":
        return "Upgrade", 0, ""
    if cat in ("Apparence / trophée", "Relique de quête"):
        return "Cosmetic", 0, ""
    return "Cosmetic", 0, ""


def set_family(item):
    name = item["name"].replace("’", "'")
    m = re.search(r"\bof the (.+)$", name, re.I)
    if m:
        return (item["game"] + ":of the " + m.group(1)).lower()
    words = name.split()
    if len(words) >= 2 and words[-1].lower() in GARMENT_WORDS:
        return (item["game"] + ":" + " ".join(words[:-1])).lower()
    return (item["game"] + ":" + name).lower()


def pct(txt):
    return float(txt.replace(",", ".")) / 100.0


def parse_effect(text):
    """Traduit les effets individuels (anneaux, masques, TFH, EOW...) en modificateurs.
    Retourne (liste de modificateurs, entièrement_modélisé)."""
    mods = []
    unparsed = []
    for clause in [c.strip() for c in re.split(r"[;,]", text) if c.strip()]:
        c = clause.lower()
        ok = True
        m = re.match(r"^(pv|pm)( max)? ([+-]\d+) %$", c)
        if m:
            mods.append({"Stat": "HP_PCT" if m.group(1) == "pv" else "MP_PCT", "Value": pct(m.group(3))}); continue
        m = re.match(r"^(for|def|mag|dfm)( et def)? ([+-]\d+) %$", c)
        if m:
            key = {"for": "STR_PCT", "def": "DEF_PCT", "mag": "MAG_PCT", "dfm": "MDEF_PCT"}[m.group(1)]
            mods.append({"Stat": key, "Value": pct(m.group(3))})
            if m.group(2):
                mods.append({"Stat": "DEF_PCT", "Value": pct(m.group(3))})
            continue
        m = re.match(r"^(agi|spr|for) ([+-]\d+)$", c)
        if m:
            mods.append({"Stat": {"agi": "AGI", "spr": "SPR", "for": "STR"}[m.group(1)], "Value": float(m.group(2))}); continue
        m = re.match(r"^(\w+) reçue? ([+-]\d+) %$", c)
        if m and m.group(1) in ELEMENT_WORDS:
            mods.append({"Stat": "RES_" + ELEMENT_WORDS[m.group(1)], "Value": -pct(m.group(2))}); continue
        m = re.match(r"^résistance (\w+) ([+-]\d+) %$", c)
        if m and m.group(1) in ELEMENT_WORDS:
            mods.append({"Stat": "RES_" + ELEMENT_WORDS[m.group(1)], "Value": pct(m.group(2))}); continue
        m = re.match(r"^(\w+) ([+-]\d+) %$", c)
        if m and m.group(1) in ELEMENT_WORDS:
            mods.append({"Stat": "DMG_" + ELEMENT_WORDS[m.group(1)], "Value": pct(m.group(2))}); continue
        m = re.match(r"^dégâts physiques reçus ([+-]\d+) %$", c)
        if m:
            mods.append({"Stat": "TAKEN_PHYS", "Value": pct(m.group(1))}); continue
        m = re.match(r"^dégâts reçus ([+-]\d+) %$", c)
        if m:
            mods.append({"Stat": "TAKEN_ALL", "Value": pct(m.group(1))}); continue
        m = re.match(r"^immunité à (\w+)$", c)
        if m and m.group(1) in STATUS_WORDS:
            mods.append({"Stat": "IMMUNE_" + STATUS_WORDS[m.group(1)], "Value": 1.0}); continue
        m = re.match(r"^(esquive|critique) ([+-]\d+) points$", c)
        if m:
            mods.append({"Stat": "EVADE" if m.group(1) == "esquive" else "CRIT", "Value": float(m.group(2))}); continue
        m = re.match(r"^régénération (\d+) % pv à la fin de son activation$", c)
        if m:
            mods.append({"Stat": "REGEN_PCT", "Value": pct(m.group(1))}); continue
        m = re.match(r"^forme : coût initial -(\d+) résonance$", c)
        if m:
            mods.append({"Stat": "FORM_COST", "Value": -float(m.group(1))}); continue
        m = re.match(r"^une victoire rend (\d+) pm au porteur$", c)
        if m:
            mods.append({"Stat": "VICTORY_MP", "Value": float(m.group(1))}); continue
        m = re.match(r"^(soins de fioles|soins des fioles|potions de soins) \+(\d+) %$", c)
        if m:
            mods.append({"Stat": "POTION_HEAL", "Value": pct(m.group(2))}); continue
        m = re.match(r"^soins (produits|reçus) \+(\d+) %$", c)
        if m:
            mods.append({"Stat": "HEAL_DONE" if m.group(1) == "produits" else "HEAL_TAKEN", "Value": pct(m.group(2))}); continue
        m = re.match(r"^(bombes|boomerang|lame|circulaire|techniques de lame|objets offensifs) \+(\d+) %$", c)
        if m:
            key = {"bombes": "DMG_BOMB", "boomerang": "DMG_BOOMERANG", "lame": "DMG_BLADE", "circulaire": "DMG_SPIN",
                   "techniques de lame": "DMG_BLADE", "objets offensifs": "DMG_ITEM"}[m.group(1)]
            mods.append({"Stat": key, "Value": pct(m.group(2))}); continue
        m = re.match(r"^physique infligé \+(\d+) %$", c)
        if m:
            mods.append({"Stat": "DMG_PHYS", "Value": pct(m.group(1))}); continue
        m = re.match(r"^rubis( de combat)? \+(\d+) %$", c)
        if m:
            mods.append({"Stat": "RUPEES_PCT", "Value": pct(m.group(2))}); continue
        m = re.match(r"^pm des techniques aquatiques -(\d+) %$", c)
        if m:
            mods.append({"Stat": "MP_COST_Water", "Value": -pct(m.group(1))}); continue
        m = re.match(r"^outil : coût pm -(\d+) %$", c)
        if m:
            mods.append({"Stat": "TOOL_MP_PCT", "Value": -pct(m.group(1))}); continue
        if c in ("équipe", "unique", "taux total plafonné à 30 %"):
            continue
        ok = False
        unparsed.append(clause)
    return mods, not unparsed


def profile_mods(item, slot, rank):
    """Statistiques par défaut issues du profil (chapitre 10 + annexe A)."""
    prof = item["profile"]
    d = {"Power": 0, "Focus": 0, "Def": 0, "MDef": 0, "Element": "Neutral", "Mods": [], "Teaches": "", "TeachAP": 0,
         "SetBonus": []}
    per_piece = 1
    if slot in ("Head", "Legs"):
        d["Def"] = DEF_HEAD_LEGS[rank]
    elif slot == "Torso":
        d["Def"] = DEF_TORSO[rank]
    elif slot == "Outfit":
        d["Def"] = DEF_HEAD_LEGS[rank] * 2 + DEF_TORSO[rank]
        per_piece = 3
    if slot in ("Head", "Legs", "Torso", "Outfit"):
        d["MDef"] = d["Def"]
        if prof == "GARDE":
            d["Mods"].append({"Stat": "DEF_PCT", "Value": 0.03 * per_piece})
            d["Mods"].append({"Stat": "MDEF_PCT", "Value": 0.03 * per_piece})
        if prof in ("VIF", "FORCE"):
            d["MDef"] = d["Def"] // 2
    if slot == "Weapon":
        wp = item.get("weapon_power") or WEAPON_POWER[rank]
        if prof == "BATON":
            d["Focus"] = wp
        else:
            d["Power"] = wp
    if slot == "Offhand":
        d["Def"] = SHIELD_DEF[rank]
        d["Teaches"], d["TeachAP"] = "G01", 30
    if prof == "LAME":
        d["Teaches"], d["TeachAP"] = "L01", 30
    elif prof == "LOURD":
        d["Teaches"], d["TeachAP"] = "R03", 80
        d["Mods"].append({"Stat": "AGI", "Value": -3})
    elif prof == "LANCE":
        d["Teaches"], d["TeachAP"] = "LANCE_ESTOC", 30
    elif prof == "ARC":
        d["Teaches"], d["TeachAP"] = "A01", 30
    elif prof == "BATON":
        d["Teaches"], d["TeachAP"] = "STAFF_BOLT", 30
    elif prof in ("FEU", "GLACE", "FOUDRE", "LUMIERE", "OMBRE"):
        d["Element"] = {"FEU": "Fire", "GLACE": "Ice", "FOUDRE": "Thunder", "LUMIERE": "Light", "OMBRE": "Shadow"}[prof]
        d["Teaches"] = {"FEU": "ROD_FIRE", "GLACE": "ROD_ICE", "FOUDRE": "ROD_THUNDER", "LUMIERE": "L05", "OMBRE": ""}[prof]
        d["TeachAP"] = {"FEU": 30, "GLACE": 80, "FOUDRE": 80, "LUMIERE": 150, "OMBRE": 0}[prof]
        if prof == "OMBRE":
            d["Mods"] += [{"Stat": "DMG_ALL", "Value": 0.10}, {"Stat": "HEAL_TAKEN", "Value": -0.10}]
        if prof == "LUMIERE":
            d["Mods"].append({"Stat": "DMG_VS_Spectre", "Value": 0.20})
    elif prof == "OUTIL":
        n = item["name"].lower()
        if "double clawshot" in n:
            d["Teaches"], d["TeachAP"] = "R05", 150
        elif any(k in n for k in ("hookshot", "clawshot", "longshot", "grappling")):
            d["Teaches"], d["TeachAP"] = "R01", 30
        elif "boomerang" in n:
            d["Teaches"], d["TeachAP"] = "R02", 80
        elif "hammer" in n:
            d["Teaches"], d["TeachAP"] = "R03", 80
        elif "seed shooter" in n or "slingshot" in n:
            d["Teaches"], d["TeachAP"] = "R04", 150
        else:
            d["Teaches"], d["TeachAP"] = "TOOL_GENERIC", 0
    elif prof == "BOMBE":
        d["Teaches"], d["TeachAP"] = "TOOL_BOMB", 0
    elif prof == "TENUE":
        d["Mods"].append({"Stat": "HP_PCT", "Value": 0.03 * per_piece})
        d["SetBonus"] = [{"Stat": "HP_PCT", "Value": 0.05}]
    elif prof in ("EAU", "IGNI", "FROID", "ISOL"):
        el = {"EAU": "Water", "IGNI": "Fire", "FROID": "Ice", "ISOL": "Thunder"}[prof]
        if slot == "Outfit":
            d["Mods"].append({"Stat": "RES_" + el, "Value": 0.50})
        elif slot == "Accessory":
            pass  # effet individuel
        else:
            d["Mods"].append({"Stat": "RES_" + el, "Value": 0.15})
            d["SetBonus"] = [{"Stat": "RES_" + el, "Value": 0.05}]
        imm = {"IGNI": "Burn", "FROID": "Freeze", "ISOL": "Shock"}.get(prof)
        if imm:
            if slot == "Outfit":
                d["Mods"].append({"Stat": "IMMUNE_" + imm, "Value": 1})
            else:
                d["SetBonus"].append({"Stat": "IMMUNE_" + imm, "Value": 1})
        if prof == "EAU":
            d["Teaches"], d["TeachAP"] = "PASSIVE_WATER_GUARD", 80
    elif prof == "VIF":
        d["Mods"].append({"Stat": "AGI", "Value": 1 * per_piece})
        d["SetBonus"] = [{"Stat": "START_GAUGE_PCT", "Value": 0.10}]
    elif prof == "FORCE":
        d["Mods"].append({"Stat": "STR_PCT", "Value": 0.03 * per_piece})
        d["SetBonus"] = [{"Stat": "DMG_TECH", "Value": 0.05}]
    elif prof == "SAGE":
        d["Mods"].append({"Stat": "MAG_PCT", "Value": 0.03 * per_piece})
        d["SetBonus"] = [{"Stat": "MP_COST_PCT", "Value": -0.05}]
    elif prof == "GARDE":
        d["SetBonus"] = [{"Stat": "GUARD_STRENGTH", "Value": 0.60}]
    elif prof == "ACCES":
        d["Mods"].append({"Stat": "SPR", "Value": 5 + 3 * (rank - 1)})
    return d


# Fiches distinctives (chapitre 11) : remplacent le profil générique
OVERRIDES = {
    "master sword": {"Power": 54, "Rank": 4, "Element": "Light", "Teaches": "L05", "TeachAP": 150,
                     "Mods": [{"Stat": "DMG_VS_Spectre", "Value": 0.20}],
                     "Special": "P1,2 contre Spectre ; une seule version active par sauvegarde.",
                     "Quest": "Q_MASTER_01 — C5 : purifier trois sceaux du Temple du Temps."},
    "biggoron's sword": {"Power": 54, "Rank": 4, "Hands": 2, "Teaches": "R03", "TeachAP": 80,
                         "Mods": [{"Stat": "ATTACK_POWER", "Value": 0.25}],
                         "Quest": "Q_FORGE_02 — C6 : réparer la forge géante, puis duel sans objet."},
    "four sword": {"Power": 54, "Rank": 4, "Teaches": "S05", "TeachAP": 300,
                   "Quest": "C4 : quatre salles de coopération avec l'équipe."},
    "great fairy's sword": {"Power": 54, "Rank": 4, "Hands": 2, "Focus": 12,
                            "Quest": "C6 : restaurer quatre fontaines."},
    "mirror shield": {"Def": 13, "Rank": 3, "Teaches": "G04", "TeachAP": 150, "Quest": "C4 : énigme de miroirs."},
    "hylian shield": {"Def": 19, "Rank": 4, "Teaches": "G03", "TeachAP": 80,
                      "Mods": [{"Stat": "IMMUNE_Disarm", "Value": 0.5}], "Quest": "C5 : épreuve de protection de trois PNJ."},
    "magic armor": {"Rank": 4, "Mods": [{"Stat": "TAKEN_ALL", "Value": -0.20}, {"Stat": "RUPEE_PER_HIT", "Value": 10}],
                    "Quest": "C5 : atelier marchand restauré ; pas de dette."},
    "iron boots": {"Slot": "Legs", "Rank": 2, "Def": 4, "MDef": 4,
                   "Mods": [{"Stat": "IMMUNE_Knockback", "Value": 1}, {"Stat": "AGI", "Value": -4}],
                   "Quest": "C2 : carrière ; retrait gratuit hors combat."},
    "hover boots": {"Slot": "Legs", "Rank": 3, "Def": 6, "MDef": 6, "Mods": [{"Stat": "AGI", "Value": 1}],
                    "Quest": "C5 : sanctuaire de l'ombre."},
    "one-hit obliterator": {"Special": "Prêt temporaire dans une épreuve dédiée ; aucun transport dans la campagne."},
}


# Fiches distinctives propres à une occurrence précise (même nom, autre jeu = autre objet, chapitre 22)
OVERRIDES_BY_ID = {
    "TP_009": {"Mods": [{"Stat": "RES_Water", "Value": 0.50}, {"Stat": "RES_Fire", "Value": -0.20},
                        {"Stat": "RES_Ice", "Value": -0.20}],
               "Special": "Version TP : protection aquatique, faiblesses au feu et à la glace (-20 %)."},
}

# Masques-reliques : la relique équipée rend sa commande disponible (chapitre 10) ; TeachAP 0 = non maîtrisable
MASK_RELIC = {"MM_026": "MASK_BREMEN", "MM_027": "MASK_KAMARO", "MM_028": "MASK_BLAST", "MM_034": "MASK_GERO",
              "MM_039": "MASK_CIRCUS", "MM_030": "MASK_KEATON"}
FORM_MASKS = {"MM_020": "forme Mojo", "MM_021": "forme Goron", "MM_022": "forme Zora", "MM_023": "forme Oni",
              "MM_043": "forme Géant"}


def build_items(cat):
    items = []
    for it in cat["items"]:
        slot, hands, kind = slot_for(it)
        rank = max(1, min(5, int(it["rank"])))
        stats = profile_mods(it, slot, rank)
        indiv, modeled = ([], True)
        prof_default = cat["profiles"][it["profile"]]["effect"]
        if it["effect"] != prof_default:
            indiv, modeled = parse_effect(it["effect"])
        entry = {
            "Id": it["id"], "Game": it["game"], "GameName": GAME_NAMES.get(it["game"], it["game"]),
            "Name": it["name"], "NameFR": NAME_FR.get(it["name"], ""),
            "Profile": it["profile"], "Category": it["category"], "Slot": slot, "Hands": hands, "Kind": kind,
            "Rank": rank, "Power": stats["Power"], "Focus": stats["Focus"], "Def": stats["Def"], "MDef": stats["MDef"],
            "Element": stats["Element"], "Mods": stats["Mods"] + indiv, "SetBonus": stats["SetBonus"],
            "SetFamily": set_family(it) if slot in ("Head", "Torso", "Legs") else "",
            "Teaches": stats["Teaches"], "TeachAP": stats["TeachAP"],
            "Unlock": it["unlock"], "UnlockText": it["unlock_rule"],
            "GateChapter": max(ARCHIVE_CHAPTER.get(it["game"], 1), int(it.get("origin_gate_chapter") or 1)),
            "Source": it["source"], "Effect": it["effect"], "CanonNote": it.get("canon_note", ""),
            "Quest": it.get("quest", ""), "Special": "", "EffectModeled": modeled,
            "CanonicalIdentity": it.get("canonical_identity", ""),
        }
        ov = OVERRIDES.get(it["name"].replace("’", "'").lower())
        if ov:
            for k, v in ov.items():
                entry[k] = v
            if "Rank" in ov and "Power" not in ov and entry["Slot"] == "Weapon":
                entry["Power"] = WEAPON_POWER[ov["Rank"]]
        for k, v in OVERRIDES_BY_ID.get(it["id"], {}).items():
            entry[k] = v
        if entry["Slot"] == "Relic" and kind == "Mask":
            entry["Kind"] = "Mask"
        if it["id"] in MASK_RELIC:
            entry["Teaches"], entry["TeachAP"] = MASK_RELIC[it["id"]], 0
        if it["id"] in FORM_MASKS:
            entry["Special"] = "Débloque la " + FORM_MASKS[it["id"]] + " (commande Transformation)."
        items.append(entry)
    return items


# ---------------------------------------------------------------------------------------------
# Techniques (chapitre 9), kits de compagnons (12), commandes de formes (14), ennemis (17)
# ---------------------------------------------------------------------------------------------
def A(id, name, school, mp, ap, kind, target, power=0.0, element="Neutral", hits=1, desc="", source="",
      tier="", **kw):
    d = {"Id": id, "Name": name, "School": school, "MP": mp, "AP": ap, "Kind": kind, "Target": target,
         "Power": power, "Element": element, "Hits": hits, "Description": desc, "Source": source, "Tier": tier,
         "Resonance": 0, "Breach": 0, "IgnoreDefPct": 0.0, "ExecutePower": 0.0, "RequireHPPct": 0.0,
         "Apply": [], "Remove": [], "PerBattle": 0, "TargetGauge": 0, "TargetGaugeBoss": 0, "SelfGauge": 0,
         "Requires": "", "Anim": "Slash", "Flags": [], "HealPct": 0.0, "BarrierPct": 0.0, "ReviveHPPct": 0.0,
         "ResonanceGain": 0, "Dispel": False}
    d.update(kw)
    return d


def ST(status, chance=1.0, duration=2, magnitude=0.0):
    return {"Status": status, "Chance": chance, "Duration": duration, "Magnitude": magnitude}


def build_abilities():
    L = []
    # Lame du Courage
    L += [
        A("L01", "Taillade", "Blade", 3, 30, "Physical", "Enemy", 1.25, desc="P1,25 ; lame ; aucun état.", source="Épée de départ", tier="Fondamental"),
        A("L02", "Attaque circulaire", "Blade", 8, 80, "Physical", "AllEnemies", 0.8, desc="Tous, P0,8 ; Brèche +20.", source="Épreuve du maître", tier="Avancé", Breach=20, Anim="Spin"),
        A("L03", "Coup de grâce", "Blade", 8, 80, "Physical", "Enemy", 1.3, desc="P1,3 ; P2 si cible sous 25 % PV.", source="Mémoire TP", tier="Avancé", ExecutePower=2.0),
        A("L04", "Frappe à revers", "Blade", 10, 150, "Physical", "Enemy", 1.5, desc="P1,5 ; ignore 25 % DEF.", source="Quête Sheik", tier="Expert", IgnoreDefPct=0.25),
        A("L05", "Rayon d'épée", "Blade", 6, 150, "Physical", "Enemy", 1.4, "Light", desc="P1,4 lumière ; exige PV à 100 %.", source="Master Sword", tier="Expert", RequireHPPct=1.0, Anim="Beam"),
        A("L06", "Grande circulaire", "Blade", 20, 300, "Physical", "AllEnemies", 1.6, desc="Tous P1,6 ; exige L02 maîtrisée.", source="Héritage MC / TP", tier="Légendaire", Anim="Spin", Requires="mastered:L02"),
    ]
    # Gardien d'Hyrule
    L += [
        A("G01", "Coup de bouclier", "Guardian", 4, 30, "Physical", "Enemy", 0.6, desc="P0,6 ; Brèche +40 ; bouclier.", source="Bouclier", tier="Fondamental", Breach=40, Requires="shield", Anim="Bash"),
        A("G02", "Provocation", "Guardian", 4, 30, "Support", "Enemy", desc="Cible un ennemi ; il vise le lanceur à sa prochaine attaque compatible.", source="Entraînement Goron", tier="Fondamental", Apply=[ST("Taunt", 1.0, 1)], Anim="Taunt"),
        A("G03", "Interception", "Guardian", 8, 80, "Support", "Ally", desc="Prend le prochain coup destiné à un allié ; Garde active.", source="Bouclier hylien", tier="Avancé", Apply=[ST("Intercept", 1.0, 1)], Flags=["SelfGuard"], Anim="Guard"),
        A("G04", "Parade miroir", "Guardian", 12, 150, "Support", "Self", desc="Renvoie le prochain sort mono-cible à 50 % ; 1 activation.", source="Bouclier miroir", tier="Expert", Apply=[ST("MirrorParry", 1.0, 1, 0.5)], Anim="Guard"),
        A("G05", "Rempart", "Guardian", 14, 150, "Support", "AllAllies", desc="Groupe : dégâts physiques ×0,75, 2 activations.", source="Quête Daruk", tier="Expert", Apply=[ST("Rampart", 1.0, 2, 0.75)], Anim="Guard"),
        A("G06", "Serment du héros", "Guardian", 24, 300, "Support", "AllAllies", desc="Groupe : barrière égale à 15 % PV max, 2 activations.", source="Finale de lien", tier="Légendaire", BarrierPct=0.15, Apply=[ST("Barrier", 1.0, 2)], Anim="Cast"),
    ]
    # Œil du Héros
    L += [
        A("A01", "Tir précis", "Eye", 4, 30, "Physical", "Enemy", 1.3, desc="P1,3 ; ignore Cécité, pas l'immunité.", source="Arc", tier="Fondamental", Flags=["IgnoreBlind", "Ranged"], Anim="Shoot"),
        A("A02", "Flèche de feu", "Eye", 6, 80, "Physical", "Enemy", 1.3, "Fire", desc="P1,3 feu ; Brûlure 40 %.", source="Arc + flèche débloquée", tier="Avancé", Apply=[ST("Burn", 0.4, 3)], Flags=["Ranged"], Anim="Shoot"),
        A("A03", "Flèche de glace", "Eye", 8, 80, "Physical", "Enemy", 1.1, "Ice", desc="P1,1 glace ; Gel 40 %.", source="Arc + flèche débloquée", tier="Avancé", Apply=[ST("Freeze", 0.4, 1)], Flags=["Ranged"], Anim="Shoot"),
        A("A04", "Flèche de foudre", "Eye", 8, 80, "Physical", "Enemy", 1.3, "Thunder", desc="P1,3 foudre ; Choc 60 %.", source="Atelier Gerudo", tier="Avancé", Apply=[ST("Shock", 0.6, 0)], Flags=["Ranged"], Anim="Shoot"),
        A("A05", "Volée", "Eye", 14, 150, "Physical", "Enemy", 0.5, hits=3, desc="Trois impacts P0,5 sur une cible ; une seule génération de Résonance.", source="Arc de Revali", tier="Expert", Flags=["Ranged"], Anim="Shoot"),
        A("A06", "Flèche de lumière", "Eye", 20, 300, "Physical", "Enemy", 2.0, "Light", desc="P2 lumière ; dissipe un bonus ennemi.", source="Sanctuaire de lumière", tier="Légendaire", Dispel=True, Flags=["Ranged"], Anim="Shoot"),
    ]
    # Art des Reliques
    L += [
        A("R01", "Grappin tactique", "Relic", 3, 30, "Physical", "Enemy", 0.7, desc="P0,7 ; enlève Couvert ; Brèche +40 sur cible ancrable ; sabote les conduits de Néréide.", source="Grappin", tier="Fondamental", Breach=40, Flags=["Hookshot", "Ranged"], Anim="Hook"),
        A("R02", "Boomerang entravant", "Relic", 5, 80, "Physical", "Enemy", 0.8, desc="P0,8 ; jauge ennemie -150 (boss -75).", source="Boomerang", tier="Avancé", TargetGauge=-150, TargetGaugeBoss=-75, Flags=["Ranged"], Anim="Throw"),
        A("R03", "Brise-garde", "Relic", 8, 80, "Physical", "Enemy", 1.1, desc="P1,1 ; Brèche +60 ; arme lourde.", source="Marteau / masse", tier="Avancé", Breach=60, Requires="heavy", Anim="Smash"),
        A("R04", "Graine mystère", "Relic", 10, 150, "Magical", "Enemy", 1.4, desc="Reproduit le dernier élément allié à P1,4 ; neutre si aucun.", source="Oracle", tier="Expert", Flags=["LastAllyElement"], Anim="Throw"),
        A("R05", "Double grappin", "Relic", 12, 150, "Physical", "Enemy", 0.8, hits=2, desc="Deux impacts P0,8 ; supprime Couvert.", source="Double Clawshots", tier="Expert", Flags=["Hookshot", "Ranged"], Anim="Hook"),
        A("R06", "Amalgame maîtrisé", "Relic", 16, 300, "Physical", "Enemy", 1.8, desc="P1,8 avec l'élément du matériau ; consomme 1 charge.", source="Atelier céleste", tier="Légendaire", Flags=["FuseElement"], Anim="Slash"),
        A("TOOL_BOMB", "Bombe", "Relic", 0, 0, "Physical", "AllEnemies", 1.3, desc="P1,3 neutre de zone ; consomme une bombe ; brise Carapace.", source="Sac de bombes", Flags=["ConsumesBomb", "Ranged"], Anim="Throw", Breach=20),
        A("TOOL_GENERIC", "Outil", "Relic", 3, 0, "Physical", "Enemy", 0.7, desc="P0,7 ; active une interaction de terrain.", source="Outil de combat", Flags=["Ranged"], Anim="Throw"),
    ]
    # Chants des Âges
    L += [
        A("C01", "Chant d'apaisement", "Chant", 8, 30, "Heal", "Ally", 1.2, desc="Une cible : soin P1,2.", source="Ocarina", tier="Fondamental", Anim="Chant"),
        A("C02", "Chant des tempêtes", "Chant", 12, 80, "Magical", "AllEnemies", 0.6, "Water", desc="Tous ennemis : P0,6 eau, Mouillé 100 %.", source="Moulin", tier="Avancé", Apply=[ST("Wet", 1.0, 2)], Anim="Chant"),
        A("C03", "Chant du temps", "Chant", 12, 150, "Support", "Ally", desc="Un allié : Hâte 2 activations.", source="Harp of Ages", tier="Expert", Apply=[ST("Haste", 1.0, 2)], Anim="Chant"),
        A("C04", "Vent de Farore", "Chant", 10, 150, "Support", "Ally", desc="Retire Enlisement / Cécité d'un allié ; jauge +100.", source="Fontaine de Farore", tier="Expert", Remove=["Mire", "Blind"], TargetGauge=100, Anim="Cast"),
        A("C05", "Amour de Nayru", "Chant", 18, 150, "Support", "Ally", desc="Un allié : dégâts magiques ×0,5, 2 activations.", source="Fontaine de Nayru", tier="Expert", Apply=[ST("MagicShield", 1.0, 2, 0.5)], Anim="Cast"),
        A("C06", "Chœur du Courage", "Chant", 24, 300, "Heal", "AllAllies", 1.1, desc="Groupe : soin P1,1 et supprime Silence.", source="Huit instruments", tier="Légendaire", Remove=["Silence"], Anim="Chant"),
    ]
    # Âmes et Masques
    L += [
        A("S01", "Accord spirituel", "Soul", 0, 30, "Support", "Self", desc="+12 Résonance ; 1 fois par personnage et par combat.", source="Premier lien", tier="Fondamental", ResonanceGain=12, PerBattle=1, Anim="Cast"),
        A("S02", "Souffle des ancêtres", "Soul", 8, 80, "Support", "Ally", desc="Un allié : SPR +15 %, 2 activations.", source="Premier esprit rang 2", tier="Avancé", Apply=[ST("SprUp", 1.0, 2, 0.15)], Anim="Cast"),
        A("S03", "Passage des âmes", "Soul", 12, 150, "Support", "Ally", desc="Retire Poison / Brûlure d'un allié.", source="Trois masques", tier="Expert", Remove=["Poison", "Burn"], Anim="Cast"),
        A("S04", "Réveil féerique", "Soul", 24, 150, "Revive", "DeadAlly", desc="Réanime un allié à 25 % PV ; 1/combat.", source="Grande Fée", tier="Expert", ReviveHPPct=0.25, PerBattle=1, Anim="Cast"),
        A("S05", "Quatre reflets", "Soul", 16, 300, "Physical", "Enemy", 0.45, hits=4, desc="Quatre impacts P0,45 ; doubles visuels seulement.", source="Four Sword", tier="Légendaire", Resonance=40, Anim="Spin"),
        A("S06", "Lame du crépuscule", "Soul", 18, 300, "Physical", "Enemy", 2.0, "Shadow", desc="P2 ombre ; Fragilité 100 %, 2 activations.", source="Lien Midna rang 3", tier="Légendaire", Resonance=40, Apply=[ST("Fragile", 1.0, 2, 0.15)], Anim="Slash"),
    ]
    # Profils d'armes (annexe A)
    L += [
        A("LANCE_ESTOC", "Estoc", "Blade", 4, 30, "Physical", "Enemy", 1.2, desc="P1,2 ; ignore la ligne arrière.", source="Lance", tier="Fondamental", Requires="spear", Anim="Thrust"),
        A("STAFF_BOLT", "Trait", "Magic", 4, 30, "Magical", "Enemy", 1.2, desc="P1,2 magique.", source="Bâton", tier="Fondamental", Anim="Cast"),
        A("ROD_FIRE", "Sort de feu", "Magic", 4, 30, "Magical", "Enemy", 1.2, "Fire", desc="Élément feu ; +10 % contre Gel.", source="Baguette de feu", tier="Fondamental", Anim="Cast"),
        A("ROD_ICE", "Sort de glace", "Magic", 8, 80, "Magical", "Enemy", 1.1, "Ice", desc="Élément glace ; Gel 25 %.", source="Baguette de glace", tier="Avancé", Apply=[ST("Freeze", 0.25, 1)], Anim="Cast"),
        A("ROD_THUNDER", "Sort de foudre", "Magic", 8, 80, "Magical", "Enemy", 1.2, "Thunder", desc="Foudre ; ×1,5 contre cible Mouillée.", source="Arme de foudre", tier="Avancé", Anim="Cast"),
        A("PASSIVE_WATER_GUARD", "Garde aquatique (passif)", "Passive", 0, 80, "Passive", "Self", desc="Maîtrisé : résistance eau +10 % sans la tenue.", source="Tenue Zora", tier="Avancé"),
    ]
    # Kits de compagnons (chapitre 12)
    L += [
        A("SHEIK_NEEDLE", "Aiguilles", "Companion", 0, 0, "Physical", "Enemy", 1.0, desc="P1 ; aiguilles de Sheik.", source="Sheik", Flags=["Ranged"], Anim="Throw"),
        A("SHEIK_VEIL", "Voile", "Companion", 8, 0, "Support", "Self", desc="Esquive +10 points, 2 activations.", source="Sheik", Apply=[ST("Evade", 1.0, 2, 10)], Anim="Cast"),
        A("SHEIK_REVEAL", "Révélation", "Companion", 0, 0, "Support", "Enemy", desc="Analyse gratuite : révèle résistances et intentions ; 1/combat.", source="Sheik", PerBattle=1, Flags=["Analyze"], Anim="Cast"),
        A("ZELDA_SEAL", "Sceau de lumière", "Companion", 10, 0, "Magical", "Enemy", 1.5, "Light", desc="P1,5 lumière (posture Zelda).", source="Zelda", Anim="Cast"),
        A("IMPA_BLADE", "Lame Sheikah", "Companion", 0, 0, "Physical", "Enemy", 1.0, desc="P1.", source="Impa", Anim="Slash"),
        A("IMPA_MARK", "Marque de faiblesse", "Companion", 8, 0, "Support", "Enemy", desc="Brèche +40 ; la cible subit +10 % de dégâts, 2 activations.", source="Impa", Breach=40, Apply=[ST("Marked", 1.0, 2, 0.10)], Anim="Cast"),
        A("MIPHA_SPEAR", "Trident", "Companion", 0, 0, "Physical", "Enemy", 1.0, desc="P1 ; lance.", source="Mipha", Anim="Thrust"),
        A("MIPHA_HEAL", "Grâce de Mipha", "Companion", 8, 0, "Heal", "Ally", 1.6, desc="Soin P1,6.", source="Mipha", Anim="Heal"),
        A("MIPHA_WAVE", "Vague", "Companion", 8, 0, "Magical", "Enemy", 1.0, "Water", desc="P1 eau ; Mouillé 50 %.", source="Mipha", Apply=[ST("Wet", 0.5, 2)], Anim="Cast"),
        A("MIPHA_REVIVE", "Réveil de Mipha", "Companion", 24, 0, "Revive", "DeadAlly", desc="Réanime à 25 % PV ; 1/combat.", source="Mipha", ReviveHPPct=0.25, PerBattle=1, Anim="Heal"),
        A("DARUK_HAMMER", "Marteau Boulder-breaker", "Companion", 0, 0, "Physical", "Enemy", 1.25, desc="P1,25.", source="Daruk", Anim="Smash"),
        A("DARUK_PERC", "Percussion", "Companion", 10, 0, "Physical", "Enemy", 1.0, "Earth", desc="P1 terre ; Brèche +60.", source="Daruk", Breach=60, Anim="Smash"),
        A("REVALI_SHOT", "Tir de Revali", "Companion", 0, 0, "Physical", "Enemy", 1.0, desc="P1.", source="Revali", Flags=["Ranged"], Anim="Shoot"),
        A("REVALI_VOLLEY", "Volée céleste", "Companion", 12, 0, "Physical", "Enemy", 0.45, hits=3, desc="3×P0,45.", source="Revali", Flags=["Ranged"], Anim="Shoot"),
        A("REVALI_GALE", "Rafale", "Companion", 10, 0, "Magical", "Enemy", 1.2, "Wind", desc="P1,2 vent.", source="Revali", Anim="Cast"),
        A("ANALYZE", "Analyse", "Companion", 0, 0, "Support", "Enemy", desc="Révèle résistances et intentions.", source="Revali / Keaton", Flags=["Analyze"], Anim="Cast"),
        A("URBOSA_SCIM", "Cimeterre", "Companion", 0, 0, "Physical", "Enemy", 1.0, desc="P1.", source="Urbosa", Anim="Slash"),
        A("URBOSA_ARC", "Arc électrique", "Companion", 10, 0, "Magical", "Enemy", 1.4, "Thunder", desc="P1,4 foudre.", source="Urbosa", Anim="Cast"),
        A("URBOSA_FURY", "Furie d'Urbosa", "Companion", 16, 0, "Magical", "AllEnemies", 1.0, "Thunder", desc="Tous P1,0 foudre.", source="Urbosa", Anim="Cast"),
        A("MIDNA_BOLT", "Trait des ombres", "Companion", 6, 0, "Magical", "Enemy", 1.3, "Shadow", desc="P1,3 ombre.", source="Midna", Anim="Cast"),
        A("MIDNA_DISPEL", "Dissipation", "Companion", 8, 0, "Support", "Enemy", desc="Retire les bonus ennemis.", source="Midna", Dispel=True, Anim="Cast"),
    ]
    # Commandes de formes (chapitre 14)
    L += [
        A("F_DEKU_BUBBLE", "Bulle Mojo", "Form", 4, 0, "Magical", "Enemy", 1.2, "Wind", desc="P1,2 vent.", source="Forme Mojo", Anim="Shoot"),
        A("F_DEKU_SEEDS", "Graines", "Form", 6, 0, "Physical", "Enemy", 0.4, hits=3, desc="3×P0,4.", source="Forme Mojo", Anim="Shoot"),
        A("F_DEKU_POLLEN", "Pollen", "Form", 6, 0, "Heal", "Ally", 0.8, desc="Soin P0,8.", source="Forme Mojo", Anim="Heal"),
        A("F_GORON_PUNCH", "Poing Goron", "Form", 0, 0, "Physical", "Enemy", 1.25, desc="P1,25.", source="Forme Goron", Anim="Smash"),
        A("F_GORON_ROLL", "Roulade", "Form", 10, 0, "Physical", "Enemy", 1.7, desc="P1,7.", source="Forme Goron", Anim="Roll"),
        A("F_GORON_POUND", "Percussion Goron", "Form", 12, 0, "Physical", "Enemy", 1.0, "Earth", desc="P1,0 ; Brèche +80.", source="Forme Goron", Breach=80, Anim="Smash"),
        A("F_ZORA_FINS", "Lames-nageoires", "Form", 0, 0, "Physical", "Enemy", 0.65, hits=2, desc="2×P0,65.", source="Forme Zora", Anim="Throw"),
        A("F_ZORA_WAVE", "Onde", "Form", 8, 0, "Magical", "Enemy", 1.4, "Water", desc="P1,4 eau.", source="Forme Zora", Apply=[ST("Wet", 0.5, 2)], Anim="Cast"),
        A("F_ZORA_BARRIER", "Barrière électrique", "Form", 8, 0, "Support", "Self", desc="Dégâts reçus ×0,65, 1 activation.", source="Forme Zora", Apply=[ST("ZoraBarrier", 1.0, 1, 0.65)], Anim="Guard"),
        A("F_ONI_BLADE", "Lame hélicoïdale", "Form", 0, 0, "Physical", "Enemy", 1.4, desc="P1,4.", source="Forme Oni", Anim="Slash"),
        A("F_ONI_BEAM", "Rayon divin", "Form", 12, 0, "Magical", "Enemy", 2.0, "Light", desc="P2 lumière.", source="Forme Oni", Anim="Beam"),
        A("F_ONI_MOON", "Lune tranchante", "Form", 20, 0, "Physical", "AllEnemies", 1.5, desc="Tous P1,5.", source="Forme Oni", Anim="Spin"),
        A("F_GIANT_STRIKE", "Frappe colossale", "Form", 0, 0, "Physical", "Enemy", 1.5, desc="P1,5.", source="Forme Géant", Anim="Smash"),
        A("F_GIANT_GRAB", "Prise", "Form", 12, 0, "Physical", "Enemy", 0.8, desc="Brèche +120.", source="Forme Géant", Breach=120, Anim="Smash"),
        A("F_WOLF_BITE", "Morsure", "Form", 0, 0, "Physical", "Enemy", 1.2, desc="P1,2.", source="Loup", Anim="Bite"),
        A("F_WOLF_HUNT", "Traque", "Form", 6, 0, "Physical", "Enemy", 1.4, desc="P1,4 (contre cible marquée).", source="Loup", Anim="Bite"),
        A("F_WOLF_SENSE", "Sens", "Form", 0, 0, "Support", "Enemy", desc="Analyse et révèle Spectre.", source="Loup", Flags=["Analyze"], Anim="Cast"),
        A("F_MAJORA_ORB", "Orbe lunaire", "Form", 10, 0, "Magical", "Enemy", 1.8, "Shadow", desc="P1,8 ombre.", source="Résonance de Majora", Anim="Cast"),
        A("F_MAJORA_DANCE", "Danse maudite", "Form", 14, 0, "Magical", "AllEnemies", 1.1, "Shadow", desc="Tous P1,1.", source="Résonance de Majora", Anim="Spin"),
        A("F_MAJORA_RIFT", "Déchirure", "Form", 22, 0, "Magical", "Enemy", 2.3, "Shadow", desc="P2,3.", source="Résonance de Majora", Anim="Beam"),
    ]
    # Masques sociaux (annexe B) utilisables comme relique
    L += [
        A("MASK_BREMEN", "Marche de Brême", "Mask", 12, 0, "Support", "AllAllies", desc="Hâte de groupe, 2 activations.", source="Masque de Brême", Apply=[ST("Haste", 1.0, 2)], Anim="Chant"),
        A("MASK_KAMARO", "Danse de Kamaro", "Mask", 8, 0, "Support", "Self", desc="Esquive +10 points, 2 activations.", source="Masque de Kamaro", Apply=[ST("Evade", 1.0, 2, 10)], Anim="Cast"),
        A("MASK_BLAST", "Explosion", "Mask", 0, 0, "Physical", "Enemy", 1.6, desc="P1,6 ; coûte 10 % PV non létaux.", source="Masque d'explosion", Flags=["SelfDamage10"], Anim="Smash"),
        A("MASK_GERO", "Chœur de Don Gero", "Mask", 8, 0, "Heal", "AllAllies", 0.0, desc="Soin 10 % PV de groupe.", source="Masque de Don Gero", HealPct=0.10, Anim="Chant"),
        A("MASK_CIRCUS", "Parade du chef de troupe", "Mask", 6, 0, "Support", "AllEnemies", desc="Provocation sur deux cibles.", source="Circus Leader's Mask", Apply=[ST("Taunt", 1.0, 1)], Anim="Taunt"),
        A("MASK_KEATON", "Flair de Keaton", "Mask", 0, 0, "Support", "Enemy", desc="Analyse gratuite une fois par combat.", source="Masque de Keaton", PerBattle=1, Flags=["Analyze"], Anim="Cast"),
    ]
    # Actions ennemies (prototype, chapitre 17)
    L += [
        A("E_OCTO_ROCK", "Crachat de roc", "Enemy", 0, 0, "Physical", "Enemy", 1.0, desc="Projectile P1.", Anim="Shoot"),
        A("E_OCTO_INK", "Encre", "Enemy", 4, 0, "Magical", "Enemy", 0.6, "Water", desc="P0,6 eau ; Cécité 30 %.", Apply=[ST("Blind", 0.3, 2)], Anim="Shoot"),
        A("E_CHUCHU_SLAM", "Écrasement gluant", "Enemy", 0, 0, "Physical", "Enemy", 1.0, desc="P1.", Anim="Smash"),
        A("E_CHUCHU_SPLASH", "Éclaboussure", "Enemy", 4, 0, "Magical", "Enemy", 0.8, "Water", desc="P0,8 eau ; Mouillé 100 %.", Apply=[ST("Wet", 1.0, 2)], Anim="Cast"),
        A("E_SENT_BOLT", "Décharge", "Enemy", 6, 0, "Magical", "Enemy", 1.2, "Thunder", desc="P1,2 foudre ; Choc 40 %.", Apply=[ST("Shock", 0.4, 0)], Anim="Beam"),
        A("E_SENT_STORM", "Orage", "Enemy", 12, 0, "Magical", "AllEnemies", 0.8, "Thunder", desc="Tous P0,8 foudre.", Anim="Beam"),
        A("E_SENT_CHARGE", "Charge statique", "Enemy", 0, 0, "Support", "Self", desc="Prépare Orage.", Flags=["Prepare:E_SENT_STORM"], Anim="Cast"),
        A("E_NER_JET", "Jet d'eau", "Enemy", 0, 0, "Magical", "Enemy", 1.0, "Water", desc="P1 eau sur une cible.", Anim="Beam"),
        A("E_NER_CRUE_PREP", "Prépare : Crue", "Enemy", 0, 0, "Support", "Self", desc="Les conduits se remplissent. Un grappin sur un conduit remplace Crue par Jet et ajoute 80 Brèche.", Flags=["Prepare:E_NER_CRUE"], Anim="Charge"),
        A("E_NER_CRUE", "Crue", "Enemy", 0, 0, "Magical", "AllEnemies", 1.2, "Water", desc="Tous P1,2 eau.", Anim="Wave"),
        A("E_NER_WETALL", "Voile de bruine", "Enemy", 0, 0, "Support", "AllEnemies", desc="Mouillé de groupe, sans dégâts.", Apply=[ST("Wet", 1.0, 2)], Anim="Wave"),
        A("E_NER_ARC_PREP", "Surcharge", "Enemy", 0, 0, "Support", "Self", desc="Charge visible : Arc électrique au prochain tour.", Flags=["Prepare:E_NER_ARC"], Anim="Charge"),
        A("E_NER_ARC", "Arc électrique", "Enemy", 0, 0, "Magical", "Enemy", 1.0, "Thunder", desc="P1 foudre sur une cible.", Anim="Beam"),
        A("E_NER_WAVE", "Vague", "Enemy", 0, 0, "Magical", "Enemy", 1.1, "Water", desc="P1,1 eau.", Anim="Wave"),
        A("E_NER_STRIKE", "Frappe de pierre", "Enemy", 0, 0, "Physical", "Enemy", 1.2, desc="Frappe neutre P1,2.", Anim="Smash"),
        A("E_NER_DELUGE_PREP", "Prépare : Déluge", "Enemy", 0, 0, "Support", "Self", desc="Deux interactions de vanne, partagées entre alliés, annulent Déluge.", Flags=["Prepare:E_NER_DELUGE"], Anim="Charge"),
        A("E_NER_DELUGE", "Déluge", "Enemy", 0, 0, "Magical", "AllEnemies", 1.6, "Water", desc="Tous P1,6 eau.", Anim="Wave"),
    ]
    # Interactions de terrain et commandes génériques
    L += [
        A("INTERACT_VALVE", "Vanne", "Field", 0, 0, "Support", "Self", desc="Fermer une vanne de la citerne (Déluge : 2 nécessaires).", Flags=["Valve"], Anim="Hook"),
        A("ATTACK", "Attaquer", "Basic", 0, 0, "Physical", "Enemy", 1.0, desc="P1 physique ; arme et portée appliquées.", Anim="Slash"),
    ]
    return L


def build_forms():
    return [
        {"Id": "Deku", "Name": "Forme Mojo", "MaskItem": "MM_020", "Resonance": 40, "ATQ": 0.8, "AMAG": 1.1, "DEF": 1.0, "AGI": 1.15,
         "Taken": {"Fire": 1.25}, "Commands": ["F_DEKU_BUBBLE", "F_DEKU_SEEDS", "F_DEKU_POLLEN"], "Scale": 0.62, "Color": "#6E8B3D",
         "Weakness": "Feu reçu ×1,25 ; fleurs de propulsion en exploration.", "Late": False, "Flag": "form.deku"},
        {"Id": "Goron", "Name": "Forme Goron", "MaskItem": "MM_021", "Resonance": 60, "ATQ": 1.25, "AMAG": 1.0, "DEF": 1.3, "AGI": 0.8,
         "Taken": {"Water": 1.25}, "Commands": ["F_GORON_PUNCH", "F_GORON_ROLL", "F_GORON_POUND"], "Scale": 1.35, "Color": "#B7874A",
         "Weakness": "Eau reçue ×1,25 ; dalles lourdes et rochers.", "Late": False, "Flag": "form.goron"},
        {"Id": "Zora", "Name": "Forme Zora", "MaskItem": "MM_022", "Resonance": 60, "ATQ": 1.0, "AMAG": 1.2, "DEF": 1.0, "AGI": 1.1,
         "Taken": {"Water": 0.75, "Thunder": 1.25}, "Commands": ["F_ZORA_FINS", "F_ZORA_WAVE", "F_ZORA_BARRIER"], "Scale": 1.05, "Color": "#9FD6E6",
         "Weakness": "Foudre reçue ×1,25 ; nage et mécanismes sous-marins.", "Late": False, "Flag": "form.zora"},
        {"Id": "Oni", "Name": "Dieu démon (Oni)", "MaskItem": "MM_023", "Resonance": 100, "ATQ": 1.35, "AMAG": 1.2, "DEF": 1.0, "AGI": 1.0,
         "Taken": {}, "Commands": ["F_ONI_BLADE", "F_ONI_BEAM", "F_ONI_MOON"], "Scale": 1.18, "Color": "#E8E6EE",
         "Weakness": "Après retour : coût PM +20 % pendant 2 activations. Forme tardive.", "Late": True, "Flag": "form.oni"},
        {"Id": "Giant", "Name": "Forme Géant", "MaskItem": "MM_043", "Resonance": 100, "ATQ": 1.3, "AMAG": 1.0, "DEF": 1.2, "AGI": 1.0,
         "Taken": {}, "Commands": ["F_GIANT_STRIKE", "F_GIANT_GRAB"], "Scale": 2.6, "Color": "#4F7A3A",
         "Weakness": "Seulement dans les arènes marquées giant_allowed.", "Late": True, "Flag": "form.giant", "ArenaOnly": True},
        {"Id": "Wolf", "Name": "Loup du crépuscule", "MaskItem": "", "Resonance": 50, "ATQ": 1.1, "AMAG": 1.0, "DEF": 1.0, "AGI": 1.2,
         "Taken": {}, "Commands": ["F_WOLF_BITE", "F_WOLF_HUNT", "F_WOLF_SENSE"], "Scale": 0.9, "Color": "#5A5F66",
         "Weakness": "Aucun arc ni objet tenu ; lié à TP, pas à MM.", "Late": False, "Flag": "form.wolf", "NoItems": True},
        {"Id": "Majora", "Name": "Résonance de Majora", "MaskItem": "", "Resonance": 100, "ATQ": 1.15, "AMAG": 1.35, "DEF": 1.0, "AGI": 1.0,
         "Taken": {}, "Commands": ["F_MAJORA_ORB", "F_MAJORA_DANCE", "F_MAJORA_RIFT"], "Scale": 1.1, "Color": "#6B3FA0",
         "Weakness": "Création de fan après fin ; corruption +20/action ; fin à 60 ; retire 10 % PV non létaux au retour.",
         "Late": True, "Flag": "form.majora", "Corruption": 20},
    ]


def build_spirits():
    return [
        {"Id": "GreatFairy", "Name": "Grande Fée", "Resonance": 60, "Kind": "Heal", "Target": "AllAllies", "Power": 2.0, "Element": "Neutral",
         "Remove": ["Poison", "Burn", "Silence"], "Apply": [], "Desc": "Groupe : soin P2 et retire Poison / Brûlure / Silence ; n'annule pas KO.",
         "Acquire": "C2, première fontaine", "Passive": "Soins +5 %", "Color": "#F4A8D8"},
        {"Id": "Valoo", "Name": "Valoo", "Resonance": 75, "Kind": "Magical", "Target": "AllEnemies", "Power": 2.3, "Element": "Fire",
         "Remove": [], "Apply": [ST("Burn", 1.0, 3)], "Desc": "Tous ennemis : feu P2,3 ; Brûlure 100 % si sensible.",
         "Acquire": "C6, sommet de l'île", "Passive": "Feu +5 %", "Color": "#E0522B"},
        {"Id": "Jabun", "Name": "Jabun", "Resonance": 75, "Kind": "Magical", "Target": "AllEnemies", "Power": 2.0, "Element": "Water",
         "Remove": [], "Apply": [ST("Wet", 1.0, 2)], "Desc": "Tous ennemis : eau P2 ; Mouillé ; alliés eau reçue -25 % durant 2 activations.",
         "Acquire": "C6, grotte marine", "Passive": "Eau +5 %", "Color": "#3E8FD1"},
        {"Id": "FourGiants", "Name": "Les Quatre Géants", "Resonance": 100, "Kind": "Magical", "Target": "AllEnemies", "Power": 2.0, "Element": "Earth",
         "Remove": [], "Apply": [], "Desc": "Tous ennemis P2 terre ; annule une attaque de catastrophe télégraphiée compatible.",
         "Acquire": "C4, pacte des quatre régions", "Passive": "Brèche +5", "Color": "#8C7A55", "CancelPrepare": True},
        {"Id": "WindFish", "Name": "Poisson-Rêve", "Resonance": 100, "Kind": "Support", "Target": "AllAllies", "Power": 0.0, "Element": "Neutral",
         "Remove": [], "Apply": [], "Desc": "Restaure 20 % PM au groupe et dissipe tous les bonus ennemis non scénarisés.",
         "Acquire": "C6, réunir huit instruments", "Passive": "PM max +5 %", "Color": "#C9D8F0", "MPRestorePct": 0.2, "Dispel": True},
        {"Id": "ForestSpirit", "Name": "Esprit de la forêt", "Resonance": 60, "Kind": "Support", "Target": "AllAllies", "Power": 0.0, "Element": "Neutral",
         "Remove": [], "Apply": [ST("Regen", 1.0, 3, 0.05)], "Desc": "Groupe : régénération 5 % PV, 3 activations ; entrave les ennemis terrestres.",
         "Acquire": "C1-3, quête Saria", "Passive": "Poison reçu -10 %", "Color": "#63B35C"},
        {"Id": "Nayru", "Name": "Protection de Nayru", "Resonance": 80, "Kind": "Support", "Target": "AllAllies", "Power": 0.0, "Element": "Neutral",
         "Remove": [], "Apply": [ST("MagicShield", 1.0, 2, 0.5)], "Desc": "Groupe : dégâts magiques ×0,5, 2 activations (bénédiction, pas contrôle d'une déesse).",
         "Acquire": "C3, fontaine", "Passive": "", "Color": "#5DA9E9"},
        {"Id": "Farore", "Name": "Souffle de Farore", "Resonance": 70, "Kind": "Support", "Target": "AllAllies", "Power": 0.0, "Element": "Neutral",
         "Remove": ["Mire"], "Apply": [ST("Haste", 1.0, 2)], "Desc": "Groupe : Hâte 2 activations ; retire Enlisement.",
         "Acquire": "C3, fontaine", "Passive": "AGI +1", "Color": "#6CCB70"},
    ]


def build_duos():
    return [
        {"Id": "DUO_SHEIK", "Name": "Ombre et lame", "A": "link", "B": "sheik", "Resonance": 40, "MPA": 0, "MPB": 0,
         "Kind": "Physical", "Target": "Enemy", "Power": 1.1, "Hits": 2, "Element": "Neutral", "Apply": [ST("Fragile", 1.0, 2, 0.15)], "HealPower": 0.0,
         "Breach": 0, "Desc": "Deux impacts P1,1 ; Fragilité 100 %.", "RequireForm": ""},
        {"Id": "DUO_MIPHA", "Name": "Marée du courage", "A": "link", "B": "mipha", "Resonance": 50, "MPA": 0, "MPB": 8,
         "Kind": "Magical", "Target": "Enemy", "Power": 1.5, "Hits": 1, "Element": "Water", "Apply": [], "HealPower": 0.7,
         "Breach": 0, "Desc": "P1,5 eau ; soin du groupe P0,7.", "RequireForm": ""},
        {"Id": "DUO_DARUK", "Name": "Roc et acier", "A": "link", "B": "daruk", "Resonance": 50, "MPA": 0, "MPB": 0,
         "Kind": "Physical", "Target": "Enemy", "Power": 1.8, "Hits": 1, "Element": "Earth", "Apply": [], "HealPower": 0.0,
         "Breach": 100, "Desc": "P1,8 terre ; Brèche +100.", "RequireForm": ""},
        {"Id": "DUO_MIDNA", "Name": "Crépuscule", "A": "link", "B": "midna", "Resonance": 60, "MPA": 0, "MPB": 0,
         "Kind": "Magical", "Target": "AllEnemies", "Power": 1.3, "Hits": 1, "Element": "Shadow", "Apply": [], "HealPower": 0.0,
         "Breach": 0, "Desc": "Tous P1,3 ombre ; retarde les petites cibles de 100.", "RequireForm": "Wolf", "TargetGauge": -100},
    ]


def build_characters():
    def C(id, name, role, mult, abilities, equip, color, recruit, portrait, **kw):
        base = {"HP": 1.0, "MP": 1.0, "STR": 1.0, "MAG": 1.0, "VIT": 1.0, "SPR": 1.0, "AGI": 1.0, "LCK": 1.0, "DEF": 1.0}
        base.update(mult)
        d = {"Id": id, "Name": name, "Role": role, "Mult": base, "Abilities": abilities, "Equipment": equip, "Color": color,
             "Recruit": recruit, "Portrait": portrait, "Identity": id, "Weapon": "Blade", "Stances": [], "Lore": ""}
        d.update(kw)
        return d
    return [
        C("link", "Link", "Polyvalent", {}, ["L01", "R01", "G01", "S01"],
          {"Weapon": "OOT_001", "Offhand": "OOT_005", "Outfit": "OOT_008"}, "#3F8F3A", "Départ", "T_Portrait_Link",
          Lore="Le héros choisi par l'épée. Porteur de la Triforce du Courage à travers les âges."),
        C("sheik", "Sheik", "Furtive / soutien", {"HP": 0.9, "STR": 0.9, "MAG": 1.1, "AGI": 1.2},
          ["SHEIK_NEEDLE", "SHEIK_VEIL", "SHEIK_REVEAL", "C01"], {}, "#3A4A7A", "C1 : Temple du Temps", "T_Portrait_Sheik",
          Weapon="Needle", Stances=["Sheik", "Zelda"],
          Lore="Identité adoptée par Zelda dans Ocarina of Time. Posture Zelda débloquée en C5 (Q_SHEIK_03)."),
        C("impa", "Impa", "Gardienne", {"HP": 1.05, "STR": 1.05, "DEF": 1.1, "AGI": 1.05},
          ["IMPA_BLADE", "G03", "IMPA_MARK"], {}, "#7A2E2E", "C2 : défense de Cocorico", "T_Portrait_Impa", Weapon="Blade",
          Lore="Sheikah, protectrice de la famille royale."),
        C("midna", "Midna", "Magie d'ombre", {"HP": 0.9, "MAG": 1.25, "SPR": 1.1},
          ["MIDNA_BOLT", "MIDNA_DISPEL"], {}, "#2F3B3F", "C5 : miroir réparé", "T_Portrait_Midna", Weapon="Magic",
          Lore="Princesse du Crépuscule ; ouvre le duo Crépuscule avec Link loup."),
        C("mipha", "Mipha", "Soigneuse", {"HP": 0.95, "MAG": 1.2, "SPR": 1.2},
          ["MIPHA_SPEAR", "MIPHA_HEAL", "MIPHA_WAVE", "MIPHA_REVIVE"], {"Weapon": "BOTW_093"}, "#C0303A", "C2 : citerne", "T_Portrait_Mipha", Weapon="Spear",
          Lore="Princesse zora et Prodige ; rencontrée comme écho conscient d'une mémoire."),
        C("daruk", "Daruk", "Protecteur", {"HP": 1.3, "STR": 1.1, "VIT": 1.3, "AGI": 0.8},
          ["DARUK_HAMMER", "G02", "G05", "DARUK_PERC"], {}, "#A0703A", "C2 : mine en péril", "T_Portrait_Daruk", Weapon="Heavy",
          Lore="Prodige goron, porteur du Bouclier de Daruk."),
        C("revali", "Revali", "Tireur", {"HP": 0.9, "STR": 1.1, "AGI": 1.2},
          ["REVALI_SHOT", "REVALI_VOLLEY", "REVALI_GALE", "ANALYZE"], {}, "#3E5AA8", "C3 : épreuve aérienne", "T_Portrait_Revali", Weapon="Bow",
          Lore="Prodige piaf ; maître de l'arc et des courants aériens."),
        C("urbosa", "Urbosa", "Foudre", {"HP": 1.1, "STR": 1.15, "MAG": 1.05},
          ["URBOSA_SCIM", "URBOSA_ARC", "R03", "URBOSA_FURY"], {}, "#D8A63A", "C3 : duel du désert", "T_Portrait_Urbosa", Weapon="Blade",
          Lore="Cheffe gerudo et Prodige ; commande la foudre."),
    ]


def enemy_xp(level):
    return (50 + 25 * level + 10 * level * level) // 12


def build_enemies():
    def E(id, name, family, level, hp, mp, atq, amag, df, dfm, agi, resist, abilities, ai, breach=100, elite=False, boss=False,
          xp=None, ap=3, rupees=None, color="#888888", mesh="Blob", scale=1.0, desc="", statusres=None, spectre=False):
        mult = 8 if boss else (3 if elite else 1)
        return {"Id": id, "Name": name, "Family": family, "Level": level, "HP": hp, "MP": mp, "ATQ": atq, "AMAG": amag,
                "DEF": df, "DFM": dfm, "AGI": agi, "LCK": 5, "Resist": resist, "StatusResist": statusres or {},
                "Abilities": abilities, "AI": ai, "BreachThreshold": breach, "Elite": elite, "Boss": boss,
                "XP": xp if xp is not None else enemy_xp(level) * mult, "AP": ap, "Rupees": rupees if rupees is not None else (12 + 3 * level) * mult,
                "Color": color, "Mesh": mesh, "Scale": scale, "Description": desc, "Spectre": spectre}
    return [
        E("octorok", "Octorok", "Octorok / Deku", 10, 180, 20, 45, 30, 20, 20, 12, {"Water": 0.5, "Thunder": -0.25},
          ["E_OCTO_ROCK", "E_OCTO_INK"], "octorok", color="#B5475A", mesh="Octorok", scale=0.9,
          desc="Couverts, projectiles. Réponse : bouclier, boomerang, vent."),
        E("chuchu_water", "Chuchu aqueux", "Chuchu", 10, 220, 20, 35, 45, 25, 25, 10, {"Water": 0.75, "Thunder": -0.5, "Fire": 0.25},
          ["E_CHUCHU_SLAM", "E_CHUCHU_SPLASH"], "chuchu", color="#3FA7D6", mesh="Chuchu", scale=0.85,
          desc="État élémentaire persistant : ses éclaboussures rendent Mouillé. Éviter la réaction nuisible ; Analyse."),
        E("sentinel_elec", "Sentinelle électrique", "Guardian / Construct", 11, 600, 40, 55, 65, 40, 40, 13,
          {"Thunder": 0.75, "Water": -0.25, "Earth": -0.5}, ["E_SENT_BOLT", "E_SENT_CHARGE", "E_SENT_STORM"], "sentinel",
          breach=200, elite=True, ap=8, color="#D9C24A", mesh="Construct", scale=1.2,
          desc="Élite : charge un orage pendant une activation. L'eau protège, mais Mouillé double la foudre."),
        E("nereide", "Gardien Néréide", "Boss", 12, 2800, 9999, 65, 60, 40, 40, 14,
          {"Water": 0.75, "Fire": 0.25, "Thunder": -0.25, "Earth": -0.25}, ["E_NER_JET", "E_NER_CRUE_PREP", "E_NER_CRUE",
          "E_NER_WETALL", "E_NER_ARC_PREP", "E_NER_ARC", "E_NER_WAVE", "E_NER_STRIKE", "E_NER_DELUGE_PREP", "E_NER_DELUGE"],
          "nereide", breach=300, boss=True, xp=400, ap=20, rupees=250, color="#A89F7E", mesh="Golem", scale=2.0,
          desc="Gardien original de la Citerne des mémoires. Phase A Conduits, B Surcharge, C Rupture.",
          statusres={"Freeze": 0.5, "Sleep": 1.0, "Shock": 0.5}),
        E("bokoblin", "Bokoblin", "Bokoblin / Moblin", 8, 160, 10, 42, 20, 18, 14, 11, {"Fire": -0.25},
          ["E_CHUCHU_SLAM"], "basic", color="#C0504D", mesh="Blob", desc="Frappe frontale."),
        E("keese", "Keese", "Keese / Peahat", 8, 90, 10, 36, 20, 12, 12, 16, {"Wind": -0.5, "Light": -0.25},
          ["E_CHUCHU_SLAM"], "basic", color="#4B3F72", mesh="Keese", scale=0.6, desc="Cible aérienne, faible PV."),
    ]


def build_encounters():
    return [
        {"Id": "ENC_VESTIBULE", "Name": "Vestibule — Octorok", "Enemies": ["octorok"], "Room": 1, "Boss": False,
         "Tutorial": "Garde : dégâts reçus ×0,5 jusqu'à ta prochaine activation."},
        {"Id": "ENC_BASSIN", "Name": "Bassin — deux Chuchus", "Enemies": ["chuchu_water", "chuchu_water"], "Room": 2, "Boss": False,
         "Tutorial": "Compare ta tenue actuelle et la tenue Zora : Eau -50 %."},
        {"Id": "ENC_ECLUSE", "Name": "Écluse — Sentinelle électrique", "Enemies": ["sentinel_elec"], "Room": 7, "Boss": False,
         "Tutorial": "Tout aquatique n'est pas une faiblesse unique : Mouillé rend la foudre dangereuse."},
        {"Id": "ENC_NEREIDE", "Name": "Gardien Néréide", "Enemies": ["nereide"], "Room": 10, "Boss": True,
         "Tutorial": "Grappin sur un conduit pendant « Crue » ; deux vannes annulent « Déluge »."},
    ]


def build_consumables():
    return [
        {"Id": "POTION_RED", "Name": "Potion rouge", "Desc": "Rend 35 % PV max à une cible.", "Target": "Ally", "HealHPPct": 0.35, "HealMPPct": 0.0, "Revive": 0.0, "Cleanse": [], "Price": 40, "Max": 10},
        {"Id": "POTION_GREEN", "Name": "Potion verte", "Desc": "Rend 25 % PM max à une cible.", "Target": "Ally", "HealHPPct": 0.0, "HealMPPct": 0.25, "Revive": 0.0, "Cleanse": [], "Price": 65, "Max": 10},
        {"Id": "POTION_BLUE", "Name": "Potion bleue", "Desc": "Rend 50 % PV et 30 % PM.", "Target": "Ally", "HealHPPct": 0.5, "HealMPPct": 0.3, "Revive": 0.0, "Cleanse": [], "Price": 150, "Max": 5},
        {"Id": "FAIRY", "Name": "Fée en flacon", "Desc": "Réanime à 25 % PV sur commande.", "Target": "DeadAlly", "HealHPPct": 0.0, "HealMPPct": 0.0, "Revive": 0.25, "Cleanse": [], "Price": 0, "Max": 3},
        {"Id": "REMEDY", "Name": "Remède", "Desc": "Retire Poison, Brûlure, Silence ou Cécité.", "Target": "Ally", "HealHPPct": 0.0, "HealMPPct": 0.0, "Revive": 0.0, "Cleanse": ["Poison", "Burn", "Silence", "Blind"], "Price": 35, "Max": 10},
        {"Id": "MILK", "Name": "Lait de Lon Lon", "Desc": "Rend 15 % PV au groupe.", "Target": "AllAllies", "HealHPPct": 0.15, "HealMPPct": 0.0, "Revive": 0.0, "Cleanse": [], "Price": 70, "Max": 5},
        {"Id": "TOWEL", "Name": "Serviette sèche", "Desc": "Retire Mouillé d'une cible.", "Target": "Ally", "HealHPPct": 0.0, "HealMPPct": 0.0, "Revive": 0.0, "Cleanse": ["Wet"], "Price": 20, "Max": 10},
    ]


def build_passives():
    return [
        {"Id": "P_POISON_RES", "Name": "Résistance poison", "Cost": 4, "AP": 80, "Mods": [{"Stat": "STATUSRES_Poison", "Value": 0.5}], "Desc": "Poison : chance d'application ×0,5."},
        {"Id": "P_COUNTER", "Name": "Contre-attaque légère", "Cost": 4, "AP": 150, "Mods": [{"Stat": "COUNTER", "Value": 0.5}], "Desc": "Après un coup physique subi : riposte P0,5 (sans Résonance)."},
        {"Id": "P_MP_UP", "Name": "PM +10 %", "Cost": 4, "AP": 80, "Mods": [{"Stat": "MP_PCT", "Value": 0.10}], "Desc": "PM max +10 %."},
        {"Id": "P_GUARD_RECOVER", "Name": "Récupération après Garde", "Cost": 2, "AP": 30, "Mods": [{"Stat": "GUARD_REGEN", "Value": 0.05}], "Desc": "Garde rend 5 % PV max."},
        {"Id": "PASSIVE_WATER_GUARD", "Name": "Garde aquatique", "Cost": 2, "AP": 80, "Mods": [{"Stat": "RES_Water", "Value": 0.10}], "Desc": "Résistance eau +10 % (appris avec la tenue Zora)."},
        {"Id": "P_HP_UP", "Name": "PV +10 %", "Cost": 6, "AP": 150, "Mods": [{"Stat": "HP_PCT", "Value": 0.10}], "Desc": "PV max +10 %."},
    ]


def build_quests():
    return [
        {"Id": "Q_WATER_01", "Name": "La rivière muette", "Chapter": 2, "Steps": ["Analyser une vanne", "Sauver le mécanicien", "Ouvrir trois conduits", "Parler à Mipha"], "Reward": "Tenue Zora de départ ; apprentissage résistance eau."},
        {"Id": "Q_FORGE_02", "Name": "Le serment de Biggoron", "Chapter": 6, "Steps": ["Obtenir le plan", "Ramener trois minerais uniques", "Réussir le duel à armes lourdes"], "Reward": "Épée de Biggoron ; apparence ancienne et moderne."},
        {"Id": "Q_MOON_24", "Name": "Les visages retrouvés", "Chapter": 8, "Steps": ["Terminer 23 quêtes de masques", "Résoudre quatre salles lunaires", "Refuser de sacrifier un écho"], "Reward": "Masque du Dieu démon et forme Oni."},
        {"Id": "Q_SHEIK_03", "Name": "Une seule mémoire", "Chapter": 5, "Steps": ["Protéger les archives", "Reconnaître les trois mélodies", "Choix de dialogue sans verrou de puissance"], "Reward": "Posture Zelda ; duo Ombre et lame."},
        {"Id": "Q_MAJOR_01", "Name": "Le reflet apaisé", "Chapter": 9, "Steps": ["Victoire au colisée", "Trois sceaux lunaires", "Duel contre sa propre forme"], "Reward": "Relique originale Résonance de Majora."},
        {"Id": "Q_MASTER_01", "Name": "Les trois sceaux", "Chapter": 5, "Steps": ["Preuve de Courage", "Preuve de Sagesse", "Preuve de Puissance", "Retour au piédestal"], "Reward": "Épée de légende R4 ; amélioration C8."},
        {"Id": "Q_RING_64", "Name": "Cabinet de Vasu", "Chapter": 3, "Steps": ["Obtenir des jetons dans les deux arcs Oracle", "Choisir un anneau manquant"], "Reward": "64 anneaux sans dépendance au hasard."},
        {"Id": "Q_TAILOR_38", "Name": "L'atelier des trois héros", "Chapter": 4, "Steps": ["Défis bombes, arc, vent, exploration, défense", "Crédits de couture"], "Reward": "38 tenues TFH ; aucun achat externe."},
    ]


def main():
    cat = json.load(open(CATALOG, encoding="utf-8"))
    os.makedirs(OUT, exist_ok=True)
    items = build_items(cat)
    # Prototype : objets supplémentaires propres au projet (P), marqués comme tels
    items.append({"Id": "P_DEMO_KNIGHT_SWORD", "Game": "P", "GameName": "Les Échos de la Triforce (création)", "Name": "Knight's Blade",
                  "NameFR": "Lame de chevalier (prêt de mémoire)", "Profile": "LAME", "Category": "Une main", "Slot": "Weapon", "Hands": 1,
                  "Kind": "Blade", "Rank": 2, "Power": 24, "Focus": 0, "Def": 0, "MDef": 0, "Element": "Neutral", "Mods": [], "SetBonus": [],
                  "SetFamily": "", "Teaches": "L02", "TeachAP": 80, "Unlock": "DEMO", "UnlockText": "Prêt de la démonstration isolée (chapitre 17).",
                  "GateChapter": 1, "Source": "P", "Effect": "Frappe P1 ; enseigne Attaque circulaire (80 PA).", "CanonNote": "Arme de prototype prêtée.",
                  "Quest": "", "Special": "", "EffectModeled": True, "CanonicalIdentity": "demo knight blade"})
    ids = [i["Id"] for i in items]
    assert len(ids) == len(set(ids)), "identifiants dupliqués"
    abilities = build_abilities()
    ab_ids = {a["Id"] for a in abilities}
    for p in build_passives():
        ab_ids.add(p["Id"])
    missing = sorted({i["Teaches"] for i in items if i["Teaches"] and i["Teaches"] not in ab_ids})
    assert not missing, "techniques inconnues : %s" % missing

    def dump(name, data):
        with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=1)
    dump("items.json", {"Items": items})
    dump("abilities.json", {"Abilities": abilities})
    dump("forms.json", {"Forms": build_forms()})
    dump("spirits.json", {"Spirits": build_spirits()})
    dump("duos.json", {"Duos": build_duos()})
    dump("characters.json", {"Characters": build_characters()})
    dump("enemies.json", {"Enemies": build_enemies()})
    dump("encounters.json", {"Encounters": build_encounters()})
    dump("consumables.json", {"Consumables": build_consumables()})
    dump("passives.json", {"Passives": build_passives()})
    dump("quests.json", {"Quests": build_quests()})
    dump("sources.json", {"Sources": [{"Id": s["id"], "Title": s["title"], "Url": s["url"], "Kind": s["kind"]} for s in cat["sources"]]})

    c = collections.Counter(i["Slot"] for i in items)
    modeled = sum(1 for i in items if i["EffectModeled"])
    print("Objets :", len(items), dict(c))
    print("Effets entièrement modélisés :", modeled, "/", len(items))
    print("Techniques :", len(abilities))


if __name__ == "__main__":
    main()
