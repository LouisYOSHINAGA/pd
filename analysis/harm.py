"""Per-harmonic power traces, averaged over aligned takes."""
import numpy as np
from rmsenv import analyse

def harmonic_traces(f, f0, nharm=16, win_s=0.046, hop_s=0.005, tmax=8.0, bw=None):
    d = analyse(f, hop_s=0.001)
    x, sr = d['x'], d['sr']
    hop_env = d['hop']
    N = int(round(win_s*sr))
    H = int(round(hop_s*sr))
    w = np.hanning(N)
    fr = np.fft.rfftfreq(N, 1/sr)
    if bw is None:
        bw = f0*0.25
    bands = [(fr > k*f0 - bw) & (fr < k*f0 + bw) for k in range(1, nharm+1)]
    nfr = int((tmax*sr - N)//H)
    acc = np.zeros((nfr, nharm)); tot = np.zeros(nfr)
    for o, s in zip(d['ons'], d['shifts']):
        i0 = int(round((o + s)*hop_env*sr))
        seg = x[i0:i0 + int(tmax*sr)]
        if len(seg) < int(tmax*sr): continue
        from numpy.lib.stride_tricks import sliding_window_view
        fr_idx = np.arange(nfr)*H
        frames = np.stack([seg[j:j+N] for j in fr_idx])*w
        P = np.abs(np.fft.rfft(frames, axis=1))**2
        for k, b in enumerate(bands):
            acc[:, k] += P[:, b].sum(axis=1)
        tot += P.sum(axis=1)
    t = (np.arange(nfr)*H + N/2)/sr
    return t, acc/len(d['ons']), tot/len(d['ons']), d
