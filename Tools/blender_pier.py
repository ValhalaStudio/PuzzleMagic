# Gothic clustered pier for the cathedral, built and baked in Blender (bpy + Cycles):
#   low poly  : core shaft + 8 engaged shafts, moulded octagonal base, annulet ring, bell capital
#   high poly : same, subdivided and weathered (noise + Voronoi cracks) with a displace modifier
#   bake      : high -> low tangent-space normal map + ambient occlusion (Cycles; GPU via HIP if the
#               driver allows, else CPU)
#   export    : SM_GothicPier.fbx + T_Pier_N.png + T_Pier_AO.png into RawMeshes/
# Units: metres (the FBX exporter scales to Unreal centimetres). Height 16 m = the wall's 1600 units.
# Run: blender -b -P blender_pier.py
import math
import os

import bpy
import bmesh

OUT = r"D:\Unreal Projects\PuzzleGame5x5\RawMeshes"
os.makedirs(OUT, exist_ok=True)
HEIGHT = 16.0

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene


def cylinder(name, radius, depth, z, segments=32, x=0.0, y=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=segments, radius=radius, depth=depth, location=(x, y, z + depth / 2))
    obj = bpy.context.active_object
    obj.name = name
    return obj


def torus(name, major, minor, z):
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, major_segments=48, minor_segments=12, location=(0, 0, z))
    obj = bpy.context.active_object
    obj.name = name
    return obj


def cone(name, r1, r2, depth, z, segments=32):
    bpy.ops.mesh.primitive_cone_add(vertices=segments, radius1=r1, radius2=r2, depth=depth, location=(0, 0, z + depth / 2))
    obj = bpy.context.active_object
    obj.name = name
    return obj


def build_pier(name):
    parts = []
    parts.append(cylinder("core", 0.34, HEIGHT, 0.0))
    for k in range(8):
        a = k * math.tau / 8
        parts.append(cylinder("shaft%d" % k, 0.095, HEIGHT - 1.0, 0.45, 16, 0.34 * math.cos(a), 0.34 * math.sin(a)))
    # Base: octagonal plinth, a chamfered step, two torus mouldings.
    parts.append(cylinder("plinth", 0.62, 0.32, 0.0, 8))
    parts.append(cone("step", 0.6, 0.5, 0.14, 0.32, 8))
    parts.append(torus("torusA", 0.47, 0.06, 0.52))
    parts.append(torus("torusB", 0.45, 0.04, 0.66))
    # Annulet ring halfway up the visible part, and a bell capital with an abacus at the top.
    parts.append(torus("annulet", 0.45, 0.05, 3.2))
    parts.append(cone("bell", 0.46, 0.66, 0.9, HEIGHT - 1.4))
    parts.append(cylinder("abacus", 0.72, 0.28, HEIGHT - 0.5, 8))

    for p in parts:
        p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    pier = bpy.context.active_object
    pier.name = name
    # Weld the overlapping pieces into one closed surface (voxel remesh), then decimate for the game.
    return pier


low = build_pier("SM_GothicPier")
remesh = low.modifiers.new("remesh", "REMESH")
remesh.mode = "VOXEL"
remesh.voxel_size = 0.02
bpy.ops.object.modifier_apply(modifier="remesh")

# High poly: a copy, weathered.
high = low.copy()
high.data = low.data.copy()
high.name = "Pier_High"
scene.collection.objects.link(high)
tex = bpy.data.textures.new("weather", type="VORONOI")
tex.noise_scale = 0.08
tex.distance_metric = "DISTANCE"
disp = high.modifiers.new("cracks", "DISPLACE")
disp.texture = tex
disp.strength = -0.006
disp.texture_coords = "GLOBAL"
tex2 = bpy.data.textures.new("pits", type="CLOUDS")
tex2.noise_scale = 0.03
disp2 = high.modifiers.new("pits", "DISPLACE")
disp2.texture = tex2
disp2.strength = 0.004
disp2.texture_coords = "GLOBAL"
bpy.context.view_layer.objects.active = high
for m in list(high.modifiers):
    bpy.ops.object.modifier_apply(modifier=m.name)

# Low poly for the game: decimate hard, smooth-shade with sharp edges kept.
bpy.context.view_layer.objects.active = low
dec = low.modifiers.new("decimate", "DECIMATE")
dec.ratio = 0.035
bpy.ops.object.modifier_apply(modifier="decimate")
bpy.ops.object.shade_auto_smooth(angle=math.radians(40))
print("low poly faces:", len(low.data.polygons), "high poly faces:", len(high.data.polygons))

# UVs: cylindrical-ish smart projection, tall islands.
bpy.ops.object.select_all(action="DESELECT")
low.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.004)
bpy.ops.object.mode_set(mode="OBJECT")

# Bake targets.
W, H = 1024, 2048
images = {}
mat = bpy.data.materials.new("PierBake")
mat.use_nodes = True
low.data.materials.append(mat)
nodes = mat.node_tree.nodes
for kind in ("N", "AO"):
    img = bpy.data.images.new("T_Pier_" + kind, W, H, alpha=False, float_buffer=False)
    img.colorspace_settings.name = "Non-Color"
    images[kind] = img

scene.render.engine = "CYCLES"
prefs = bpy.context.preferences.addons["cycles"].preferences
try:
    prefs.compute_device_type = "HIP"
    prefs.get_devices()
    gpus = [d for d in prefs.devices if d.type == "HIP"]
    for d in prefs.devices:
        d.use = d.type == "HIP"
    scene.cycles.device = "GPU" if gpus else "CPU"
except Exception as e:  # no HIP runtime: CPU
    print("HIP unavailable:", e)
    scene.cycles.device = "CPU"
print("cycles device:", scene.cycles.device)
scene.cycles.samples = 64

def bake(kind):
    node = nodes.new("ShaderNodeTexImage")
    node.image = images[kind]
    nodes.active = node
    bpy.ops.object.select_all(action="DESELECT")
    high.select_set(True)
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    bpy.ops.object.bake(type="NORMAL" if kind == "N" else "AO", use_selected_to_active=True,
                        cage_extrusion=0.03, max_ray_distance=0.06, margin=8)
    path = os.path.join(OUT, "T_Pier_%s.png" % kind)
    images[kind].filepath_raw = path
    images[kind].file_format = "PNG"
    images[kind].save()
    print("baked", path)
    nodes.remove(node)

bake("N")
bake("AO")

# Export only the low poly. Blender Z-up metres -> FBX; Unreal converts to Z-up centimetres.
bpy.ops.object.select_all(action="DESELECT")
low.select_set(True)
bpy.context.view_layer.objects.active = low
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "SM_GothicPier.fbx"), use_selection=True, apply_unit_scale=True,
                         apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE", use_tspace=True, bake_space_transform=True)
print("exported", os.path.join(OUT, "SM_GothicPier.fbx"))
