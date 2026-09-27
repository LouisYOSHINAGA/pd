"""czenvrec/20260927_5: DCO rate law. Key 72, DCW 0 (pure sine), DCO step 1 = (r, 66)
(+12 semitones) for r = 10..60, and (20, 32) (+4 semitones).

The pitch is tracked by interpolated zero crossings; the glide is linear in semitones, and
its speed is compared with the chip EG: step(n) = (8 + (n & 7)) << (n >> 3) per 35 kHz tick
with n = 127 * r / 99 (Casio's sysex rate value for the DCO) and 14 * 2^16 units per
semitone (eg.cpp). The old model (n = 119 * r / 99 + 2, 12 * 2^16) is shown for reference.
"""
import numpy as np, os, re
from dcwkf import load, REC
from pitch import zc_freq

D = os.path.join(REC, '20260927_5')
BASE = 523.25*2**(9.6/1200)      # key 72; this CZ-101 plays 9.6 cents sharp on every key
TICK = 8.96e6/256

def step(n):
    return (8 + n % 8) << (n // 8)

def takes(name):
    """(t, semitones) per take, over the part where the note sounds."""
    x, sr = load(os.path.join(D, name))
    h = int(0.005*sr); n = len(x)//h
    env = np.sqrt((x[:n*h].reshape(n, h)**2).mean(1))
    loud = env > 0.3*np.percentile(env, 99)
    out, i = [], 0
    while i < n:
        if not loud[i]:
            i += 1; continue
        j = i
        while j < n and loud[j]: j += 1
        if (j - i)*h/sr > 0.5:
            tz, fz = zc_freq(x[i*h: j*h], sr)
            out.append((tz, 12*np.log2(fz/BASE)))
        i = j
    return out

def glide(tz, st, lo, hi):
    """Slope (semitones/s) of a line fitted between lo and hi semitones during the glide."""
    i = np.argmax(st > hi) if np.any(st > hi) else len(st)
    m = (st > lo) & (st < hi) & (np.arange(len(st)) < i + 50) & (tz > 0.001)
    return np.polyfit(tz[m], st[m], 1)[0] if m.sum() > 8 else np.nan

if __name__ == '__main__':
    print(' rate level | final st | glide st/s (per take)       | model n  st/s  | old model st/s')
    for name in sorted(os.listdir(D), key=lambda s: [int(v) for v in re.findall(r'\d+', s)]):
        r, level = (int(v) for v in re.match(r'dco_r1_(\d+)_l1_(\d+)_note_72\.wav', name).groups())
        target = level/8 if level < 64 else 2*(level - 60)
        rows = takes(name)
        final = np.median([np.median(st[-200:]) for _, st in rows])
        slopes = [glide(tz, st, 0.25*target, 0.75*target) for tz, st in rows]
        n = 127*r//99
        print('  %3d  %3d  |  %6.2f  | %s | %3d %7.2f  | %7.2f'
              % (r, level, final, ' '.join('%7.2f' % s for s in slopes), n, step(n)*TICK/(14*65536),
                 step(119*r//99 + 2)*TICK/(12*65536)))
