# Lovecraftian layer: M_Tentacle (wet writhing flesh, sway in World Position Offset),
# M_EldritchEye (glowing slit-pupil eyes that open and blink in the dark) and
# M_PPMadness (breathing screen warp, chromatic split, sickly grade, dark pulses).
# All are loaded at runtime with LoadObject (never ConstructorHelpers), so rebuilding them
# here can't hit the "rooted asset" commandlet crash.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>
import unreal

exec(open(r"D:\Unreal Projects\PuzzleGame5x5\Tools\arcane_head.py", encoding="utf-8").read())

# ---------------------------------------------------------------- M_Tentacle
SWAY_HLSL = r"""
float h = saturate((InWorldPos.z - InObjPos.z) / max(InHeight, 1.0));
float ph = InSeed * 6.2831853;
float t = InTime;
float w = h * h * InHeight * InWrithe;
float3 o;
o.x = (sin(h * 4.0 - t * 0.9 + ph) * 0.2 + sin(h * 9.0 - t * 1.9 + ph * 1.3) * 0.05) * w;
o.y = (cos(h * 3.1 - t * 0.7 + ph * 0.7) * 0.14 + sin(h * 11.0 - t * 2.3 + ph) * 0.03) * w;
// The tip curls: it drops a little as it swings out.
o.z = -abs(o.x) * 0.25 * h;
return o;
"""

SKIN_HLSL = "struct Fns {" + NOISE + r"""};
Fns F;
float u = InUV.x;
float v = InUV.y;
float bands = F.Fbm(float2(u * 5.0 + v * 2.0, v * 16.0));
float3 skin = lerp(float3(0.012, 0.022, 0.024), float3(0.05, 0.075, 0.07), bands);
skin = lerp(skin, float3(0.07, 0.03, 0.06), smoothstep(0.55, 0.9, v) * 0.6);   // bruised tip
// Rows of suckers along the underside (u = 0.5 faces the board).
float2 g = frac(float2(u * 12.0, v * 26.0)) - 0.5;
float under = smoothstep(0.2, 0.06, abs(u - 0.5));
float sucker = smoothstep(0.34, 0.22, length(g)) * under * (1.0 - smoothstep(0.85, 1.0, v));
float ring = smoothstep(0.34, 0.3, length(g)) - smoothstep(0.26, 0.22, length(g));
skin = lerp(skin, float3(0.22, 0.12, 0.14), sucker * 0.75);
// Bioluminescent spots that pulse up the tentacle.
float spots = smoothstep(0.74, 0.8, F.Noise(float2(u * 20.0, v * 44.0)));
float pulse = 0.35 + 0.65 * pow(0.5 + 0.5 * sin(InTime * 2.2 - v * 9.0), 3.0);
OutRoughness = lerp(0.14, 0.45, sucker) + bands * 0.1;
OutEmissive = InGlowColor * (spots * pulse + ring * under * 0.25) * InGlow;
return skin;
"""

tent = new_material("M_Tentacle")
tent.set_editor_property("two_sided", False)
sway = custom_node(tent, SWAY_HLSL, ["InWorldPos", "InObjPos", "InTime", "InHeight", "InWrithe", "InSeed"], F3, [], y=400)
wire([(expr(tent, unreal.MaterialExpressionWorldPosition, y=300), "InWorldPos"),
      (expr(tent, unreal.MaterialExpressionObjectPositionWS, y=360), "InObjPos"),
      (expr(tent, unreal.MaterialExpressionTime, y=420), "InTime"),
      (scalar_param(tent, "Height", 500.0, y=480), "InHeight"),
      (scalar_param(tent, "Writhe", 1.0, y=540), "InWrithe"),
      (scalar_param(tent, "Seed", 0.0, y=600), "InSeed")], sway)
skin = custom_node(tent, SKIN_HLSL, ["InUV", "InTime", "InGlowColor", "InGlow"], F3, [("OutRoughness", F1), ("OutEmissive", F3)])
wire([(expr(tent, unreal.MaterialExpressionTextureCoordinate, y=-200), "InUV"),
      (expr(tent, unreal.MaterialExpressionTime, y=-100), "InTime"),
      (vec_param(tent, "GlowColor", unreal.LinearColor(0.1, 1.0, 0.7, 1.0), y=0), "InGlowColor"),
      (scalar_param(tent, "Glow", 2.0, y=100), "InGlow")], skin)
mel.connect_material_property(skin, "", MP.MP_BASE_COLOR)
mel.connect_material_property(skin, "OutRoughness", MP.MP_ROUGHNESS)
mel.connect_material_property(skin, "OutEmissive", MP.MP_EMISSIVE_COLOR)
mel.connect_material_property(sway, "", MP.MP_WORLD_POSITION_OFFSET)
finish(tent)

# ---------------------------------------------------------------- M_EldritchEye
EYE_HLSL = r"""
float2 p = (InUV - 0.5) * 2.0;
float lid = (1.0 - p.x * p.x) * 0.62 * InOpen;
float inside = saturate((lid - abs(p.y)) * 40.0) * step(abs(p.x), 1.0);
float2 q = p - float2(InLook, 0.0) * 0.35;
float r = length(q * float2(1.0, 0.85));
float iris = saturate((0.46 - r) * 30.0);
float slit = saturate((0.07 - abs(q.x) * (1.0 + 2.0 * abs(q.y))) * 40.0) * saturate((0.44 - abs(q.y)) * 30.0);
float fibres = 0.75 + 0.25 * sin(atan2(q.y, q.x) * 24.0);
float3 irisCol = InColor * fibres * (0.5 + 1.2 * saturate(1.0 - r / 0.46)) * (1.0 - slit);
float sclera = inside * (1.0 - iris) * 0.08;
float lidLine = saturate(1.0 - abs(abs(p.y) - lid) * 25.0) * step(abs(p.x), 1.0) * InOpen * 0.25;
float halo = exp(-length(p * float2(0.7, 1.4)) * 2.5) * 0.35 * InOpen;
return (irisCol * iris * inside + InColor * (sclera + lidLine + halo)) * InIntensity;
"""
eye = new_material("M_EldritchEye", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_ADDITIVE)
eye.set_editor_property("two_sided", True)
eye_node = custom_node(eye, EYE_HLSL, ["InUV", "InOpen", "InLook", "InColor", "InIntensity"], F3, [])
wire([(expr(eye, unreal.MaterialExpressionTextureCoordinate, y=-150), "InUV"),
      (scalar_param(eye, "Open", 0.0, y=-50), "InOpen"),
      (scalar_param(eye, "Look", 0.0, y=50), "InLook"),
      (vec_param(eye, "Color", unreal.LinearColor(0.35, 1.0, 0.45, 1.0), y=150), "InColor"),
      (scalar_param(eye, "Intensity", 4.0, y=250), "InIntensity")], eye_node)
mel.connect_material_property(eye_node, "", MP.MP_EMISSIVE_COLOR)
finish(eye)

# ---------------------------------------------------------------- M_PPMadness
MAD_HLSL = r"""
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float m = InMadness;
float t = InTime;
float2 c = uv - 0.5;
float r = length(c);
// The frame breathes: edges swell and shrink, and ripple a little, more the madder it gets.
float breath = sin(t * 0.9);
float2 warp = c * r * r * m * 0.07 * breath;
warp += float2(sin(uv.y * 23.0 + t * 1.7), cos(uv.x * 19.0 - t * 1.3)) * 0.0022 * m * smoothstep(0.15, 0.7, r);
float2 suv = uv + warp;
float ca = 0.006 * m * r;
float3 col;
col.r = SceneTextureLookup(suv + c * ca, 14, false).r;
col.g = SceneTextureLookup(suv, 14, false).g;
col.b = SceneTextureLookup(suv - c * ca, 14, false).b;
// Sickly grade: drain the colour toward a drowned green.
float lum = dot(col, float3(0.3, 0.59, 0.11));
col = lerp(col, lum * float3(0.8, 1.05, 0.95), m * 0.4);
// Darkness closes in from the edges in slow pulses.
float vig = smoothstep(0.3, 0.8, r) * m * (0.5 + 0.3 * sin(t * 0.9 + 1.0));
col *= 1.0 - vig;
return col * InExposure + InScene.rgb * 0.0;
"""
mad = new_material("M_PPMadness", unreal.MaterialShadingModel.MSM_UNLIT)
mad.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
location = getattr(unreal.BlendableLocation, "BL_SCENE_COLOR_AFTER_TONEMAPPING", None) or unreal.BlendableLocation.BL_AFTER_TONEMAPPING
mad.set_editor_property("blendable_location", location)
mad_node = custom_node(mad, MAD_HLSL, ["InMadness", "InTime", "InExposure", "InScene"], F3, [])
# Wired in (though the HLSL samples it itself): a connected SceneTexture node is what declares
# SceneTextureLookup for the custom code; an unconnected one is compiled out.
pp_input = expr(mad, unreal.MaterialExpressionSceneTexture, y=-300)
pp_input.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
mel.connect_material_expressions(pp_input, "Color", mad_node, "InScene")
wire([(scalar_param(mad, "Madness", 0.0, y=-100), "InMadness"),
      (expr(mad, unreal.MaterialExpressionTime, y=0), "InTime"),
      (scalar_param(mad, "Exposure", 1.0, y=100), "InExposure")], mad_node)
mel.connect_material_property(mad_node, "", MP.MP_EMISSIVE_COLOR)
finish(mad)
unreal.log("build_eldritch_fx.py: done")
