"""Measured dB vs chip accumulator position x (L7 units) for slow decays (rho=1)."""
import numpy as np, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from rmsenv import analyse
from wavutil import files
import czeg

# (file, rate, x_start, t_start_guess_window, ref level x for alignment)
DEC = [
 ('vibraphone',    30, 127, (0.0, 0.3)),
 ('synth_bass',    21, 127, (0.0, 0.3)),
 ('fairy',         31,  93, (4.8, 5.4)),
 ('synth_strings', 32, 116, (4.8, 5.4)),
 ('brass_ens2',    33,  84, (5.0, 5.6)),
 ('whistle',       31, 127, (0.05, 0.2)),
]
fig, ax = plt.subplots(1, 2, figsize=(16, 7))
for sub, rate, x0, (ta, tb) in DEC:
    f = [x for x in files() if sub in x][0]
    d = analyse(f, hop_s=0.001, win_s=None)
    t, db = d['t'], d['db']
    v = czeg.step_of(czeg.rate_code(rate))*czeg.TICK/(1 << czeg.SHIFT)   # units/s
    # find decay start: the point where the trace leaves the plateau / peak
    w = (t >= ta) & (t <= tb)
    if sub in ('vibraphone', 'synth_bass', 'whistle'):
        i0 = np.where(w)[0][np.argmax(db[w])]
    else:
        # plateau before t=5, start where drop exceeds 0.5 dB
        plateau = np.median(db[(t > 4.0) & (t < 4.8)])
        idx = np.where(w & (db < plateau - 0.5))[0]
        i0 = idx[0]
    t0 = t[i0]
    x = x0 - v*(t - t0)
    sel = (t >= t0) & (x > -5)
    ref = db[i0]
    ax[0].plot(x[sel], db[sel] - ref + 0.495*(x0 - 127), lw=0.8, label='%s r%d from x=%d' % (sub, rate, x0))
    ax[1].plot(t[sel] - t0, db[sel] - ref, lw=0.8, label=sub)
xx = np.linspace(0, 127, 200)
ax[0].plot(xx, 0.495*(xx - 127), 'k--', lw=0.6, label='0.495 dB/unit')
ax[0].set_xlabel('x (L7 units)'); ax[0].set_ylabel('dB rel. x=127'); ax[0].grid(alpha=0.3); ax[0].legend(fontsize=8)
ax[0].set_xlim(130, -5); ax[0].set_ylim(-80, 3)
ax[1].grid(alpha=0.3); ax[1].legend(fontsize=8)
plt.tight_layout(); plt.savefig('plots/volcurve.png', dpi=80)
print('ok')
