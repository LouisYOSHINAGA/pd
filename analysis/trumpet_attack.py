"""Trumpet (preset 2): speed and start of the first DCA stage (rate 70 -> level 91), CZ-101 vs VST.

The rise toward the first target is fitted with the volume model (attack_start.volume_db),
code(t) = speed * (t - t0). For the CZ-101, E = release start - t0 - gate is compared with the
DCA sweeps (+12.3 ms, attack_start.py); the release here is slower (rates 56 / 48), which
delays the 1 dB release point by ~7 ms.
Result: CZ-101 1990 codes/s, VST 2128 codes/s (the VST is ~7% faster; KF 2 at A4 gives
13/12 in eg.cpp, the CZ-101 looks closer to 12/12); both start at code 0.
"""
import numpy as np, os
from scipy.optimize import least_squares
from attack_start import volume_db
from rmsenv import analyse
from wavutil import files
from czparam import load
from render_all import render, SR

HOP = 0.0005

def trace(y, sr, period):
    w = int(round(period*sr)); h = int(HOP*sr)
    c = np.concatenate([[0], np.cumsum(y*y)])
    idx = np.arange((len(y) - w)//h)*h
    return (idx + w/2)/sr, 10*np.log10((c[idx + w] - c[idx])/w + 1e-14)

def fit_first_rise(t, db, sus_db, peak_code=119):
    """Fit code(t) = speed * (t - t0) to the rise toward the first step target."""
    rel = db - sus_db
    top = np.argmax(rel > volume_db(peak_code) - 1.5)          # just below the step-1 target
    m = (rel > -50) & (np.arange(len(rel)) < top)
    lm = m & (rel < -8)
    slope, icpt = np.polyfit(t[lm], rel[lm], 1)
    sp0 = slope/0.5017; t00 = t[lm][0] - (rel[lm][0]/0.5017 + 127)/sp0
    f = least_squares(lambda p: volume_db(np.clip(p[1]*(t[m] - p[0]), 0, 127)) - rel[m], [t00, sp0],
                      x_scale=[1e-3, sp0])
    return f.x, np.sqrt(np.mean(f.fun**2))

P = {p['no']: p for p in load()}
f = [x for x in files() if 'trumpet' in x][0]
d = analyse(f, hop_s=0.001); x, sr = d['x'], d['sr']
period = 1/441.4
res = []
for o, s in zip(d['ons'], d['shifts']):
    i0 = int(round((o + s)*d['hop']*sr))
    seg = x[i0 - int(0.3*sr): i0 + int(5.6*sr)]
    t, db = trace(seg, sr, period)
    t -= 0.3
    sus = np.median(db[(t > 3.5) & (t < 4.8)])
    (t0, speed), rms = fit_first_rise(t, db, sus)
    after = (t > 4.8) & (t < 5.6)
    rel_t = t[after][np.argmax(db[after] - sus < -1)]
    res.append((t0, speed, rms, rel_t - t0 - 5.0))
res = np.array(res)
print('CZ-101: first rise %.0f codes/s (takes %s), fit rms %.2f dB, E = release - t0 - gate = %+.1f ms (takes %s)'
      % (np.median(res[:, 1]), ' '.join('%.0f' % v for v in res[:, 1]), np.median(res[:, 2]),
         1e3*np.median(res[:, 3]), ' '.join('%+.1f' % (1e3*v) for v in res[:, 3])))
y = render(P[2], tag='new', gate=5.0, total=5.6)
t, db = trace(y, SR, period)
sus = np.median(db[(t > 3.5) & (t < 4.8)])
(t0, speed), rms = fit_first_rise(t, db, sus)
print('VST:    first rise %.0f codes/s, fit rms %.2f dB, t0 = %+.1f ms after note-on' % (speed, rms, 1e3*t0))
