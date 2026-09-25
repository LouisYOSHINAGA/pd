import numpy as np
from harm import harmonic_traces
from wavutil import files
import pdosc

CASES = [  # sub, f0 grid, wf1, wf2, dcw level, plateau window
 ('brass_ens1',    440, 1, 0, 52, (2.5, 4.8)),
 ('brass_ens2',    440, 1, 0, 88, (2.5, 4.8)),
 ('strings_ens1',  440, 1, 0, 99, (1.5, 4.0)),
 ('synth_strings', 440, 1, 3, 99, (1.5, 4.8)),
 ('violin',        880, 1, 3, 84, (1.5, 4.8)),
 ('flute',         880, 5, 0, 57, (1.5, 4.8)),
 ('synth_bass',    220, 2, 3, 74, (0.8, 4.8)),
 ('accordion',     220, 2, 3, 52, (1.5, 4.8)),
]
NH = 12
for sub, f0, w1, w2, lev, (a, b) in CASES:
    f = [x for x in files() if sub in x][0]
    t, P, tot, d = harmonic_traces(f, f0, NH, tmax=5.0)
    sel = (t >= a) & (t <= b)
    rec = P[sel].mean(0)
    rdb = 10*np.log10(rec/rec.max())
    best = None
    for dcw in np.linspace(0, 1, 201):
        h = pdosc.harmonics(w1, w2, dcw, NH)
        mdb = 10*np.log10(h/h.max() + 1e-12)
        m = rdb > -45
        e = np.mean(np.abs(np.maximum(mdb, -60)[m] - rdb[m]))
        if best is None or e < best[0]: best = (e, dcw, mdb)
    print('%-14s wf=%d,%d level=%d -> best dcw=%.3f (level/99=%.3f) err=%.2f dB' % (sub, w1, w2, lev, best[1], lev/99, best[0]))
    print('   rec  ', np.round(rdb, 1))
    print('   model', np.round(best[2], 1))
