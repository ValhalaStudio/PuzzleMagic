# Imports and builds the "all options" assets (run after gen_textures.py, blender_pier.py,
# cathedral_ir.py and the audio scripts):
#   /Game/Meshes/SM_GothicPier (+ T_Pier_N, T_Pier_AO)      Blender-baked clustered pier
#   /Game/Textures/T_PillarStone, T_FloorSlab (+ _N)          ComfyUI seamless textures + DeepBump normals
#   /Game/Materials/M_PierStone, M_FloorSlab                  triplanar stone, damp reflective slabs
#   /Game/Audio/SM_Cathedral                                  submix for the runtime convolution reverb; SFXG_* send to it
#   /Game/Audio/AMB_* stems + MS_Abyss                        MetaSound: 4 looping stems x gain inputs,
#                                                             summed, through a ladder filter ("Cutoff")
import os

import unreal

exec(open(r"D:\Unreal Projects\PuzzleGame5x5\Tools\arcane_head.py", encoding="utf-8").read())

PROJ = r"D:\Unreal Projects\PuzzleGame5x5"


def import_file(filename, dest, name, factory=None, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    if factory:
        task.set_editor_property("factory", factory)
    if options:
        task.set_editor_property("options", options)
    asset_tools.import_asset_tasks([task])
    path = "%s/%s" % (dest, name)
    ok = eal.does_asset_exist(path)
    unreal.log("allopts: import %s -> %s" % (path, "OK" if ok else "FAILED"))
    return eal.load_asset(path) if ok else None


def texture(filename, name, normal=False, linear=False):
    tex = import_file(filename, "/Game/Textures", name)
    if tex:
        if normal:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
        elif linear:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property("srgb", False)
        eal.save_loaded_asset(tex)
    return tex


# ------------------------------------------------------------------ textures
T = {}
for name in ("T_PillarStone", "T_FloorSlab", "T_WallStone"):
    color = os.path.join(PROJ, "RawTextures", name + ".png")
    if os.path.exists(color):
        T[name] = texture(color, name)
        T[name + "_N"] = texture(os.path.join(PROJ, "RawTextures", name + "_N.png"), name + "_N", normal=True)
T["T_Pier_N"] = texture(os.path.join(PROJ, "RawMeshes", "T_Pier_N.png"), "T_Pier_N", normal=True)
T["T_Pier_AO"] = texture(os.path.join(PROJ, "RawMeshes", "T_Pier_AO.png"), "T_Pier_AO", linear=True)

# ------------------------------------------------------------------ pier mesh
pier = import_file(os.path.join(PROJ, "RawMeshes", "SM_GothicPier.fbx"), "/Game/Meshes", "SM_GothicPier")

# ------------------------------------------------------------------ materials
def tex_object(mat, tex, y):
    node = expr(mat, unreal.MaterialExpressionTextureObjectParameter, y=y)
    node.set_editor_property("parameter_name", tex.get_name())
    node.set_editor_property("texture", tex)
    return node


def tex_sample(mat, tex, y, sampler=None):
    node = expr(mat, unreal.MaterialExpressionTextureSample, y=y)
    node.set_editor_property("texture", tex)
    if sampler:
        node.set_editor_property("sampler_type", sampler)
    return node


TRIPLANAR = r"""
float3 w = pow(abs(InNormal), 4.0);
w /= (w.x + w.y + w.z + 1e-4);
float s = 1.0 / InTile;
float3 cx = Texture2DSample(InAlbedo, InAlbedoSampler, InWorldPos.yz * s).rgb;
float3 cy = Texture2DSample(InAlbedo, InAlbedoSampler, InWorldPos.xz * s).rgb;
float3 cz = Texture2DSample(InAlbedo, InAlbedoSampler, InWorldPos.xy * s).rgb;
return (cx * w.x + cy * w.y + cz * w.z) * InTint;
"""

if pier and "T_PillarStone" in T:
    m = new_material("M_PierStone")
    node = custom_node(m, TRIPLANAR, ["InAlbedo", "InWorldPos", "InNormal", "InTile", "InTint"], F3, [])
    wire([(tex_object(m, T["T_PillarStone"], -300), "InAlbedo"),
          (expr(m, unreal.MaterialExpressionWorldPosition, y=-200), "InWorldPos"),
          (expr(m, unreal.MaterialExpressionVertexNormalWS, y=-100), "InNormal"),
          (scalar_param(m, "Tile", 170.0, y=0), "InTile"),
          (vec_param(m, "Tint", unreal.LinearColor(0.42, 0.4, 0.43, 1.0), y=100), "InTint")], node)
    ao = tex_sample(m, T["T_Pier_AO"], 250, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    nrm = tex_sample(m, T["T_Pier_N"], 400, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, 100, -100)
    mel.connect_material_expressions(node, "", mul, "A")
    mel.connect_material_expressions(ao, "R", mul, "B")
    mel.connect_material_property(mul, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(ao, "R", MP.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(nrm, "RGB", MP.MP_NORMAL)
    mel.connect_material_property(const(m, 0.78, y=500), "", MP.MP_ROUGHNESS)
    finish(m)
    pier.set_material(0, m)
    eal.save_loaded_asset(pier)

if "T_FloorSlab" in T:
    m = new_material("M_FloorSlab")
    uv = custom_node(m, "return InWorldPos.xy / InTile;", ["InWorldPos", "InTile"], unreal.CustomMaterialOutputType.CMOT_FLOAT2, [], y=-300)
    wire([(expr(m, unreal.MaterialExpressionWorldPosition, y=-400), "InWorldPos"),
          (scalar_param(m, "Tile", 320.0, y=-300), "InTile")], uv)
    col = tex_sample(m, T["T_FloorSlab"], -100)
    nrm = tex_sample(m, T["T_FloorSlab_N"], 100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(uv, "", col, "UVs")
    mel.connect_material_expressions(uv, "", nrm, "UVs")
    tint = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, 100, -100)
    mel.connect_material_expressions(col, "RGB", tint, "A")
    mel.connect_material_expressions(vec_param(m, "Tint", unreal.LinearColor(0.35, 0.33, 0.36, 1.0), y=0), "", tint, "B")
    mel.connect_material_property(tint, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(nrm, "RGB", MP.MP_NORMAL)
    # Damp stone: dark areas wetter (glossier), so candles and lightning reflect in the slabs.
    rough = custom_node(m, "return lerp(0.22, 0.7, saturate(dot(InC, float3(0.33,0.33,0.33)) * 2.2));", ["InC"], F1, [], y=300)
    mel.connect_material_expressions(col, "RGB", rough, "InC")
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    finish(m)

# ------------------------------------------------------------------ convolution reverb submix
# The impulse response and the convolution effect are created at runtime by AGothicEnvironment
# (importing a .wav through AudioImpulseResponseFactory crashes in commandlet mode); this only makes
# the submix they attach to and routes the SFX into it.
submix_path = "/Game/Audio/SM_Cathedral"
submix = eal.load_asset(submix_path) if eal.does_asset_exist(submix_path) else asset_tools.create_asset("SM_Cathedral", "/Game/Audio", unreal.SoundSubmix, unreal.SoundSubmixFactory())
unreal.log("allopts: submix %s" % submix)
if submix:
    eal.save_loaded_asset(submix)
    # Every gameplay SFX sends a share of itself into the cathedral (the music has its own reverb).
    for asset_path in eal.list_assets("/Game/Audio", recursive=False):
        name = asset_path.split("/")[-1].split(".")[0]
        if not name.startswith("SFXG_"):
            continue
        wave = eal.load_asset(asset_path)
        send = unreal.SoundSubmixSendInfo()
        send.set_editor_property("sound_submix", submix)
        send.set_editor_property("send_level", 0.3)
        wave.set_editor_property("enable_submix_sends", True)
        wave.set_editor_property("sound_submix_sends", [send])
        eal.save_loaded_asset(wave)
        unreal.log("allopts: send %s -> SM_Cathedral" % name)

# ------------------------------------------------------------------ MetaSound: tension ambience
stems = ["Drone", "Shepard", "Whisper", "Air"]
waves = {}
for stem in stems:
    wave = import_file(os.path.join(PROJ, "RawAudio", "AMB_%s.wav" % stem), "/Game/Audio", "AMB_" + stem, unreal.SoundFactory())
    if wave:
        wave.set_editor_property("looping", True)
        eal.save_loaded_asset(wave)
        waves[stem] = wave

# Built once: the graph only references the stem assets, so re-importing stems needs no rebuild
# (and build_to_asset can't overwrite an existing MetaSound; delete MS_Abyss by hand to rebuild).
if len(waves) == len(stems) and not eal.does_asset_exist("/Game/Audio/MS_Abyss"):
    sub = unreal.get_engine_subsystem(unreal.MetaSoundBuilderSubsystem)
    builder, on_play, on_finished, audio_outs, result = sub.create_source_builder(
        "MS_AbyssBuilder", output_format=unreal.MetaSoundOutputAudioFormat.STEREO, is_one_shot=False)

    def node(ns, name, variant):
        handle, res = builder.add_node_by_class_name(unreal.MetasoundFrontendClassName(namespace=ns, name=name, variant=variant), 1)
        assert res == unreal.MetaSoundBuilderResult.SUCCEEDED, (name, variant, res)
        return handle

    def pin_in(n, name):
        handle, res = builder.find_node_input_by_name(n, name)
        assert res == unreal.MetaSoundBuilderResult.SUCCEEDED, name
        return handle

    def pin_out(n, name):
        handle, res = builder.find_node_output_by_name(n, name)
        assert res == unreal.MetaSoundBuilderResult.SUCCEEDED, name
        return handle

    def connect(out_handle, in_handle):
        res = builder.connect_nodes(out_handle, in_handle)
        assert res == unreal.MetaSoundBuilderResult.SUCCEEDED, res

    def set_default(in_handle, literal):
        lit = literal[0] if isinstance(literal, tuple) else literal
        builder.set_node_input_default(in_handle, lit)

    def graph_float(name, default):
        lit = sub.create_float_meta_sound_literal(default)
        lit = lit[0] if isinstance(lit, tuple) else lit
        handle, res = builder.add_graph_input_node(name, "Float", lit, False)
        assert res == unreal.MetaSoundBuilderResult.SUCCEEDED, name
        return handle

    gains = {stem: graph_float(stem, 1.0 if stem == "Air" else 0.0) for stem in stems}
    cutoff = graph_float("Cutoff", 16000.0)

    channel_sums = {"Left": None, "Right": None}
    for stem in stems:
        player = node("UE", "Wave Player", "Stereo")
        set_default(pin_in(player, "Wave Asset"), sub.create_object_meta_sound_literal(waves[stem]))
        set_default(pin_in(player, "Loop"), sub.create_bool_meta_sound_literal(True))
        connect(on_play, pin_in(player, "Play"))
        for side in ("Left", "Right"):
            scaled = node("UE", "Multiply", "Audio by Float")
            connect(pin_out(player, "Out " + side), pin_in(scaled, "PrimaryOperand"))
            connect(gains[stem], pin_in(scaled, "AdditionalOperands"))
            if channel_sums[side] is None:
                channel_sums[side] = pin_out(scaled, "Out")
            else:
                adder = node("UE", "Add", "Audio")
                ins = builder.find_node_inputs(adder)
                ins = ins[0] if isinstance(ins, tuple) else ins
                connect(channel_sums[side], ins[0])
                connect(pin_out(scaled, "Out"), ins[1])
                outs = builder.find_node_outputs(adder)
                outs = outs[0] if isinstance(outs, tuple) else outs
                channel_sums[side] = outs[0]

    for index, side in enumerate(("Left", "Right")):
        filt = node("UE", "Ladder Filter", "Audio")
        connect(channel_sums[side], pin_in(filt, "In"))
        connect(cutoff, pin_in(filt, "Cutoff Frequency"))
        set_default(pin_in(filt, "Resonance"), sub.create_float_meta_sound_literal(1.2))
        connect(pin_out(filt, "Out"), audio_outs[index])

    editor = unreal.get_editor_subsystem(unreal.MetaSoundEditorSubsystem)
    doc, res = editor.build_to_asset(builder, "PuzzleGame5x5", "MS_Abyss", "/Game/Audio")
    unreal.log("allopts: MetaSound MS_Abyss -> %s" % res)
    eal.save_asset("/Game/Audio/MS_Abyss")

unreal.log("allopts: done")
