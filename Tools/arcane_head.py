import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
eal = unreal.EditorAssetLibrary
F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

FOLDER = "/Game/Materials"


def new_material(name, shading=unreal.MaterialShadingModel.MSM_DEFAULT_LIT, blend=unreal.BlendMode.BLEND_OPAQUE):
    # Rebuild in place when the asset exists: deleting fails while code (a CDO) still references it.
    path = f"{FOLDER}/{name}"
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", shading)
    mat.set_editor_property("blend_mode", blend)
    return mat


def custom_node(mat, code, inputs, out_type, extra_outputs, x=-300, y=0):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", out_type)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    node.set_editor_property("inputs", pins)
    outs = []
    for name, kind in extra_outputs:
        out = unreal.CustomOutput()
        out.set_editor_property("output_name", name)
        out.set_editor_property("output_type", kind)
        outs.append(out)
    if outs:
        node.set_editor_property("additional_outputs", outs)
    return node


def vec_param(mat, name, value, x=-800, y=0):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def scalar_param(mat, name, value, x=-800, y=0):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def const(mat, value, x=-800, y=0):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def expr(mat, cls, x=-800, y=0):
    return mel.create_material_expression(mat, cls, x, y)


def wire(pairs, target):
    for src, pin in pairs:
        mel.connect_material_expressions(src, "", target, pin)


def finish(mat):
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    unreal.log(f"build_arcane_assets.py: built {mat.get_name()}")


NOISE = r"""
    float Hash(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
    float2 Hash2(float2 p) { return frac(sin(float2(dot(p, float2(127.1, 311.7)), dot(p, float2(269.5, 183.3)))) * 43758.5453); }
    float Noise(float2 p)
    {
        float2 i = floor(p);
        float2 f = frac(p);
        float2 u = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(Hash(i), Hash(i + float2(1.0, 0.0)), u.x), lerp(Hash(i + float2(0.0, 1.0)), Hash(i + float2(1.0, 1.0)), u.x), u.y);
    }
    float Fbm(float2 p)
    {
        float v = 0.0;
        float a = 0.5;
        for (int k = 0; k < 4; k++) { v += a * Noise(p); p *= 2.03; a *= 0.5; }
        return v;
    }
"""

SYMBOLS = r"""
    float Dot2(float2 v) { return dot(v, v); }
    float Heart(float2 p)
    {
        p.x = abs(p.x);
        if (p.y + p.x > 1.0) return sqrt(Dot2(p - float2(0.25, 0.75))) - 0.35355339;
        return sqrt(min(Dot2(p - float2(0.0, 1.0)), Dot2(p - 0.5 * max(p.x + p.y, 0.0)))) * sign(p.x - p.y);
    }
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
    float Vesica(float2 p, float r, float d)
    {
        p = abs(p);
        float b = sqrt(r * r - d * d);
        return ((p.y - b) * d > p.x * b) ? length(p - float2(0.0, b)) : length(p - float2(-d, 0.0)) - r;
    }
    float Rhombus(float2 p, float2 b)
    {
        p = abs(p);
        float h = clamp((b.x * (b.x - 2.0 * p.x) - b.y * (b.y - 2.0 * p.y)) / dot(b, b), -1.0, 1.0);
        float d = length(p - 0.5 * b * float2(1.0 - h, 1.0 + h));
        return d * sign(p.x * b.y + p.y * b.x - b.x * b.y);
    }
    float Segment(float2 p, float2 a, float2 b)
    {
        float2 pa = p - a;
        float2 ba = b - a;
        float h = saturate(dot(pa, ba) / dot(ba, ba));
        return length(pa - ba * h);
    }
    float Symbol(float2 p, float id, out float detail)
    {
        detail = 1000.0;
        if (id < 0.5) { return Heart(p * 1.05 + float2(0.0, 0.56)) / 1.05; }
        if (id < 1.5)
        {
            float c = 0.70710678;
            float2 q = float2(c * p.x - c * p.y, c * p.x + c * p.y);
            detail = Segment(q, float2(0.0, -0.52), float2(0.0, 0.42));
            return Vesica(q, 0.78, 0.44);
        }
        if (id < 2.5) { return Moon(p + float2(0.08, 0.0), 0.34, 0.6, 0.5); }
        if (id < 3.5) { return Star5(p + float2(0.0, 0.04), 0.62, 0.45) - 0.05; }
        detail = abs(Rhombus(p, float2(0.28, 0.4)));
        return Rhombus(p, float2(0.5, 0.66));
    }
"""

