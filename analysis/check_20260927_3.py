"""czenvrec/20260927_3: DCW step 1 = (24, 50) (code 64) sustain, at (note 96, KF 0) and
(note 72, KF 9). With level 99 (20260927_2) the rise stops at code 75.1 and 100.8 there.
How the limit acts on a lower level:
  cap       min(64, S)                -> 64    / 64
  scale     64 * S / 127              -> 37.8  / 50.8
  subtract  max(0, 64 - (127 - S))    -> 12.1  / 37.8
The stop is found as the time the take departs from the level-99 take at the same note/KF
(both rise at 14.95 codes/s until then) and as the knee of dcw(t).
"""
import numpy as np, os
from dcwkf import files, dcw_track, REC
from dcwkf2 import spectra, hinge, CODES_PER_S

if __name__ == '__main__':
    l99 = {(kf, n): f for f, kf, n in files()}
    for kf, n, s99 in ((0, 96, 75.1), (9, 72, 100.8)):
        f50 = os.path.join(REC, '20260927_3', 'dcw_r1_24_l1_50_kf_%d_note_%d.wav' % (kf, n))
        t, a = spectra(l99[(kf, n)], n); _, b = spectra(f50, n)
        i = np.argmax((np.abs(b[:, 1] - a[:, 1]) > 0.1) & (t > 0.3))
        te, est, _ = dcw_track(f50, n)
        ts = hinge(te, est, t0=0.1)
        print('note %d KF %d: stop at code %.1f (departure) / %.1f (knee)   '
              'cap %.1f  scale %.1f  subtract %.1f'
              % (n, kf, CODES_PER_S*t[i], CODES_PER_S*ts, min(64, s99), 64*s99/127, max(0, 64 - (127 - s99))))
