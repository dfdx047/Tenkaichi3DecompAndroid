"""Compare the port's audio with a recording of the original (two 16-bit stereo WAV files, same rate).

Finds where the port's audio lies in the reference (cross-correlation of the loudness envelopes, then of
the samples), and reports per segment: the offset, the level ratio port/reference (RMS and the gain of a
least-squares fit), how alike the waveforms are, and which reference channel each port channel matches.
The files stay local; only numbers are printed.
"""
import argparse
import wave
from pathlib import Path

import numpy as np
from scipy.signal import correlate


def load(path):
    with wave.open(str(path)) as w:
        if w.getsampwidth() != 2 or w.getnchannels() != 2:
            raise ValueError(f'{path}: expected 16-bit stereo')
        data = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').reshape(-1, 2).astype(np.float64)
        return w.getframerate(), data


def envelope(mono, step):
    """RMS per `step` samples."""
    count = len(mono) // step
    return np.sqrt((mono[:count * step].reshape(count, step) ** 2).mean(axis=1))


def best_lag(a, b):
    """Lag of `a` inside `b` (b[lag + n] ~ a[n]) that maximizes their correlation, and that correlation."""
    a = a - a.mean()
    b = b - b.mean()
    c = correlate(b, a, mode='full', method='fft')
    k = int(np.argmax(c))
    lag = k - (len(a) - 1)
    lo, hi = max(0, lag), min(len(b), lag + len(a))
    x, y = a[lo - lag:hi - lag], b[lo:hi]
    denom = np.sqrt((x * x).sum() * (y * y).sum())
    return lag, float((x * y).sum() / denom) if denom else 0.0


def similarity(x, y):
    denom = np.sqrt((x * x).sum() * (y * y).sum())
    return float((x * y).sum() / denom) if denom else 0.0


def segment_report(port, ref, rate, start, length, coarse_lag, search):
    """One port segment against the reference near `coarse_lag`: exact lag, level and channel match."""
    a = port[start:start + length]
    lo = max(0, start + coarse_lag - search)
    b = ref[lo:lo + length + 2 * search]
    if len(a) < length or len(b) < length or not a.any():
        return None
    lag, _ = best_lag(a.sum(axis=1), b.sum(axis=1))
    lag = min(max(lag, 0), len(b) - length)
    r = b[lag:lag + length]
    rms = lambda v: float(np.sqrt((v * v).mean()))
    fit = lambda x, y: float((x * y).sum() / (y * y).sum()) if (y * y).sum() else 0.0  # x ~ gain * y
    return {
        'port_s': start / rate, 'ref_s': (lo + lag) / rate, 'offset_s': (lo + lag - start) / rate,
        'rms_ratio': [rms(a[:, c]) / rms(r[:, c]) if rms(r[:, c]) else 0.0 for c in (0, 1)],
        'fit_gain': [fit(a[:, c], r[:, c]) for c in (0, 1)],
        'same': [similarity(a[:, c], r[:, c]) for c in (0, 1)],
        'swapped': [similarity(a[:, c], r[:, 1 - c]) for c in (0, 1)],
        'port_peak': float(np.abs(a).max()), 'ref_peak': float(np.abs(r).max()),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('port', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--segment', type=float, default=5.0, help='Segment length in seconds (default: %(default)s).')
    parser.add_argument('--search', type=float, default=2.0, help='Fine search around the coarse offset, seconds (default: %(default)s).')
    args = parser.parse_args()
    rate, port = load(args.port)
    ref_rate, ref = load(args.reference)
    if rate != ref_rate:
        raise SystemExit('Sampling rates differ')
    step = rate // 100
    lag, score = best_lag(envelope(port.sum(axis=1), step), envelope(ref.sum(axis=1), step))
    coarse = lag * step
    print(f'port {len(port) / rate:.1f} s, reference {len(ref) / rate:.1f} s; envelope offset {coarse / rate:+.2f} s (correlation {score:.3f})')
    print(' port s   ref s  offset s  rms L/R (port/ref)  fit gain L/R   same L/R      swapped L/R   peak port/ref')
    length, search = int(args.segment * rate), int(args.search * rate)
    for start in range(0, len(port) - length + 1, length):
        s = segment_report(port, ref, rate, start, length, coarse, search)
        if s is None:
            continue
        print(f"{s['port_s']:7.1f} {s['ref_s']:7.1f} {s['offset_s']:+9.4f}   {s['rms_ratio'][0]:6.3f} {s['rms_ratio'][1]:6.3f}     "
              f"{s['fit_gain'][0]:6.3f} {s['fit_gain'][1]:6.3f}   {s['same'][0]:+.3f} {s['same'][1]:+.3f}   "
              f"{s['swapped'][0]:+.3f} {s['swapped'][1]:+.3f}   {s['port_peak']:6.0f} {s['ref_peak']:6.0f}")


if __name__ == '__main__':
    main()
