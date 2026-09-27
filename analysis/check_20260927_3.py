"""czenvrec/20260927_3: DCW step 1 = (24, level) sustain with a level below 99, to see how
the high-note limit acts on a lower level (DCW rate unchanged: 14.95 codes/s).
With level 99 (20260927_2) the rise stops at code S99(KF, note). Candidate models for a
level code L:
  cap       min(L, S99)
  scale     L * S99 / 127
  subtract  max(0, L - (127 - S99))
The stop is found as the time the take departs from the level-99 take at the same note/KF
(both rise identically until then) and as the knee of dcw(t).
"""
import numpy as np, os, re
from dcwkf import files, dcw_track, REC
from dcwkf2 import spectra, hinge, CODES_PER_S

S99 = {(kf, 96): 75.1 for kf in range(7)}          # stop codes at level 99 (dcwkf2.py)
S99.update({(kf, 84): 102.3 for kf in range(8)})
S99.update({(7, 96): 74.5, (8, 96): 53.3, (8, 84): 94.9, (9, 96): 0.0, (9, 84): 68.8, (9, 72): 100.8})

def level_code(level):
    return (127*level + 49)//99

def stop(f, f99, note):
    t, b = spectra(f, note)
    if np.median(b[-10:, 1]) < -40:
        return 0.0, 0.0
    _, a = spectra(f99, note)
    i = np.argmax((np.abs(b[:, 1] - a[:, 1]) > 0.1) & (t > 0.3))
    te, est, _ = dcw_track(f, note)
    return CODES_PER_S*t[i], CODES_PER_S*hinge(te, est, t0=0.1)

if __name__ == '__main__':
    l99 = {(kf, n): f for f, kf, n in files()}
    d = os.path.join(REC, '20260927_3')
    rows = []
    for name in os.listdir(d):
        m = re.match(r'dcw_r1_24_l1_(\d+)_kf_(\d)_note_(\d+)\.wav$', name)
        if m:
            rows.append(tuple(int(x) for x in m.groups()) + (os.path.join(d, name),))
    print('note  KF level   L | stop: departure  knee | S99  | cap  scale subtract')
    for level, kf, n, f in sorted(rows, key=lambda r: (r[2], r[1])):
        L = level_code(level); s99 = S99[(kf, n)]
        dep, knee = stop(f, l99[(kf, n)], n)
        print('%4d %3d %5d %4d |      %6.1f %6.1f | %5.1f | %5.1f %5.1f %6.1f'
              % (n, kf, level, L, dep, knee, s99, min(L, s99), L*s99/127, max(0, L - (127 - s99))))
