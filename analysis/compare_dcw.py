import numpy as np, os, sys, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from czparam import load
from dcwtrack import track, build_table
from render_all import render, SR
import render_all

CASES = [  # sub, preset no, f0 grid, wf1, wf2, smoothing, shift(rec->render), tmax
 ('synth_bass',     8, 220, 2, 3, 0.0,   0.018, 1.0),
 ('brass_ens1',     1, 440, 1, 0, 0.336, 0.298, 2.5),
 ('brass_ens2',     9, 440, 1, 0, 0.294, 0.256, 2.0),
 ('violin',         3, 880, 1, 3, 0.0,   0.012, 0.6),
 ('flute',          7, 880, 5, 0, 0.0,   0.010, 1.0),
 ('strings_ens1',   4, 440, 1, 0, 0.336, 0.282, 7.0),
 ('vibraphone',    10, 440, 1, 0, 0.0,   0.042, 1.0),
]
P = {p['no']: p for p in load()}

def track_signal(y, f0, w1, w2, nh=12, win_s=0.046, hop_s=0.005, smooth_s=0.0):
    N = int(round(win_s*SR)); H = int(round(hop_s*SR)); w = np.hanning(N)
    fr = np.fft.rfftfreq(N, 1/SR); bw = f0*0.25
    bands = [(fr > k*f0-bw) & (fr < k*f0+bw) for k in range(1, nh+1)]
    nfr = (len(y)-N)//H
    frames = np.stack([y[j*H:j*H+N] for j in range(nfr)])*w
    S = np.abs(np.fft.rfft(frames, axis=1))**2
    Pw = np.stack([S[:, b].sum(1) for b in bands], axis=1)
    if smooth_s > 0:
        k = max(1, int(round(smooth_s/hop_s))); ker = np.ones(k)/k
        Pw = np.stack([np.convolve(Pw[:, j], ker, mode='same') for j in range(nh)], axis=1)
    grid, T = build_table(w1, w2, nh)
    rdb = 10*np.log10(Pw+1e-30); rdb -= rdb.max(1, keepdims=True)
    est = np.array([grid[np.argmin(np.mean(np.abs(T[:, rdb[i] > -45] - np.maximum(rdb[i, rdb[i] > -45], -70)), 1))]
                    for i in range(nfr)])
    t = (np.arange(nfr)*H + N/2)/SR
    return t, est

fig, axes = plt.subplots(len(CASES), 1, figsize=(14, 4*len(CASES)))
for ax, (sub, no, f0, w1, w2, sm, sh, tmax) in zip(axes, CASES):
    t, est, err, tot = track(sub, f0, w1, w2, tmax=min(8.0, tmax+sh+0.2), smooth_s=sm)
    ax.plot(t - sh, est, lw=1.0, label='CZ-101')
    for tag in ('new', 'old'):
        render_all.EXE = os.path.join(render_all.HERE, 'harness', *(['old'] if tag == 'old' else []), 'render.exe')
        y = render(P[no], tag=tag)
        tr, er = track_signal(y, f0, w1, w2, smooth_s=sm)
        ax.plot(tr, er, lw=1.0, label='VST %s' % tag)
        m = (t - sh > 0.05) & (t - sh < tmax) & (tot > tot.max()-40)
        ei = np.interp(t[m]-sh, tr, er)
        print('%-14s %s: mean|dcw diff|=%.3f' % (sub, tag, np.mean(np.abs(ei-est[m]))))
    ax.set_xlim(0, tmax); ax.set_ylim(0, 1); ax.grid(alpha=0.3); ax.set_title('%d %s' % (no, sub)); ax.legend(fontsize=8)
plt.tight_layout(); plt.savefig('plots/vst_vs_cz_dcw.png', dpi=60)
