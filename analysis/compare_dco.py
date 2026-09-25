import numpy as np, os
from scipy.signal import butter, sosfiltfilt
from czparam import load
from rmsenv import analyse
from wavutil import files
from pitch import zc_freq
import render_all
from render_all import render, SR

P = {p['no']: p for p in load()}
CASES = [('whistle', 15, 1000, 2600, 0.12), ('violin', 3, 1000, 2400, 0.08)]

def curve(sig, sos, ref, pre=0):
    y = sosfiltfilt(sos, sig)
    env = np.abs(y); on = np.argmax(env > 0.05*env.max())
    tz, fz = zc_freq(y, SR)
    return tz - on/SR, 12*np.log2(fz/ref)

for sub, no, lo, hi, t1 in CASES:
    f = [x for x in files() if sub in x][0]
    d = analyse(f, hop_s=0.001)
    sos = butter(4, [lo, hi], btype='band', fs=SR, output='sos')
    grid = np.arange(0, t1, 0.004)
    M = []
    for o, s in zip(d['ons'], d['shifts']):
        i0 = int(round((o+s)*d['hop']*SR))
        tz, st = curve(d['x'][i0-int(0.05*SR): i0+int(0.3*SR)], sos, 441.4)
        M.append(np.interp(grid, tz, st, left=np.nan, right=np.nan))
    rec = np.nanmedian(np.array(M), axis=0) - 12*np.log2(441.4/440.0)*0  # CZ tuned ~5 cents sharp; compare in its own reference
    rows = {'CZ': rec}
    for tag in ('new', 'old'):
        render_all.EXE = os.path.join(render_all.HERE, 'harness', *(['old'] if tag == 'old' else []), 'render.exe')
        y = render(P[no], tag=tag)
        tz, st = curve(np.concatenate([np.zeros(int(0.05*SR)), y[:int(0.3*SR)]]), sos, 440.0)
        rows[tag] = np.interp(grid, tz, st, left=np.nan, right=np.nan)
    print('== %d %s  (semitones above A4; t from audible onset)' % (no, sub))
    print('   t     CZ-101    new     old')
    for i in range(0, len(grid), 2):
        print('  %.3f  %6.2f  %6.2f  %6.2f' % (grid[i], rows['CZ'][i], rows['new'][i], rows['old'][i]))
