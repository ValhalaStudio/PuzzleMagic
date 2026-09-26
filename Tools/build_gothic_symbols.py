# Gothic tile symbols: blood drop (red), skull (green), crescent moon (blue), budded cross
# (gold), bat (purple). Builds a NEW tile material M_TileGothic (M_TileArcane is rooted by C++,
# so it can't be rebuilt in place) and rebuilds M_UIIcon in place (runtime-loaded), plus the
# new M_GroundMist shader.
import unreal

SCR = r"C:\Users\VagDi\AppData\Local\Temp\claude\C--Users-VagDi\57e5f065-d886-4909-abc7-41ade08db499\scratchpad"
exec(open(SCR + r"\arcane_head.py", encoding="utf-8").read())

SYMBOLS = r"""
    float Dot2(float2 v) { return dot(v, v); }
    float Star5(float2 p, float r, float rf)
    {
        const float2 k1 = float2(0.809016994375, -0.587785252292);
        const float2 k2 = float2(-k1.x, k1.y);
        p.x = abs(p.x);
        p -= 2.0 * max(dot(k1, p), 0.0) * k1;
        p -= 2.0 * max(dot(k2, p), 0.0) * k2;
        p.x = abs(p.x);
        p.y -= r;
        float2 ba = rf * float2(-k1.y, k1.x) - float2(0.0, 1.0);
        float h = clamp(dot(p, ba) / dot(ba, ba), 0.0, r);
        return length(p - ba * h) * sign(p.y * ba.x - p.x * ba.y);
    }
    float Moon(float2 p, float d, float ra, float rb)
    {
        p.y = abs(p.y);
        float a = (ra * ra - rb * rb + d * d) / (2.0 * d);
        float b = sqrt(max(ra * ra - a * a, 0.0));
        if (d * (p.x * b - p.y * a) > d * d * max(b - p.y, 0.0)) return length(p - float2(a, b));
        return max(length(p) - ra, -(length(p - float2(d, 0.0)) - rb));
    }
    float Segment(float2 p, float2 a, float2 b)
    {
        float2 pa = p - a;
        float2 ba = b - a;
        float h = saturate(dot(pa, ba) / dot(ba, ba));
        return length(pa - ba * h);
    }
    float GBox(float2 p, float2 b, float r)
    {
        float2 q = abs(p) - b + r;
        return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
    }
    float Teardrop(float2 p, float r1, float r2, float h)
    {
        p.x = abs(p.x);
        float b = (r1 - r2) / h;
        float a = sqrt(1.0 - b * b);
        float k = dot(p, float2(-b, a));
        if (k < 0.0) return length(p) - r1;
        if (k > a * h) return length(p - float2(0.0, h)) - r2;
        return dot(p, float2(a, b)) - r1;
    }
    float SkullSym(float2 p, out float detail)
    {
        float d = min(length(p - float2(0.0, 0.14)) - 0.5, GBox(p - float2(0.0, -0.36), float2(0.28, 0.2), 0.1));
        float eyes = min(length(p - float2(-0.19, 0.06)) - 0.15, length(p - float2(0.19, 0.06)) - 0.15);
        float nose = length((p - float2(0.0, -0.17)) * float2(1.7, 1.0)) - 0.075;
        d = max(d, -eyes);
        d = max(d, -nose);
        detail = min(min(Segment(p, float2(-0.12, -0.46), float2(-0.12, -0.3)), Segment(p, float2(0.0, -0.47), float2(0.0, -0.3))),
                     Segment(p, float2(0.12, -0.46), float2(0.12, -0.3)));
        return d;
    }
    float CrossSym(float2 p, out float detail)
    {
        float d = min(GBox(p - float2(0.0, -0.06), float2(0.12, 0.6), 0.04), GBox(p - float2(0.0, 0.24), float2(0.44, 0.12), 0.04));
        d = min(d, length(p - float2(0.0, 0.62)) - 0.15);
        d = min(d, length(float2(abs(p.x), p.y) - float2(0.46, 0.24)) - 0.15);
        detail = min(Segment(p, float2(0.0, -0.5), float2(0.0, 0.5)), Segment(p, float2(-0.34, 0.24), float2(0.34, 0.24)));
        return d;
    }
    float BatSym(float2 p, out float detail)
    {
        float2 q = float2(abs(p.x), p.y);
        float wing = length((q - float2(0.44, 0.02)) * float2(1.0, 1.75)) - 0.52;
        float scallop = min(min(length(q - float2(0.2, -0.33)) - 0.15, length(q - float2(0.5, -0.33)) - 0.15),
                            length(q - float2(0.8, -0.25)) - 0.14);
        wing = max(max(wing, -scallop), q.x - 0.95);
        float body = length((p - float2(0.0, -0.04)) * float2(1.0, 0.72)) - 0.15;
        float head = length(p - float2(0.0, 0.19)) - 0.12;
        float ears = Segment(q, float2(0.05, 0.25), float2(0.1, 0.4)) - 0.045;
        detail = min(min(Segment(q, float2(0.12, 0.08), float2(0.36, -0.18)), Segment(q, float2(0.12, 0.08), float2(0.65, -0.16))),
                     Segment(q, float2(0.12, 0.08), float2(0.88, -0.08)));
        return min(min(wing, body), min(head, ears));
    }
    float Symbol(float2 p, float id, out float detail)
    {
        detail = 1000.0;
        if (id < 0.5)
        {
            float2 hp = p - float2(-0.11, -0.3);
            detail = (hp.x < 0.0 && hp.y > -0.05) ? abs(length(hp) - 0.2) : 1000.0;
            return Teardrop(p - float2(0.0, -0.28), 0.4, 0.03, 0.92);
        }
        if (id < 1.5) { return SkullSym(p * 1.05, detail) / 1.05; }
        if (id < 2.5) { return Moon(p + float2(0.08, 0.0), 0.34, 0.6, 0.5); }
        if (id < 3.5) { return CrossSym(p, detail); }
        return BatSym(p * 1.1, detail) / 1.1;
    }
"""

_body = open(SCR + r"\arcane_body.py", encoding="utf-8").read()
exec(_body[:_body.index("# ======")])  # EXTRA


def section(title, end_marker):
    start = _body.index("# ======", _body.index(title))
    start = _body.index("\n", start) + 1
    return _body[start:_body.index(end_marker, start)]


tile_code = section("# M_TileArcane", "# ======")
tile_code = tile_code.replace('"M_TileArcane"', '"M_TileGothic"')
exec(tile_code)

icon_code = section("# M_UIIcon", 'unreal.log("build_arcane_assets.py: done")')
exec(icon_code)

# ---------------------------------------------------------------------------
# M_GroundMist: low creeping fog layer. Two fbm layers drifting against each other,
# thinned over the board so play stays readable, soft-faded where it meets geometry.
# ---------------------------------------------------------------------------
MIST_HLSL = "struct Fns {" + NOISE + r"""
    float Fbm(float2 p)
    {
        float v = 0.0;
        float a = 0.5;
        for (int i = 0; i < 5; i++) { v += a * Noise(p); p = p * 2.03 + float2(17.1, 9.2); a *= 0.5; }
        return v;
    }
};
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
mist = new_material("M_GroundMist", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_TRANSLUCENT)
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

unreal.log("build_gothic_symbols.py: done")
