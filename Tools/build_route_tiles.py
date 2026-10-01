# Halloween route tiles: builds/rebuilds M_TileRoute so every tile carries a skeleton hand whose pointing
# finger shows the direction its route flows (Direction 0 = up, 1 = right, 2 = down, 3 = left; -1 = no hand).
# The hand is bone white, or black on the ash-grey tiles (Symbol = tile colour index: 0 black, 1 purple, 2 red,
# 3 ash). Only the hand is new: the enamel, clear coat and glow come from the original arcane tile material.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>   (the editor must be closed)
import unreal

SCR = unreal.SystemLibrary.get_project_directory() + "Tools"
exec(open(SCR + "/arcane_head.py", encoding="utf-8").read())

# The gothic sigils (SYMBOLS in build_gothic_symbols.py override the ones in arcane_head.py).
_gothic = open(SCR + "/build_gothic_symbols.py", encoding="utf-8").read()
_s = _gothic.index('SYMBOLS = r"""')
_e = _gothic.index('"""', _s + len('SYMBOLS = r"""')) + 3
exec(_gothic[_s:_e])

_body = open(SCR + "/arcane_body.py", encoding="utf-8").read()
exec(_body[:_body.index("# ======")])  # EXTRA

EXTRA += r"""
    float Cap(float2 p, float2 a, float2 b, float r)
    {
        float2 pa = p - a;
        float2 ba = b - a;
        float h = saturate(dot(pa, ba) / dot(ba, ba));
        return length(pa - ba * h) - r;
    }
    float Seg2(float2 p, float2 a, float2 b)
    {
        float2 pa = p - a;
        float2 ba = b - a;
        float h = saturate(dot(pa, ba) / dot(ba, ba));
        return length(pa - ba * h);
    }
    float SMinH(float a, float b, float k)
    {
        float h = saturate(0.5 + 0.5 * (b - a) / k);
        return lerp(b, a, h) - k * h * (1.0 - h);
    }
    // A skeleton hand in a fist with the index finger out, pointing up (+y). Detail = the joint and bone lines.
    float Hand(float2 p, out float detail)
    {
        p.y += 0.12;
        float2 f0 = float2(-0.12, 0.00);
        float2 f1 = float2(-0.12, 0.27);
        float2 f2 = float2(-0.12, 0.49);
        float2 f3 = float2(-0.12, 0.69);
        // wrist bones and the fist
        float d = RoundBox(p - float2(0.04, -0.27), float2(0.31, 0.24), 0.12);
        // the three curled fingers show as knuckles
        d = min(d, length(p - float2(0.07, -0.02)) - 0.095);
        d = min(d, length(p - float2(0.19, -0.04)) - 0.095);
        d = min(d, length(p - float2(0.30, -0.10)) - 0.088);
        // thumb
        d = min(d, Cap(p, float2(-0.22, -0.22), float2(-0.42, -0.02), 0.075));
        d = min(d, length(p - float2(-0.42, -0.02)) - 0.088);
        // index finger: three phalanges and their joints
        d = min(d, Cap(p, f0, f1, 0.062));
        d = min(d, Cap(p, f1, f2, 0.055));
        d = min(d, Cap(p, f2, f3, 0.046));
        d = min(d, length(p - f1) - 0.08);
        d = min(d, length(p - f2) - 0.072);
        d = min(d, length(p - f3) - 0.06);
        detail = min(Seg2(p, f1 + float2(-0.06, 0.0), f1 + float2(0.06, 0.0)), Seg2(p, f2 + float2(-0.055, 0.0), f2 + float2(0.055, 0.0)));
        detail = min(detail, Seg2(p, float2(0.07, -0.10), float2(0.06, -0.44)));
        detail = min(detail, Seg2(p, float2(0.18, -0.11), float2(0.18, -0.44)));
        detail = min(detail, Seg2(p, float2(0.28, -0.16), float2(0.28, -0.40)));
        return d;
    }
"""


def section(title, end_marker):
    start = _body.index("# ======", _body.index(title))
    start = _body.index("\n", start) + 1
    return _body[start:_body.index(end_marker, start)]


def swap(text, old, new):
    assert old in text, "pattern not found: " + old[:60]
    return text.replace(old, new, 1)


tile_code = section("# M_TileArcane", "# ======")
tile_code = tile_code.replace('"M_TileArcane"', '"M_TileRoute"')
tile_code = swap(tile_code, """        if (InSymbol > -0.5)
        {
            float detail;
            float d = F.Symbol(p, InSymbol, detail);""", """        if (InDir > -0.5)
        {
            // Rotate the sample point so the hand points the way the route flows.
            float2 dv = float2(InDir > 0.5 && InDir < 1.5 ? 1.0 : (InDir > 2.5 ? -1.0 : 0.0),
                               InDir < 0.5 ? 1.0 : (InDir > 1.5 && InDir < 2.5 ? -1.0 : 0.0));
            float2 pl = float2(dot(p, float2(dv.y, -dv.x)), dot(p, dv));
            float handDetail;
            float hd = F.Hand(pl * 0.72, handDetail) / 0.72;
            float haa = max(fwidth(hd), 0.0001);
            float hfill = 1.0 - smoothstep(-haa, haa, hd);
            float hedge = (1.0 - smoothstep(0.018 - haa, 0.018 + haa, abs(hd))) * hfill;
            float hline = (1.0 - smoothstep(0.012 - haa, 0.012 + haa, handDetail / 0.72)) * hfill;
            float dark = InSymbol > 2.5 ? 1.0 : 0.0;
            float3 bone = lerp(float3(0.93, 0.89, 0.76), float3(0.02, 0.02, 0.025), dark);
            base = lerp(base, bone, hfill);
            base = lerp(base, lerp(bone * 0.35, float3(0.6, 0.6, 0.65), dark), max(hedge, hline * 0.8));
            metal = 0.0;
            rough = lerp(rough, 0.5, hfill);
            emis += bone * 0.12 * hfill;
        }
        else if (InSymbol > -0.5)
        {
            float detail;
            float d = F.Symbol(p, InSymbol, detail);""")
tile_code = swap(tile_code, '"InSymbol", "InGlow", "InStone", "InShimmer", "InTime"]', '"InSymbol", "InGlow", "InStone", "InShimmer", "InTime", "InDir"]')
tile_code = swap(tile_code, '      (scalar_param(tile, "Shimmer", 0.0, y=360), "InShimmer"),', '      (scalar_param(tile, "Shimmer", 0.0, y=360), "InShimmer"),\n      (scalar_param(tile, "Direction", -1.0, y=520), "InDir"),')
exec(tile_code)
unreal.log("build_route_tiles.py: done")
