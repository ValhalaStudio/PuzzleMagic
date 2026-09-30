# Halloween scenery materials:
#   M_CauldronLiquid : opaque unlit lime-green liquid that slowly churns (emissive).
#   M_SteamPuff      : soft translucent puff of steam, lime-tinted; params Opacity, Seed, Tint.
#   M_BatSilhouette  : translucent unlit black bat (the BatSym shape of the old sigils); params Color, Opacity.
# C++ (AHalloweenProps) loads them when the level starts, not from a constructor, so they can be rebuilt in place.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>   (the editor must be closed)
import unreal

SCR = unreal.SystemLibrary.get_project_directory() + "Tools"
exec(open(SCR + "/arcane_head.py", encoding="utf-8").read())

_gothic = open(SCR + "/build_gothic_symbols.py", encoding="utf-8").read()
_s = _gothic.index('SYMBOLS = r"""')
_e = _gothic.index('"""', _s + len('SYMBOLS = r"""')) + 3
exec(_gothic[_s:_e])

FBM = "struct Fns {" + NOISE + "};\nFns F;\n"

# ---------------------------------------------------------------------------
# Lime liquid
# ---------------------------------------------------------------------------
LIQUID_HLSL = FBM + r"""
float2 w = InWorldPos.xy * 0.03;
float n1 = F.Fbm(w * 2.0 + float2(InTime * 0.25, InTime * 0.15));
float n2 = F.Fbm(w * 4.0 - float2(InTime * 0.3, 0.0) + n1 * 2.0);
float v = saturate(n1 * 0.6 + n2 * 0.6);
float3 c = lerp(float3(0.10, 0.45, 0.0), float3(0.75, 1.0, 0.12), saturate(v * v * 1.8));
return c * 2.4;
"""
liquid = new_material("M_CauldronLiquid", unreal.MaterialShadingModel.MSM_UNLIT)
liquid_node = custom_node(liquid, LIQUID_HLSL, ["InWorldPos", "InTime"], F3, [])
wire([(expr(liquid, unreal.MaterialExpressionWorldPosition, y=-100), "InWorldPos"),
      (expr(liquid, unreal.MaterialExpressionTime, y=0), "InTime")], liquid_node)
mel.connect_material_property(liquid_node, "", MP.MP_EMISSIVE_COLOR)
finish(liquid)

# ---------------------------------------------------------------------------
# Steam puff
# ---------------------------------------------------------------------------
STEAM_HLSL = FBM + r"""
float2 uv = InUV - 0.5;
float r = length(uv) * 2.0;
float n = F.Fbm(uv * 3.0 + InSeed * 7.0 + InTime * 0.3);
float a = saturate(1.0 - r);
a = a * a * (0.55 + 0.9 * n);
OutAlpha = saturate(a) * InOpacity;
return lerp(float3(0.85, 0.95, 0.8), InTint.rgb, 0.6);
"""
steam = new_material("M_SteamPuff", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_TRANSLUCENT)
steam_node = custom_node(steam, STEAM_HLSL, ["InUV", "InSeed", "InTime", "InOpacity", "InTint"], F3, [("OutAlpha", F1)])
wire([(expr(steam, unreal.MaterialExpressionTextureCoordinate, y=-300), "InUV"),
      (scalar_param(steam, "Seed", 0.0, y=-200), "InSeed"),
      (expr(steam, unreal.MaterialExpressionTime, y=-100), "InTime"),
      (scalar_param(steam, "Opacity", 0.5, y=0), "InOpacity"),
      (vec_param(steam, "Tint", unreal.LinearColor(0.55, 1.0, 0.25, 1.0), y=100), "InTint")], steam_node)
mel.connect_material_property(steam_node, "", MP.MP_EMISSIVE_COLOR)
mel.connect_material_property(steam_node, "OutAlpha", MP.MP_OPACITY)
finish(steam)

# ---------------------------------------------------------------------------
# Bat silhouette
# ---------------------------------------------------------------------------
BAT_HLSL = "struct Fns {" + NOISE + SYMBOLS + "};\nFns F;\n" + r"""
float2 p = float2(InUV.x - 0.5, 0.5 - InUV.y) * 2.0;
float detail;
float d = F.BatSym(p * 0.85, detail) / 0.85;
float aa = max(fwidth(d), 0.0001);
OutAlpha = (1.0 - smoothstep(-aa, aa, d)) * InOpacity;
return InColor;
"""
bat = new_material("M_BatSilhouette", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_TRANSLUCENT)
bat_node = custom_node(bat, BAT_HLSL, ["InUV", "InColor", "InOpacity"], F3, [("OutAlpha", F1)])
wire([(expr(bat, unreal.MaterialExpressionTextureCoordinate, y=-200), "InUV"),
      (vec_param(bat, "Color", unreal.LinearColor(0.015, 0.008, 0.025, 1.0), y=-100), "InColor"),
      (scalar_param(bat, "Opacity", 0.95, y=0), "InOpacity")], bat_node)
mel.connect_material_property(bat_node, "", MP.MP_EMISSIVE_COLOR)
mel.connect_material_property(bat_node, "OutAlpha", MP.MP_OPACITY)
finish(bat)

unreal.log("build_halloween_fx.py: done")
