"""Robust DCA key follow measurement: crossing times of -35/-5 dB (attack) and -5/-35 dB (release).

The same measurement is applied to VST renders (no key follow) to expose analysis artifacts.
"""
import numpy as np, os
from keyfollow import files, load, f0_of, code_of_speed, DB_PER_CODE, D
from compare_sweep import render_sweep, SR

def envelope(x, sr, f0):
    w = max(int(0.004*sr), int(2.0/f0*sr)); hh = max(1, int(0.0005*sr))
    c = np.concatenate([[0], np.cumsum(x*x)])
    idx = np.arange(0, len(x) - w, hh)
    return (idx + w/2)/sr, 10*np.log10((c[idx + w] - c[idx])/w + 1e-14)

def takes_of(x, sr):
    h = int(0.005*sr); m = len(x)//h
    db = 10*np.log10((x[:m*h].reshape(m, h)**2).mean(1) + 1e-14)
    floor = np.percentile(db, 5); loud = db > floor + 12
    on = np.where(loud[1:] & ~loud[:-1])[0] + 1
    ons = [i for i in on if i > 100 and not loud[max(0, i - 160):i].any()]
    out = []
    for k, i0 in enumerate(ons):
        end = ons[k+1]*h - int(0.2*sr) if k + 1 < len(ons) else len(x)
        out.append((max(0, i0*h - int(0.5*sr)), end))
    return out

def measure(x, sr, f0):
    t, d = envelope(x, sr, f0)
    top = np.percentile(d, 99)
    sus_idx = np.where(d > top - 1.5)[0]
    sus = np.median(d[sus_idx])
    r = d - sus
    i_on = sus_idx[0]; i_off = sus_idx[-1]
    def first_above(v, lo, hi):
        j = np.where(r[lo:hi] > v)[0]; return lo + j[0] if len(j) else None
    def first_below(v, lo):
        j = np.where(r[lo:] < v)[0]; return lo + j[0] if len(j) else None
    a0 = first_above(-35, 0, i_on + 1); a1 = first_above(-5, 0, i_on + 1)
    att = 30.0/(t[a1] - t[a0]) if a0 is not None and a1 is not None and a1 > a0 else np.nan
    r0 = first_below(-5, i_off); r1 = first_below(-35, i_off) if r0 is not None else None
    rel = 30.0/(t[r1] - t[r0]) if r0 is not None and r1 is not None and r1 > r0 else np.nan
    return att, rel, sus

def summarize(vals):
    v = np.array([x for x in vals if np.isfinite(x)])
    return (np.median(v), np.std(v), len(v)) if len(v) else (np.nan, np.nan, 0)

if __name__ == '__main__':
    table = {}
    f0s = {}
    for f, kf, note in files():
        x, sr = load(f)
        f0 = f0_of(x[int(2*sr):int(8*sr)], sr)
        ms = [measure(x[a:b], sr, f0) for a, b in takes_of(x, sr)]
        att = summarize([m[0] for m in ms]); rel = summarize([m[1] for m in ms])
        table[(kf, note)] = (code_of_speed(att[0]/DB_PER_CODE), att, code_of_speed(rel[0]/DB_PER_CODE), rel,
                             np.median([m[2] for m in ms]))
        f0s[note] = f0
    notes = sorted(n for n in set(n for _, n in table) if all((kf, n) in table for kf in range(10)))
    chroma = sorted(n for kf, n in table if kf == 9 and n not in notes)
    print('attack rate code n (rate 24; key follow 0 expectation = 30)')
    print('  kf / note ' + ' '.join('%7d' % n for n in notes))
    for kf in range(10):
        print('  %2d        ' % kf + ' '.join('%7.2f' % table[(kf, n)][0] if (kf, n) in table else '      -' for n in notes))
    def step_of(c):
        e = np.floor(c/8); return (8 + (c - 8*e))*2**e
    print('speed ratio F = step(kf)/step(kf 0); 12*F is an integer (see eg.cpp comment)')
    for kf in range(10):
        print('  %2d        ' % kf + ' '.join('%7.3f' % (step_of(table[(kf, n)][0])/step_of(table[(0, n)][0]))
                                          if (kf, n) in table else '      -' for n in notes))
    print('sustain level relative to kf 0 [dB]')
    for kf in range(10):
        print('  %2d        ' % kf + ' '.join('%7.2f' % (table[(kf, n)][4] - table[(0, n)][4])
                                          if (kf, n) in table else '      -' for n in notes))
    print('key follow 9, chromatic: k = 12F - 12')
    print('  ' + ' '.join('%d:%.1f' % (n, 12*step_of(table[(9, n)][0])/step_of(table[(0, 60 if n < 69 else (69 if n < 72 else (72 if n < 81 else (81 if n < 84 else 84))))][0]) - 12)
                        for n in sorted(chroma + [n for n in notes if 60 <= n <= 84])))
    print('release rate code n (rate 99 after note-off)')
    for kf in range(10):
        print('  %2d        ' % kf + ' '.join('%7.2f' % table[(kf, n)][2] if (kf, n) in table else '      -' for n in notes))
    # VST (no key follow) through the same measurement, at the sounding pitch of each note
    import subprocess, render_all
    print('VST render (key follow not implemented), same measurement:')
    row_a, row_r = [], []
    for n in notes:
        note_eff = min(max(n, 36), 96)
        y = render_sweep(24, 99, gate=9.0, total=11.0) if note_eff == 69 else None
        # render_sweep plays A4; re-render at the effective note
        fn = os.path.join(render_all.OUT, 'kf_note%d.txt' % note_eff)
        lines = open(os.path.join(render_all.OUT, 'sweep_r24_l99.txt')).read().split('\n')
        head = lines[0].split(); head[2] = str(note_eff); head[3] = '9.0'; head[4] = '11.0'
        open(fn, 'w').write(' '.join(head) + '\n' + '\n'.join(lines[1:]))
        subprocess.run([render_all.EXE, fn, fn.replace('.txt', '.raw')], check=True)
        y = np.fromfile(fn.replace('.txt', '.raw'), dtype=np.float32).astype(float)
        y = np.concatenate([np.zeros(int(0.5*SR)), y])
        att, rel, sus = measure(y, SR, 440.0*2**((note_eff - 69)/12))
        row_a.append(code_of_speed(att/DB_PER_CODE)); row_r.append(code_of_speed(rel/DB_PER_CODE))
    print('  attack    ' + ' '.join('%7.2f' % v for v in row_a))
    print('  release   ' + ' '.join('%7.2f' % v for v in row_r))
    print('sounding f0: ' + ' '.join('%d:%.1f' % (n, f0s[n]) for n in notes))
    np.save('keyfollow_table.npy', np.array([[kf, n, table[(kf, n)][0], table[(kf, n)][2]] for (kf, n) in sorted(table)]))
