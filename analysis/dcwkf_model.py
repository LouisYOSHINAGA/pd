"""DCW key follow and high-note DCW limit (provisional, from 20260927_2..4).

Two mechanisms (check_20260927_3.py, check_20260927_4.py):
  * key follow subtracts S(KF, key) level codes from every DCW step target (floor 0);
    the key is the sounding key (key + 12 * octave range), the DCO envelope is not included.
  * independently of the key follow, the DCW output is capped at cap(f), f = the current
    oscillator frequency including the DCO envelope.

S(KF, key) is modelled as s(KF) * g(f(key)): s(KF) = S at key 84 (KF 1..9 measured), g is
piecewise linear in frequency through the keys where S was measured (g(84) = 1). Measured
values are stop codes from dcwkf2.py (level 99) and check_20260927_3.py (level 50 / 75).
Writes dcw_keyfollow_s.csv (rows KF 0..9, columns keys 36..96) and prints the cap points.
"""
import numpy as np

def freq(note):
    return 440*2**((note - 69)/12)

# S(KF, 84): level 75 (code 96) - stop, KF 8/9 averaged with 127 - stop at level 99
S84 = [0, 5.0, 8.3, 10.7, 14.6, 17.3, 20.9, 24.5, (32.9 + 32.1)/2, (58.9 + 58.2)/2]
# S(KF, 96) for KF 1..8: level 50 (code 64) - stop; KF 8 from level 99 (127 - 53.3)
S96 = [None, 14.9, 19.7, 25.7, 33.7, 40.3, 46.6, 54.1, 73.7]
# other keys, KF 8/9 (127 - stop at level 99; key 72 KF 9 averaged with level 50)
S_OTHER = {(9, 69): 19.9, (9, 72): (26.2 + 27.2)/2, (8, 72): 14.2, (9, 81): 48.3}
# DCW cap (code) at the oscillator frequency: KF 0 stops at level 99 (key 81, 84, 96; DCO +12
# at key 84 and the DCO glide give the key 96 value). At 440 Hz and below no cap was seen.
CAP_POINTS = [(freq(69), 127.0), (freq(81), 106.2), (freq(84), 102.3), (freq(96), 75.1)]

def g_points():
    pts = {freq(48): 0.0, freq(84): 1.0}
    pts[freq(96)] = float(np.median([S96[kf]/S84[kf] for kf in range(1, 9)]))
    for n in (69, 72, 81):
        pts[freq(n)] = float(np.mean([v/S84[kf] for (kf, m), v in S_OTHER.items() if m == n]))
    return sorted(pts.items())

def s_table():
    fx, gy = zip(*g_points())
    tab = np.zeros((10, 61), dtype=int)
    for kf in range(10):
        for i, n in enumerate(range(36, 97)):
            tab[kf, i] = min(127, int(round(S84[kf]*np.interp(freq(n), fx, gy, left=0.0))))
    return tab

def cap(f):
    fx, cy = zip(*CAP_POINTS)
    if f > fx[-1]:
        slope = (cy[-1] - cy[-2])/(fx[-1] - fx[-2])
        return max(0.0, cy[-1] + slope*(f - fx[-1]))
    return float(np.interp(f, fx, cy))

if __name__ == '__main__':
    print('g(f):', ' '.join('%.0fHz:%.3f' % p for p in g_points()))
    tab = s_table()
    path = 'dcw_keyfollow_s.csv'
    with open(path, 'w') as f:
        f.write('# DCW key follow: level codes subtracted from the DCW targets, rows = key follow 0..9, columns = key 36..96\n')
        f.write('kf,' + ','.join(str(n) for n in range(36, 97)) + '\n')
        for kf in range(10):
            f.write('%d,' % kf + ','.join(str(v) for v in tab[kf]) + '\n')
    print('wrote', path)
    for kf in range(10):
        print('  kf %d: ' % kf + ' '.join('%3d' % v for v in tab[kf, ::6]) + '   (keys 36,42,...,96)')
    print('check against the measured values (model - measured):')
    meas = {(kf, 84): S84[kf] for kf in range(1, 10)}
    meas.update({(kf, 96): S96[kf] for kf in range(1, 9)})
    meas.update(S_OTHER)
    for (kf, n), v in sorted(meas.items()):
        print('  kf %d key %d: %+5.1f' % (kf, n, tab[kf, n - 36] - v))
    print('cap points (Hz, code):', ', '.join('(%.1f, %.1f)' % p for p in CAP_POINTS),
          ' slope above: %.5f codes/Hz' % ((CAP_POINTS[-1][1] - CAP_POINTS[-2][1])/(CAP_POINTS[-1][0] - CAP_POINTS[-2][0])))
