# Seamless PBR-ish stone textures for the cathedral: Stable Diffusion 1.5 (DreamShaper 8) through the
# local ComfyUI API with ComfyUI-seamless-tiling (circular padding on the UNet and VAE, so the images
# tile), then DeepBump (ONNX, CPU) derives a normal map from each colour image.
# Needs ComfyUI running on 127.0.0.1:8188 with the ComfyUI-seamless-tiling node allowed.
# Run with the rocm-env python.
import json
import os
import shutil
import subprocess
import sys
import time
import urllib.request

COMFY = "http://127.0.0.1:8188"
COMFY_OUT = r"C:\Users\VagDi\ComfyUI\output"
OUT = r"D:\Unreal Projects\PuzzleGame5x5\RawTextures"
DEEPBUMP = r"D:\UEDeps\DeepBump"
NEGATIVE = "text, watermark, signature, people, face, perspective, vanishing point, shadow of camera, frame, border, blurry, lowres"

TEXTURES = {
    "T_PillarStone": "weathered gothic cathedral limestone surface, carved vertical fluting, fine cracks, lichen, grey-brown, "
                     "flat orthographic seamless texture, highly detailed, photographic, even lighting",
    "T_WallStone": "ancient dark cathedral stone blocks, rough ashlar masonry, mortar joints, damp stains, moss in cracks, "
                   "flat orthographic seamless texture, highly detailed, photographic, even lighting",
    "T_FloorSlab": "worn dark stone floor slabs of an old cathedral, cracked flagstones, grime, faint carved runes, "
                   "flat top-down orthographic seamless texture, highly detailed, photographic, even lighting",
}


def post(path, payload):
    req = urllib.request.Request(COMFY + path, data=json.dumps(payload).encode(), headers={"Content-Type": "application/json"})
    return json.loads(urllib.request.urlopen(req).read())


def get(path):
    return json.loads(urllib.request.urlopen(COMFY + path).read())


def workflow(prompt, prefix, seed):
    return {
        "1": {"class_type": "CheckpointLoaderSimple", "inputs": {"ckpt_name": "DreamShaper_8_pruned.safetensors"}},
        "2": {"class_type": "SeamlessTile", "inputs": {"model": ["1", 0], "tiling": "enable", "copy_model": "Make a copy"}},
        "3": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["1", 1], "text": prompt}},
        "4": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["1", 1], "text": NEGATIVE}},
        "5": {"class_type": "EmptyLatentImage", "inputs": {"width": 512, "height": 512, "batch_size": 1}},
        "6": {"class_type": "KSampler", "inputs": {"model": ["2", 0], "positive": ["3", 0], "negative": ["4", 0], "latent_image": ["5", 0],
                                                   "seed": seed, "steps": 28, "cfg": 6.5, "sampler_name": "dpmpp_2m",
                                                   "scheduler": "karras", "denoise": 1.0}},
        "7": {"class_type": "CircularVAEDecode", "inputs": {"samples": ["6", 0], "vae": ["1", 2], "tiling": "enable"}},
        "8": {"class_type": "SaveImage", "inputs": {"images": ["7", 0], "filename_prefix": prefix}},
    }


def generate(name, prompt, seed):
    prompt_id = post("/prompt", {"prompt": workflow(prompt, "puzzle_" + name, seed)})["prompt_id"]
    while True:
        history = get("/history/" + prompt_id)
        if prompt_id in history:
            break
        time.sleep(2)
    images = history[prompt_id]["outputs"]["8"]["images"]
    src = os.path.join(COMFY_OUT, images[0].get("subfolder", ""), images[0]["filename"])
    dst = os.path.join(OUT, name + ".png")
    shutil.copy(src, dst)
    print("generated", dst)
    return dst


# DeepBump prints arrows; the Windows console codepage can't encode them.
DEEPBUMP_ENV = dict(os.environ, PYTHONIOENCODING="utf-8")


def upscale(color_png):
    """512 -> 1024 with DeepBump's upscaler (seamless-aware tiling keeps the wrap)."""
    subprocess.run([sys.executable, os.path.join(DEEPBUMP, "cli.py"), color_png, color_png, "lowres_to_highres",
                    "--lowres_to_highres-scale_factor", "x2"], check=True, cwd=DEEPBUMP, env=DEEPBUMP_ENV)
    print("upscaled", color_png)


def normal_map(color_png, name):
    dst = os.path.join(OUT, name + "_N.png")
    subprocess.run([sys.executable, os.path.join(DEEPBUMP, "cli.py"), color_png, dst, "color_to_normals", "--color_to_normals-overlap", "LARGE"],
                   check=True, cwd=DEEPBUMP, env=DEEPBUMP_ENV)
    print("normals", dst)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for index, (name, prompt) in enumerate(TEXTURES.items()):
        png = os.path.join(OUT, name + ".png")
        if not os.path.exists(png):  # resume: keep textures already generated and upscaled
            png = generate(name, prompt, 1928 + index)
            upscale(png)
        if not os.path.exists(os.path.join(OUT, name + "_N.png")):
            normal_map(png, name)
