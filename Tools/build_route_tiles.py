# Route-puzzle tiles: builds M_TileRoute (a copy of M_TileGothic; an asset that C++ loads can't be
# rebuilt in place, so each new tile look gets a new name) so every tile carries a triangle that shows
# the direction its route flows (Direction 0 = up, 1 = right, 2 = down, 3 = left; -1 = no triangle,
# the five sigils show instead). The triangle is drawn as a gold inlay like the sigils were.
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
    // Triangle with vertices a, b, c (signed distance, negative inside).
    float Tri(float2 p, float2 a, float2 b, float2 c)
    {
        float2 e0 = b - a; float2 e1 = c - b; float2 e2 = a - c;
        float2 v0 = p - a; float2 v1 = p - b; float2 v2 = p - c;
        float2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
        float2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
        float2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
        float s = sign(e0.x * e2.y - e0.y * e2.x);
        float2 d = min(min(float2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                           float2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                           float2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
        return -sqrt(d.x) * sign(d.y);
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
            float d = F.Symbol(p, InSymbol, detail);""", """        if (InDir > -0.5 || InSymbol > -0.5)
        {
            float detail = 1000.0;
            float d;
            if (InDir > -0.5)
            {
                // Rotate the sample point so the triangle (apex up) points the way the route flows.
                float2 dv = float2(InDir > 0.5 && InDir < 1.5 ? 1.0 : (InDir > 2.5 ? -1.0 : 0.0),
                                   InDir < 0.5 ? 1.0 : (InDir > 1.5 && InDir < 2.5 ? -1.0 : 0.0));
                float2 pl = float2(dot(p, float2(dv.y, -dv.x)), dot(p, dv));
                d = F.Tri(pl * 0.95, float2(0.0, 0.62), float2(-0.52, -0.42), float2(0.52, -0.42)) / 0.95;
            }
            else
            {
                d = F.Symbol(p, InSymbol, detail);
            }""")
tile_code = swap(tile_code, '"InSymbol", "InGlow", "InStone", "InShimmer", "InTime"]', '"InSymbol", "InGlow", "InStone", "InShimmer", "InTime", "InDir"]')
tile_code = swap(tile_code, '      (scalar_param(tile, "Shimmer", 0.0, y=360), "InShimmer"),', '      (scalar_param(tile, "Shimmer", 0.0, y=360), "InShimmer"),\n      (scalar_param(tile, "Direction", -1.0, y=520), "InDir"),')
# Light tiles (green, blue/teal, yellow) get a red arrow so it reads against them; the dark ones keep gold.
tile_code = swap(tile_code, "            float3 gold = float3(1.0, 0.82, 0.45);\n", "            float isRed = (InDir > -0.5 && InSymbol > 0.5 && InSymbol < 3.5) ? 1.0 : 0.0;\n            float3 gold = lerp(float3(1.0, 0.82, 0.45), float3(0.95, 0.03, 0.03), isRed);\n")
tile_code = swap(tile_code, "            metal = fill * 0.75;", "            metal = fill * lerp(0.75, 0.1, isRed);")
exec(tile_code)
unreal.log("build_route_tiles.py: done")
