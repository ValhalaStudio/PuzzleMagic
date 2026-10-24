# BONUS ICONS: builds M_UIBonusIcon, a UI material that draws the three bonus tile symbols of the plain-tile themes (Desk, Classic) the way
# the tiles show them, for the help pages: Emblem 0 = a plain glowing tile (basic), 1 = a white diamond round a black diamond (the outgoing
# tile), 2 = a white ring round a black disc, a fisheye (the incoming tile). Colour is the tile colour.
# The HUD (UPuzzleHUDWidget::MakeBonusIcon) loads it when a page is built, so it can be rebuilt in place.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>   (the editor must be closed)
import unreal

SCR = unreal.SystemLibrary.get_project_directory() + "Tools"
exec(open(SCR + "/arcane_head.py", encoding="utf-8").read())

ICON_HLSL = r"""
float2 p = (InUV - 0.5) * 2.2;
p.y = -p.y;
float aa = max(fwidth(p.x), 0.0001) * 1.2;
// the tile: a rounded square, lit from the top left like the tiles are
float2 q = abs(p) - 0.85 + 0.2;
float dTile = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - 0.2;
float inside = 1.0 - smoothstep(-aa, aa, dTile);
float3 body = lerp(InColor * 1.25, InColor * 0.7, InUV.y);
float rim = 1.0 - smoothstep(0.0, 0.16, -dTile);
body = lerp(body, InColor * 1.6, rim * 0.35);
float3 col = body;
if (InEmblem > 0.5)
{
    float dOutline;
    float dCore;
    if (InEmblem < 1.5)
    {
        dOutline = abs((abs(p.x) + abs(p.y)) * 0.7071 - 0.46) - 0.05;
        dCore = (abs(p.x) + abs(p.y)) * 0.7071 - 0.18;
    }
    else
    {
        dOutline = abs(length(p) - 0.52) - 0.06;
        dCore = length(p) - 0.24;
    }
    float ring = 1.0 - smoothstep(-aa, aa, dOutline);
    float core = 1.0 - smoothstep(-aa, aa, dCore);
    col = lerp(col, float3(0.95, 0.95, 0.98), ring);
    col = lerp(col, float3(0.01, 0.01, 0.015), core);
}
float ink = 1.0 - smoothstep(0.035 - aa, 0.035 + aa, abs(dTile));
col = lerp(col, float3(0.06, 0.03, 0.08), ink);
float glow = InGlow * saturate(1.0 - max(dTile, 0.0) * 5.0) * (1.0 - inside);
OutAlpha = max(max(inside, ink * 0.95), glow * 0.8);
return lerp(InColor * 1.5, col, max(inside, ink));
"""

icon = new_material("M_UIBonusIcon", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_TRANSLUCENT)
icon.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
node = custom_node(icon, ICON_HLSL, ["InUV", "InEmblem", "InColor", "InGlow"], F3, [("OutAlpha", F1)])
wire([(expr(icon, unreal.MaterialExpressionTextureCoordinate, y=-200), "InUV"),
      (scalar_param(icon, "Emblem", 0.0, y=-100), "InEmblem"),
      (vec_param(icon, "Color", unreal.LinearColor(1.0, 0.93, 0.55, 1.0), y=0), "InColor"),
      (scalar_param(icon, "Glow", 0.4, y=100), "InGlow")], node)
mel.connect_material_property(node, "", MP.MP_EMISSIVE_COLOR)
mel.connect_material_property(node, "OutAlpha", MP.MP_OPACITY)
finish(icon)
unreal.log("build_bonus_icons.py: done")
