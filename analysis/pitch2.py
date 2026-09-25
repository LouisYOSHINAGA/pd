import numpy as np, sys
from scipy.signal import butter, sosfiltfilt
from rmsenv import analyse
from wavutil import files
from pitch import zc_freq

sub = sys.argv[1]; lo=float(sys.argv[2]); hi=float(sys.argv[3]); ref=float(sys.argv[4])
t1 = float(sys.argv[5]) if len(sys.argv)>5 else 0.2
f = [x for x in files() if sub in x][0]
d = analyse(f, hop_s=0.001)
x, sr = d['x'], d['sr']
sos = butter(4, [lo, hi], btype='band', fs=sr, output='sos')
curves = []
for o, s in zip(d['ons'], d['shifts']):
    i0 = int(round((o+s)*d['hop']*sr))
    seg = x[i0-int(0.05*sr): i0 + int(t1*sr)]
    y = sosfiltfilt(sos, seg)
    # onset of this take: first time |y| exceeds 5% of its max
    env = np.abs(y); on = np.argmax(env > 0.05*env.max())
    tz, fz = zc_freq(y, sr)
    st = 12*np.log2(fz/ref)
    curves.append((tz - on/sr, st))
grid = np.arange(0, t1-0.05, 0.002)
M = []
for tz, st in curves:
    M.append(np.interp(grid, tz, st, left=np.nan, right=np.nan))
M = np.array(M)
med = np.nanmedian(M, axis=0)
for g, v, sp in zip(grid, med, np.nanstd(M, axis=0)):
    if int(round(g*1000)) % 4 == 0:
        print('%.3f  st=%6.2f  spread=%.2f' % (g, v, sp))
