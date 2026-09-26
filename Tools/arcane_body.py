
# Fonts are loaded from raw .ttf files at runtime (importing them needs Slate, absent in commandlets).

EXTRA = r"""
    float RoundBox(float2 p, float2 b, float r)
    {
        float2 q = abs(p) - b + r;
        return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
    }
    // Horned gargoyle face (relief) with eye sockets reported separately.
    float Skull(float2 p, out float eyes)
    {
        float head = length(p - float2(0.0, 0.1)) - 0.48;
        float jaw = RoundBox(p - float2(0.0, -0.36), float2(0.28, 0.16), 0.08);
        float hornL = length(p - float2(-0.43, 0.5)) - 0.13;
        float hornR = length(p - float2(0.43, 0.5)) - 0.13;
        float d = min(min(head, jaw), min(hornL, hornR));
        float sockets = min(length(p - float2(-0.19, 0.1)) - 0.13, length(p - float2(0.19, 0.1)) - 0.13);
        eyes = 1.0 - smoothstep(-0.03, 0.01, sockets);
        float nose = max(abs(p.x) * 1.7 + (p.y + 0.12) * 0.9 - 0.1, -(p.y + 0.2));
        float teeth = abs(p.y + 0.36) - 0.025;
        d = max(d, -sockets);
        d = max(d, -nose);
        d = max(d, -max(teeth, abs(p.x) - 0.22));
        return d;
    }
    float Cracks(float2 p)
    {
        float2 i = floor(p);
        float2 f = frac(p);
        float best = 1e5;
        float second = 1e5;
        for (int y = -1; y <= 1; y++)
        {
            for (int x = -1; x <= 1; x++)
            {
                float2 o = float2(x, y) + Hash2(i + float2(x, y)) - f;
                float d = length(o);
                if (d < best) { second = best; best = d; }
                else if (d < second) { second = d; }
            }
        }
        return 1.0 - smoothstep(0.02, 0.07, second - best);
    }
    float TriangleIso(float2 p, float2 q)
    {
        p.x = abs(p.x);
        float2 a = p - q * clamp(dot(p, q) / dot(q, q), 0.0, 1.0);
        float2 b = p - q * float2(clamp(p.x / q.x, 0.0, 1.0), 1.0);
        float s = -sign(q.y);
        float2 d = min(float2(dot(a, a), s * (p.x * q.y - p.y * q.x)), float2(dot(b, b), s * (p.y - q.y)));
        return -sqrt(d.x) * sign(d.y);
    }
"""

# ===========================================================================
# M_TileArcane: glossy candy-enamel tile (clear coat), coloured fresnel inner
# glow, gold symbol inlays, shimmer sweep for blessed tiles, and cursed
# gargoyle stone (Stone 2 = intact, 1 = cracked) with glowing eyes.
# Clear coat goes through MakeMaterialAttributes (its pins aren't exposed otherwise).
# ===========================================================================
ARCANE_HLSL = "struct Fns {" + NOISE + SYMBOLS + EXTRA + r"""};
Fns F;
float3 N = normalize(InN);
float3 V = normalize(InV);
float fres = pow(1.0 - saturate(dot(N, V)), 3.0);
bool top = InVC.r > 0.5;
float2 p = float2(InUV.x - 0.5, 0.5 - InUV.y) * 2.0;
float3 base = InBase;
float metal = 0.0;
float rough = 0.3;
float coat = 1.0;
float3 emis = 0.0;
if (InStone > 0.5)
{
    base = float3(0.16, 0.15, 0.17) * (0.65 + 0.7 * F.Fbm(p * 3.0 + 7.1));
    rough = 0.85;
    coat = 0.0;
    if (top)
    {
        float eyes;
        float d = F.Skull(p * 1.12, eyes);
        float aa = max(fwidth(d), 0.0001);
        float fill = 1.0 - smoothstep(-aa, aa, d);
        base = lerp(base, base * 1.35, fill);
        base = lerp(base, float3(0.04, 0.035, 0.045), 1.0 - smoothstep(0.03 - aa, 0.03 + aa, abs(d)));
        float pulse = 0.65 + 0.35 * sin(InTime * 3.0);
        emis += float3(1.0, 0.1, 0.03) * eyes * 4.0 * pulse;
        if (InStone < 1.5)
        {
            float crack = F.Cracks(p * 2.3 + 1.7);
            base = lerp(base, float3(0.02, 0.015, 0.02), crack);
            emis += float3(1.0, 0.3, 0.05) * crack * 2.0 * pulse;
        }
    }
}
else
{
    if (top)
    {
        base *= lerp(1.2, 0.84, saturate(length(p) * 0.8));
        if (InSymbol > -0.5)
        {
            float detail;
            float d = F.Symbol(p, InSymbol, detail);
            float aa = max(fwidth(d), 0.0001);
            float fill = 1.0 - smoothstep(-aa, aa, d);
            float engrave = max(1.0 - smoothstep(0.06 - aa, 0.06 + aa, abs(d)),
                                (1.0 - smoothstep(0.035 - aa, 0.035 + aa, detail)) * fill);
            float3 gold = float3(1.0, 0.82, 0.45);
            base = lerp(base, gold, fill);
            metal = fill * 0.75;
            rough = lerp(rough, 0.22, fill);
            emis += gold * (0.4 + 0.15 * sin(InTime * 2.0 + InUV.x * 3.0)) * fill;
            base = lerp(base, InBase * 0.12, engrave);
            metal = lerp(metal, 0.0, engrave);
            rough = lerp(rough, 0.6, engrave);
            emis *= 1.0 - engrave;
        }
        float sweep = frac((p.x + p.y) * 0.25 - InTime * 0.55);
        float band = smoothstep(0.0, 0.07, sweep) * (1.0 - smoothstep(0.07, 0.16, sweep));
        emis += float3(1.0, 0.9, 0.6) * band * InShimmer * 1.8;
    }
    emis += InBase * fres * (0.45 + 0.6 * InShimmer);
}
emis += (InBase * 2.2 + 0.35) * InGlow;
OutMetal = metal;
OutRough = rough;
OutEmissive = emis;
OutCoat = coat;
return base;
"""
tile = new_material("M_TileArcane", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
tile.set_editor_property("use_material_attributes", True)
tile_node = custom_node(tile, ARCANE_HLSL,
                        ["InUV", "InVC", "InN", "InV", "InBase", "InSymbol", "InGlow", "InStone", "InShimmer", "InTime"], F3,
                        [("OutMetal", F1), ("OutRough", F1), ("OutEmissive", F3), ("OutCoat", F1)])
wire([(expr(tile, unreal.MaterialExpressionTextureCoordinate, y=-300), "InUV"),
      (expr(tile, unreal.MaterialExpressionVertexColor, y=-220), "InVC"),
      (expr(tile, unreal.MaterialExpressionVertexNormalWS, y=-140), "InN"),
      (expr(tile, unreal.MaterialExpressionCameraVectorWS, y=-60), "InV"),
      (vec_param(tile, "BaseColor", unreal.LinearColor(0.8, 0.1, 0.12, 1.0), y=20), "InBase"),
      (scalar_param(tile, "Symbol", -1.0, y=120), "InSymbol"),
      (scalar_param(tile, "Glow", 0.0, y=200), "InGlow"),
      (scalar_param(tile, "Stone", 0.0, y=280), "InStone"),
      (scalar_param(tile, "Shimmer", 0.0, y=360), "InShimmer"),
      (expr(tile, unreal.MaterialExpressionTime, y=440), "InTime")], tile_node)
attrs = mel.create_material_expression(tile, unreal.MaterialExpressionMakeMaterialAttributes, 100, 0)
mel.connect_material_expressions(tile_node, "", attrs, "BaseColor")
mel.connect_material_expressions(tile_node, "OutMetal", attrs, "Metallic")
mel.connect_material_expressions(tile_node, "OutRough", attrs, "Roughness")
mel.connect_material_expressions(tile_node, "OutEmissive", attrs, "EmissiveColor")
mel.connect_material_expressions(tile_node, "OutCoat", attrs, "ClearCoat")
mel.connect_material_expressions(const(tile, 0.08, y=560), "", attrs, "ClearCoatRoughness")
mel.connect_material_property(attrs, "", MP.MP_MATERIAL_ATTRIBUTES)
finish(tile)

# ===========================================================================
# M_HolyAura: animated golden rune circle (additive) for the blessed box and relics.
# ===========================================================================
AURA_HLSL = r"""
float2 p = (InUV - 0.5) * 2.0;
float r = length(p);
float a = atan2(p.y, p.x);
float t = InTime;
float ring1 = saturate(1.0 - abs(r - 0.92) * 40.0);
float ring2 = saturate(1.0 - abs(r - 0.78) * 50.0);
float seg = frac((a + t * 0.4) / 6.2831853 * 24.0);
float ticks = step(0.35, seg) * step(seg, 0.65) * step(0.8, r) * step(r, 0.9);
float inner = saturate(1.0 - abs(r - 0.55) * 30.0) * (0.6 + 0.4 * sin(a * 6.0 - t * 1.5));
float star = 0.0;
for (int k = 0; k < 3; k++)
{
    float th = k * 2.0943951 - t * 0.25;
    float2 dir = float2(cos(th), sin(th));
    star += saturate(1.0 - abs(dot(p, float2(-dir.y, dir.x))) * 60.0) * step(r, 0.78);
}
float pulse = 0.75 + 0.25 * sin(t * 2.5);
float m = (ring1 + ring2 * 0.8 + ticks * 0.9 + inner * 0.6 + star * 0.3 + saturate(1.0 - r) * 0.25) * pulse;
m *= saturate((1.0 - r) * 8.0);
return InColor * InIntensity * m;
"""
aura = new_material("M_HolyAura", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_ADDITIVE)
aura_node = custom_node(aura, AURA_HLSL, ["InUV", "InTime", "InColor", "InIntensity"], F3, [])
wire([(expr(aura, unreal.MaterialExpressionTextureCoordinate, y=-150), "InUV"),
      (expr(aura, unreal.MaterialExpressionTime, y=-50), "InTime"),
      (vec_param(aura, "Color", unreal.LinearColor(1.0, 0.75, 0.3, 1.0), y=50), "InColor"),
      (scalar_param(aura, "Intensity", 2.0, y=200), "InIntensity")], aura_node)
mel.connect_material_property(aura_node, "", MP.MP_EMISSIVE_COLOR)
finish(aura)

# ===========================================================================
# M_PPInkOutline: post-process ink lines where depth or normals break
# (silhouettes, bevels). Realistic lighting + cartoon lines.
# SceneTextureLookup ids: 1 = SceneDepth, 8 = WorldNormal.
# ===========================================================================
INK_HLSL = r"""
float2 uv = GetDefaultSceneTextureUV(Parameters, 1);
float2 px = View.BufferSizeAndInvSize.zw * InThickness;
float dC = SceneTextureLookup(uv, 1, false).r;
float dL = SceneTextureLookup(uv + float2(-px.x, 0.0), 1, false).r;
float dR = SceneTextureLookup(uv + float2(px.x, 0.0), 1, false).r;
float dU = SceneTextureLookup(uv + float2(0.0, -px.y), 1, false).r;
float dD = SceneTextureLookup(uv + float2(0.0, px.y), 1, false).r;
float lap = abs(dL + dR + dU + dD - 4.0 * dC);
float depthEdge = saturate(lap / max(dC, 1.0) * InDepthSens - 0.15);
float3 nC = SceneTextureLookup(uv, 8, false).rgb;
float3 nL = SceneTextureLookup(uv + float2(-px.x, 0.0), 8, false).rgb;
float3 nR = SceneTextureLookup(uv + float2(px.x, 0.0), 8, false).rgb;
float3 nU = SceneTextureLookup(uv + float2(0.0, -px.y), 8, false).rgb;
float3 nD = SceneTextureLookup(uv + float2(0.0, px.y), 8, false).rgb;
float nMin = min(min(dot(nC, nL), dot(nC, nR)), min(dot(nC, nU), dot(nC, nD)));
float normalEdge = saturate((1.0 - nMin) * InNormalSens - 0.25);
float edge = saturate(max(depthEdge, normalEdge)) * step(dC, InMaxDistance);
float3 col = InSceneColor.rgb;
return lerp(col, col * InInk, edge * InStrength);
"""
ink = new_material("M_PPInkOutline", unreal.MaterialShadingModel.MSM_UNLIT)
ink.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
ink.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_DOF)
ink_node = custom_node(ink, INK_HLSL, ["InSceneColor", "InThickness", "InDepthSens", "InNormalSens", "InStrength", "InInk", "InMaxDistance"], F3, [])
scene_color = expr(ink, unreal.MaterialExpressionSceneTexture, y=-250)
scene_color.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
# These two nodes aren't wired; they make the depth/normal buffers available to SceneTextureLookup.
for tex_id, y in [(unreal.SceneTextureId.PPI_SCENE_DEPTH, -400), (unreal.SceneTextureId.PPI_WORLD_NORMAL, -500)]:
    node = expr(ink, unreal.MaterialExpressionSceneTexture, y=y)
    node.set_editor_property("scene_texture_id", tex_id)
mel.connect_material_expressions(scene_color, "Color", ink_node, "InSceneColor")
wire([(scalar_param(ink, "Thickness", 1.0, y=-120), "InThickness"),
      (scalar_param(ink, "DepthSensitivity", 14.0, y=-40), "InDepthSens"),
      (scalar_param(ink, "NormalSensitivity", 2.2, y=40), "InNormalSens"),
      (scalar_param(ink, "Strength", 0.85, y=120), "InStrength"),
      (vec_param(ink, "InkColor", unreal.LinearColor(0.06, 0.03, 0.1, 1.0), y=200), "InInk"),
      (scalar_param(ink, "MaxDistance", 6000.0, y=300), "InMaxDistance")], ink_node)
mel.connect_material_property(ink_node, "", MP.MP_EMISSIVE_COLOR)
finish(ink)

# ===========================================================================
# M_UIPanel: bevelled enamel panel with gold rim, gloss, inner shadow, soft
# drop shadow and optional outer glow. Aspect = widget width / height.
# ===========================================================================
PANEL_HLSL = "struct Fns {" + NOISE + EXTRA + r"""};
Fns F;
float2 uv = InUV;
float2 p = (uv - 0.5) * float2(InAspect, 1.0);
float margin = 0.08;
float2 halfSize = float2(InAspect * 0.5, 0.5) - margin;
float r = min(InRadius, min(halfSize.x, halfSize.y));
float d = F.RoundBox(p, halfSize, r);
float ds = F.RoundBox(p - float2(0.0, 0.035), halfSize, r);
float shadowA = (1.0 - smoothstep(-0.01, 0.07, ds)) * 0.55;
float aa = max(fwidth(d), 0.0001);
float inside = 1.0 - smoothstep(-aa, aa, d);
float t = saturate(-d / InRimWidth);
float2 g = float2(ddx(d), ddy(d));
float2 n2 = g / max(length(g), 0.00001);
float bulge = sqrt(saturate(1.0 - (1.0 - 2.0 * t) * (1.0 - 2.0 * t)));
float3 n3 = normalize(float3(n2 * (1.0 - 2.0 * t) * 1.2, bulge + 0.2));
float3 L = normalize(float3(-0.5, -0.7, 0.6));
float ndl = saturate(dot(n3, L));
float spec = pow(saturate(dot(reflect(-L, n3), float3(0.0, 0.0, 1.0))), 18.0);
float3 rimCol = InRim * (0.45 + 0.85 * ndl) + spec * 0.9;
float rimMask = inside * (1.0 - smoothstep(InRimWidth - aa, InRimWidth + aa, -d));
float3 fill = lerp(InFill, InFill2, uv.y);
fill *= 0.94 + 0.06 * F.Noise(uv * float2(InAspect, 1.0) * 60.0);
fill *= 1.0 - 0.45 * (1.0 - smoothstep(0.0, 0.06, -d - InRimWidth));
fill += (1.0 - smoothstep(0.1, 0.45, uv.y)) * 0.08 * (1.0 - InPressed);
fill *= 1.0 - 0.3 * InPressed;
float3 panel = lerp(fill, rimCol, rimMask);
float glowA = InGlow * saturate(1.0 - max(d, 0.0) * 14.0) * (1.0 - inside);
float3 outside = lerp(float3(0.0, 0.0, 0.02), InRim * 1.5, saturate(glowA / max(glowA + shadowA, 0.0001)));
OutAlpha = lerp(saturate(shadowA + glowA), InOpacity, inside);
return lerp(outside, panel, inside);
"""
panel = new_material("M_UIPanel", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_TRANSLUCENT)
panel.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
panel_node = custom_node(panel, PANEL_HLSL,
                         ["InUV", "InAspect", "InRadius", "InFill", "InFill2", "InRim", "InRimWidth", "InGlow", "InPressed", "InOpacity"], F3,
                         [("OutAlpha", F1)])
wire([(expr(panel, unreal.MaterialExpressionTextureCoordinate, y=-300), "InUV"),
      (scalar_param(panel, "Aspect", 3.0, y=-220), "InAspect"),
      (scalar_param(panel, "Radius", 0.12, y=-140), "InRadius"),
      (vec_param(panel, "Fill", unreal.LinearColor(0.12, 0.05, 0.2, 1.0), y=-60), "InFill"),
      (vec_param(panel, "Fill2", unreal.LinearColor(0.04, 0.02, 0.08, 1.0), y=40), "InFill2"),
      (vec_param(panel, "Rim", unreal.LinearColor(1.0, 0.72, 0.3, 1.0), y=140), "InRim"),
      (scalar_param(panel, "RimWidth", 0.06, y=240), "InRimWidth"),
      (scalar_param(panel, "Glow", 0.0, y=320), "InGlow"),
      (scalar_param(panel, "Pressed", 0.0, y=400), "InPressed"),
      (scalar_param(panel, "Opacity", 0.92, y=480), "InOpacity")], panel_node)
mel.connect_material_property(panel_node, "", MP.MP_EMISSIVE_COLOR)
mel.connect_material_property(panel_node, "OutAlpha", MP.MP_OPACITY)
finish(panel)

# ===========================================================================
# M_UIIcon: embossed coin-style icons. Shape: 0-4 tile symbols, 5 gargoyle,
# 6 holy sun, 7 reroll arrow, 8 3x3 box, 9 line, 10 rating star (Fill 0 = empty).
# ===========================================================================
ICON_HLSL = "struct Fns {" + NOISE + SYMBOLS + EXTRA + r"""
    float Shape(float2 p, float id, float filled)
    {
        float detail;
        if (id < 4.5) { return Symbol(p, id, detail); }
        if (id < 5.5) { float eyes; return Skull(p * 1.1, eyes) / 1.1; }
        if (id < 6.5)
        {
            float a = atan2(p.y, p.x);
            float rays = 0.36 + 0.24 * pow(abs(cos(a * 4.0)), 8.0);
            return min(length(p) - 0.3, (length(p) - rays) * 0.8);
        }
        if (id < 7.5)
        {
            float a = atan2(p.y, p.x);
            float ring = abs(length(p) - 0.46) - 0.1;
            ring = (a > 0.25 && a < 1.45) ? max(ring, 0.12) : ring;
            float2 tip = float2(0.46 * cos(0.25), 0.46 * sin(0.25));
            float2 q = p - tip;
            float2 qr = float2(q.x * cos(1.82) + q.y * sin(1.82), -q.x * sin(1.82) + q.y * cos(1.82));
            float head = TriangleIso(qr - float2(0.0, -0.05), float2(0.24, 0.3));
            return min(ring, head);
        }
        if (id < 8.5)
        {
            float outline = abs(RoundBox(p, float2(0.58, 0.58), 0.12)) - 0.06;
            float grid = min(abs(abs(p.x) - 0.19), abs(abs(p.y) - 0.19)) - 0.04;
            grid = max(grid, RoundBox(p, float2(0.55, 0.55), 0.1));
            return min(outline, grid);
        }
        if (id < 9.5)
        {
            float2 q = float2((frac((p.x + 0.6) / 0.3) - 0.5) * 0.3, p.y);
            return max(RoundBox(q, float2(0.12, 0.12), 0.04), abs(p.x) - 0.6);
        }
        float star = Star5(p + float2(0.0, 0.05), 0.72, 0.45) - 0.04;
        return filled > 0.5 ? star : abs(star) - 0.05;
    }
};
Fns F;
float2 p = (InUV - 0.5) * 2.2;
p.y = -p.y;
float d = F.Shape(p, InShape, InFill);
float aa = max(fwidth(d), 0.0001);
float inside = 1.0 - smoothstep(-aa, aa, d);
float ds = F.Shape(p + float2(-0.05, 0.08), InShape, InFill);
float shadowA = (1.0 - smoothstep(-0.02, 0.08, ds)) * 0.5;
float2 g = float2(ddx(d), -ddy(d));
float2 n2 = g / max(length(g), 0.00001);
float edgeT = saturate(-d / 0.12);
float3 n3 = normalize(float3(n2 * (1.0 - edgeT), 0.6 + edgeT));
float ndl = saturate(dot(n3, normalize(float3(-0.5, 0.7, 0.6))));
float3 body = lerp(InColor * 1.3, InColor * 0.7, InUV.y) * (0.55 + 0.7 * ndl);
body += pow(ndl, 12.0) * 0.5;
float ink = 1.0 - smoothstep(0.035 - aa, 0.035 + aa, abs(d));
float3 col = lerp(body, float3(0.06, 0.03, 0.08), ink);
float glowA = InGlow * saturate(1.0 - max(d, 0.0) * 5.0) * (1.0 - inside);
float3 outside = lerp(float3(0.0, 0.0, 0.0), InGlowColor, saturate(glowA / max(glowA + shadowA, 0.0001)));
OutAlpha = lerp(saturate(shadowA + glowA), 1.0, max(inside, ink * 0.95));
return lerp(outside, col, max(inside, ink));
"""
icon = new_material("M_UIIcon", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_TRANSLUCENT)
icon.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
icon_node = custom_node(icon, ICON_HLSL, ["InUV", "InShape", "InFill", "InColor", "InGlow", "InGlowColor"], F3, [("OutAlpha", F1)])
wire([(expr(icon, unreal.MaterialExpressionTextureCoordinate, y=-200), "InUV"),
      (scalar_param(icon, "Shape", 3.0, y=-120), "InShape"),
      (scalar_param(icon, "Fill", 1.0, y=-40), "InFill"),
      (vec_param(icon, "Color", unreal.LinearColor(1.0, 0.75, 0.25, 1.0), y=40), "InColor"),
      (scalar_param(icon, "Glow", 0.0, y=140), "InGlow"),
      (vec_param(icon, "GlowColor", unreal.LinearColor(1.0, 0.8, 0.3, 1.0), y=220), "InGlowColor")], icon_node)
mel.connect_material_property(icon_node, "", MP.MP_EMISSIVE_COLOR)
mel.connect_material_property(icon_node, "OutAlpha", MP.MP_OPACITY)
finish(icon)

unreal.log("build_arcane_assets.py: done")
