"""Compare the C++ EG with the DCA rate/level sweep recordings (czenvrec/20260922).

The sweep patch runs 22 rate codes faster than the factory presets at the same
panel rate (presumably its key follow setting), so each sweep rate is rendered
with the VST rate that yields the same chip rate code: 119*r'/99 + 2 = 119*r/99 + 24.
"""
import numpy as np, os, subprocess
from dcasweep import sweep_files, takes
from render_all import EXE, OUT, SR, LS

REF = -14.90
VST_RATE = {24: 42, 49: 67, 74: 92, 99: 99}

def render_sweep(rate, level, gate=10.0, total=12.0):
    v = [0.0]*53
    v[0] = 0.0; v[1] = 0.0                    # saw, no 2nd waveform
    for k in range(2):                        # DCO, DCW: all zero, end = 2
        base = 2 + 17*k
        v[base+16] = 0.0
    base = 2 + 17*2                           # DCA
    v[base+0] = rate/99.0; v[base+1] = 1.0    # step1 rate, step2 (release) rate 99
    v[base+8] = level/99.0
    v[base+15] = 1/7.0                        # sustain point 1
    v[base+16] = 0.0                          # end point 2
    fn = os.path.join(OUT, 'sweep_r%02d_l%02d.txt' % (rate, level))
    with open(fn, 'w') as f:
        f.write('%d %.12f %d %f %f\n' % (LS['1'], 1.0, 69, gate, total))
        for _ in range(2):
            f.write(' '.join('%.12f' % x for x in v) + '\n')
    raw = fn.replace('.txt', '.raw')
    subprocess.run([EXE, fn, raw], check=True)
    return np.fromfile(raw, dtype=np.float32).astype(np.float64)

def env(y, win_s, hop_s):
    w = int(round(win_s*SR)); h = int(round(hop_s*SR))
    y = np.concatenate([np.zeros(int(0.3*SR)), y])
    c = np.concatenate([[0], np.cumsum(y*y)])
    idx = np.arange((len(y)-w)//h)*h
    return idx/SR - 0.3 + win_s/2, 10*np.log10((c[idx+w]-c[idx])/w + 1e-14)

if __name__ == '__main__':
    print('level sweep (rate 99): sustain dB relative to level 99')
    y99 = render_sweep(99, 99); t, d99 = env(y99, 0.01, 0.01); s99 = np.median(d99[(t > 3) & (t < 9)])
    for f, r, l in sweep_files():
        if r != 99 or l == 99: continue
        tt, S, floor, h, ons = takes(f)
        rec = 10*np.log10(np.median(S.mean(0)[(tt > 3) & (tt < 9)])) - REF
        y = render_sweep(99, l); t, d = env(y, 0.01, 0.01)
        vst = np.median(d[(t > 3) & (t < 9)]) - s99
        print('  level %2d  CZ %7.2f  VST %7.2f  diff %+5.2f' % (l, rec, vst, vst-rec))
    print('rate sweep (level 99): time to reach -50/-30/-10/-1 dB (s)')
    for f, r, l in sweep_files():
        if l != 99 or r not in VST_RATE or r == 99: continue
        win = {24: 0.004, 49: 0.001, 74: 0.0005}[r]; hop = win/4
        tt, S, floor, h, ons = takes(f, win_s=win, hop_s=hop, pre=0.3, post=1.6)
        drec = 10*np.log10(S.mean(0)+1e-14) - REF; tt = tt + win/2
        y = render_sweep(VST_RATE[r], 99); tv, dv = env(y, win, hop); dv = dv - s99
        def cross(t, d, a):
            i = np.where(d > a)[0][0]; return t[i]
        cz = [cross(tt, drec, a) for a in (-50, -30, -10, -1)]
        vs = [cross(tv, dv, a) for a in (-50, -30, -10, -1)]
        # recording onset is detected when sound appears; align on the -50 dB crossing
        print('  rate %2d (VST %2d)  CZ %s  VST %s   (-50->-1: CZ %.4f  VST %.4f)' % (
            r, VST_RATE[r], np.round(np.array(cz) - cz[0], 4), np.round(np.array(vs) - vs[0], 4),
            cz[3]-cz[0], vs[3]-vs[0]))


def fundamental_release(x, starts, t_off=0.3, span=0.6, ms_list=(0, 1, 2, 3, 4, 6, 8, 10, 12, 15, 20)):
    """Per-take 442 Hz band Hilbert envelope, aligned at the -3 dB crossing, median over takes."""
    from scipy.signal import hilbert, butter, sosfiltfilt
    sos = butter(2, [300, 600], btype='band', fs=SR, output='sos')
    rows = []
    for a in starts:
        seg = x[a: a + int(span*SR)]
        db = 20*np.log10(np.abs(hilbert(sosfiltfilt(sos, seg))) + 1e-9)
        top = np.median(db[int((t_off-0.2)*SR): int((t_off-0.05)*SR)])
        s0 = int((t_off-0.04)*SR)
        k = s0 + np.argmax(db[s0:] < top - 3)
        rows.append([db[min(len(db)-1, k + int(m*SR/1000))] - top for m in ms_list])
    return np.median(np.array(rows), 0)

if __name__ == '__main__':
    from dcasweep import load_raw
    print('release (step 2 = rate 99 -> 0), 442 Hz band, from -3 dB crossing: +0,1,2,3,4,6,8,10,12,15,20 ms')
    for f, r, l in sweep_files():
        if r != 99: continue
        X, sr = load_raw(f); tt, S, fl, h, ons = takes(f)
        cz = fundamental_release(X[:, 0], [int(o*h*sr) + int(9.7*sr) for o in ons])
        y = render_sweep(99, l)
        vs = fundamental_release(y, [int(9.7*SR)])
        print('  level %2d  CZ  %s' % (l, ' '.join('%6.1f' % v for v in cz)))
        print('            VST %s' % ' '.join('%6.1f' % v for v in vs))
