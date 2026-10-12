# Imports the pictures of the lightweight (mobile) mode and builds its two cheap unlit materials:
#   /Game/Lite/T_LiteBackdrop  (Tools/LiteArt/T_LiteBackdrop.png, rendered by the game with -backdropcapture)
#   /Game/Lite/T_LiteBat       (Tools/make_lite_sprites.py)
#   /Game/Lite/T_LiteSteam     (Tools/make_lite_sprites.py)
#   M_LiteBackdrop : opaque unlit picture
#   M_LiteSprite   : translucent unlit sprite sheet; params Tex, Cols, Rows, Frame, Tint, Opacity
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>   (the editor must be closed)
import os

import unreal

SCR = unreal.SystemLibrary.get_project_directory() + "Tools"
exec(open(SCR + "/arcane_head.py", encoding="utf-8").read())

ART = SCR + "/LiteArt"
TEX_FOLDER = "/Game/Lite"

# ---------------------------------------------------------------------------
# Textures
# ---------------------------------------------------------------------------
def import_texture(name, filename, is_backdrop):
    path = f"{ART}/{filename}"
    if not os.path.exists(path):
        unreal.log_warning(f"build_lite_assets.py: {path} is missing, skipped")
        return
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", TEX_FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])
    tex = eal.load_asset(f"{TEX_FOLDER}/{name}")
    if tex:
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property("never_stream", True)
        if is_backdrop:
            # One screen-sized picture: no mip chain, shown 1:1.
            tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        eal.save_loaded_asset(tex)
        unreal.log(f"build_lite_assets.py: imported {name}")


import_texture("T_LiteBackdrop", "T_LiteBackdrop.png", True)
import_texture("T_LiteBat", "T_LiteBat.png", False)
import_texture("T_LiteSteam", "T_LiteSteam.png", False)

# ---------------------------------------------------------------------------
# M_LiteBackdrop: the picture, unlit.
# ---------------------------------------------------------------------------
backdrop = new_material("M_LiteBackdrop", unreal.MaterialShadingModel.MSM_UNLIT)
backdrop.set_editor_property("two_sided", True)
tex = expr(backdrop, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
tex.set_editor_property("parameter_name", "Tex")
mel.connect_material_property(tex, "RGB", MP.MP_EMISSIVE_COLOR)
finish(backdrop)

# ---------------------------------------------------------------------------
# M_LiteSprite: one frame of a sprite sheet, tinted, with an opacity.
# ---------------------------------------------------------------------------
FRAME_UV = """
float f = floor(Frame + 0.0001);
float col = fmod(f, Cols);
float row = fmod(floor(f / Cols), Rows);
return float2((col + InUV.x) / Cols, (row + InUV.y) / Rows);
"""
sprite = new_material("M_LiteSprite", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_TRANSLUCENT)
sprite.set_editor_property("two_sided", True)
uv = expr(sprite, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
frame_node = custom_node(sprite, FRAME_UV, ["InUV", "Frame", "Cols", "Rows"], unreal.CustomMaterialOutputType.CMOT_FLOAT2, [], x=-700, y=0)
wire([(uv, "InUV"),
      (scalar_param(sprite, "Frame", 0.0, x=-1100, y=120), "Frame"),
      (scalar_param(sprite, "Cols", 1.0, x=-1100, y=200), "Cols"),
      (scalar_param(sprite, "Rows", 1.0, x=-1100, y=280), "Rows")], frame_node)
sheet = expr(sprite, unreal.MaterialExpressionTextureSampleParameter2D, -450, 0)
sheet.set_editor_property("parameter_name", "Tex")
mel.connect_material_expressions(frame_node, "", sheet, "UVs")

tint = vec_param(sprite, "Tint", unreal.LinearColor(1.0, 1.0, 1.0, 1.0), x=-450, y=300)
colour = expr(sprite, unreal.MaterialExpressionMultiply, -150, 0)
mel.connect_material_expressions(sheet, "RGB", colour, "A")
mel.connect_material_expressions(tint, "", colour, "B")
mel.connect_material_property(colour, "", MP.MP_EMISSIVE_COLOR)

opacity = scalar_param(sprite, "Opacity", 1.0, x=-450, y=420)
alpha = expr(sprite, unreal.MaterialExpressionMultiply, -150, 220)
mel.connect_material_expressions(sheet, "A", alpha, "A")
mel.connect_material_expressions(opacity, "", alpha, "B")
mel.connect_material_property(alpha, "", MP.MP_OPACITY)
finish(sprite)

unreal.log("build_lite_assets.py: done")
