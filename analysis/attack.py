"""Per-take attack traces at ~1 ms resolution, aligned at the audible onset."""
import numpy as np
from rmsenv import analyse
from wavutil import files

def attack_trace(sub, win_s=0.0025, hop_s=0.0005, t1=0.3):
    f = [x for x in files() if sub in x][0]
    d = analyse(f, hop_s=0.001)
    x, sr = d['x'], d['sr']
    w = int(win_s*sr); h = int(hop_s*sr)
    segs = []
    for o, s in zip(d['ons'], d['shifts']):
        i0 = int(round((o+s)*d['hop']*sr))
        seg = x[i0-int(0.2*sr): i0+int(t1*sr)]
        c = np.concatenate([[0], np.cumsum(seg*seg)])
        idx = np.arange((len(seg)-w)//h)*h
        ms = (c[idx+w]-c[idx])/w
        db = 10*np.log10(ms+1e-14)
        nf = np.median(db[:int(0.1*sr/h)])
        on = np.argmax(db > nf + 10)
        segs.append((np.arange(len(db))*h/sr - on*h/sr, ms))
    grid = np.arange(-0.01, t1-0.05, hop_s)
    M = np.array([np.interp(grid, tt, ms) for tt, ms in segs])
    return grid, 10*np.log10(M.mean(0)+1e-14)

if __name__ == '__main__':
    import sys
    sub = sys.argv[1]; step = float(sys.argv[2]); t1 = float(sys.argv[3])
    g, db = attack_trace(sub, t1=t1+0.06)
    top = db.max()
    prev = None
    for tt in np.arange(-0.004, t1, step):
        i = np.argmin(abs(g-tt)); v = db[i]-top
        print('  %.4f %7.2f%s' % (g[i], v, '' if prev is None else '  %8.1f dB/s' % ((v-prev)/step)))
        prev = v
