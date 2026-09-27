"""Attack of the 16 presets, CZ-101 vs VST (offline harness), at ~1 ms resolution.

Both are measured the same way: 2.5 ms RMS every 0.5 ms (CZ: power averaged over the takes,
aligned at the audible onset). Reported is the time from the -40 dB point (re the maximum in
the first 0.3 s) to the -20, -10, -6, -3 and -1 dB points.
"""
import numpy as np
from czparam import load
from render_all import render, SR
from wavutil import files
from attack import attack_trace

LEVELS = (-20, -10, -6, -3, -1)

def crossings(t, db, t1=0.3):
    m = t < t1
    top = db[m].max()
    rel = db - top
    t40 = t[np.argmax(rel > -40)]
    return [t[np.argmax(rel > lv)] - t40 for lv in LEVELS]

def vst_trace(y, win_s=0.0025, hop_s=0.0005, t1=0.35):
    w = int(win_s*SR); h = int(hop_s*SR)
    seg = y[:int(t1*SR) + w]
    c = np.concatenate([[0], np.cumsum(seg*seg)])
    idx = np.arange((len(seg) - w)//h)*h
    ms = (c[idx + w] - c[idx])/w
    return idx/SR, 10*np.log10(ms + 1e-14)

if __name__ == '__main__':
    P = {p['no']: p for p in load()}
    print('time from -40 dB to %s dB [ms]' % '/'.join(str(l) for l in LEVELS))
    for f in files():
        no = int(f.split('_')[1])
        sub = f[len('cz101_00_'):-len('_vanilla.wav')]
        g, db = attack_trace(sub, t1=0.35)
        cz = crossings(g, db)
        tv, dv = vst_trace(render(P[no], tag='new'))
        vs = crossings(tv, dv)
        print('%2d %-17s CZ  %s' % (no, P[no]['name'], ' '.join('%6.1f' % (1e3*x) for x in cz)))
        print('   %-17s VST %s   diff(-3 dB) %+.1f ms' % ('', ' '.join('%6.1f' % (1e3*x) for x in vs), 1e3*(vs[3] - cz[3])))
