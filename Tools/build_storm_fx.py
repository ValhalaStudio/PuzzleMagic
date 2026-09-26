# M_FXBolt: additive lightning bolt for board strikes (a jagged channel with two forks).
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>
import unreal

exec(open(r"D:\Unreal Projects\PuzzleGame5x5\Tools\arcane_head.py", encoding="utf-8").read())

BOLT_HLSL = r"""struct BoltFns {
    float H(float n) { return frac(sin(n * 127.1) * 43758.5453); }
    float N1(float x)
    {
        float i = floor(x);
        float f = frac(x);
        float u = f * f * (3.0 - 2.0 * f);
        return lerp(H(i), H(i + 1.0), u) * 2.0 - 1.0;
    }
    // Sideways offset of a channel at height y: fractal zig-zag, sharper at small scales.
    float Path(float y, float s)
    {
        float o = 0.0;
        float a = 0.3;
        float fr = 4.0;
        for (int k = 0; k < 6; k++)
        {
            o += a * N1(y * fr + s * 17.13 + k * 31.7);
            fr *= 2.2;
            a *= 0.52;
        }
        return o;
    }
};
BoltFns B;
float y = InUV.y;
// Both ends pinned to the quad's centre line, so the quad's orientation doesn't matter.
float pin = sqrt(saturate(sin(y * 3.14159265)));
float cx = 0.5 + B.Path(y, InSeed) * pin * 0.9;
float d = abs(InUV.x - cx);
float core = exp(-d * d / 0.00006);
float glow = exp(-d / 0.035) * 0.55 + exp(-d / 0.16) * 0.18;

// Forks: leave the main channel, wander off and fade.
float fork = 0.0;
for (int k = 0; k < 2; k++)
{
    float y0 = 0.28 + 0.3 * k + 0.08 * B.H(InSeed + k * 3.1);
    float len = 0.22;
    float u = (y - y0) / len;
    if (u > 0.0 && u < 1.0)
    {
        float side = (B.H(InSeed * 1.7 + k) > 0.5) ? 1.0 : -1.0;
        float fx = 0.5 + B.Path(y0, InSeed) * sqrt(saturate(sin(y0 * 3.14159265))) * 0.9
                 + side * u * 0.22 + B.Path(y, InSeed + 7.0 + k) * 0.25 * u;
        float fd = abs(InUV.x - fx);
        fork += (exp(-fd * fd / 0.00005) + exp(-fd / 0.03) * 0.3) * (1.0 - u);
    }
}
float m = core * 2.5 + glow + fork * 0.9;
float3 col = lerp(InColor, float3(1.0, 1.0, 1.0), saturate(core * 0.8 + fork * 0.3));
return col * m * InIntensity;
"""

bolt = new_material("M_FXBolt", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_ADDITIVE)
bolt.set_editor_property("two_sided", True)
bolt_node = custom_node(bolt, BOLT_HLSL, ["InUV", "InSeed", "InColor", "InIntensity"], F3, [])
wire([(expr(bolt, unreal.MaterialExpressionTextureCoordinate, y=-150), "InUV"),
      (scalar_param(bolt, "Seed", 1.0, y=-50), "InSeed"),
      (vec_param(bolt, "Color", unreal.LinearColor(0.55, 0.7, 1.0, 1.0), y=50), "InColor"),
      (scalar_param(bolt, "Intensity", 0.0, y=200), "InIntensity")], bolt_node)
mel.connect_material_property(bolt_node, "", MP.MP_EMISSIVE_COLOR)
finish(bolt)
unreal.log("build_storm_fx.py: done")
