"""DCW key follow model checked with presets whose DCW sustains at a known level.

check_20260927_3.py shows two separate limits:
  * a cap that does not depend on the level (KF 0 at note 96: level 50 is not reduced),
    127 - cap ~ 0.024 * f[Hz] (fit to the KF 0 stops of dcwkf2.py at notes 81, 84, 96)
  * a key follow value S(KF, note) subtracted from the level (KF 9 at note 72)
Model: dcw code = min(max(0, L - S(KF, sounding note)), cap(f)).
Whether f is the key or the oscillator frequency including the DCO envelope is compared
with Violin, whose DCO sustains 12 semitones up.
"""
import numpy as np
from dcwtrack import track

S = {8: {72: 14.2, 84: 32.1, 96: 73.7},              # 127 - stop code at level 99 (dcwkf2.py)
     9: {69: 19.9, 72: 26.2, 81: 48.3, 84: 58.2}}

def freq(note):
    return 440*2**((note - 69)/12)

def subtract(kf, note):
    if kf not in S:
        return 0.0
    n = sorted(S[kf])
    return float(np.interp(freq(note), [freq(x) for x in n], [S[kf][x] for x in n]))

def cap(f):
    return min(127.0, 127 - 0.024*f)

CASES = [  # preset, grid f0, wf1, wf2, DCW sustain level code, sounding note, DCO shift [st], KF
    ('violin',     880, 1, 3, 108, 81, 12, 0),
    ('flute',      880, 5, 0,  73, 81,  0, 8),
    ('synth_bass', 220, 2, 3,  95, 69,  0, 7),
]

if __name__ == '__main__':
    print('%-11s %5s %4s %4s %3s | %6s | %11s %11s' % ('preset', 'level', 'note', 'DCO', 'KF', 'CZ', 'f = key', 'f = osc'))
    for sub, f0, w1, w2, L, note, dco, kf in CASES:
        t, est, err, _ = track(sub, f0, w1, w2, tmax=3.0)
        m = (t > 0.5) & (t < 2.5)
        cz = np.median(est[m])*127
        lv = max(0.0, L - subtract(kf, note))
        print('%-11s %5d %4d %+4d %3d | %6.1f | %11.1f %11.1f'
              % (sub, L, note, dco, kf, cz, min(lv, cap(freq(note))), min(lv, cap(freq(note + dco)))))
