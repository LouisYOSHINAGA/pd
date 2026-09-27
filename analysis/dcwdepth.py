"""How deep is the phase distortion of the CZ-101 at a given DCW level code?

czenvrec/20260927_2 (KF 0, saw): the DCW rises 14.95 codes/s from 0 on keys 36..96, so the
level code is known at every frame. The harmonic levels (relative to H1) are compared with the
oscillator model at depth c = a * code / 127 (pd.cpp: breakpoint pi * (1 - c)), with a fixed
per-key, per-harmonic offset for the output frequency response. For each a the offsets are the
least-squares optimum; the a with the smallest residual is the depth of the CZ-101 at code 127.
Result: a = 0.970 on every key, linear in the code (the VST used 0.95 * 0.95 = 0.9025 before;
now pd.h kDcwMaxDepth = 0.97).
"""
import numpy as np
import pdosc
from dcwkf import files
from dcwkf2 import spectra, CODES_PER_S

STOP = {81: 7.0, 84: 6.7, 96: 4.9}   # keys whose rise stops at the cap: use frames before it

def model_db(c, nh):
    h = pdosc.harmonics(1, 0, c/pdosc.C, nh)     # pdosc takes the dcw value, depth = C * dcw
    return 10*np.log10(h/h[0] + 1e-30)

def load_data():
    data = []
    for f, kf, n in files():
        if kf != 0:
            continue
        t, r = spectra(f, n)
        tmax = STOP.get(n, 7.8)
        sel = (t > 0.5) & (t < tmax)
        data.append((n, CODES_PER_S*t[sel], r[sel]))
    return data

def residual(data, depth_of_code, floor=-45.0):
    sse = 0.0; cnt = 0; offsets = {}
    for n, code, r in data:
        nh = r.shape[1]
        M = np.array([model_db(depth_of_code(c), nh) for c in code])
        D = r[:, 1:] - M[:, 1:]
        use = (r[:, 1:] > floor) & (M[:, 1:] > floor)
        G = np.array([np.mean(D[use[:, k], k]) if use[:, k].any() else 0.0 for k in range(nh - 1)])
        E = (D - G)[use]
        sse += np.sum(E**2); cnt += E.size; offsets[n] = G
    return np.sqrt(sse/cnt), offsets

if __name__ == '__main__':
    data = load_data()
    print('depth at code 127 (linear in the code): rms residual [dB]')
    best = None
    for a in np.arange(0.88, 1.001, 0.01):
        rms, _ = residual(data, lambda c: min(a*c/127, 0.999))
        print('   a = %.3f: %.3f dB' % (a, rms))
        if best is None or rms < best[0]:
            best = (rms, a)
    rms, G = residual(data, lambda c: min(best[1]*c/127, 0.999))
    print('best a = %.3f (rms %.3f dB); response offsets H2..H5 per key [dB]:' % (best[1], rms))
    for n in sorted(G):
        print('   key %2d: %s' % (n, ' '.join('%+.2f' % g for g in G[n][:4])))
