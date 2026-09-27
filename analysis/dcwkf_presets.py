"""Which way does the DCW key follow limit act on a DCW level below 99? Checked with presets
whose DCW sustains at a known level at a note where the limit is measured (dcwkf2.py).

For a level code L and the stop code S measured with level 99 (code 127) at that note:
  cap       min(L, S)
  scale     L * S / 127
  subtract  max(0, L - (127 - S))
The note is the sounding note (key + 12 * octave range); the DCO envelope is not included.
"""
import numpy as np
from dcwtrack import track

CASES = [  # preset, grid f0, wf1, wf2, DCW sustain level code, sounding note, KF, S
    ('violin',     880, 1, 3, 108, 81, 0, 106.2),  # DCO +12 st sustained: oscillator at 1760 Hz
    ('flute',      880, 5, 0,  73, 81, 8, 99.4),   # KF 8 at note 81: interpolated between 72 and 84
    ('synth_bass', 220, 2, 3,  95, 69, 7, None),   # limit at note 69 not measured (> 117)
]

if __name__ == '__main__':
    print('%-11s %5s %4s %3s %6s | %6s %6s %6s %6s' % ('preset', 'level', 'note', 'KF', 'S', 'CZ', 'cap', 'scale', 'subtr'))
    for sub, f0, w1, w2, L, note, kf, S in CASES:
        t, est, err, _ = track(sub, f0, w1, w2, tmax=3.0)
        m = (t > 0.5) & (t < 2.5)
        cz = np.median(est[m])/0.95*127
        if S is None:
            print('%-11s %5d %4d %3d %6s | %6.1f  (level - CZ = %.1f)' % (sub, L, note, kf, '-', cz, L - cz))
            continue
        print('%-11s %5d %4d %3d %6.1f | %6.1f %6.1f %6.1f %6.1f' % (sub, L, note, kf, S, cz, min(L, S), L*S/127, max(0, L - (127 - S))))
