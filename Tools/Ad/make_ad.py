#!/usr/bin/env python3
"""Puzzle Magic Halloween ad (quick cut): a Halloween-night cold open in type, firelight and sound, a knock, the
kids at the door, then real gameplay with the Trick-or-Treat packet, narrated, and an end card.

    KOKORO_DIR=<dir with kokoro-v1.0.onnx and voices-v1.0.bin> python Tools/Ad/make_ad.py <gameplay.mp4>

<gameplay.mp4> is a PrintWindow capture of a 1280x720 game window (-demo -treatpacket -treatchoice=trick
-treatoffer=3 -tricksegment=4: a bundle bought, then the wheel); CLIP_CUTS below pick its beats. Needs numpy, Pillow, soundfile, kokoro-onnx and ffmpeg.
Output: Demo/PuzzleMagic_Halloween_Ad.mp4 (1920x1080, 30 fps).
"""
import math, os, subprocess, sys, tempfile, wave
import numpy as np
import soundfile as sf
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
W, H, FPS, SR = 1920, 1080, 30, 44100
FONTS = os.path.join(ROOT, "Content", "UI", "Fonts")
TITLE_FONT = os.path.join(FONTS, "CinzelDecorative-Black.ttf")
BODY_FONT = os.path.join(FONTS, "LilitaOne-Regular.ttf")
RAW = os.path.join(ROOT, "RawAudio")

# The game window inside the capture (client area at the window's border offset), and the beats used from it.
CLIENT_CROP = "crop=1280:720:8:31"
CLIP_CUTS = [(4.5, 8.4), (8.6, 16.6), (22.0, 28.8), (29.0, 33.0)]   # play, the packet and the treat shop, the wheel, play

# (id, voice, speed, pitch factor, text)
VO = [
    ("n_veil", "bm_george", 0.78, 0.97, "One night a year, the veil is thin."),
    ("n_fire", "bm_george", 0.80, 0.97, "The fire crackles. Your pumpkin latte is still warm."),
    ("kid_trick", "af_sky", 1.05, 1.28, "Trick or treat!"),
    ("kid_witch", "af_nicole", 0.85, 1.18, "Choose carefully."),
    ("n_tonight", "bm_george", 0.78, 0.97, "Tonight, every choice is a trick. Or a treat."),
    ("n_routes", "bm_george", 0.86, 0.97, "Point the skeleton hands, and clear a route from edge to edge."),
    ("n_packet", "bm_george", 0.86, 0.97, "Every three hundred points, someone knocks. Treat yourself, to a single sweet, or a Halloween bundle."),
    ("n_trick", "bm_george", 0.86, 0.97, "Or dare a trick, and spin the wheel of fortunes."),
    ("n_luck", "bm_george", 0.86, 0.97, "Keep your luck, or the dark wakes."),
    ("n_end", "bm_george", 0.82, 0.97, "Puzzle Magic. Trick or treat, every night."),
]


def font(path, size):
    return ImageFont.truetype(path, size)


def ease(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)


def render_vo(tmp):
    from kokoro_onnx import Kokoro
    kdir = os.environ.get("KOKORO_DIR", ".")
    k = Kokoro(os.path.join(kdir, "kokoro-v1.0.onnx"), os.path.join(kdir, "voices-v1.0.bin"))
    out = {}
    for vid, voice, speed, pitch, text in VO:
        samples, sr = k.create(text, voice=voice, speed=speed, lang="en-gb" if voice.startswith("b") else "en-us")
        raw = os.path.join(tmp, vid + ".raw.wav")
        sf.write(raw, samples, sr)
        dst = os.path.join(tmp, vid + ".wav")
        # pitch: resample-shift (kids up, narrator a touch down), tempo back; presence lift, a small room
        fx = ("asetrate=%d,aresample=%d,atempo=%.4f,highpass=f=70,equalizer=f=320:t=q:w=1.2:g=-2,"
              "equalizer=f=4200:t=q:w=1.4:g=3,acompressor=threshold=-22dB:ratio=3:attack=8:release=150:makeup=3,"
              "aecho=0.9:0.9:30|50:0.08|0.04" % (int(sr * pitch), SR, 1 / pitch))
        subprocess.check_call(["ffmpeg", "-y", "-loglevel", "error", "-i", raw, "-af", fx, "-ac", "1", "-ar", str(SR), dst])
        data, _ = sf.read(dst, dtype="float32")
        out[vid] = np.stack([data, data], 1)
    return out


def load_wav(name, start=0.0, length=None):
    cmd = ["ffmpeg", "-loglevel", "error", "-ss", str(start), "-i", os.path.join(RAW, name)]
    if length:
        cmd += ["-t", str(length)]
    cmd += ["-f", "f32le", "-ac", "2", "-ar", str(SR), "-"]
    return np.frombuffer(subprocess.check_output(cmd), np.float32).reshape(-1, 2).copy()


def synth(kind, rng):
    n = int({"knock": 0.35, "creak": 2.2, "whoosh": 1.2, "crackle": 1.0}[kind] * SR)
    t = np.arange(n) / SR
    if kind == "knock":
        s = np.sin(2 * np.pi * 95 * t) * np.exp(-t * 28) + rng.normal(0, 1, n) * np.exp(-t * 90) * 0.5
        s = np.convolve(s, np.ones(12) / 12, "same")
    elif kind == "creak":
        f = 180 + 90 * np.sin(2 * np.pi * 0.7 * t) + 30 * np.sin(2 * np.pi * 5.3 * t)
        ph = 2 * np.pi * np.cumsum(f) / SR
        pulses = (np.sin(ph) > 0.92).astype(float)
        s = np.convolve(pulses, np.exp(-np.arange(400) / 60), "same") * np.sin(np.pi * t / t[-1]) * 0.6
    elif kind == "whoosh":
        noise = rng.normal(0, 1, n)
        s = np.convolve(noise, np.ones(40) / 40, "same") * np.sin(np.pi * t / t[-1]) ** 2 * 2
    else:  # fire crackle bed (a second, looped)
        s = np.convolve(rng.normal(0, 1, n), np.ones(30) / 30, "same") * 0.25
        pops = rng.random(n) < 0.0009
        s += np.convolve(pops * rng.normal(0, 1, n), np.exp(-np.arange(300) / 40), "same") * 1.2
    s = np.asarray(s, np.float32)
    s /= np.abs(s).max() + 1e-6
    return np.stack([s, s], 1)


def place(bus, clip, at, gain=1.0):
    i = int(at * SR)
    if i >= len(bus):
        return
    m = min(len(clip), len(bus) - i)
    bus[i:i + m] += clip[:m] * gain


# ---------------- pictures ----------------
_grain = np.random.default_rng(3)


def night_frame(t, warm, captions, knock_flash=0.0, door=0.0):
    """A dark room lit by a fire below frame: flickering warm glow, drifting embers, captions."""
    yy, xx = np.mgrid[0:H:4, 0:W:4].astype(np.float32)
    flick = 0.8 + 0.12 * math.sin(t * 13.1) + 0.08 * math.sin(t * 7.3 + 1) + 0.06 * math.sin(t * 23.7)
    flick *= 1 - 0.55 * knock_flash
    d = np.sqrt(((xx - W * 0.5) / (W * 0.7)) ** 2 + ((yy - H * 1.05) / (H * 0.75)) ** 2)
    glow = np.clip(1.15 - d, 0, 1) ** 2 * flick * warm
    img = np.stack([glow * 255 * 1.0, glow * 255 * 0.42, glow * 255 * 0.12], -1)
    # moonlit blue from the left (the window), stronger as the door opens
    moon = np.clip(1 - np.sqrt(((xx - W * 0.1) / (W * 0.5)) ** 2 + ((yy - H * 0.3) / (H * 0.7)) ** 2), 0, 1) ** 2
    img += np.stack([moon * 30, moon * 45, moon * 80], -1) * (0.6 + 2.5 * door)
    if door > 0:  # a doorway of orange porch light opening in the middle
        half = W * 0.12 * door
        mask = ((np.abs(xx - W * 0.5) < half) & (yy > H * 0.12) & (yy < H * 0.98)).astype(np.float32)
        img += np.stack([mask * 255, mask * 140, mask * 40], -1) * 0.75 * door
    frame = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).resize((W, H), Image.BILINEAR)
    # embers
    d2 = ImageDraw.Draw(frame)
    for i in range(40):
        ph = (t * 0.12 + i * 0.137) % 1.0
        x = (i * 397 % W) + 40 * math.sin(t * 1.3 + i)
        y = H * (1.0 - ph * 0.9)
        a = int(255 * (1 - ph) * warm * flick)
        r = 2 + (i % 3)
        d2.ellipse((x - r, y - r, x + r, y + r), fill=(255, 150 + i % 60, 40, a))
    frame = frame.filter(ImageFilter.GaussianBlur(0.6))
    draw = ImageDraw.Draw(frame, "RGBA")
    for text, size, y, a, color, kind in captions:
        if a <= 0:
            continue
        f = font(TITLE_FONT if kind == "title" else BODY_FONT, size)
        draw.text((W / 2 + 3, y + 3), text, font=f, fill=(0, 0, 0, int(180 * a)), anchor="mm")
        draw.text((W / 2, y), text, font=f, fill=color + (int(255 * a),), anchor="mm")
    return np.asarray(frame)


def caption_alpha(t, t0, t1, fade=0.5):
    return ease((t - t0) / fade) * ease((t1 - t) / fade)


def main():
    clip_path = sys.argv[1]
    tmp = tempfile.mkdtemp(prefix="pm_ad_")
    vo = render_vo(tmp)
    vlen = {k: len(v) / SR for k, v in vo.items()}

    # ---- timeline (seconds) ----
    OPEN = 19.0                                   # cold open length
    play_len = sum(b - a for a, b in CLIP_CUTS)   # gameplay length
    END = 6.5
    total = OPEN + play_len + END
    knocks = [10.2, 10.75, 11.3]
    door_at = 12.6
    cues = [(0.6, "n_veil"), (4.2, "n_fire"), (14.1, "kid_trick"), (15.6, "kid_witch"), (16.9, "n_tonight"),
            (OPEN + 0.3, "n_routes"), (OPEN + 4.4, "n_packet"), (OPEN + 11.9, "n_trick"), (OPEN + 18.7, "n_luck"), (OPEN + play_len + 0.7, "n_end")]

    # ---- gameplay clip, cut and scaled ----
    parts = []
    for i, (a, b) in enumerate(CLIP_CUTS):
        p = os.path.join(tmp, "part%d.mp4" % i)
        subprocess.check_call(["ffmpeg", "-y", "-loglevel", "error", "-ss", str(a), "-t", str(b - a), "-i", clip_path,
                               "-vf", CLIENT_CROP + ",delogo=x=8:y=96:w=400:h=112,scale=%d:%d:flags=lanczos,fps=%d" % (W, H, FPS),
                               "-an", "-c:v", "libx264", "-crf", "12", "-preset", "veryfast", p])
        parts.append(p)
    lst = os.path.join(tmp, "parts.txt")
    open(lst, "w").write("".join("file '%s'\n" % p.replace("\\", "/") for p in parts))
    play = os.path.join(tmp, "play.mp4")
    subprocess.check_call(["ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", lst, "-c", "copy", play])
    reader = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-i", play, "-f", "rawvideo", "-pix_fmt", "rgb24", "-"], stdout=subprocess.PIPE)

    ftitle, fbody = font(TITLE_FONT, 120), font(BODY_FONT, 46)
    play_caps = [(OPEN + 0.3, OPEN + 4.0, "Point the hands. Clear a route."),
                 (OPEN + 4.4, OPEN + 11.5, "TREAT: single sweets or Halloween bundles"),
                 (OPEN + 11.9, OPEN + 18.3, "TRICK: spin the wheel of fortunes"),
                 (OPEN + 18.7, OPEN + play_len - 0.3, "Keep your luck... or the dark wakes.")]

    silent = os.path.join(tmp, "video.mp4")
    enc = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "%dx%d" % (W, H),
                            "-r", str(FPS), "-i", "-", "-c:v", "libx264", "-preset", "slow", "-crf", "25", "-pix_fmt", "yuv420p", silent],
                           stdin=subprocess.PIPE)
    last_play = np.zeros((H, W, 3), np.uint8)
    for i in range(int(total * FPS)):
        t = i / FPS
        if t < OPEN:
            knock = max([max(0.0, 1 - abs(t - k) / 0.18) for k in knocks] + [0.0])
            door = ease((t - door_at) / 1.6)
            caps = [
                ("October 31st", 54, H * 0.40, caption_alpha(t, 0.4, 4.0), (255, 190, 110), "body"),
                ("One night a year, the veil is thin.", 64, H * 0.50, caption_alpha(t, 0.8, 4.0), (240, 225, 200), "body"),
                ("The fire crackles. The latte is warm.", 60, H * 0.48, caption_alpha(t, 4.4, 9.4), (240, 225, 200), "body"),
                ("knock.   knock.   knock.", 70, H * 0.48, caption_alpha(t, 10.0, 12.4, 0.2), (255, 120, 60), "body"),
                ("“Trick or treat!”", 84, H * 0.80, caption_alpha(t, 14.0, 15.6, 0.25), (255, 200, 120), "body"),
                ("“Choose carefully...”", 64, H * 0.80, caption_alpha(t, 15.6, 16.9, 0.25), (200, 170, 255), "body"),
                ("Tonight, every choice is a trick... or a treat.", 62, H * 0.20, caption_alpha(t, 16.9, OPEN + 0.2, 0.35), (255, 170, 60), "body"),
            ]
            frame = night_frame(t, warm=1.0 - 0.5 * door, captions=caps, knock_flash=knock, door=door)
            # the dive: the doorway light swallows the frame
            if t > OPEN - 0.9:
                w = ease((t - (OPEN - 0.9)) / 0.9)
                frame = (frame * (1 - w) + np.array([255, 170, 70]) * w).astype(np.uint8)
        elif t < OPEN + play_len:
            raw = reader.stdout.read(W * H * 3)
            if len(raw) == W * H * 3:
                last_play = np.frombuffer(raw, np.uint8).reshape(H, W, 3)
            frame = last_play
            tl = t - OPEN
            if tl < 0.6:  # out of the orange flash
                w = 1 - ease(tl / 0.6)
                frame = (frame * (1 - w) + np.array([255, 170, 70]) * w).astype(np.uint8)
            im = Image.fromarray(frame)
            draw = ImageDraw.Draw(im, "RGBA")
            for a0, a1, text in play_caps:
                a = caption_alpha(t, a0, a1, 0.35)
                if a > 0:
                    tw = draw.textlength(text, font=fbody)
                    draw.rounded_rectangle((W / 2 - tw / 2 - 30, H - 132, W / 2 + tw / 2 + 30, H - 52), 18, fill=(10, 4, 18, int(200 * a)))
                    draw.text((W / 2, H - 92), text, font=fbody, fill=(255, 220, 160, int(255 * a)), anchor="mm")
            frame = np.asarray(im)
        else:
            te = t - OPEN - play_len
            caps = [("PUZZLE MAGIC", 120, H * 0.40, ease(te / 0.6), (255, 150, 40), "title"),
                    ("Your Halloween puzzle.  Trick or treat, every night.", 52, H * 0.56, ease((te - 0.6) / 0.6), (240, 225, 200), "body"),
                    ("Coming to iPhone", 40, H * 0.66, ease((te - 1.2) / 0.6), (200, 170, 255), "body")]
            frame = night_frame(t, warm=0.7, captions=caps)
            frame = (frame * ease((END - te) / 0.6)).astype(np.uint8) if te > END - 0.6 else frame
        if OPEN <= t < OPEN + play_len:
            enc.stdin.write(np.ascontiguousarray(frame, np.uint8).tobytes())
        else:   # film grain on the night scenes only (it costs bitrate on the game footage)
            g = _grain.normal(0, 2.0, (H // 2, W // 2, 1)).repeat(2, 0).repeat(2, 1)
            enc.stdin.write(np.clip(frame.astype(np.float32) + g, 0, 255).astype(np.uint8).tobytes())
        if i % 300 == 0:
            print("frame %d/%d" % (i, int(total * FPS)), flush=True)
    enc.stdin.close(); enc.wait(); reader.kill()

    # ---- audio ----
    n = int((total + 1) * SR)
    vo_bus, fx, mus = (np.zeros((n, 2), np.float32) for _ in range(3))
    for at, vid in cues:
        place(vo_bus, vo[vid], at)
    rng = np.random.default_rng(5)
    crackle = synth("crackle", rng)
    for k in range(int(OPEN)):
        place(fx, crackle, k, 0.18 * (1 - 0.6 * ease((k - door_at) / 2)))
    for k in knocks:
        place(fx, synth("knock", rng), k, 1.0)
    place(fx, synth("creak", rng), door_at - 0.2, 0.5)
    place(fx, synth("whoosh", rng), OPEN - 1.0, 0.8)
    place(fx, load_wav("SFXG_Holy.wav"), OPEN + 8.5, 0.4)
    place(fx, load_wav("SFXG_Clear.wav"), OPEN + 2.2, 0.4)
    place(fx, load_wav("SFXG_Relic.wav"), OPEN + 17.0, 0.4)
    place(fx, load_wav("SFXG_Thunder.wav"), OPEN + play_len + 0.1, 0.5)
    # music: the low drone under the cold open, the chant under the game, the organ on the end card
    drone = load_wav("AMB_Drone.wav", 0, OPEN + 1)
    drone *= np.minimum(1, np.arange(len(drone)) / (3 * SR))[:, None]
    place(mus, drone, 0, 0.5)
    place(mus, load_wav("MUS_Chant.wav", 4, play_len + 1), OPEN - 0.3, 0.9)
    place(mus, load_wav("MUS_Gothic.wav", 0, END + 1), OPEN + play_len - 0.2, 0.9)

    def rms(x):
        sel = x[np.abs(x).max(1) > 1e-4]
        return float(np.sqrt(np.mean(sel ** 2))) if len(sel) else 1.0
    vo_bus *= 0.17 / rms(vo_bus)
    mus *= (0.17 / rms(mus)) * 10 ** (-17 / 20)
    lvl = np.convolve(np.abs(vo_bus).max(1), np.ones(2205) / 2205, "same")
    duck = np.convolve((lvl > 0.01).astype(np.float32), np.ones(int(0.3 * SR)) / int(0.3 * SR), "same")
    mus *= (1 - np.clip(duck, 0, 1) * (1 - 10 ** (-7 / 20)))[:, None]
    mix = vo_bus + fx * 0.5 + mus
    mix = np.tanh(mix * 1.1) / np.tanh(1.1)
    mix *= 0.89 / np.abs(mix).max()
    wav = os.path.join(tmp, "mix.wav")
    with wave.open(wav, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((mix * 32767).astype(np.int16).tobytes())

    out_dir = os.path.join(ROOT, "Demo")
    out = os.path.join(out_dir, "PuzzleMagic_Halloween_Ad.mp4")
    subprocess.check_call(["ffmpeg", "-y", "-loglevel", "error", "-i", silent, "-i", wav, "-map", "0:v", "-map", "1:a", "-c:v", "copy",
                           "-c:a", "aac", "-b:a", "160k", "-t", "%.2f" % total, "-movflags", "+faststart", out])
    print("wrote", out, "%.1fs" % total, "%.1f MB" % (os.path.getsize(out) / 1e6))


if __name__ == "__main__":
    main()
