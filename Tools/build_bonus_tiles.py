# Bonus tiles: builds M_TileBonus, the route tile material plus an Emblem parameter for tiles that point
# nowhere: Emblem 1 = a white diamond holding a black diamond (outgoing: a route can leave in any direction),
# Emblem 2 = a white ring round a black disc, a fisheye (incoming: a route can arrive from any direction),
# Emblem 0 with no Direction and no Symbol = a plain glowing tile (basic).
# C++ loads this material lazily when a bonus tile is made (not from a constructor), so it can be rebuilt in place.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>   (the editor must be closed)
import unreal

_SCR = unreal.SystemLibrary.get_project_directory() + "Tools"
_src = open(_SCR + "/build_route_tiles.py", encoding="utf-8").read()
_src = _src.replace('"M_TileRoute"', '"M_TileBonus"')

_EMBLEM_HLSL = """        if (InEmblem > 0.5)
        {
            float dOutline;
            float dCore;
            if (InEmblem < 1.5)
            {
                dOutline = abs((abs(p.x) + abs(p.y)) * 0.7071 - 0.46) - 0.05;
                dCore = (abs(p.x) + abs(p.y)) * 0.7071 - 0.18;
            }
            else
            {
                dOutline = abs(length(p) - 0.52) - 0.06;
                dCore = length(p) - 0.24;
            }
            float aaE = max(max(fwidth(dOutline), fwidth(dCore)), 0.0001);
            float ring = 1.0 - smoothstep(-aaE, aaE, dOutline);
            float core = 1.0 - smoothstep(-aaE, aaE, dCore);
            base = lerp(base, float3(0.95, 0.95, 0.98), ring);
            base = lerp(base, float3(0.01, 0.01, 0.015), core);
            metal = lerp(metal, 0.0, max(ring, core));
            rough = lerp(rough, 0.3, max(ring, core));
            emis += float3(1.0, 1.0, 1.0) * ring * 0.35;
        }
"""

_INJECT = '''
tile_code = swap(tile_code, '"InShimmer", "InTime", "InDir"]', '"InShimmer", "InTime", "InDir", "InEmblem"]')
tile_code = swap(tile_code, '      (scalar_param(tile, "Direction", -1.0, y=520), "InDir"),', '      (scalar_param(tile, "Direction", -1.0, y=520), "InDir"),\\n      (scalar_param(tile, "Emblem", 0.0, y=600), "InEmblem"),')
tile_code = swap(tile_code, '        float sweep = frac((p.x + p.y) * 0.25 - InTime * 0.55);', _EMBLEM_HLSL + '        float sweep = frac((p.x + p.y) * 0.25 - InTime * 0.55);')
exec(tile_code)
'''
assert "exec(tile_code)" in _src
_src = _src.replace("exec(tile_code)", _INJECT, 1)
exec(_src)
unreal.log("build_bonus_tiles.py: done")
