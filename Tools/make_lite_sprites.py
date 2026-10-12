# Draws the 2D sprite sheets of the lightweight (mobile) mode, into Tools/LiteArt/:
#   T_LiteBat.png   - 4 wing-flap frames of a bat silhouette, in a row (1024 x 256)
#   T_LiteSteam.png - 16 frames of drifting steam that loop seamlessly, 4 x 4 (512 x 512)
# Tools/build_lite_assets.py imports them into the project.
# Run: python Tools/make_lite_sprites.py
import math
import os

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "LiteArt")
os.makedirs(OUT, exist_ok=True)

# ---------------------------------------------------------------------------
# Bat: body, head with ears, two wings. Four poses: up, middle, down, middle.
# ---------------------------------------------------------------------------
FRAME = 256
SS = 4  # supersampling for clean edges


def wing(draw, sign, lift):
    """One wing: a fixed membrane shape (leading edge, then a scalloped trailing edge) swung up or down about the shoulder."""
    cx, cy = FRAME * SS / 2, FRAME * SS * 0.52
    s = FRAME * SS / 256.0
    shoulder = (8.0, -10.0)
    outline = [shoulder, (70, -16), (122, -4), (106, 24), (88, 10), (68, 30), (48, 12), (26, 32), (8, 22)]
    a = lift * 0.62  # radians, positive = wing raised
    ca, sa = math.cos(a), math.sin(a)
    pts = []
    for x, y in outline:
        dx, dy = x - shoulder[0], y - shoulder[1]
        rx = dx * ca + dy * sa
        ry = -dx * sa + dy * ca
        pts.append((sign * (shoulder[0] + rx), shoulder[1] + ry))
    draw.polygon([(cx + x * s, cy + y * s) for x, y in pts], fill=255)


def bat_frame(lift):
    img = Image.new("L", (FRAME * SS, FRAME * SS), 0)
    d = ImageDraw.Draw(img)
    s = SS
    cx, cy = FRAME * s / 2, FRAME * s * 0.52
    for sign in (-1, 1):
        wing(d, sign, lift)
    # body, head, ears
    d.ellipse([cx - 11 * s, cy - 22 * s, cx + 11 * s, cy + 30 * s], fill=255)
    d.ellipse([cx - 11 * s, cy - 38 * s, cx + 11 * s, cy - 14 * s], fill=255)
    for sign in (-1, 1):
        d.polygon([(cx + sign * 4 * s, cy - 34 * s), (cx + sign * 12 * s, cy - 54 * s), (cx + sign * 13 * s, cy - 30 * s)], fill=255)
    img = img.resize((FRAME, FRAME), Image.LANCZOS)
    return img


def bat_sheet():
    sheet = Image.new("RGBA", (FRAME * 4, FRAME), (0, 0, 0, 0))
    for i, lift in enumerate([1.0, 0.25, -1.0, 0.25]):
        alpha = bat_frame(lift)
        colour = Image.new("RGBA", (FRAME, FRAME), (10, 6, 16, 255))
        colour.putalpha(alpha)
        sheet.paste(colour, (i * FRAME, 0))
    sheet.save(os.path.join(OUT, "T_LiteBat.png"))


# ---------------------------------------------------------------------------
# Steam: soft drifting smoke. Each frame mixes two noise fields with cos/sin of its phase, so frame 16 flows
# straight back into frame 1.
# ---------------------------------------------------------------------------
STEAM_FRAME = 128
FRAMES = 16
rng = np.random.default_rng(7)


def noise_field(size=STEAM_FRAME):
    out = np.zeros((size, size), dtype=np.float32)
    for octave, (cells, weight) in enumerate([(4, 0.55), (8, 0.3), (16, 0.15)]):
        grid = rng.random((cells, cells)).astype(np.float32)
        img = Image.fromarray((grid * 255).astype(np.uint8)).resize((size, size), Image.BICUBIC)
        out += weight * np.asarray(img, dtype=np.float32) / 255.0
    return out


def steam_sheet():
    A, B = noise_field(), noise_field()
    yy, xx = np.mgrid[0:STEAM_FRAME, 0:STEAM_FRAME].astype(np.float32)
    u = (xx / (STEAM_FRAME - 1)) * 2 - 1
    v = (yy / (STEAM_FRAME - 1)) * 2 - 1
    # a soft blob, wider low down and thinning upward, fading at the edges
    radial = np.clip(1.0 - np.sqrt((u / (0.85 - 0.25 * (-v))) ** 2 * 1.0 + (v * 0.95) ** 2), 0, 1)
    radial = radial ** 1.6
    sheet = Image.new("RGBA", (STEAM_FRAME * 4, STEAM_FRAME * 4), (0, 0, 0, 0))
    for f in range(FRAMES):
        phase = 2 * math.pi * f / FRAMES
        field = 0.5 + 0.5 * (math.cos(phase) * (A - 0.5) + math.sin(phase) * (B - 0.5)) * 2.2
        alpha = np.clip(radial * (0.35 + 1.05 * field), 0, 1)
        alpha = np.clip(alpha * 0.9, 0, 1)
        a_img = Image.fromarray((alpha * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.6))
        rgb = Image.new("RGBA", (STEAM_FRAME, STEAM_FRAME), (205, 255, 175, 255))
        rgb.putalpha(a_img)
        sheet.paste(rgb, ((f % 4) * STEAM_FRAME, (f // 4) * STEAM_FRAME))
    sheet.save(os.path.join(OUT, "T_LiteSteam.png"))


if __name__ == "__main__":
    bat_sheet()
    steam_sheet()
    print("wrote", os.listdir(OUT))
