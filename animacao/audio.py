"""Synthesizes the sound design for intro.html (timeline mirrors the T object there).

Usage: python3 audio.py out.wav
Only needs numpy. Everything is generated procedurally (no samples).
"""
import sys
import wave
import numpy as np

SR = 48000
DUR = 7.9
N = int(SR * DUR)
rng = np.random.default_rng(7)

T = dict(charge=(0, .45), appear=(.45, .72), shine=(.88, 1.18), antic=(1.35, 1.5), launch=(1.5, 1.85),
         side1=(1.85, 3.4), insert1=(2.5, 2.68), boom1=2.95, front=(3.4, 3.95),
         side2=(3.95, 5.0), boom2=4.2, insert2=(4.33, 4.5), surge=(4.6, 4.88),
         arrive=(5.12, 5.33), hit=5.33)
HIT = T['hit']

out = np.zeros((N, 2))
t_all = np.arange(N) / SR


# ---------------- helpers ----------------
def clamp(x, a=0.0, b=1.0):
    return np.minimum(b, np.maximum(a, x))


def prog(t, a, b):
    return clamp((t - a) / (b - a))


def band(n, lo, hi, seed=None):
    """Band-limited noise via FFT masking (normalized to unit RMS)."""
    r = np.random.default_rng(seed) if seed is not None else rng
    x = r.standard_normal(n)
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(n, 1 / SR)
    m = ((f >= lo) & (f <= hi)).astype(float)
    # soft edges
    m = np.convolve(m, np.hanning(31) / np.hanning(31).sum(), mode='same')
    y = np.fft.irfft(X * m, n)
    return y / (np.sqrt(np.mean(y ** 2)) + 1e-9)


def osc(freq, n, shape='sine', phase0=0.0):
    """Oscillator with per-sample frequency array (or scalar)."""
    f = np.broadcast_to(np.asarray(freq, dtype=float), (n,))
    ph = phase0 + 2 * np.pi * np.cumsum(f) / SR
    if shape == 'sine':
        return np.sin(ph)
    if shape == 'saw':
        return 2 * ((ph / (2 * np.pi)) % 1) - 1
    if shape == 'tri':
        return 2 * np.abs(2 * ((ph / (2 * np.pi)) % 1) - 1) - 1
    raise ValueError(shape)


def env_ad(n, attack, decay_tau):
    t = np.arange(n) / SR
    a = clamp(t / max(attack, 1e-4))
    return a * np.exp(-np.maximum(0, t - attack) / decay_tau)


def lowpass(x, cutoff):
    """One-pole low-pass, cutoff may be an array (time-varying)."""
    c = np.broadcast_to(np.asarray(cutoff, dtype=float), x.shape)
    a = 1 - np.exp(-2 * np.pi * c / SR)
    y = np.empty_like(x)
    s = 0.0
    for i in range(len(x)):
        s += a[i] * (x[i] - s)
        y[i] = s
    return y


def put(sig, start, gain=1.0, pan=0.0):
    """Add mono (or stereo) signal at time `start` with equal-power pan (-1..1, may be an array)."""
    i0 = int(round(start * SR))
    if i0 >= N:
        return
    if i0 < 0:
        sig = sig[-i0:]
        if isinstance(pan, np.ndarray):
            pan = pan[-i0:]
        i0 = 0
    n = min(len(sig), N - i0)
    sig = sig[:n] * gain
    if sig.ndim == 2:
        out[i0:i0 + n] += sig
        return
    p = pan[:n] if isinstance(pan, np.ndarray) else pan
    ang = (np.asarray(p, dtype=float) + 1) * np.pi / 4
    out[i0:i0 + n, 0] += sig * np.cos(ang)
    out[i0:i0 + n, 1] += sig * np.sin(ang)


def seg(a, b):
    n = int(round((b - a) * SR))
    return n, np.arange(n) / SR + a


# ---------------- sound building blocks ----------------
def kick(f0=160, f1=40, dur=.6, tau=.18, click=.4):
    n = int(dur * SR)
    t = np.arange(n) / SR
    f = f1 + (f0 - f1) * np.exp(-t / .045)
    s = osc(f, n) * np.exp(-t / tau)
    s += click * band(n, 1500, 7000) * np.exp(-t / .006) * .3
    return np.tanh(s * 1.6)


def bell(freqs, dur=1.6, tau=.5, amps=None, detune=1.5):
    n = int(dur * SR)
    t = np.arange(n) / SR
    s = np.zeros(n)
    amps = amps or [1 / (i + 1) ** .7 for i in range(len(freqs))]
    for i, (f, a) in enumerate(zip(freqs, amps)):
        s += a * np.sin(2 * np.pi * f * t + i) * np.exp(-t / (tau / (1 + i * .35)))
        s += a * .5 * np.sin(2 * np.pi * (f + detune) * t) * np.exp(-t / (tau / (1 + i * .35)))
    return s * clamp(t / .002)


def shing(dur=.9):
    n = int(dur * SR)
    t = np.arange(n) / SR
    s = bell([2637, 3951, 5274, 6645], dur, .35) * .6
    s += band(n, 4000, 12000) * np.exp(-t / .08) * .25
    return s


def whoosh(dur, lo0, hi0, lo1, hi1, shape='rise', seed=None):
    """Noise whoosh sweeping from band (lo0,hi0) to (lo1,hi1) by crossfading 6 bands."""
    n = int(round(dur * SR))
    k = np.linspace(0, 1, n)
    s = np.zeros(n)
    steps = 6
    for j in range(steps):
        a = j / (steps - 1)
        lo, hi = lo0 + (lo1 - lo0) * a, hi0 + (hi1 - hi0) * a
        w = np.maximum(0, 1 - np.abs(k * (steps - 1) - j))
        s += band(n, lo, hi, None if seed is None else seed + j) * w
    if shape == 'rise':
        e = k ** 2.2
    elif shape == 'fall':
        e = (1 - k) ** 1.5 * clamp(k / .03)
    else:  # swell
        e = np.sin(np.pi * k) ** 1.5
    return s * e


def crackle(n, density, seed=3):
    """Flame crackle: sparse filtered clicks, density (clicks/sec) may be an array."""
    r = np.random.default_rng(seed)
    d = np.broadcast_to(np.asarray(density, float), (n,))
    imp = (r.random(n) < d / SR) * r.uniform(.3, 1, n) * r.choice([-1, 1], n)
    ker = np.exp(-np.arange(int(.004 * SR)) / (.0008 * SR)) * r.standard_normal(int(.004 * SR))
    return np.convolve(imp, ker, mode='same')


def sonic_boom():
    n = int(1.6 * SR)
    t = np.arange(n) / SR
    s = kick(120, 32, 1.6, .45, 0) * .9
    for d in (0, .028):                               # N-wave double crack
        i = int(d * SR)
        s[i:] += band(n - i, 300, 9000) * np.exp(-np.arange(n - i) / SR / .02) * .9
    s += band(n, 60, 400) * np.exp(-t / .35) * .35      # rumble tail
    return np.tanh(s * 1.3)


# ---------------- timeline ----------------
# 1) charge: rising energy hum + riser noise
n, tt = seg(0, .47)
k = tt / .45
f = 90 + 520 * k ** 2
s = (osc(f, n, 'saw') * .35 + osc(f * 1.5, n, 'sine') * .3) * k ** 1.5
s = lowpass(s, 400 + 5000 * k ** 2)
s += band(n, 2000, 9000) * k ** 3 * .35
s *= clamp((.47 - tt) / .02)
put(s, 0, .5)

# 2) appear pop: kick + chime + spark
put(kick(170, 45, .8, .2), T['appear'][0], .9)
put(bell([1318, 1976, 2637, 3951], 2.0, .6), T['appear'][0], .32)
put(band(int(.25 * SR), 3000, 12000) * env_ad(int(.25 * SR), .001, .04), T['appear'][0], .35)

# 3) shine "shing" and twinkle "ting"
put(shing(1.2), T['shine'][0] + .05, .5, -.2)
put(bell([4186, 6272], .7, .18), T['shine'][1] - .02, .3, .4)

# 4) anticipation: reverse whoosh (suck-in)
put(whoosh(.17, 2000, 6000, 300, 1500, 'rise', 11), T['antic'][0] - .02, .45)

# 5) launch into lens: big rising whoosh + roar, cut on a whump
n, tt = seg(*T['launch'])
k = (tt - T['launch'][0]) / (T['launch'][1] - T['launch'][0])
s = whoosh(T['launch'][1] - T['launch'][0], 200, 1200, 1500, 8000, 'rise', 21) * 1.2
s += osc(120 + 500 * k ** 2, n, 'saw') * k ** 2 * .3
put(s, T['launch'][0], .7, np.linspace(0, .5, n))
put(kick(140, 38, .9, .25, .8), T['launch'][1], 1.0)
put(band(int(.15 * SR), 800, 9000) * env_ad(int(.15 * SR), .001, .03), T['launch'][1], .5)


# 6) side tracking: speed-driven ki roar + spin whir + crackle
def speed_at(t):
    t = np.asarray(t)
    s1 = .05 + .5 * prog(t, *T['side1']) ** 2
    s2 = .7 + .3 * (1 - (1 - prog(t, T['side2'][0], T['surge'][0])) ** 3)
    return np.where(t < T['side1'][1], s1, s2)


def roar(a, b, seed, sp_override=None):
    n, tt = seg(a, b)
    sp = speed_at(tt) if sp_override is None else sp_override(tt)
    low = band(n, 40, 180, seed) * (.5 + .5 * sp)
    mid = band(n, 180, 900, seed + 1) * (.25 + .6 * sp)
    hi = band(n, 900, 4000, seed + 2) * (.1 + .7 * sp ** 1.5)
    air = band(n, 4000, 12000, seed + 3) * sp ** 2 * .45
    flame = (low + mid + hi + air) * (1 + .25 * np.sin(2 * np.pi * 11 * tt) * np.sin(2 * np.pi * 3.3 * tt))
    hum_f = 70 + 160 * sp
    hum = osc(hum_f, n, 'saw') * .25 + osc(hum_f * 2.01, n, 'sine') * .15
    # spin whir: amplitude-modulated tone at the star passing rate
    whir = osc(300 + 900 * sp, n, 'tri') * (.5 + .5 * np.sin(2 * np.pi * (6 + 30 * sp) * tt)) * .12
    crk = crackle(n, 30 + 220 * sp, seed + 4) * .9
    s = flame * .3 + lowpass(hum, 900) + whir + crk
    fade = clamp((tt - a) / .03) * clamp((b - tt) / .02)
    return s * fade


put(roar(*T['side1'], 100), T['side1'][0], .55)
put(roar(*T['side2'], 200), T['side2'][0], .6)

# cut-in flashes (white frames at start of each side shot)
for ts in (T['side1'][0], T['side2'][0]):
    put(band(int(.3 * SR), 1500, 10000) * env_ad(int(.3 * SR), .001, .06), ts, .4)

# sonic booms
put(sonic_boom(), T['boom1'], .95)
put(sonic_boom(), T['boom2'], 1.05)

# insert close-ups: sharp shing + whoosh stab
for (a, b), pan in ((T['insert1'], -.3), (T['insert2'], .3)):
    put(shing(.6), a, .45, pan)
    put(whoosh(b - a + .05, 800, 3000, 2000, 9000, 'swell', 31), a, .5, pan)
    put(kick(220, 70, .25, .06, .5), a, .35)

# 7) front shot: rising scream + approach, then glued-to-lens pass-by (doppler drop, L->R pan)
n, tt = seg(*T['front'])
k = (tt - T['front'][0]) / (T['front'][1] - T['front'][0])
s = roar(*T['front'], 300, sp_override=lambda x: .55 + .45 * prog(x, *T['front']))
s += osc(300 + 900 * k ** 2, n, 'saw') * k ** 2 * .12
put(s, T['front'][0], .55)
pb = T['front'][1] - .16
n = int(.45 * SR)
tt = np.arange(n) / SR
f = np.where(tt < .16, 900 + 600 * tt / .16, 1500 * np.exp(-(tt - .16) / .08) + 180)
s = osc(f, n, 'saw') * .25 + whoosh(.45, 1500, 9000, 150, 1200, 'swell', 41)
s *= np.where(tt < .16, (tt / .16) ** 2, np.exp(-(tt - .16) / .09))
put(lowpass(s, 7000), pb, .9, np.clip(-1 + 2 * tt / .3, -1, 1))
put(kick(130, 36, .7, .2, .6), T['front'][1], .8)

# 8) surge: ball rips out of frame to the right
n, tt = seg(T['surge'][0], T['surge'][1] + .3)
k = clamp((tt - T['surge'][0]) / (T['surge'][1] - T['surge'][0]))
s = whoosh(len(tt) / SR, 600, 3000, 2500, 12000, 'swell', 51) * 1.1
s += osc(400 + 1400 * k ** 2, n, 'saw') * np.sin(np.pi * clamp(k)) * .15
put(s, T['surge'][0], .6, np.clip(-.2 + 1.4 * k, -1, 1))

# 9) arrival: incoming howl from the left, then the HIT
n, tt = seg(T['side2'][1], HIT)
k = (tt - T['side2'][1]) / (HIT - T['side2'][1])
s = whoosh(HIT - T['side2'][1], 300, 2000, 1500, 10000, 'rise', 61) * 1.1
s += osc(220 + 1100 * k ** 2.5, n, 'saw') * k ** 2 * .22
s += crackle(n, 300, 9) * .6
put(lowpass(s, 2000 + 8000 * k), T['side2'][1], .75, np.clip(-1 + 1.1 * k, -1, 1))
put(band(int(.3 * SR), 1500, 10000) * env_ad(int(.3 * SR), .001, .06), T['side2'][1], .35)

# THE HIT: sub boom + crack + metallic clang of the sphere + shimmer
put(kick(150, 30, 2.2, .5, 1.0), HIT, 1.25)
n = int(.25 * SR)
put(band(n, 400, 12000) * env_ad(n, .0005, .03), HIT, .9)
put(bell([392, 931, 1532, 2266, 3013, 3870], 3.0, 1.1, [1, .8, .7, .5, .35, .25], 2.2), HIT, .38)
put(bell([1568, 2349, 3136], 2.5, .8), HIT + .02, .18)
n = int(1.4 * SR)
tt = np.arange(n) / SR
put(band(n, 30, 140) * np.exp(-tt / .5) * clamp(tt / .01), HIT, .6)

# title letters: quick slaps, each a tiny kick + tick
for i in range(11):
    put(kick(300, 110, .12, .03, .8), HIT + .03 + i * .03 + .12, .22, -.3 + .06 * i)

# subtitle typing ticks
for i in range(14):
    put(band(int(.02 * SR), 2000, 8000) * env_ad(int(.02 * SR), .0005, .004), HIT + .45 + i * .3 / 14, .12, .3)

# glints across the title, twinkles on the ball
for ts in (HIT + .95, HIT + 2.1):
    put(shing(1.0), ts, .3, .4)
for ts in (HIT + 1.2, HIT + 2.4):
    put(bell([4699, 7040], .6, .15), ts, .2, -.4)

# warm resolving pad under the final hold
n, tt = seg(HIT + .2, DUR)
pad = sum(osc(f, n, 'tri') * a for f, a in ((110, .5), (164.8, .35), (220, .3), (277.2, .18), (329.6, .15)))
pad = lowpass(pad, 1400) * clamp((tt - HIT - .2) / .6) * clamp((DUR - tt) / 1.0)
put(pad * (1 + .05 * np.sin(2 * np.pi * .7 * tt)), HIT + .2, .12)
put(crackle(n, 12, 77) * clamp((DUR - tt) / 1.0), HIT + .2, .15)

# ---------------- reverb + master ----------------
ir_n = int(1.3 * SR)
ir_t = np.arange(ir_n) / SR
ir = np.stack([np.random.default_rng(s).standard_normal(ir_n) for s in (1, 2)], 1) * np.exp(-ir_t / .32)[:, None]
ir[:int(.012 * SR)] = 0
ir /= np.sqrt((ir ** 2).sum(0))
L = N + ir_n
wet = np.stack([np.fft.irfft(np.fft.rfft(out[:, c], L) * np.fft.rfft(ir[:, c], L), L)[:N] for c in (0, 1)], 1)
mix = out + wet * .28

# gentle high-shelf tame + soft clip + normalize
mix = np.tanh(mix * 1.1)
mix /= np.max(np.abs(mix)) + 1e-9
mix *= 10 ** (-1 / 20)
fade = clamp((DUR - t_all) / .25)[:, None]
mix *= fade

pcm = (mix * 32767).astype('<i2')
with wave.open(sys.argv[1] if len(sys.argv) > 1 else 'audio.wav', 'wb') as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes(pcm.tobytes())
print('ok', N / SR, 's')
