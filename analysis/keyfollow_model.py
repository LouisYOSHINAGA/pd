"""DCA key follow model from the 20260926 recordings: speed factor F = (12 + k) / 12.

k(kf, note) is measured at a few notes; between them it is interpolated geometrically
(~x4 per octave), or linearly where one end is 0. Notes are clamped to C2..C7 (36..96)
like the CZ-101 does. Also cross-checks the factory-preset speed factors.
"""
import numpy as np

def step_of(c):
    e = np.floor(c/8)
    return (8 + (c - 8*e))*2**e

def k_table(path='keyfollow_table.npy'):
    T = np.load(path)
    base = {int(n): step_of(ca) for kf, n, ca, cr in T if kf == 0}
    return {(int(kf), int(n)): 12*step_of(ca)/base[int(n)] - 12 for kf, n, ca, cr in T if 36 <= n <= 96}

def interp_k(kt, kf, note, notes):
    note = min(max(note, 36), 96)
    xs = sorted(n for n in notes if (kf, n) in kt)
    if note in xs:
        return kt[(kf, note)]
    lo = max(n for n in xs if n < note); hi = min(n for n in xs if n > note)
    a, b = max(kt[(kf, lo)], 0.0), max(kt[(kf, hi)], 0.0)
    w = (note - lo)/(hi - lo)
    if a > 0.5 and b > 0.5:
        return a*(b/a)**w
    return a + (b - a)*w

if __name__ == '__main__':
    kt = k_table()
    octs = [36, 48, 60, 72, 84, 96]
    print('k measured (12F - 12), and predicted from the octave points only:')
    print('  kf   A4 meas  A4 pred   A5 meas  A5 pred')
    for kf in range(1, 10):
        print('  %d   %7.2f  %7.2f   %7.2f  %7.2f' % (kf, kt[(kf, 69)], interp_k(kt, kf, 69, octs),
                                                   kt[(kf, 81)], interp_k(kt, kf, 81, octs)))
