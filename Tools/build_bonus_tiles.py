# Halloween bonus tiles: builds M_TileBonus, the route tile material plus an Emblem for tiles that point nowhere:
#   Emblem 1 = a glass potion bottle with magenta liquid, its level set by the Fill parameter
#              (outgoing tiles start full and drain as a route leaves them; incoming tiles start empty and fill),
#   Emblem 2 = a carved pumpkin lit from inside (basic tiles; it swells and bursts when its chain clears),
#   Emblem 0 with no Direction and no Symbol = a plain tile.
# C++ loads this material lazily when a bonus tile is made (not from a constructor), so it can be rebuilt in place.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>   (the editor must be closed)
import unreal

_SCR = unreal.SystemLibrary.get_project_directory() + "Tools"
_src = open(_SCR + "/build_route_tiles.py", encoding="utf-8").read()
_src = _src.replace('"M_TileRoute"', '"M_TileBonus"')

_EMBLEM_FUNCS = r'''
    float Tri2(float2 p, float2 a, float2 b, float2 c)
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
'''

_EMBLEM_HLSL = """        if (InEmblem > 0.5)
        {
            float t = InTime;
            if (InEmblem < 1.5)
            {
                // Glass potion bottle; the liquid level follows Fill (0 empty, 1 full).
                float2 q = p * 0.5;
                float body = length(q - float2(0.0, -0.17)) - 0.40;
                float neck = F.RoundBox(q - float2(0.0, 0.31), float2(0.12, 0.22), 0.03);
                float lip = F.RoundBox(q - float2(0.0, 0.54), float2(0.17, 0.05), 0.03);
                float glass = F.SMinH(F.SMinH(body, neck, 0.10), lip, 0.03);
                float cork = F.RoundBox(q - float2(0.0, 0.68), float2(0.11, 0.09), 0.03);
                float aaE = max(fwidth(glass), 0.0001);
                float inside = 1.0 - smoothstep(-aaE, aaE, glass + 0.04);
                float edge = 1.0 - smoothstep(0.022 - aaE, 0.022 + aaE, abs(glass));
                float level = lerp(-0.55, 0.30, saturate(InFill));
                float wob = 0.014 * sin(q.x * 16.0 + t * 4.0);
                float liquid = inside * (1.0 - smoothstep(-aaE, aaE, q.y - level - wob)) * step(0.01, InFill);
                float corkF = 1.0 - smoothstep(-aaE, aaE, cork);
                float3 mag = float3(1.0, 0.05, 0.75);
                base = lerp(base, float3(0.03, 0.05, 0.09), inside * 0.6);
                base = lerp(base, mag * 0.85, liquid);
                emis += mag * liquid * (1.0 + 0.3 * sin(t * 3.0));
                float hl = (1.0 - smoothstep(0.02 - aaE, 0.02 + aaE, abs(length(q - float2(0.0, -0.17)) - 0.29))) * step(q.x, -0.10) * step(q.y, 0.0) * step(-0.36, q.y);
                base = lerp(base, float3(0.85, 0.96, 1.0), edge * 0.9);
                base = lerp(base, float3(1.0, 1.0, 1.0), hl * 0.85);
                base = lerp(base, float3(0.38, 0.21, 0.09), corkF);
                metal = lerp(metal, 0.0, max(edge, corkF));
                rough = lerp(rough, 0.2, max(inside, corkF));
            }
            else
            {
                // Carved pumpkin lit from inside.
                float2 q = p * 0.52;
                float2 e = q - float2(0.0, -0.07);
                float bodyL = length((e - float2(-0.20, 0.0)) * float2(1.0 / 0.44, 1.0 / 0.50)) - 1.0;
                float bodyR = length((e - float2(0.20, 0.0)) * float2(1.0 / 0.44, 1.0 / 0.50)) - 1.0;
                float bodyM = length(e * float2(1.0 / 0.46, 1.0 / 0.53)) - 1.0;
                float pump = min(min(bodyL, bodyR), bodyM) * 0.42;
                float aaE = max(fwidth(pump), 0.0001);
                float pf = 1.0 - smoothstep(-aaE, aaE, pump);
                float rib = smoothstep(0.78, 1.0, abs(cos(e.x * 10.5)));
                float3 orange = float3(1.0, 0.36, 0.02) * (1.0 - 0.28 * rib);
                orange *= 0.7 + 0.45 * saturate(1.0 - length(e) * 1.3);
                float stem = 1.0 - smoothstep(-aaE, aaE, F.RoundBox(q - float2(0.03, 0.47), float2(0.06, 0.10), 0.03));
                float eyeL = F.Tri2(q, float2(-0.32, -0.04), float2(-0.10, -0.04), float2(-0.21, 0.16));
                float eyeR = F.Tri2(q, float2(0.10, -0.04), float2(0.32, -0.04), float2(0.21, 0.16));
                float zz = abs(frac(q.x * 3.6 + 0.5) - 0.5) * 2.0;
                float mouth = max(abs(q.x) - 0.31, abs(q.y + 0.25 - 0.07 * zz) - 0.05);
                float carve = max(1.0 - smoothstep(-aaE, aaE, eyeL), max(1.0 - smoothstep(-aaE, aaE, eyeR), 1.0 - smoothstep(-aaE, aaE, mouth)));
                float flick = 1.0 + 0.25 * sin(t * 7.0 + q.x * 3.0);
                base = lerp(base, orange, pf);
                base = lerp(base, float3(0.22, 0.30, 0.06), stem);
                base = lerp(base, float3(1.0, 0.82, 0.22), carve * pf);
                metal = lerp(metal, 0.0, pf);
                rough = lerp(rough, 0.45, pf);
                emis += float3(1.0, 0.55, 0.06) * carve * pf * 2.2 * flick;
                emis += orange * pf * 0.25;
            }
        }
"""

_INJECT = '''
EXTRA += _EMBLEM_FUNCS
tile_code = swap(tile_code, '"InShimmer", "InTime", "InDir"]', '"InShimmer", "InTime", "InDir", "InEmblem", "InFill"]')
tile_code = swap(tile_code, '      (scalar_param(tile, "Direction", -1.0, y=520), "InDir"),', '      (scalar_param(tile, "Direction", -1.0, y=520), "InDir"),\\n      (scalar_param(tile, "Emblem", 0.0, y=600), "InEmblem"),\\n      (scalar_param(tile, "Fill", 0.0, y=680), "InFill"),')
tile_code = swap(tile_code, '        float sweep = frac((p.x + p.y) * 0.25 - InTime * 0.55);', _EMBLEM_HLSL + '        float sweep = frac((p.x + p.y) * 0.25 - InTime * 0.55);')
exec(tile_code)
'''
assert "exec(tile_code)" in _src
_src = _src.replace("exec(tile_code)", _INJECT, 1)
exec(_src)
unreal.log("build_bonus_tiles.py: done")
