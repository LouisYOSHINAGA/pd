"""Render the 16 presets with the C++ harness and compare DCA envelopes."""
import numpy as np, os, subprocess, sys, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from czparam import load
from rmsenv import analyse, _meansq, detune_semitones
from wavutil import files

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(HERE, 'harness', 'render.exe')
if len(sys.argv) > 1 and sys.argv[1] == 'old':
    EXE = os.path.join(HERE, 'harness', 'old', 'render.exe')
OUT = os.path.join(HERE, 'render'); os.makedirs(OUT, exist_ok=True)
SR = 44100
LS = {'1': 0, '2': 1, "1+1'": 2, "1+2'": 3}
NLP = 2 + 3*17

def line_params(line):
    v = [0.0]*NLP
    v[0] = (line['wf1']-1)/7.0
    v[1] = (line['wf2'] or 0)/8.0
    for k, name in enumerate(('dco', 'dcw', 'dca')):
        eg = line[name]; base = 2 + 17*k
        steps = [(r if r is not None else 0, l if l is not None else 0) for r, l in eg['steps']]
        end = eg['end']
        if end == 1:            # not representable: end=2 with an extra 0 step
            steps[1] = (steps[0][0], 0); end = 2
        for i in range(8):
            v[base+i] = steps[i][0]/99.0
        for i in range(7):
            v[base+8+i] = steps[i][1]/99.0
        v[base+15] = (eg['sus'] or 0)/7.0
        v[base+16] = (end-2)/6.0
    return v

def render(p, gate=5.0, total=8.0, tag='new', key_follow=True):
    st = detune_semitones(p)
    ratio = 2**(st/12.0)
    octave = p['octave_range'] or 0
    fn = os.path.join(OUT, '%02d_%s.txt' % (p['no'], tag))
    with open(fn, 'w') as f:
        # the old harness has no octave range: shift the note instead
        note = 69 + 12*octave if tag == 'old' else 69
        f.write('%d %.12f %d %f %f\n' % (LS[p['line_select']], ratio, note, gate, total))
        for line in p['lines']:
            f.write(' '.join('%.12f' % x for x in line_params(line)) + '\n')
        if tag != 'old':
            kfs = [(line['dca']['kf'] or 0) if key_follow else 0 for line in p['lines']]
            f.write('%d %d %d\n' % (octave, kfs[0], kfs[1]))
    raw = fn.replace('.txt', '.raw')
    subprocess.run([EXE, fn, raw], check=True)
    y = np.fromfile(raw, dtype=np.float32).astype(np.float64)
    return y

def env_db(y, win_s, hop_s=0.001):
    ms, hop = _meansq(y, SR, win_s, hop_s)
    return np.arange(len(ms))*hop, 10*np.log10(ms + 1e-14)

if __name__ == '__main__':
    tag = sys.argv[1] if len(sys.argv) > 1 else 'new'
    P = {p['no']: p for p in load()}
    fig, axes = plt.subplots(8, 2, figsize=(20, 30))
    rows = []
    for ax, f in zip(axes.T.ravel(), files()):
        d = analyse(f)
        p = d['preset']
        y = render(p, tag=tag, key_follow=(tag != 'nokf'))
        pad = int(0.5*SR)
        tm, dbm = env_db(np.concatenate([np.zeros(pad), y]), d['win_s'])
        tm = tm - 0.5
        t, db = d['t'], d['db']
        floor = np.percentile(10**(db/10), 3)
        best = None
        for sh in np.arange(-0.10, 0.40, 0.002):
            mi = np.interp(t, tm + sh, dbm, left=-140, right=-140)
            g = np.median((db - mi)[db > db.max()-30])
            pr = 10*np.log10(10**((mi+g)/10) + floor)
            e = np.mean(np.abs(db - pr))
            if best is None or e < best[0]: best = (e, sh, pr)
        e, sh, pr = best
        rows.append((p['no'], p['name'], e, sh))
        top = db.max()
        ax.plot(t, db-top, lw=0.8, label='CZ-101')
        ax.plot(t, pr-top, lw=0.8, label='VST (%s)' % tag)
        ax.set_ylim(-80, 3); ax.set_xlim(0, 8); ax.grid(alpha=0.3)
        ax.set_title('%d %s   mean|err|=%.2f dB' % (p['no'], p['name'], e), fontsize=9)
        print('%2d %-18s mean|err|=%.2f dB  shift=%+.3f' % (p['no'], p['name'], e, sh), flush=True)
    axes[0, 0].legend()
    plt.tight_layout(); plt.savefig('plots/vst_vs_cz_dca_%s.png' % tag, dpi=65)
    print('avg err %.2f dB' % np.mean([r[2] for r in rows]))
