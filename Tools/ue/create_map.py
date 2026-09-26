# Crée /Game/Maps/L_Cistern avec l'environnement, la citerne et un PlayerStart.
# Lancer : UnrealEditor-Cmd EchosTriforce.uproject -run=pythonscript -script=Tools/ue/create_map.py
import unreal

MAP = "/Game/Maps/L_Cistern"
ell = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary

if eal.does_asset_exist(MAP):
    ell.load_level(MAP)
else:
    ok = ell.new_level(MAP)
    unreal.log("new_level: %s" % ok)

env_cls = unreal.load_class(None, "/Script/EchosTriforce.ZEnvironment")
cis_cls = unreal.load_class(None, "/Script/EchosTriforce.ZCistern")
for a in ell.get_all_level_actors():
    n = a.get_class().get_name()
    if n in ("ZEnvironment", "ZCistern", "PlayerStart"):
        ell.destroy_actor(a)
ell.spawn_actor_from_class(env_cls, unreal.Vector(0, 0, 0))
ell.spawn_actor_from_class(cis_cls, unreal.Vector(0, 0, 0))
ell.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-500, 0, 120))
ok = ell.save_current_level()
unreal.log("ECHOS: map saved %s" % ok)
