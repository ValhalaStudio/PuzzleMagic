# Higher-realism building blocks for the offline sound pipeline (rocm-env python):
#   faust_bell   - Faust physmodels churchBell (a modal model of a real bell), pitch-shifted by resampling
#   faust_voice  - Faust FOF formant singer (pm.SFFormantModelFofSmooth) driven by audio-rate curves
#   church_reverb - convolution with a recorded church (Voxengo "St Nicolaes Church", royalty-free
#                   incl. commercial use; only processed audio ships, never the impulse file itself)
#   master       - pedalboard mastering chain (rumble cut, glue compression, limiter)
# Everything degrades gracefully: if a dependency is missing the callers fall back to numpy synthesis.
import functools
import os

import numpy as np

SR = 44100
IR_PATH = r"D:\UEDeps\IR\St Nicolaes Church.wav"

try:
    import dawdreamer as daw
    HAVE_FAUST = True
except ImportError:
    HAVE_FAUST = False

try:
    import pedalboard as pb
    HAVE_PEDALBOARD = True
except ImportError:
    HAVE_PEDALBOARD = False

try:
    import soundfile as sf
    HAVE_IR = os.path.exists(IR_PATH)
except ImportError:
    HAVE_IR = False

# Faust FOF vowel index and voice types.
VOWEL_INDEX = {"a": 0.0, "e": 1.0, "i": 2.0, "o": 3.0, "u": 4.0, "m": 4.0}
VOICE_BASS, VOICE_TENOR = 1, 4


_ENGINES = {}


def _render(dsp, seconds, automation=None):
    # One engine + compiled processor per DSP program, reused: compiling Faust (LLVM JIT) costs ~5 s
    # and ~280 MB that DawDreamer never releases, so a fresh engine per phrase ran the chant to 16 GB.
    if dsp not in _ENGINES:
        engine = daw.RenderEngine(SR, 256)
        proc = engine.make_faust_processor("dsp")
        proc.set_dsp_string(dsp)
        if not proc.compile():
            raise RuntimeError("Faust compile failed")
        engine.load_graph([(proc, [])])
        _ENGINES[dsp] = (engine, proc)
    engine, proc = _ENGINES[dsp]
    for name, values in (automation or {}).items():
        proc.set_automation("/dawdreamer/" + name, np.asarray(values, dtype=np.float32))
    engine.render(seconds)
    return engine.get_audio()[0].astype(np.float64)


@functools.lru_cache(maxsize=None)
def _bell_base(strike):
    """One strike of the modal church bell (fixed pitch), and its estimated hum/fundamental."""
    n = int(SR * 9.0)
    gate = np.zeros(n)
    gate[1:300] = 1.0
    audio = _render('import("stdfaust.lib"); process = pm.churchBell(%d, 6500, 0.5, 0.9, hslider("gate",0,0,1,1));' % strike,
                    9.0, {"gate": gate})
    spec = np.abs(np.fft.rfft(audio[: SR * 2] * np.hanning(SR * 2)))
    freqs = np.fft.rfftfreq(SR * 2, 1 / SR)
    band = (freqs > 60) & (freqs < 1200)
    fundamental = freqs[band][np.argmax(spec[band])]
    return audio / max(np.abs(audio).max(), 1e-9), fundamental


def faust_bell(freq, dur, strike=0):
    """Church bell whose strongest partial lands on freq (resampled from the modal model)."""
    from scipy.signal import resample
    base, f0 = _bell_base(strike)
    ratio = f0 / freq                       # >1 lowers the pitch (longer)
    shifted = resample(base, int(len(base) * ratio))
    out = np.zeros(int(SR * dur))
    m = min(len(out), len(shifted))
    out[:m] = shifted[:m]
    fade = np.clip((dur - np.arange(len(out)) / SR) / 0.5, 0, 1)
    return out * fade


def faust_voice(f0, vowel, gain, voice_type=VOICE_TENOR):
    """Formant (FOF) singer: f0, vowel (0 a .. 4 u, continuous) and gain are per-sample arrays."""
    dsp = ('import("stdfaust.lib"); process = pm.SFFormantModelFofSmooth(%d, hslider("vowel",0,0,4,0.001),'
           ' hslider("freq",146,40,1200,0.01), hslider("gain",0.5,0,1,0.001));' % voice_type)
    return _render(dsp, len(f0) / SR, {"freq": f0, "vowel": vowel, "gain": gain})[: len(f0)]


@functools.lru_cache(maxsize=None)
def _church_ir():
    ir, sr = sf.read(IR_PATH, always_2d=True)
    assert sr == SR
    ir = ir / np.sqrt(np.sum(ir ** 2) / ir.shape[1])
    return ir


def church_reverb(x, wet=0.4, circular=False, predelay=0.0):
    """Stereo convolution with the recorded church. circular=True keeps loops seamless."""
    if x.ndim == 1:
        x = np.stack([x, x], 1)
    ir = _church_ir()
    if predelay > 0:
        ir = np.concatenate([np.zeros((int(predelay * SR), 2)), ir])
    n_ir = len(ir)
    L = len(x) if circular else len(x) + n_ir
    out = np.zeros((L, 2))
    for ch in range(2):
        h = ir[:, ch]
        if circular:
            hp = np.zeros(L)
            hp[: min(L, n_ir)] = h[: min(L, n_ir)]
            if n_ir > L:  # fold a tail longer than the loop back onto it
                for start in range(L, n_ir, L):
                    seg = h[start: start + L]
                    hp[: len(seg)] += seg
            out[:, ch] = np.fft.irfft(np.fft.rfft(x[:, ch]) * np.fft.rfft(hp), L)
        else:
            out[:, ch] = np.fft.irfft(np.fft.rfft(x[:, ch], L) * np.fft.rfft(h, L), L)
    dry = np.zeros_like(out)
    dry[: len(x)] = x
    return dry * (1 - wet) + out * wet * 0.35


def master(x, loud=False):
    """Rumble cut, gentle glue compression and a limiter (pedalboard); identity if unavailable."""
    if not HAVE_PEDALBOARD:
        return x
    board = pb.Pedalboard([
        pb.HighpassFilter(cutoff_frequency_hz=32.0),
        pb.Compressor(threshold_db=-20.0 if loud else -16.0, ratio=2.5, attack_ms=15.0, release_ms=220.0),
        pb.Limiter(threshold_db=-1.5, release_ms=120.0),
    ])
    y = board(x.T.astype(np.float32), SR).T.astype(np.float64)
    return y
