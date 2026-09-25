"""Effective DCA volume curve dB(x) from the slow sweep attacks (x = level code 0..127)."""
import numpy as np, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from dcasweep import *

REF = -14.90
ANCH = [(29, -50.60), (52, -37.83), (77, -25.10), (102, -12.52), (127, 0.0)]

def curve(fname, win, hop, post):
    t, S, floor, h, ons = takes(fname, win_s=win, hop_s=hop, pre=0.3, post=post)
    m = S.mean(0)
    nfl = np.median(m[t < -0.1])
    db = 10*np.log10(np.maximum(m - nfl, 1e-20)) - REF
    t = t + win/2
    ts = []
    for x, a in ANCH[1:4]:
        i = np.where(db > a)[0][0]
        ts.append(t[i-1] + (a-db[i-1])/(db[i]-db[i-1])*(t[i]-t[i-1]))
    A = np.polyfit(ts, [52, 77, 102], 1)       # x(t) from the well-conditioned anchors
    return np.polyval(A, t), db, A

if __name__ == '__main__':
    fig, ax = plt.subplots(1, 2, figsize=(16, 7))
    res = {}
    for f, r, win, hop, post in [('dca_r1_01_l1_99.wav', 1, 0.02, 0.002, 15.0),
                                 ('dca_r1_24_l1_99.wav', 24, 0.004, 0.0005, 1.6)]:
        x, db, A = curve(f, win, hop, post)
        res[r] = (x, db)
        sel = (x > -2) & (x < 128)
        ax[0].plot(x[sel], db[sel], lw=0.8, label='rate %d (v=%.2f code/s, x0=%.2f)' % (r, A[0], np.polyval(A, 0)))
        ax[1].plot(x[sel], db[sel] - 0.5008*(x[sel]-127), lw=0.8, label='rate %d' % r)
    ax[0].plot([a[0] for a in ANCH], [a[1] for a in ANCH], 'ko', label='level sweep sustain (l+28)')
    ax[1].plot([a[0] for a in ANCH], [a[1] - 0.5008*(a[0]-127) for a in ANCH], 'ko')
    xx = np.linspace(0, 127, 128)
    ax[0].plot(xx, 0.5008*(xx-127), 'k--', lw=0.5)
    for a in ax: a.grid(alpha=0.3); a.legend(fontsize=8); a.set_xlabel('x (DCA level code)')
    ax[0].set_ylim(-75, 3); ax[1].set_ylim(-10, 3); ax[1].set_ylabel('deviation from 0.5008 dB/code line')
    plt.tight_layout(); plt.savefig('plots/volcurve2.png', dpi=80)
    x, db = res[1]
    for xi in list(range(0, 40, 2)) + list(range(40, 128, 10)):
        i = np.argmin(abs(x - xi)); print('x=%3d  dB=%7.2f  dev=%6.2f' % (xi, db[i], db[i] - 0.5008*(xi-127)))
