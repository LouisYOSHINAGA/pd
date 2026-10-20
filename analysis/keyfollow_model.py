"""DCA key follow model from the 20260926 recordings: an integer x(kf, note) added to the rate code.

In the rate-code frame of the CZ book (DCA code = 119 * rate / 99, step with the half LSB from
exponent 9 up, see eg.cpp) every measured key follow speed is the step of code + x for an integer
x (rms distance 0.02 code over all points). At rate 24 (code 28, mantissa 12) this looks like a
speed factor F = (12 + k) / 12 with integer k, the first model (k_table/interp_k, kept for the
older checks); at other rates the code offset is what holds (check_20260926_2.py: rate 36).

x is measured at a few notes and interpolated linearly in between (the speed geometrically),
rounded to an integer. Notes are clamped to C2..C7 (36..96) like the CZ-101 does.
"""
import numpy as np

# Measured elsewhere: key 81 with Octave Range +1 (sounding note 93) at key follow 9, see
# check_20260926_2.py; key follow follows the sounding note, so this is the note-93 value.
EXTRA_POINTS = {(9, 93): 340.0}

RATE24_CODE = 119*24//99          # the 20260926 recordings use DCA rate 24
OLD_TO_BOOK = 14/16               # old frame: 2^18 units per level code, book frame: 14 * 2^14

def step_of(c):
    """Old frame: continuous inverse of the measurement unit, (8 + (c & 7)) << (c >> 3)."""
    e = np.floor(c/8)
    return (8 + (c - 8*e))*2**e

def book_step(c):
    e = c >> 3
    return ((8 + (c & 7)) << e) + ((1 << (e - 1)) if e >= 9 else 0)

_BOOK_STEPS = np.array([book_step(c) for c in range(160)], float)

def book_code(step):
    """Continuous inverse of book_step (log-linear between integer codes)."""
    i = np.searchsorted(_BOOK_STEPS, step) - 1
    return i + np.log(step/_BOOK_STEPS[i])/np.log(_BOOK_STEPS[i + 1]/_BOOK_STEPS[i])

def k_table(path='keyfollow_table.npy'):
    T = np.load(path)
    # key follow 0 runs at the same speed on every key, so one reference serves all notes
    base = np.mean([step_of(ca) for kf, n, ca, cr in T if kf == 0])
    kt = {(int(kf), int(n)): 12*step_of(ca)/base - 12 for kf, n, ca, cr in T if 36 <= n <= 96}
    kt.update(EXTRA_POINTS)
    return kt

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

def x_table(path='keyfollow_table.npy'):
    """Measured code offset x (not rounded) per (kf, note), notes 36..96."""
    T = np.load(path)
    base = book_code(OLD_TO_BOOK*np.mean([step_of(ca) for kf, n, ca, cr in T if kf == 0]))
    xt = {(int(kf), int(n)): book_code(OLD_TO_BOOK*step_of(ca)) - base
          for kf, n, ca, cr in T if 36 <= n <= 96}
    for key, k in EXTRA_POINTS.items():
        xt[key] = book_code(book_step(RATE24_CODE)*(12 + k)/12) - RATE24_CODE
    return xt

def full_x_table(xt):
    """Integer x for key follow 0..9 and notes 36..96 (measured points kept, others interpolated)."""
    out = np.zeros((10, 61), int)
    for kf in range(1, 10):
        notes = sorted(n for (f, n) in xt if f == kf)
        vals = [xt[(kf, n)] for n in notes]
        out[kf] = np.floor(np.interp(np.arange(36, 97), notes, vals) + 0.5).astype(int)
    return out

if __name__ == '__main__':
    xt = x_table()
    frac = np.array([x - np.round(x) for x in xt.values()])
    print('measured x: distance from the nearest integer rms %.3f, max %.3f (%d points)'
          % (np.sqrt(np.mean(frac**2)), np.abs(frac).max(), len(frac)))
    tab = full_x_table(xt)
    path = 'dca_keyfollow_code.csv'
    with open(path, 'w') as f:
        f.write('# DCA key follow: rate codes added to every step, rows = key follow 0..9, columns = MIDI note 36..96\n')
        f.write('kf,' + ','.join(str(n) for n in range(36, 97)) + '\n')
        for kf in range(10):
            f.write('%d,' % kf + ','.join(str(v) for v in tab[kf]) + '\n')
    print('wrote', path)
    for kf in range(10):
        print('  kf %d: ' % kf + ' '.join('%d' % v for v in tab[kf, ::6]) + '   (notes 36,42,...,96)')
    octs = [36, 48, 60, 72, 84, 96]
    print('x measured, and predicted from the octave points only:')
    print('  kf   A4 meas  A4 pred   A5 meas  A5 pred')
    for kf in range(1, 10):
        pred = lambda n: np.interp(n, octs, [xt[(kf, o)] for o in octs])
        print('  %d   %7.2f  %7.2f   %7.2f  %7.2f' % (kf, xt[(kf, 69)], pred(69), xt[(kf, 81)], pred(81)))
