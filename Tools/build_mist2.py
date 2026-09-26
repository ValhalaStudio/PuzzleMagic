# M_GroundMist2 (M_GroundMist is rooted by C++ and failed to compile: NOISE already defines Fbm).
import unreal

SCR = r"C:\Users\VagDi\AppData\Local\Temp\claude\C--Users-VagDi\57e5f065-d886-4909-abc7-41ade08db499\scratchpad"
exec(open(SCR + r"\arcane_head.py", encoding="utf-8").read())

MIST_HLSL = "struct Fns {" + NOISE + r"""};
Fns F;
float2 w = InWorldPos.xy;
float t = InTime;
float n1 = F.Fbm(w * 0.0016 + float2(t * 0.018, t * 0.007));
float n2 = F.Fbm(w * 0.0031 - float2(t * 0.011, -t * 0.015) + n1 * 1.7);
float density = saturate((n1 * 0.6 + n2 * 0.6 - 0.42) * 2.2);
// Keep the play area mostly clear: mist pools around the board and in the corners.
float2 fromBoard = abs(w - float2(0.0, 80.0)) - float2(430.0, 560.0);
float edge = saturate(length(max(fromBoard, 0.0)) / 260.0);
density *= lerp(0.18, 1.0, edge);
OutAlpha = density * InDensity;
return InColor * (0.55 + 0.45 * n2);
"""
mist = new_material("M_GroundMist2", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_TRANSLUCENT)
mist_node = custom_node(mist, MIST_HLSL, ["InWorldPos", "InTime", "InColor", "InDensity"], F3, [("OutAlpha", F1)])
wire([(expr(mist, unreal.MaterialExpressionWorldPosition, y=-200), "InWorldPos"),
      (expr(mist, unreal.MaterialExpressionTime, y=-100), "InTime"),
      (vec_param(mist, "Color", unreal.LinearColor(0.16, 0.17, 0.26, 1.0), y=0), "InColor"),
      (scalar_param(mist, "Density", 0.75, y=100), "InDensity")], mist_node)
fade = mel.create_material_expression(mist, unreal.MaterialExpressionDepthFade, -100, 200)
fade.set_editor_property("fade_distance_default", 90.0)
mult = mel.create_material_expression(mist, unreal.MaterialExpressionMultiply, 100, 150)
mel.connect_material_expressions(mist_node, "OutAlpha", mult, "A")
mel.connect_material_expressions(fade, "", mult, "B")
mel.connect_material_property(mist_node, "", MP.MP_EMISSIVE_COLOR)
mel.connect_material_property(mult, "", MP.MP_OPACITY)
finish(mist)
unreal.log("build_mist2.py: done")
