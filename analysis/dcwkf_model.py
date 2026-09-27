"""DCW key follow table and high-note DCW cap, from the recordings 20260927_2 .. 20260928_2.

Two mechanisms (check_20260927_3.py, check_20260927_4.py):
  * key follow subtracts S(KF, key) level codes from every DCW step target (floor 0);
    the key is the sounding key (key + 12 * octave range), the DCO envelope is not included.
  * independently of the key follow, the DCW output is capped at cap(f), f = the current
    oscillator frequency including the DCO envelope.

S is modelled as s(KF) * g(key) (least squares over all measured points; the residuals are
printed), with g linear in frequency between the measured keys and falling to 0 below
key 60. Writes dcw_keyfollow_s.csv (rows KF 0..9, columns keys 36..96) and prints the cap
points used by eg.cpp.
"""
import numpy as np

def freq(note):
    return 440*2**((note - 69)/12)

# Measured S(KF, key) in level codes. Level 50/75: level code - stop; level 99: 127 - stop.
S_MEAS = {
    # 20260927_3: key 84 at level 75, key 96 at level 50
    (1, 84): 5.0, (2, 84): 8.3, (3, 84): 10.7, (4, 84): 14.6, (5, 84): 17.3, (6, 84): 20.9,
    (7, 84): 24.5, (8, 84): (32.9 + 32.1)/2, (9, 84): (58.9 + 58.2)/2,
    (1, 96): 14.9, (2, 96): 19.7, (3, 96): 25.7, (4, 96): 33.7, (5, 96): 40.3, (6, 96): 46.6,
    (7, 96): 54.1,
    # 20260927_2 (level 99) and 20260927_3 (key 72, level 50)
    (8, 72): 14.2, (8, 96): 73.7, (9, 69): 19.9, (9, 81): 48.3,
    # 20260928 (level 50) and 20260927_2/_3 for KF 9 key 72
    (3, 60): 0.9, (3, 66): 2.9, (3, 72): 4.7, (3, 78): 7.7, (3, 90): 18.5,
    (5, 60): 2.0, (5, 66): 5.0, (5, 72): 8.0, (5, 78): 11.9, (5, 90): 26.3,
    (7, 60): 3.8, (7, 66): 6.8, (7, 72): 10.7, (7, 78): 17.0, (7, 90): 37.0,
    (9, 60): 9.5, (9, 66): 17.0, (9, 72): (26.2 + 27.2 + 27.2)/3, (9, 78): 40.3,
    # 20260928_2 (level 99)
    (9, 90): 86.9, (9, 93): 104.2,
}
# KF 9 at key 96 goes to a pure sine at level 99: S(9, 96) >= 127 (checked, not fitted)

# DCW cap (level code) per key with key follow 0: 20260928_2 (rate 30) and 20260927_2 (keys
# 81, 84, 96; key 96 also reached through the DCO envelope in 20260927_4).
CAP_MEAS = {60: 123.9, 66: 120.9, 72: 116.1, 75: 113.1, 78: 110.1, 81: 106.2, 84: 102.3,
            87: 98.2, 90: 91.6, 93: 82.0, 96: 75.1}

def fit_separable(meas, iters=200):
    kfs = sorted(set(kf for kf, _ in meas)); keys = sorted(set(k for _, k in meas))
    s = {kf: 1.0 for kf in kfs}
    g = {k: np.mean([v for (kf, kk), v in meas.items() if kk == k]) for k in keys}
    for _ in range(iters):
        for kf in kfs:
            pts = [(g[k], v) for (f, k), v in meas.items() if f == kf]
            s[kf] = sum(a*v for a, v in pts)/sum(a*a for a, _ in pts)
        for k in keys:
            pts = [(s[f], v) for (f, kk), v in meas.items() if kk == k]
            g[k] = sum(a*v for a, v in pts)/sum(a*a for a, _ in pts)
        norm = g[84]
        g = {k: v/norm for k, v in g.items()}
        s = {kf: v*norm for kf, v in s.items()}
    return s, g

def g_interp(g, note):
    keys = sorted(g)
    fx = [freq(k) for k in keys]; gy = [g[k] for k in keys]
    f = freq(note)
    if f < fx[0]:  # below the lowest measured key: linear in frequency down to 0
        return max(0.0, gy[0] + (gy[1] - gy[0])/(fx[1] - fx[0])*(f - fx[0]))
    return float(np.interp(f, fx, gy))

def s_table(s, g):
    tab = np.zeros((10, 61), dtype=int)
    for kf in range(1, 10):
        for i, n in enumerate(range(36, 97)):
            tab[kf, i] = min(127, int(round(s[kf]*g_interp(g, n))))
    return tab

def cap_points():
    keys = sorted(CAP_MEAS)
    f0, f1 = freq(keys[0]), freq(keys[1])
    c0, c1 = CAP_MEAS[keys[0]], CAP_MEAS[keys[1]]
    f127 = f0 + (127 - c0)*(f1 - f0)/(c1 - c0)   # extend the lowest segment up to code 127
    return [(f127, 127.0)] + [(freq(k), CAP_MEAS[k]) for k in keys]

if __name__ == '__main__':
    s, g = fit_separable(S_MEAS)
    print('s(KF):', ' '.join('%d:%.1f' % kv for kv in sorted(s.items())))
    print('g(key):', ' '.join('%d:%.3f' % kv for kv in sorted(g.items())))
    tab = s_table(s, g)
    print('residuals (table - measured), codes:')
    res = [(kf, k, tab[kf, k - 36] - v) for (kf, k), v in sorted(S_MEAS.items())]
    for kf in range(1, 10):
        print('  kf %d: ' % kf + ' '.join('%d:%+.1f' % (k, r) for f, k, r in res if f == kf))
    print('  rms %.2f, max |%.1f|;  S(9, 96) = %d (must be >= 127)' %
          (np.sqrt(np.mean([r*r for *_, r in res])), max(abs(r) for *_, r in res), tab[9, 60]))
    path = 'dcw_keyfollow_s.csv'
    with open(path, 'w', newline='\r\n') as f:
        f.write('# DCW key follow: level codes subtracted from the DCW targets, rows = key follow 0..9, columns = key 36..96\n')
        f.write('kf,' + ','.join(str(n) for n in range(36, 97)) + '\n')
        for kf in range(10):
            f.write('%d,' % kf + ','.join(str(v) for v in tab[kf]) + '\n')
    print('wrote', path)
    for kf in range(10):
        print('  kf %d: ' % kf + ' '.join('%3d' % v for v in tab[kf, ::6]) + '   (keys 36,42,...,96)')
    print('cap points (Hz, code):', ', '.join('(%.1f, %.1f)' % p for p in cap_points()))
