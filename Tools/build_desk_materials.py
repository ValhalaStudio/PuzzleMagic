# DESK THEME: builds M_DeskWood (the desk top: planks with grain, drawn from the world position, so any plane size works) and
# M_DeskProp (a plain lit material with Color, Rough and Metal parameters, for the mug, notebook, pencil and the wall).
# C++ (ADeskEnvironment) loads them when the map starts, so they can be rebuilt in place.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>   (the editor must be closed)
import unreal

SCR = unreal.SystemLibrary.get_project_directory() + "Tools"
exec(open(SCR + "/arcane_head.py", encoding="utf-8").read())

WOOD_HLSL = "struct Fns {" + NOISE + r"""
    // Planks run along X. Each plank has its own tint and grain, and its own end joints.
    float3 Wood(float3 wp, float3 dark, float3 light, float plank)
    {
        float row = floor(wp.y / plank);
        float across = frac(wp.y / plank);
        float rowHash = Hash(float2(row, 3.7));
        float jointPitch = plank * 5.0;
        float along = wp.x + rowHash * jointPitch;
        float seg = floor(along / jointPitch);
        float segHash = Hash(float2(row, seg + 11.0));
        // Grain: long streaks along the plank, wobbling a little, and a few darker rings.
        float warp = Fbm(float2(along * 0.0045 + segHash * 40.0, wp.y * 0.02));
        float streak = Noise(float2(along * 0.0035 + segHash * 30.0, wp.y * 0.55));
        float rings = 0.5 + 0.5 * sin((wp.y * 0.11 + warp * 7.0 + segHash * 20.0));
        float g = saturate(streak * 0.55 + rings * 0.3 + warp * 0.35);
        float3 col = lerp(dark, light, g);
        col *= 0.86 + 0.28 * segHash;
        // The gaps between planks and between the ends of two planks.
        float gapSide = smoothstep(0.0, 0.007, min(across, 1.0 - across));
        float endDist = abs(frac(along / jointPitch + 0.5) - 0.5) * jointPitch;
        float gapEnd = smoothstep(0.0, 1.8, endDist);
        col *= lerp(0.28, 1.0, gapSide * gapEnd);
        return col;
    }
};
Fns f;
return f.Wood(InPos, InDark.rgb, InLight.rgb, InPlank);
"""

wood = new_material("M_DeskWood")
node = custom_node(wood, WOOD_HLSL, ["InPos", "InDark", "InLight", "InPlank"], F3, [])
wire([(expr(wood, unreal.MaterialExpressionWorldPosition, y=-300), "InPos"),
      (vec_param(wood, "Dark", unreal.LinearColor(0.11, 0.06, 0.028, 1.0), y=-100), "InDark"),
      (vec_param(wood, "Light", unreal.LinearColor(0.30, 0.18, 0.085, 1.0), y=100), "InLight"),
      (scalar_param(wood, "Plank", 240.0, y=300), "InPlank")], node)
mel.connect_material_property(node, "", MP.MP_BASE_COLOR)
mel.connect_material_property(const(wood, 0.52, y=500), "", MP.MP_ROUGHNESS)
finish(wood)

prop = new_material("M_DeskProp")
mel.connect_material_property(vec_param(prop, "Color", unreal.LinearColor(0.5, 0.5, 0.5, 1.0), y=-200), "", MP.MP_BASE_COLOR)
mel.connect_material_property(scalar_param(prop, "Rough", 0.6, y=0), "", MP.MP_ROUGHNESS)
mel.connect_material_property(scalar_param(prop, "Metal", 0.0, y=200), "", MP.MP_METALLIC)
finish(prop)
unreal.log("build_desk_materials.py: done")
