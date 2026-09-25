"""Estimate dcw(t) by matching per-frame harmonic spectra with the oscillator model."""
import numpy as np, sys, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from harm import harmonic_traces
from wavutil import files
import pdosc

def build_table(w1, w2, nh, grid=np.linspace(0, 1, 401)):
    T = np.array([10*np.log10(pdosc.harmonics(w1, w2, g, nh) + 1e-30) for g in grid])
    T = T - T.max(axis=1, keepdims=True)
    return grid, np.maximum(T, -70)

def track(sub, f0, w1, w2, nh=12, tmax=6.0, win_s=0.046, hop_s=0.005, smooth_s=0.0):
    f = [x for x in files() if sub in x][0]
    t, P, tot, d = harmonic_traces(f, f0, nh, tmax=tmax, win_s=win_s, hop_s=hop_s)
    if smooth_s > 0:
        k = max(1, int(round(smooth_s/hop_s)))
        ker = np.ones(k)/k
        P = np.stack([np.convolve(P[:, j], ker, mode='same') for j in range(P.shape[1])], axis=1)
        tot = np.convolve(tot, ker, mode='same')
    grid, T = build_table(w1, w2, nh)
    rdb = 10*np.log10(P + 1e-30)
    rdb = rdb - rdb.max(axis=1, keepdims=True)
    est = np.zeros(len(t)); err = np.zeros(len(t))
    for i in range(len(t)):
        m = rdb[i] > -45
        e = np.mean(np.abs(T[:, m] - np.maximum(rdb[i, m], -70)), axis=1)
        j = int(np.argmin(e)); est[i] = grid[j]; err[i] = e[j]
    return t, est, err, 10*np.log10(tot + 1e-30)

if __name__ == '__main__':
    sub, f0, w1, w2 = sys.argv[1], float(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
    tmax = float(sys.argv[5]) if len(sys.argv) > 5 else 6.0
    sm = float(sys.argv[6]) if len(sys.argv) > 6 else 0.0
    t, est, err, tot = track(sub, f0, w1, w2, tmax=tmax, smooth_s=sm)
    np.save('dcw_%s.npy' % sub, np.vstack([t, est, err, tot]))
    fig, ax = plt.subplots(2, 1, figsize=(16, 9), sharex=True)
    ax[0].plot(t, est, lw=0.8); ax[0].set_ylabel('dcw est'); ax[0].grid(alpha=0.3)
    ax[1].plot(t, err, lw=0.6); ax[1].set_ylabel('fit err dB'); ax[1].grid(alpha=0.3)
    a2 = ax[1].twinx(); a2.plot(t, tot, 'r', lw=0.5)
    plt.tight_layout(); plt.savefig('plots/dcw_%s.png' % sub, dpi=70)
    for tt in np.arange(0, tmax, 0.1):
        i = np.argmin(abs(t-tt)); print('%.2f %.3f %.2f %.1f' % (t[i], est[i], err[i], tot[i]))
