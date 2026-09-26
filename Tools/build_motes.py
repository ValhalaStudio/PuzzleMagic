# M_Mote: additive glowing mote for instanced particles (dust in candlelight, embers, eldritch
# wisps). Colour and brightness come per instance from PerInstanceCustomData 0-3 (R, G, B, I),
# set by AGothicEnvironment, so hundreds of motes are one draw call.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>
import unreal

exec(open(r"D:\Unreal Projects\PuzzleGame5x5\Tools\arcane_head.py", encoding="utf-8").read())

MOTE_HLSL = r"""
float2 p = (InUV - 0.5) * 2.0;
float r = length(p);
float core = exp(-r * r * 18.0);
float halo = exp(-r * 3.5) * 0.35;
float fade = saturate(1.0 - r);
return float3(InR, InG, InB) * (core + halo) * fade * InI;
"""

mote = new_material("M_Mote", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_ADDITIVE)
mote.set_editor_property("two_sided", True)
mote.set_editor_property("used_with_instanced_static_meshes", True)
node = custom_node(mote, MOTE_HLSL, ["InUV", "InR", "InG", "InB", "InI"], F3, [])
mel.connect_material_expressions(expr(mote, unreal.MaterialExpressionTextureCoordinate, y=-200), "", node, "InUV")
for index, pin in enumerate(["InR", "InG", "InB", "InI"]):
    data = expr(mote, unreal.MaterialExpressionPerInstanceCustomData, y=-100 + 100 * index)
    data.set_editor_property("data_index", index)
    data.set_editor_property("const_default_value", 1.0)
    mel.connect_material_expressions(data, "", node, pin)
mel.connect_material_property(node, "", MP.MP_EMISSIVE_COLOR)
finish(mote)
unreal.log("build_motes.py: done")
