"""Key follow checks of czenvrec/20260926_2.

A) DCA step 1 = (36, 99) sustain, step 2 = (36, 0) end, note 84, key follow 0 and 9:
   is the key follow factor the same at another rate, and does it also apply to the release?
B) Octave Range +1, note 81, key follow 9: does key follow use the key or the sounding pitch?
"""
import numpy as np, os, wave
from keyfollow import code_of_speed, DB_PER_CODE, f0_of
from keyfollow2 import measure, takes_of
from keyfollow_model import step_of, k_table, interp_k

D = os.path.join(os.path.dirname(__file__), '..', 'czenvrec', '20260926_2')

def load(f):
    with wave.open(os.path.join(D, f)) as w:
        nch, sr = w.getnchannels(), w.getframerate()
        return np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(float).reshape(-1, nch)[:, 0]/32768, sr

def speeds(f):
    x, sr = load(f)
    f0 = f0_of(x[int(2*sr):int(8*sr)], sr)
    ms = [measure(x[a:b], sr, f0) for a, b in takes_of(x, sr)]
    att = np.array([m[0] for m in ms]); rel = np.array([m[1] for m in ms])
    return f0, len(ms), code_of_speed(np.nanmedian(att)/DB_PER_CODE), code_of_speed(np.nanmedian(rel)/DB_PER_CODE)

if __name__ == '__main__':
    kt = k_table()
    kf9 = sorted(n for (f, n) in kt if f == 9)
    print('A) rate 36 attack and release at note 84 (law: rate 36 -> code 45)')
    res = {}
    for kf in (0, 9):
        f0, n, ca, cr = speeds('dca_r1_36_l1_99_r2_36_kf_%d_note_84.wav' % kf)
        res[kf] = (ca, cr)
        print('   kf %d: f0=%.1f takes=%d  attack code %.2f  release code %.2f' % (kf, f0, n, ca, cr))
    Fa = step_of(res[9][0])/step_of(res[0][0]); Fr = step_of(res[9][1])/step_of(res[0][1])
    print('   factor kf9/kf0: attack %.3f, release %.3f   (rate 24 measurement at note 84: %.3f)'
          % (Fa, Fr, 1 + kt[(9, 84)]/12))
    print('B) Octave Range +1, key 81, key follow 9 (rate 24)')
    f0, n, ca, cr = speeds('dca_r1_24_l1_99_kf_9_note_81_oct_1.wav')
    base = np.mean([step_of(c) for c in [29.80, 29.81, 29.82]])
    F = step_of(ca)/base
    print('   f0=%.1f takes=%d attack code %.2f -> F=%.3f' % (f0, n, ca, F))
    print('   expected if key follow uses the key (81): F=%.3f ; the sounding pitch (93): F=%.3f'
          % (1 + kt[(9, 81)]/12, 1 + interp_k(kt, 9, 93, kf9)/12))
