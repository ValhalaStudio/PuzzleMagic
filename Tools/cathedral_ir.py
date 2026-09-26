# IR_Cathedral.wav: our own synthetic cathedral impulse response for the in-engine convolution
# reverb (Synthesis plugin, submix SM_Cathedral). Synthesized rather than recorded, so it can ship
# inside the game (the recorded church IR used offline may not be redistributed): sparse early
# reflections off stone, then a dense tail that darkens as it decays (air absorbs highs first).
import os
import wave

import numpy as np

from gothic_audio import SR, lowpass

OUT = r"D:\Unreal Projects\PuzzleGame5x5\RawAudio"
rng = np.random.default_rng(1248)

dur = 5.5
n = int(SR * dur)
t = np.arange(n) / SR
ir = np.zeros((n, 2))
for ch in range(2):
    tail = rng.standard_normal(n) * np.exp(-t * 6.9 / dur)
    dark = lowpass(tail, 1800)
    ir[:, ch] = tail * np.exp(-t * 1.2) + dark * (1 - np.exp(-t * 1.2))
    for _ in range(14):
        i = int(rng.uniform(0.012, 0.11) * SR)
        ir[i, ch] += rng.uniform(0.3, 0.9) * rng.choice([-1, 1])
ir[: int(0.01 * SR)] = 0.0
ir /= np.abs(ir).max()
with wave.open(os.path.join(OUT, "IR_Cathedral.wav"), "wb") as f:
    f.setnchannels(2)
    f.setsampwidth(2)
    f.setframerate(SR)
    f.writeframes((ir * 0.9 * 32767).astype("<i2").tobytes())
print("wrote IR_Cathedral.wav", dur, "s")
