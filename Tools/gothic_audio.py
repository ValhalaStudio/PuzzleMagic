# Synthesizes the gothic soundtrack + horror SFX (numpy; run with the rocm-env python).
# Everything is additive/noise synthesis with an FFT "cathedral" reverb; the music loop is
# built with wrap-around note placement and circular reverb so it loops seamlessly.
import os
import wave
import numpy as np

import audio_fx

SR = 44100
OUT = r"D:\Unreal Projects\PuzzleGame5x5\RawAudio"
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(1666)


def T(dur):
    return np.arange(int(SR * dur)) / SR


def norm(x):
    return x / max(np.abs(x).max(), 1e-9)


def write(name, x, peak=0.89, loop=False, scale=None):
    if x.ndim == 1:
        x = np.stack([x, x], 1)
    if scale is not None:
        # Fixed gain (stems that must keep their balance against each other): no normalizing.
        x = np.clip(x * scale, -1.0, 1.0)
        with wave.open(os.path.join(OUT, name), "wb") as f:
            f.setnchannels(2)
            f.setsampwidth(2)
            f.setframerate(SR)
            f.writeframes((x * 32767).astype("<i2").tobytes())
        print("wrote", name, round(len(x) / SR, 2), "s (fixed gain)")
        return
    x = audio_fx.master(norm(x) * 0.7, loud=loop)
    if not loop:
        # Trim the silent tail (the recorded church rings on for seconds below audibility).
        level = np.abs(x).max(axis=1)
        audible = np.nonzero(level > 10 ** (-66 / 20) * level.max())[0]
        if len(audible):
            end = min(len(x), audible[-1] + int(0.05 * SR))
            x = x[:end] * np.clip((end - np.arange(end))[:, None] / (0.05 * SR), 0, 1)
    x = norm(x) * peak
    with wave.open(os.path.join(OUT, name), "wb") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(SR)
        f.writeframes((x * 32767).astype("<i2").tobytes())
    print("wrote", name, round(len(x) / SR, 2), "s")


def adsr(t, dur, a=0.01, d=1.0, s=0.8, r=0.3):
    e = np.where(t < a, t / a, s + (1 - s) * np.exp(-(t - a) / d))
    return e * np.clip((dur - t) / r, 0, 1)


def spectral(x, gain_fn):
    """Zero-phase FFT filter (circular, so filtered loops stay seamless)."""
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    return np.fft.irfft(X * gain_fn(np.maximum(f, 1e-3)), len(x))


def lowpass(x, fc):
    return spectral(x, lambda f: 1 / (1 + (f / fc) ** 4))


def bandpass(x, lo, hi):
    return spectral(x, lambda f: 1 / (1 + (lo / f) ** 4) / (1 + (f / hi) ** 4))


def noise(dur):
    return rng.standard_normal(int(SR * dur))


# ---------------------------------------------------------------- instruments
def organ(f, dur, amp=1.0, a=0.12, r=0.6):
    t = T(dur)
    s = np.zeros_like(t)
    for mult, w in [(0.5, 0.55), (1, 1.0), (2, 0.5), (3, 0.28), (4, 0.25), (6, 0.1), (8, 0.08)]:
        if f * mult > 12000:
            continue
        for det in (-0.0012, 0.0012):  # two ranks, slightly out of tune: the "celeste" shimmer
            s += w * np.sin(2 * np.pi * f * mult * (1 + det) * t + rng.uniform(0, 6.3))
    s *= 1 + 0.05 * np.sin(2 * np.pi * 5.3 * t)  # tremulant
    return s * adsr(t, dur, a, 1.5, 0.85, r) * amp


AH = [(710, 110, 1.0), (1150, 130, 0.55), (2650, 170, 0.28), (3300, 220, 0.12)]


def formant(freq):
    return 0.015 + sum(a * np.exp(-0.5 * ((freq - fc) / bw) ** 2) for fc, bw, a in AH)


def choir(f, dur, amp=1.0, a=0.6, r=0.8, voices=4):
    t = T(dur)
    s = np.zeros_like(t)
    for _ in range(voices):
        det = 1 + rng.uniform(-0.007, 0.007)
        vib = 1 + 0.0045 * np.sin(2 * np.pi * rng.uniform(4.6, 5.6) * t + rng.uniform(0, 6.3))
        ph = 2 * np.pi * f * det * np.cumsum(vib) / SR
        for n in range(1, int(5200 / f) + 1):
            s += formant(n * f) / n * np.sin(n * ph)
    breath = bandpass(noise(dur), 900, 3500) * 0.02
    return (s / voices + breath) * adsr(t, dur, a, 2.0, 0.9, r) * amp


def bell(f, dur, amp=1.0):
    if audio_fx.HAVE_FAUST:
        # Faust's modal church-bell model, plus the same bright strike transient.
        t = T(dur)
        strike = bandpass(noise(dur), 1500, 8000) * np.exp(-t * 80) * 0.3
        return (norm(audio_fx.faust_bell(f, dur)) + strike) * amp
    t = T(dur)
    s = np.zeros_like(t)
    # Church-bell partials: hum, prime, minor-third tierce (the mournful one), quint, nominal...
    for ratio, a, k in [(0.5, 0.9, 0.35), (1.0, 0.8, 0.6), (1.19, 0.65, 0.8), (1.5, 0.45, 1.0),
                        (2.0, 0.7, 1.2), (2.52, 0.3, 1.8), (3.0, 0.25, 2.2), (4.07, 0.15, 3.0), (5.43, 0.1, 4.0)]:
        s += a * np.sin(2 * np.pi * f * ratio * t + rng.uniform(0, 6.3)) * np.exp(-t * k * 3.0 / max(dur, 1.0))
    strike = bandpass(noise(dur), 1500, 8000) * np.exp(-t * 80) * 0.3
    return (s + strike) * np.clip(t / 0.003, 0, 1) * amp


def musicbox(f, dur=1.6, amp=1.0):
    t = T(dur)
    s = np.zeros_like(t)
    for det in (-0.004, 0.004):  # detuned pair -> the unsettling warble
        s += np.sin(2 * np.pi * f * (1 + det) * t) + 0.25 * np.sin(2 * np.pi * f * 4.01 * t) * np.exp(-t * 9)
    return s * np.exp(-t * 3.2) * np.clip(t / 0.002, 0, 1) * amp


def thunder(dur=6.0):
    t = T(dur)
    n = len(t)
    brown = np.cumsum(rng.standard_normal(n))
    rumble = norm(bandpass(brown, 25, 170))
    roll = np.zeros(n)
    for _ in range(7):
        c, w = rng.uniform(0.1, dur * 0.65), rng.uniform(0.15, 0.7)
        roll += rng.uniform(0.4, 1.0) * np.exp(-0.5 * ((t - c) / w) ** 2)
    crack = norm(bandpass(noise(dur), 400, 5000)) * np.exp(-t * 7)
    s = rumble * (0.35 + roll) * np.exp(-t * 0.45) + 0.4 * crack
    return s * np.clip(t / 0.01, 0, 1) * np.clip((dur - t) / 1.2, 0, 1)


def stone_hit(dur=0.5, pitch=1.0, weight=0.0):
    t = T(dur)
    f0 = 85 * pitch * (1 + 1.8 * np.exp(-t * 35))
    thump = np.sin(2 * np.pi * np.cumsum(f0) / SR) * np.exp(-t * (16 - 9 * weight))
    click = norm(bandpass(noise(dur), 900, 7000)) * np.exp(-t * 70)
    grit = norm(bandpass(noise(dur), 250, 2500)) * np.exp(-t * (12 - 6 * weight))
    return thump + 0.45 * click + 0.35 * grit


def reverb(x, seconds=3.2, wet=0.35, circular=False, tone=4500):
    if audio_fx.HAVE_IR:
        # A recorded church instead of the noise model (its own ~8 s tail; seconds/tone unused).
        return audio_fx.church_reverb(x, wet=min(wet * 1.3, 0.8), circular=circular)
    if x.ndim == 1:
        x = np.stack([x, x], 1)
    n_ir = int(SR * seconds)
    tt = np.arange(n_ir) / SR
    L = len(x) if circular else len(x) + n_ir
    out = np.zeros((L, 2))
    for ch in range(2):
        ir = lowpass(rng.standard_normal(n_ir), tone) * np.exp(-tt * 6.9 / seconds)
        ir[: int(0.03 * SR)] = 0.0  # pre-delay: a big stone room
        ir /= np.sqrt(np.sum(ir ** 2))
        if circular:
            irp = np.zeros(L)
            irp[: min(L, n_ir)] = ir[: min(L, n_ir)]
            out[:, ch] = np.fft.irfft(np.fft.rfft(x[:, ch]) * np.fft.rfft(irp), L)
        else:
            out[:, ch] = np.fft.irfft(np.fft.rfft(x[:, ch], L) * np.fft.rfft(ir, L), L)
    dry = np.zeros_like(out)
    dry[: len(x)] = x
    return dry * (1 - wet) + out * wet * 2.0


def mix(dur, *parts):
    """parts: (signal, start_sec, gain, pan -1..1). Linear sum, stereo."""
    buf = np.zeros((int(SR * dur), 2))
    for sig, start, gain, pan in parts:
        i = int(start * SR)
        seg = sig[: max(0, len(buf) - i)]
        buf[i: i + len(seg), 0] += seg * gain * np.sqrt((1 - pan) / 2)
        buf[i: i + len(seg), 1] += seg * gain * np.sqrt((1 + pan) / 2)
    return buf


def hz(name):
    names = {"C": -9, "C#": -8, "D": -7, "D#": -6, "Eb": -6, "E": -5, "F": -4, "F#": -3, "G": -2, "G#": -1,
             "A": 0, "Bb": 1, "B": 2}
    return 440.0 * 2 ** ((names[name[:-1]] + 12 * (int(name[-1]) - 4)) / 12)


# ---------------------------------------------------------------- SFX
def sfx():
    write("SFXG_Place.wav", reverb(mix(0.9, (stone_hit(0.6), 0, 1.0, 0), (bell(hz("A3"), 0.8), 0.0, 0.12, 0.2)), 1.2, 0.18))

    clear = mix(2.6,
                (bell(hz("D5"), 2.6), 0, 0.55, 0.15),
                (choir(hz("D4"), 1.3, a=0.08, r=0.6), 0, 0.35, -0.3),
                (choir(hz("F4"), 1.3, a=0.08, r=0.6), 0, 0.3, 0.3),
                (choir(hz("A4"), 1.3, a=0.08, r=0.6), 0, 0.3, 0.0),
                *[(musicbox(hz(n), 1.2), 0.08 + 0.06 * i, 0.12, 0.5) for i, n in enumerate(["A5", "D6", "F6", "A6"])])
    write("SFXG_Clear.wav", reverb(clear, 3.0, 0.35))

    blessed = mix(3.0,
                  *[(choir(hz(n), 1.8, a=0.15, r=0.9), 0, 0.3, p) for n, p in [("D4", -0.4), ("F#4", 0.4), ("A4", -0.1), ("D5", 0.2)]],
                  (bell(hz("D6"), 2.5), 0.05, 0.3, 0.4), (bell(hz("A5"), 2.5), 0.25, 0.25, -0.4),
                  *[(musicbox(hz(n), 1.2), 0.1 + 0.07 * i, 0.12, 0.3) for i, n in enumerate(["D6", "F#6", "A6", "D7"])])
    write("SFXG_Blessed.wav", reverb(blessed, 3.5, 0.4))

    t = T(3.2)
    whoosh = norm(bandpass(noise(3.2), 600, 5000)) * np.clip(t / 1.0, 0, 1) * np.exp(-np.maximum(t - 1.0, 0) * 2.5)
    holy = mix(3.2,
               *[(choir(hz(n), 2.4, a=0.35, r=1.0), 0.1, 0.28, p) for n, p in [("D4", -0.3), ("A4", 0.3), ("D5", 0.0), ("F#5", 0.2)]],
               (whoosh, 0, 0.12, 0), (bell(hz("D6"), 2.8), 0.9, 0.35, 0.3), (bell(hz("A6"), 2.4), 1.0, 0.2, -0.3))
    write("SFXG_Holy.wav", reverb(holy, 4.0, 0.42))

    over = mix(5.5,
               (organ(hz("D2"), 4.0, a=0.3, r=1.5), 0, 0.5, 0), (organ(hz("D3"), 4.0, a=0.3, r=1.5), 0, 0.35, -0.2),
               (organ(hz("F3"), 4.0, a=0.3, r=1.5), 0, 0.3, 0.2), (organ(hz("A3"), 4.0, a=0.3, r=1.5), 0, 0.28, -0.1),
               (organ(hz("Bb3"), 3.0, a=1.2, r=1.2), 1.0, 0.18, 0.3),  # the creeping minor-sixth
               (bell(hz("D3"), 4.5), 0.0, 0.5, 0.1), (thunder(5.0), 0.4, 0.45, 0))
    write("SFXG_GameOver.wav", reverb(over, 4.0, 0.4))

    t = T(1.4)
    growl = norm(lowpass(noise(1.4), 260)) * (0.6 + 0.4 * np.sign(np.sin(2 * np.pi * 31 * t))) * np.exp(-t * 3) * np.clip(t / 0.05, 0, 1)
    crumble = np.zeros(len(t))
    for _ in range(26):
        i = int(rng.uniform(0.05, 0.7) * SR)
        crumble[i: i + 220] += rng.uniform(0.2, 1.0) * np.exp(-np.arange(min(220, len(t) - i)) / 30) * rng.choice([-1, 1])
    garg = mix(1.6, (stone_hit(1.2, 0.55, 1.0), 0, 1.0, 0), (growl, 0.05, 0.45, 0), (bandpass(crumble, 800, 6000), 0.02, 0.6, 0.3))
    write("SFXG_Gargoyle.wav", reverb(garg, 2.5, 0.3))

    write("SFXG_Relic.wav", reverb(mix(1.8, *[(musicbox(hz(n), 1.4), 0.09 * i, 0.3, -0.3 + 0.2 * i)
                                            for i, n in enumerate(["D5", "F5", "A5", "D6", "E6"])]), 3.0, 0.38))

    t = T(1.2)
    glide = np.sin(2 * np.pi * np.cumsum(hz("A2") * (1 - 0.25 * t / 1.2)) / SR)
    cello = sum(np.sin(k * np.arcsin(np.clip(glide, -1, 1)) + 0.0) / k for k in (1, 2, 3)) * adsr(t, 1.2, 0.05, 0.5, 0.7, 0.4)
    write("SFXG_ComboLost.wav", reverb(mix(1.3, (cello, 0, 0.6, 0), (musicbox(hz("C#5"), 1.0), 0.0, 0.15, 0.3)), 2.5, 0.35))

    write("SFXG_Thunder.wav", reverb(thunder(6.5), 3.0, 0.25, tone=2500))


# ---------------------------------------------------------------- music (64 s seamless loop)
def music():
    dur = 64.0
    n = int(SR * dur)
    buf = np.zeros((n, 2))

    def place(sig, start, gain, pan):
        idx = (int(start * SR) + np.arange(len(sig))) % n  # wrap tails into the loop start
        np.add.at(buf[:, 0], idx, sig * gain * np.sqrt((1 - pan) / 2))
        np.add.at(buf[:, 1], idx, sig * gain * np.sqrt((1 + pan) / 2))

    chords = [("D2", ["D3", "F3", "A3"]), ("Bb1", ["Bb2", "D3", "F3"]), ("G1", ["G2", "Bb2", "D3"]), ("A1", ["A2", "C#3", "E3"])]
    melody = [["A5", "F5", "D5", "E5", "F5", "E5", "D5", "C#5"], ["D5", "F5", "Bb5", "A5", "F5", "D5", "E5", "F5"],
              ["G5", "Bb5", "D6", "C6", "Bb5", "A5", "G5", "F5"], ["E5", "C#5", "A4", "C#5", "E5", "G5", "F5", "E5"]]
    for cycle in range(4):
        for ci, (root, triad) in enumerate(chords):
            start = cycle * 16 + ci * 4
            place(organ(hz(root), 4.6, r=0.8), start, 0.42, 0.0)
            for k, note in enumerate(triad):
                place(organ(hz(note), 4.6, r=0.8), start, 0.26, -0.3 + 0.3 * k)
            if cycle >= 1:
                for k, note in enumerate(triad):
                    up = note[:-1] + str(int(note[-1]) + 1)
                    place(choir(hz(up), 4.8, a=1.0, r=1.2), start, 0.22 if cycle > 1 else 0.14, 0.4 - 0.4 * k)
            if cycle >= 2:
                for k, note in enumerate(melody[ci]):
                    place(musicbox(hz(note), 1.8), start + 0.5 * k, 0.1, -0.45)
        place(bell(hz("D3"), 9.0), cycle * 16, 0.32, 0.25)
        if cycle >= 2:
            place(bell(hz("A3"), 7.0), cycle * 16 + 8, 0.18, -0.35)

    place(thunder(6.0), 41.0, 0.3, 0.0)

    # Wind: circular-filtered noise with a 64 s-periodic swell, so the loop has no seam.
    t = np.arange(n) / SR
    for ch in range(2):
        w = norm(bandpass(rng.standard_normal(n), 250, 1400))
        swell = 0.5 + 0.35 * np.sin(2 * np.pi * 3 * t / dur + ch) + 0.15 * np.sin(2 * np.pi * 7 * t / dur)
        buf[:, ch] += w * swell * 0.035

    write("MUS_Gothic.wav", reverb(buf, 4.5, 0.4, circular=True), peak=0.8, loop=True)


if __name__ == "__main__":
    sfx()
    music()
