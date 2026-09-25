import numpy as np, os
from wavutil import load, REC_DIR, files

def mono(path):
    x, sr = load(path)
    if x.ndim > 1:
        x = x[:, 0]
    return x, sr

def slide_max(x, w):
    """Sliding maximum of |x| over window w (centered-ish, causal start)."""
    n = len(x)
    a = np.abs(x)
    pad = (-n) % w
    if pad:
        a = np.concatenate([a, np.zeros(pad)])
    b = a.reshape(-1, w)
    bm = b.max(axis=1)
    # max over two consecutive blocks -> window between w and 2w
    out = np.maximum(bm, np.concatenate([bm[1:], bm[-1:]]))
    return out, w  # decimated by w

def onsets(env, hop_sr, period_s, thresh_rel=0.05):
    """Find note onsets from a decimated envelope."""
    thr = env.max() * thresh_rel
    on = env > thr
    idx = np.where((~on[:-1]) & (on[1:]))[0] + 1
    # keep only edges separated by > 0.5*period
    keep = []
    last = -1e9
    for i in idx:
        if i / hop_sr - last > 0.5 * period_s:
            keep.append(i)
            last = i / hop_sr
    return np.array(keep)

def note_period(ons, hop_sr):
    d = np.diff(ons) / hop_sr
    return np.median(d)
