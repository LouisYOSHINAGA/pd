"""Release target when the key is released before the sustain point (czenvrec/20260927).

DCA: (50, 99), (99, 80) [sustain point or not], (30, 30), (99, 0) [end], note 69, hold 0.3 / 3.0 s.
Released at the sustain point the CZ-101 continues with the step after it; released before
the sustain point (or without one) it jumps straight to the end step.
`python check_20260927.py compare` renders the same cases with the harness for comparison.
"""
import numpy as np, os, wave, sys

D = os.path.join(os.path.dirname(__file__), '..', 'czenvrec', '20260927')

def load(f):
    with wave.open(os.path.join(D, f)) as w:
        nch, sr = w.getnchannels(), w.getframerate()
        return np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(float).reshape(-1, nch)[:, 0]/32768, sr

def takes(x, sr, win=1/442.8, hop=0.001):
    w = int(round(win*sr)); h = int(hop*sr)
    c = np.concatenate([[0], np.cumsum(x*x)])
    idx = np.arange(0, len(x) - w, h)
    db = 10*np.log10((c[idx + w] - c[idx])/w + 1e-14)
    floor = np.percentile(db, 5)
    loud = db > floor + 10
    on = np.where(loud[1:] & ~loud[:-1])[0] + 1
    ons = [i for i in on if not loud[max(0, i - 500):i].any()]
    return db, ons, floor, hop

if __name__ == '__main__':
    for f in sorted(os.listdir(D)):
        if not f.endswith('.wav'): continue
        x, sr = load(f)
        db, ons, floor, hop = takes(x, sr)
        n = int(3.2/hop) if '_03' in f else int(6.0/hop)
        segs = np.array([db[i - 50:i - 50 + n] for i in ons if i - 50 + n <= len(db)])
        t = (np.arange(n) - 50)*hop
        m = 10*np.log10(np.mean(10**(segs/10), axis=0))
        ref = m.max()
        print('== %s  takes=%d  floor=%.1f dB (rel. peak %.1f)' % (f, len(segs), floor, floor - ref))
        marks = [0.0, 0.1, 0.2, 0.25, 0.3, 0.31, 0.32, 0.33, 0.35, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.5, 2.0, 2.2, 2.3, 2.5, 3.0]
        if '_3.' in f:
            marks = [0.0, 0.2, 0.5, 0.6, 1.0, 2.0, 2.2, 2.3, 2.5, 2.9, 3.0, 3.01, 3.02, 3.05, 3.1, 3.5, 4.0, 4.5, 4.7, 4.8, 5.0, 5.5]
        print('   ' + '  '.join('%.2f:%6.1f' % (tt, m[np.argmin(abs(t - tt))] - ref) for tt in marks))

def render_case(exe, sustain, gate, total, tag):
    import subprocess
    from render_all import OUT
    v = [0.0]*53
    base = 2 + 17*2
    rates, levels = [50, 99, 30, 99], [99, 80, 30]
    for i, r in enumerate(rates):
        v[base + i] = r/99.0
    for i, l in enumerate(levels):
        v[base + 8 + i] = l/99.0
    v[base + 15] = (2/7.0) if sustain else 0.0     # sustain point 2 / off
    v[base + 16] = 2/6.0                            # end point 4
    fn = os.path.join(OUT, 'relabl_%s_%d_%s.txt' % ('w' if sustain else 'wo', int(gate*10), tag))
    with open(fn, 'w') as f:
        f.write('0 1.0 69 %f %f\n' % (gate, total))
        for _ in range(2):
            f.write(' '.join('%.12f' % a for a in v) + '\n')
    subprocess.run([exe, fn, fn.replace('.txt', '.raw')], check=True)
    y = np.fromfile(fn.replace('.txt', '.raw'), dtype=np.float32).astype(float)
    return np.concatenate([np.zeros(int(0.5*44100)), y])

def compare():
    H = os.path.join(os.path.dirname(__file__), 'harness')
    for f, sustain, gate in (('dca_relabl_w_sustain_hold_03.wav', True, 0.3),
                             ('dca_relabl_wo_sustain_hold_03.wav', False, 0.3),
                             ('dca_relabl_w_sustain_hold_3.wav', True, 3.0)):
        x, sr = load(f)
        db, ons, floor, hop = takes(x, sr)
        n = int((gate + 2.2)/hop)
        cz = 10*np.log10(np.mean([10**(db[i - 50:i - 50 + n]/10) for i in ons if i - 50 + n <= len(db)], axis=0))
        rows = {'CZ-101': cz}
        y = render_case(os.path.join(H, 'render.exe'), sustain, gate, gate + 2.5, 'vst')
        d, o, fl, _ = takes(y, 44100)
        rows['VST'] = d[o[0] - 50:o[0] - 50 + n]
        top = cz.max()
        t = (np.arange(n) - 50)*hop
        marks = [gate - 0.05, gate - 0.01, gate, gate + 0.01, gate + 0.02, gate + 0.05, gate + 0.1, gate + 0.3, gate + 0.5, gate + 1.0, gate + 1.5, gate + 1.8]
        print('== %s (dB rel. peak, t from sound onset)' % f)
        print('   %-9s ' % 't' + ' '.join('%7.2f' % m for m in marks))
        for k, r in rows.items():
            top_k = r.max() if k != 'CZ-101' else top
            print('   %-9s ' % k + ' '.join('%7.1f' % max(r[np.argmin(abs(t - m))] - top_k, -60) for m in marks))

if __name__ == '__main__' and len(sys.argv) > 1 and sys.argv[1] == 'compare':
    compare()
