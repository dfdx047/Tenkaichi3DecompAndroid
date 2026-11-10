#!/usr/bin/env python3
"""
Makes the thunder of the full power storm (port/src/gs/storm_snd.c): port/tbo/ui/thunder_1.wav .. thunder_4.wav.

Nothing recorded: every sound is filtered noise, shaped by envelopes, so the files are our own. A far thunder is a
long low rumble that rolls (several swells, each softer), sometimes after a crack. Phone speakers give almost
nothing below about 200 Hz, so besides the deep rumble each sound has a body of 80..1000 Hz and a little saturation
(harmonics a small speaker can play).

    python3 port/tools/make_thunder.py            (numpy and scipy)
"""
import pathlib
import wave

import numpy as np
from scipy.signal import butter, fftconvolve, lfilter, sosfilt

RATE = 32000
OUT = pathlib.Path(__file__).resolve().parents[1] / "tbo" / "ui"


def lowpass(x, fc, order=4):
    return sosfilt(butter(order, fc, "lowpass", fs=RATE, output="sos"), x)


def highpass(x, fc, order=2):
    return sosfilt(butter(order, fc, "highpass", fs=RATE, output="sos"), x)


def bandpass(x, lo, hi, order=2):
    return sosfilt(butter(order, [lo, hi], "bandpass", fs=RATE, output="sos"), x)


def swell(t, t0, attack, decay):
    """0 before t0, then a rise over `attack` seconds and an exponential fall with time constant `decay`."""
    d = np.clip(t - t0, 0.0, None)
    return np.where(t >= t0, (1.0 - np.exp(-d / attack)) * np.exp(-d / decay), 0.0)


def thunder(seed, seconds, rolls, deep_hz, crack, spread):
    rng = np.random.default_rng(seed)
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    white = rng.standard_normal(n)
    brown = lfilter([1.0], [1.0, -0.995], white)
    brown /= np.abs(brown).max()

    # the rolls: swells that start one after another, each softer and slower than the last
    env = np.zeros(n)
    start = 0.05 if crack else 0.0
    amp = 1.0
    for k in range(rolls):
        env += amp * swell(t, start, rng.uniform(0.04, 0.18), rng.uniform(0.45, 1.1) * (1.0 + 0.25 * k))
        start += rng.uniform(0.25, spread)
        amp *= rng.uniform(0.55, 0.85)
    env /= env.max()
    # its grain: a slow random flutter (a rumble is many echoes off clouds and hills)
    flutter = lowpass(rng.standard_normal(n), 9.0, 2)
    flutter = 0.55 + 0.45 * flutter / np.abs(flutter).max()

    deep = lowpass(brown, deep_hz)
    deep /= np.abs(deep).max()
    body = bandpass(white, 80.0, 1000.0)
    body /= np.abs(body).max()
    body_env = env ** 1.6  # the body dies sooner than the deep part
    sig = 1.0 * deep * env * flutter + 0.45 * body * body_env * flutter

    if crack:
        # the crack: a sharp tearing burst, then a short bright rattle, and a thump under it
        tear = highpass(white, 1500.0) * np.exp(-t / 0.05)
        jitter = (rng.random(n) < 0.04).astype(float)
        jitter = lowpass(jitter, 3000.0, 2)
        rattle = bandpass(white, 250.0, 2500.0) * np.exp(-t / 0.22) * (0.4 + 3.0 * np.abs(jitter))
        freq = 30.0 + 45.0 * np.exp(-t / 0.35)
        thump = np.sin(2.0 * np.pi * np.cumsum(freq) / RATE) * np.exp(-t / 0.45)
        sig += crack * (0.35 * tear / np.abs(tear).max() + 0.5 * rattle / np.abs(rattle).max() + 0.6 * thump)

    # space: a long dark tail (decaying noise as the room's answer), part of the sound
    ir_n = int(2.2 * RATE)
    ir_t = np.arange(ir_n) / RATE
    ir = lowpass(rng.standard_normal(ir_n), 1200.0, 2) * np.exp(-ir_t / 0.7)
    ir /= np.sqrt(np.sum(ir ** 2))
    wet = fftconvolve(sig, ir)[:n]
    sig = 0.75 * sig / np.abs(sig).max() + 0.45 * wet / np.abs(wet).max()

    # a little saturation for small speakers, then the edges
    sig = np.tanh(1.6 * sig / np.abs(sig).max()) / np.tanh(1.6)
    sig = lowpass(sig, 1800.0, 2)  # far away: the highs are lost on the way (and hiss sounds like rain)
    sig = highpass(sig, 28.0, 2)  # (no inaudible push on the speaker)
    fade_in = int(0.008 * RATE)
    fade_out = int(0.6 * RATE)
    sig[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    sig[-fade_out:] *= np.linspace(1.0, 0.0, fade_out) ** 2
    return sig / np.abs(sig).max() * 0.89  # -1 dBFS


def write(path, x):
    pcm = np.clip(np.round(x * 32767.0), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())


SOUNDS = [
    # seed, seconds, rolls, deep part's top (Hz), crack (0 none .. 1), seconds between rolls (up to)
    (11, 5.5, 4, 220.0, 0.0, 0.9),  # a far rolling rumble
    (23, 7.0, 6, 180.0, 0.0, 1.1),  # a long one, rolling on and on
    (37, 5.0, 3, 260.0, 1.0, 0.7),  # a crack, then the rumble
    (41, 6.0, 2, 240.0, 0.5, 1.3),  # a softer crack and two big rolls
]

if __name__ == "__main__":
    for i, (seed, seconds, rolls, deep, crack, spread) in enumerate(SOUNDS, 1):
        x = thunder(seed, seconds, rolls, deep, crack, spread)
        path = OUT / f"thunder_{i}.wav"
        write(path, x)
        rms = float(np.sqrt(np.mean(x ** 2)))
        print(f"{path.name}: {seconds} s, rms {20 * np.log10(rms):.1f} dBFS")
