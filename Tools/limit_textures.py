# Caps the generated/baked textures for 4 GB cards (RX 6400: the desktop already holds ~2 GB) and
# for phones: 512 px colour/normal tiles, 512x1024 pier bakes. The source PNGs stay full size.
import unreal

eal = unreal.EditorAssetLibrary
for name, size in [("T_PillarStone", 512), ("T_PillarStone_N", 512), ("T_FloorSlab", 512), ("T_FloorSlab_N", 512),
                   ("T_WallStone", 256), ("T_WallStone_N", 256), ("T_Pier_N", 1024), ("T_Pier_AO", 512)]:
    path = "/Game/Textures/" + name
    if not eal.does_asset_exist(path):
        continue
    tex = eal.load_asset(path)
    tex.set_editor_property("max_texture_size", size)
    eal.save_loaded_asset(tex)
    unreal.log("limit_textures: %s <= %d" % (name, size))
