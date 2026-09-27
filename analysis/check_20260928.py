"""czenvrec/20260928, 20260928_2: DCW key follow table and high-note cap over more keys.

  20260928    KF 3, 5, 7, 9 x keys 60..90, DCW step 1 = (24, 50) (code 64, below the cap):
              stop = 64 - S(KF, key)
  20260928_2  KF 9 x keys 90, 93, DCW (24, 99): stop = 127 - S(9, key)
              KF 0 x keys 60..93, DCW (30, 99): stop = cap(key) (127 if no cap)
The stop time is the knee of a line + flat fit to dcw(t) (dcwkf2.hinge); the rise speed of
the rate gives the code. The key of every file is measured from its pitch (the KF 3 "note 69"
take is at key 66).
"""
import numpy as np, os, re
from dcwkf import dcw_track, load, onsets, REC
from dcwkf2 import spectra, hinge

TICK = 8.96e6/256

def codes_per_s(rate):
    n = 119*rate//99 + 2
    return ((8 + n % 8) << (n // 8))*TICK/(1 << 18)

def key_of(path):
    x, sr = load(path)
    o = onsets(x, sr)[0]
    N = 1 << 15
    X = np.abs(np.fft.rfft(x[o + sr: o + sr + N]*np.hanning(N)))
    f = np.fft.rfftfreq(N, 1/sr)[np.argmax(X)]
    return 69 + 12*np.log2(f/(440*1.0032))

def stop_code(path, key, rate):
    t, r = spectra(path, key)
    if np.median(r[-10:, 1]) < -40:
        return 0.0
    te, est, _ = dcw_track(path, key)
    return min(127.0, codes_per_s(rate)*hinge(te, est, t0=0.1))

def files():
    out = []
    for sub in ('20260928', '20260928_2'):
        d = os.path.join(REC, sub)
        for name in os.listdir(d):
            m = re.match(r'dcw_r1_(\d+)_l1_(\d+)_kf_(\d)_note_(\d+)\.wav$', name)
            if m:
                out.append(tuple(int(v) for v in m.groups()) + (os.path.join(d, name),))
    return sorted(out, key=lambda r: (r[2], r[3]))

if __name__ == '__main__':
    print('rate level KF key (measured) |  stop code | level code - stop')
    for rate, level, kf, key, path in files():
        k = key_of(path)
        c = stop_code(path, int(round(k)), rate)     # analyse at the measured key
        L = 127*level//99
        print('  %2d   %2d   %d  %2d (%5.2f)     |   %6.1f   |  %6.1f%s' % (rate, level, kf, key, k, c, L - c,
              '' if abs(k - key) < 0.5 else '   <- file name says key %d' % key))
