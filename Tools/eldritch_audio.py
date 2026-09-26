# Lovecraftian ambience (numpy; run with the rocm-env python):
#   AMB_Abyss.wav   48 s seamless loop: beating sub drone (saturated so phone speakers imply it),
#                   an endlessly falling Shepard glissando, distant groans and whispers.
#   SFXG_Flicker.wav  the lights stutter: a gust, guttering flames, a whisper and a low thump.
# The game fades AMB_Abyss in as the player's luck runs out.
import numpy as np

from gothic_audio import SR, T, norm, write, bandpass, lowpass, noise, reverb, mix

rng = np.random.default_rng(1928)  # "The Call of Cthulhu"

VOWEL_FORMANTS = [(700, 1100), (480, 1750), (300, 2050), (520, 850), (330, 760)]


def whisper(dur, syllables=None):
    """Unvoiced speech: noise through a pair of formant bands that jump per syllable."""
    syllables = syllables or max(2, int(dur / 0.13))
    seg = int(dur * SR / syllables)
    out = []
    for _ in range(syllables):
        f1, f2 = VOWEL_FORMANTS[rng.integers(len(VOWEL_FORMANTS))]
        s = rng.standard_normal(seg)
        s = bandpass(s, f1 * 0.8, f1 * 1.25) + 0.7 * bandpass(s, f2 * 0.85, f2 * 1.2) + 0.25 * bandpass(s, 3500, 7000)
        env = np.hanning(seg) ** 0.6 * rng.uniform(0.5, 1.0)
        out.append(s * env)
    return norm(np.concatenate(out))


def abyss():
    dur = 48.0
    n = int(SR * dur)
    t = np.arange(n) / SR
    # Separate stems, so the game's MetaSound (MS_Abyss) can crossfade them by tension.
    stems = {name: np.zeros((n, 2)) for name in ("Drone", "Shepard", "Whisper", "Air")}
    buf = stems["Drone"]

    def place(sig, start, gain, pan=0.0, stem="Drone"):
        target = stems[stem]
        idx = (int(start * SR) + np.arange(len(sig))) % n
        np.add.at(target[:, 0], idx, sig * gain * np.sqrt((1 - pan) / 2))
        np.add.at(target[:, 1], idx, sig * gain * np.sqrt((1 + pan) / 2))

    # Sub drone: E1 against a slightly sharp twin (a slow 1.8 Hz beat), saturated for harmonics.
    sub = np.sin(2 * np.pi * 41.2 * t) + np.sin(2 * np.pi * 43.0 * t + 1.0)
    sub = np.tanh(1.8 * sub) * (0.75 + 0.25 * np.sin(2 * np.pi * t / dur * 2))
    place(sub, 0, 0.5)

    # Shepard glissando: partials an octave apart, all falling one octave every 24 s, each
    # faded by a bell curve over log-pitch, so the fall never arrives anywhere (2 cycles = loop).
    octave_pos = (-t / 24.0) % 1.0
    shep = np.zeros(n)
    for k in range(7):
        lp = k + octave_pos                      # log2 position above 55 Hz
        f = 55.0 * 2 ** lp
        amp = np.exp(-0.5 * ((lp - 3.5) / 1.2) ** 2)
        phase = 2 * np.pi * np.cumsum(f) / SR
        shep += amp * np.sin(phase)
    place(norm(shep), 0, 0.12, stem="Shepard")

    # Groans from the deep: low sawtooth glides through a moving resonance.
    for start in (3.0, 15.5, 27.0, 38.5):
        d = rng.uniform(3.5, 5.5)
        tt = T(d)
        f = rng.uniform(48, 70) * (1 + 0.35 * np.sin(np.pi * tt / d)) * (1 - 0.2 * tt / d)
        ph = np.cumsum(f) / SR
        saw = 2 * (ph % 1.0) - 1
        g = bandpass(saw, 90, 420) * np.sin(np.pi * tt / d) ** 2
        place(norm(g), start, 0.28, rng.uniform(-0.5, 0.5))

    # Whispers at the edges of hearing, hard left or right.
    for _ in range(9):
        d = rng.uniform(0.8, 1.8)
        place(whisper(d), rng.uniform(0, dur), rng.uniform(0.05, 0.1), rng.choice([-0.9, 0.9]), stem="Whisper")

    # Deep-water air: dark filtered noise swelling with the loop.
    for ch in range(2):
        w = norm(bandpass(rng.standard_normal(n), 60, 600))
        stems["Air"][:, ch] += w * (0.5 + 0.3 * np.sin(2 * np.pi * 2 * t / dur + ch * 2.0)) * 0.05

    wet = {name: reverb(stem, 5.0, 0.45, circular=True, tone=2500) for name, stem in stems.items()}
    total = sum(wet.values())
    write("AMB_Abyss.wav", total, peak=0.8, loop=True)
    # Stems share one scale (the full mix's), so they sum back to AMB_Abyss's balance.
    scale = 0.8 / max(np.abs(total).max(), 1e-9)
    for name, stem in wet.items():
        write("AMB_%s.wav" % name, stem, scale=scale)


def flicker():
    d = 2.2
    t = T(d)
    gust = norm(bandpass(noise(d), 250, 2500)) * np.sin(np.pi * np.clip(t / 1.4, 0, 1)) ** 2
    flutter = norm(bandpass(noise(d), 600, 4000)) * (0.5 + 0.5 * np.sign(np.sin(2 * np.pi * 14 * t))) * np.exp(-t * 1.5)
    thump = np.sin(2 * np.pi * np.cumsum(55 * (1 + np.exp(-t * 20))) / SR) * np.exp(-t * 6)
    s = mix(d, (gust, 0, 0.5, 0), (flutter, 0.05, 0.25, 0.3), (whisper(0.9, 6), 0.25, 0.4, -0.6),
            (thump, 0.0, 0.6, 0))
    write("SFXG_Flicker.wav", reverb(s, 3.5, 0.45, tone=3000))


if __name__ == "__main__":
    flicker()
    abyss()
