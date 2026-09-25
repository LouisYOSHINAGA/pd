"""Instantaneous frequency via interpolated zero crossings (single takes)."""
import numpy as np, sys
from rmsenv import analyse
from wavutil import files

def zc_freq(seg, sr):
    s = seg - np.mean(seg)
    i = np.where((s[:-1] < 0) & (s[1:] >= 0))[0]
    frac = -s[i]/(s[i+1]-s[i])
    tz = (i + frac)/sr
    return tz[1:], 1.0/np.diff(tz)

if __name__ == '__main__':
    sub = sys.argv[1]; t0=float(sys.argv[2]); t1=float(sys.argv[3]); step=float(sys.argv[4])
    ref = float(sys.argv[5]) if len(sys.argv) > 5 else 440.0
    f = [x for x in files() if sub in x][0]
    d = analyse(f, hop_s=0.001)
    x, sr = d['x'], d['sr']
    allf = []
    for o, s in list(zip(d['ons'], d['shifts']))[:12]:
        i0 = int(round((o+s)*d['hop']*sr))
        seg = x[i0 + int(t0*sr): i0 + int(t1*sr)]
        tz, fz = zc_freq(seg, sr)
        allf.append((tz + t0, fz))
    for tt in np.arange(t0, t1, step):
        vals = []
        for tz, fz in allf:
            m = (tz >= tt) & (tz < tt+step)
            if m.sum(): vals.append(np.median(fz[m]))
        if vals:
            v = np.median(vals)
            print('%.4f  f=%8.2f  st=%6.2f  (n=%d spread=%.2f)' % (tt, v, 12*np.log2(v/ref), len(vals), np.std(vals)))
