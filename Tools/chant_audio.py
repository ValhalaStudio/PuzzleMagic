# Gregorian chant soundtrack + storm/omen SFX (numpy; run with the rocm-env python).
# The chant is formant-synthesized male voices (additive harmonics shaped by moving vowel
# resonances) singing Dorian-mode phrases over a Byzantine-style ison drone, with a long
# circular cathedral reverb so the loop is seamless. Opens with the Dies irae incipit (13th c.).
import numpy as np

import gothic_audio as g
from gothic_audio import SR, T, norm, write, bandpass, lowpass, noise, bell, organ, thunder, reverb, mix, hz
import audio_fx

rng = np.random.default_rng(1231)

# Male vowel formants: (freq, bandwidth, gain) for F1..F4, darkened for a soft choral sound.
VOWELS = {
    "a": [(700, 90, 1.0), (1100, 110, 0.55), (2500, 160, 0.22), (3300, 220, 0.1)],
    "e": [(480, 80, 1.0), (1750, 120, 0.4), (2450, 160, 0.22), (3300, 220, 0.1)],
    "i": [(300, 70, 1.0), (2050, 130, 0.28), (2750, 170, 0.2), (3400, 220, 0.1)],
    "o": [(520, 80, 1.0), (850, 100, 0.55), (2450, 160, 0.14), (3200, 220, 0.07)],
    "u": [(330, 70, 1.0), (760, 100, 0.4), (2300, 160, 0.1), (3100, 220, 0.05)],
    "m": [(260, 60, 1.0), (1000, 160, 0.08), (2200, 220, 0.05), (3000, 260, 0.02)],  # hummed ison
}
BEAT = 0.5  # seconds per chant beat


def envelope_from_points(n, points):
    """points: list of (sample_index, value); linear interpolation, held at the ends."""
    xs = np.array([p[0] for p in points], dtype=float)
    ys = np.array([p[1] for p in points], dtype=float)
    return np.interp(np.arange(n), xs, ys)


def sing(notes, transpose=1.0, detune_cents=0.0, vib=0.004, breath=0.02, jitter=0.02):
    """One voice singing a phrase. notes: [(note name, vowel, beats, consonant)].
    Returns a mono signal. Pitch glides between notes; vowel formants morph at syllables."""
    total = sum(b for _, _, b, _ in notes) * BEAT + 0.6
    n = int(total * SR)
    t = np.arange(n) / SR
    det = 2 ** (detune_cents / 1200)

    # Pitch and vowel control curves (one point per note boundary, short glides).
    f_pts, a_pts, starts = [], [], []
    cur = int(rng.uniform(0, jitter) * SR)
    for i, (note, vowel, beats, cons) in enumerate(notes):
        f = hz(note) * transpose * det
        dur = int(beats * BEAT * SR)
        glide = int(0.07 * SR)
        f_pts += [(cur, f), (cur + dur - glide, f)]
        starts.append((cur, dur, vowel, cons))
        cur += dur
    f0 = envelope_from_points(n, f_pts)
    # Slow drift + a little vibrato: chant is steady, but real voices wander.
    f0 *= 1 + vib * np.sin(2 * np.pi * rng.uniform(4.2, 5.4) * t + rng.uniform(0, 6.3))
    f0 *= 1 + 0.003 * np.sin(2 * np.pi * rng.uniform(0.15, 0.35) * t + rng.uniform(0, 6.3))
    phase = 2 * np.pi * np.cumsum(f0) / SR

    # Formant trajectories: hold each syllable's vowel, morph over 90 ms into the next.
    ftraj = np.zeros((4, 3, n))
    for k in range(4):
        for j in range(3):
            pts = []
            for cur_i, dur, vowel, _ in starts:
                v = VOWELS[vowel][k][j]
                pts += [(cur_i + int(0.045 * SR), v), (cur_i + dur - int(0.045 * SR), v)]
            ftraj[k, j] = envelope_from_points(n, pts)

    # Loudness: phrase swell, small dips at syllable boundaries (the consonants), release at the end.
    amp_pts = []
    for i, (cur_i, dur, _, cons) in enumerate(starts):
        last = i == len(starts) - 1
        if i == 0:
            amp_pts += [(cur_i, 0.0), (cur_i + int(0.12 * SR), 1.0)]
        else:
            amp_pts += [(cur_i, 0.35 if cons else 0.8), (cur_i + int(0.06 * SR), 1.0)]
        amp_pts += [(cur_i + dur - int((0.4 if last else 0.05) * SR), 1.0)]
    end = starts[-1][0] + starts[-1][1]
    amp_pts += [(end, 0.0)]
    amp = envelope_from_points(n, amp_pts)
    phrase = 0.85 + 0.15 * np.sin(np.pi * np.clip(t / (end / SR), 0, 1))
    amp *= phrase

    if audio_fx.HAVE_FAUST:
        # Faust FOF singer: bass range for the low doubling/drone, tenor above.
        vowel_curve = np.zeros(n)
        pts = []
        for cur_i, dur, vowel, _ in starts:
            v = audio_fx.VOWEL_INDEX[vowel]
            pts += [(cur_i + int(0.045 * SR), v), (cur_i + dur - int(0.045 * SR), v)]
        vowel_curve = envelope_from_points(n, pts)
        voice_type = audio_fx.VOICE_BASS if f0.mean() < 120 else audio_fx.VOICE_TENOR
        # The shared code below multiplies by amp again, so the singer gets its square root.
        # x26: the FOF model is ~26x quieter (RMS) than the additive voices the mix was balanced for.
        out = audio_fx.faust_voice(f0, vowel_curve, np.sqrt(amp) * 0.8, voice_type) * 26.0
        out = np.pad(out, (0, max(0, n - len(out))))[:n]
    else:
        out = np.zeros(n)
    kmax = 0 if audio_fx.HAVE_FAUST else int(4800 / f0.min())
    for h in range(1, kmax + 1):
        fh = f0 * h
        env = 0.012
        for k in range(4):
            fc, bw, gain = ftraj[k]
            env = env + gain / (1 + ((fh - fc) / (bw * 0.5)) ** 2)
        env *= (fh < 5200)
        out += env / h ** 0.7 * np.sin(h * phase)
    out *= amp

    # Consonants: short noise bursts at syllable onsets (s = hiss, t/k/d = plosive tick).
    for cur_i, dur, _, cons in starts:
        if not cons or cons == "m":
            continue
        m = int((0.09 if cons == "s" else 0.025) * SR)
        burst = noise(m / SR)
        burst = bandpass(burst, 4000, 9000) if cons == "s" else bandpass(burst, 1500, 5000)
        burst *= np.hanning(m) * (0.08 if cons == "s" else 0.12)
        i0 = max(cur_i - m // 2, 0)
        out[i0: i0 + m] += burst[: len(out[i0: i0 + m])] * np.abs(out).max()
    out += bandpass(noise(total), 800, 3000) * breath * amp * np.abs(out).max()
    return out


def schola(notes, voices=6, transpose=1.0, spread=9.0, pan_width=0.7):
    """A group singing in unison: detuned, slightly out of time, spread across the stereo field."""
    parts = []
    for v in range(voices):
        sig = sing(notes, transpose, detune_cents=rng.uniform(-spread, spread), jitter=0.05)
        parts.append((sig, rng.uniform(0, 0.03), 1.0 / np.sqrt(voices), rng.uniform(-pan_width, pan_width)))
    L = max(len(p[0]) + int(p[1] * SR) for p in parts)
    return mix(L / SR + 0.01, *parts)


def ison(dur, note="D2", vowel="m"):
    """The held drone under the chant (circular: constant pitch, so its loop point is invisible)."""
    notes = [(note, vowel, dur / BEAT, None)]
    s = sing(notes, vib=0.0015, breath=0.01, jitter=0.0)
    return s[: int(dur * SR)]


# Phrases: (note, vowel, beats, consonant at the syllable start)
DIES_IRAE = [("F3", "i", 1, "t"), ("E3", "e", 1, None), ("F3", "i", 1, None), ("D3", "e", 1, None),
             ("E3", "i", 1, "t"), ("C3", "e", 1, None), ("D3", "i", 1, None), ("D3", "a", 2.5, None)]
SOLVET = [("F3", "o", 1, "s"), ("F3", "e", 1, None), ("G3", "e", 1, "s"), ("F3", "u", 1, "k"),
          ("E3", "i", 1, None), ("D3", "a", 1, None), ("C3", "a", 1, None), ("E3", "i", 1, None), ("D3", "a", 2.5, None)]
QUANTUS = [("A3", "a", 1, "k"), ("A3", "u", 1, "t"), ("G3", "e", 1, "t"), ("A3", "o", 1, None),
           ("F3", "e", 1, None), ("G3", "u", 1, None), ("E3", "u", 1, "t"), ("D3", "u", 2.5, "s")]
QUANDO = [("C3", "a", 1, "k"), ("D3", "o", 1, None), ("F3", "u", 1, None), ("E3", "e", 1, "k"),
          ("D3", "e", 1, "s"), ("E3", "e", 1, None), ("C3", "u", 1, "t"), ("D3", "u", 2.5, "s")]
REQUIEM = [("F3", "e", 1, None), ("G3", "i", 1, "k"), ("A3", "e", 1.5, None), ("A3", "e", 1, "t"),
           ("G3", "e", 1, None), ("A3", "a", 2.5, None)]
DONA = [("A3", "o", 1, "t"), ("C4", "a", 1, None), ("A3", "e", 1, None), ("G3", "i", 1, "s"),
        ("F3", "o", 1, "t"), ("G3", "i", 1, None), ("A3", "e", 1, None), ("G3", "e", 0.5, None),
        ("F3", "e", 1, None), ("E3", "e", 1, None), ("D3", "e", 3, None)]
AMEN = [("D3", "a", 1.5, None), ("E3", "a", 1, None), ("F3", "a", 1, None), ("E3", "e", 1.5, "m"),
        ("D3", "e", 4, None)]


def phrase_len(notes):
    return sum(b for _, _, b, _ in notes) * BEAT


def music():
    dur = 106.0
    n = int(SR * dur)
    buf = np.zeros((n, 2))

    def place(sig, start, gain, pan=0.0):
        if sig.ndim == 1:
            sig = np.stack([sig * np.sqrt((1 - pan) / 2), sig * np.sqrt((1 + pan) / 2)], 1)
        idx = (int(start * SR) + np.arange(len(sig))) % n  # wrap tails into the loop start
        np.add.at(buf[:, 0], idx, sig[:, 0] * gain)
        np.add.at(buf[:, 1], idx, sig[:, 1] * gain)

    gap = 1.1  # a breath between phrases
    t = 2.0
    # I. Cantor intones, the schola answers.
    for notes, voices, gain in [(DIES_IRAE, 2, 0.55), (SOLVET, 2, 0.55), (DIES_IRAE, 7, 0.75), (SOLVET, 7, 0.75)]:
        place(schola(notes, voices, spread=5 if voices < 3 else 9, pan_width=0.2 if voices < 3 else 0.75), t, gain)
        t += phrase_len(notes) + gap
    # II. Full schola with the basses doubling an octave below, then the same answered in organum.
    for fourth in (False, True):
        for notes in [QUANTUS, QUANDO]:
            place(schola(notes, 7), t, 0.7)
            if fourth:
                place(schola(notes, 5, transpose=2 ** (-5 / 12), pan_width=0.5), t, 0.42)
            else:
                place(schola(notes, 4, transpose=0.5, pan_width=0.4), t, 0.42)
            t += phrase_len(notes) + gap
    interlude = t  # the schola rests; bell and drone
    t += 4.0
    # III. Requiem in organum (the lower voices shadow the melody a fourth below), sung twice.
    organ_start = t - 2.0
    for basses in (False, True):
        for notes in [REQUIEM, DONA]:
            place(schola(notes, 7), t, 0.66)
            place(schola(notes, 5, transpose=2 ** (-5 / 12), pan_width=0.5), t, 0.44)
            if basses:
                place(schola(notes, 4, transpose=0.5, pan_width=0.3), t, 0.3)
            t += phrase_len(notes) + gap
    # IV. Quiet return and a long Amen.
    for notes in [DIES_IRAE, SOLVET]:
        place(schola(notes, 5), t, 0.5)
        t += phrase_len(notes) + gap
    amen = t
    place(schola(AMEN, 7), t, 0.6)
    place(schola(AMEN, 4, transpose=0.5, pan_width=0.4), t, 0.35)
    t += phrase_len(AMEN)
    print("chant ends at", round(t, 1), "s of", dur)
    assert t < dur - 4.0, "chant overruns the loop"

    # Ison: a hummed D drone (bass on the octave, tenors on the fifth), across the whole loop.
    for note, gain, pan in [("D2", 0.42, -0.2), ("D2", 0.38, 0.25), ("A2", 0.16, 0.0)]:
        place(ison(dur, note), 0.0, gain, pan)
    # A quiet organ pedal under the Requiem and the Amen.
    pedal = amen + phrase_len(AMEN) - organ_start
    place(organ(hz("D1"), pedal, amp=1.0, a=4.0, r=5.0), organ_start, 0.1)
    place(organ(hz("A1"), pedal - 6.0, amp=1.0, a=4.0, r=5.0), organ_start + 6.0, 0.05)
    # Distant bells and the storm outside.
    for start, note, gain in [(0.0, "D3", 0.22), (interlude + 0.5, "A2", 0.18), (amen + phrase_len(AMEN) + 0.5, "D3", 0.18)]:
        place(bell(hz(note), 9.0), start, gain, 0.3)
    place(thunder(6.0), interlude + 20.0, 0.12)
    tt = np.arange(n) / SR
    for ch in range(2):
        w = norm(bandpass(rng.standard_normal(n), 200, 1200))
        swell = 0.5 + 0.35 * np.sin(2 * np.pi * 3 * tt / dur + ch) + 0.15 * np.sin(2 * np.pi * 5 * tt / dur)
        buf[:, ch] += w * swell * 0.018
        rain = norm(bandpass(rng.standard_normal(n), 2500, 9000))
        buf[:, ch] += rain * 0.006

    write("MUS_Chant.wav", reverb(buf, 6.0, 0.5, circular=True, tone=3500), peak=0.8, loop=True)


def sfx():
    # Close lightning strike: a white crack, sizzle, and a short heavy boom.
    t = T(3.0)
    crack = norm(bandpass(noise(3.0), 1200, 12000)) * np.exp(-t * 28)
    snap = np.zeros(len(t))
    for _ in range(9):
        i = int(rng.uniform(0.0, 0.12) * SR)
        m = int(0.004 * SR)
        snap[i: i + m] += rng.uniform(0.5, 1.0) * rng.choice([-1, 1])
    sizzle = norm(bandpass(noise(3.0), 3000, 10000)) * np.exp(-t * 6) * (0.6 + 0.4 * np.sin(2 * np.pi * 37 * t))
    boom = norm(lowpass(np.cumsum(rng.standard_normal(len(t))), 120)) * np.exp(-t * 1.6) * np.clip(t / 0.02, 0, 1)
    write("SFXG_Strike.wav", reverb(mix(3.0, (crack, 0, 1.0, 0), (bandpass(snap, 800, 9000), 0, 0.8, 0.1),
                                        (sizzle, 0.03, 0.25, -0.2), (boom, 0.02, 0.9, 0)), 3.0, 0.3, tone=3000))

    # Witch's hex: a descending, whispering tritone cluster with a reversed swell.
    t = T(2.6)
    swell = norm(bandpass(noise(2.6), 500, 4000)) * np.clip(t / 0.9, 0, 1) ** 2 * np.exp(-np.maximum(t - 0.9, 0) * 6)
    down = []
    for k, (f, pan) in enumerate([(hz("B3"), -0.4), (hz("F4"), 0.4), (hz("C5"), 0.0)]):
        glide = f * (1 - 0.3 * np.clip((t - 0.8) / 1.6, 0, 1))
        s = sum(np.sin(2 * np.pi * np.cumsum(glide * m) / SR) / m for m in (1, 2, 3, 5))
        s *= np.clip((t - 0.75) / 0.05, 0, 1) * np.exp(-np.maximum(t - 0.8, 0) * 1.6)
        down.append((s, 0, 0.22, pan))
    whisper = norm(bandpass(noise(2.6), 1800, 6000)) * np.exp(-np.maximum(t - 0.8, 0) * 3) * np.clip((t - 0.8) / 0.03, 0, 1) \
        * (0.5 + 0.5 * np.sin(2 * np.pi * 7 * t))
    write("SFXG_Hex.wav", reverb(mix(2.6, (swell, 0, 0.35, 0), *down, (whisper, 0, 0.12, 0.3)), 3.0, 0.4))

    # Ward: bright shimmering chime + a sung "ah" as the omen is turned aside.
    ward = mix(2.4,
               *[(g.musicbox(hz(n), 1.6), 0.05 * i, 0.25, -0.3 + 0.15 * i) for i, n in enumerate(["A5", "D6", "E6", "A6", "D7"])],
               (g.choir(hz("A4"), 1.6, a=0.08, r=0.8), 0, 0.3, 0.0),
               (g.choir(hz("D5"), 1.6, a=0.08, r=0.8), 0, 0.25, 0.2),
               (bell(hz("A5"), 2.2), 0.0, 0.3, 0.2))
    write("SFXG_Ward.wav", reverb(ward, 3.2, 0.42))


if __name__ == "__main__":
    import sys
    if "music" not in sys.argv[1:]:
        sfx()
    music()
