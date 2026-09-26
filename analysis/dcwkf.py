"""DCW key follow analysis (czenvrec/20260927_2).

DCW step 1 = (24, 99) sustain, step 2 = (99, 0) end; DCA constant; waveform 1 (saw).
For every key follow value and note, dcw(t) is estimated by matching the take-averaged
harmonic spectrum to the oscillator model; reported are the depth reached at the end of
the hold and the rise speed.
"""
import numpy as np, os, re, wave
import pdosc

D = os.path.join(os.path.dirname(__file__), '..', 'czenvrec', '20260927_2')

def files():
    out = []
    for f in os.listdir(D):
        m = re.match(r'dcw_r1_24_l1_99_kf_(\d)_note_(\d+)\.wav$', f)
        if m:
            out.append((f, int(m.group(1)), int(m.group(2))))
    return sorted(out, key=lambda r: (r[1], r[2]))

def load(f):
    with wave.open(os.path.join(D, f)) as w:
        nch, sr = w.getnchannels(), w.getframerate()
        return np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(float).reshape(-1, nch)[:, 0]/32768, sr

def onsets(x, sr):
    h = int(0.005*sr); n = len(x)//h
    db = 10*np.log10((x[:n*h].reshape(n, h)**2).mean(1) + 1e-14)
    loud = db > np.percentile(db, 5) + 15
    on = np.where(loud[1:] & ~loud[:-1])[0] + 1
    return [i*h for i in on if not loud[max(0, i - 200):i].any()]

_tables = {}
def table(nh):
    if nh not in _tables:
        grid = np.linspace(0, 1, 501)
        T = np.array([10*np.log10(pdosc.harmonics(1, 0, g, nh) + 1e-30) for g in grid])
        _tables[nh] = (grid, np.maximum(T - T.max(1, keepdims=True), -80))
    return _tables[nh]

def dcw_track(f, note, hold=7.9, hop_s=0.02):
    x, sr = load(f)
    f0 = 440.0*2**((note - 69)/12)*1.0032          # the CZ-101 is tuned ~5 cents sharp
    nh = int(min(12, 15000//f0))
    N = int(max(0.046, 8/f0)*sr); H = int(hop_s*sr)
    w = np.hanning(N); fr = np.fft.rfftfreq(N, 1/sr); bw = min(f0*0.3, 60)
    bands = [(fr > k*f0 - bw) & (fr < k*f0 + bw) for k in range(1, nh + 1)]
    nfr = int(hold/hop_s)
    P = np.zeros((nfr, nh)); ons = onsets(x, sr); used = 0
    for o in ons:
        if o + int(hold*sr) + N > len(x): continue
        frames = np.stack([x[o + j*H: o + j*H + N] for j in range(nfr)])*w
        S = np.abs(np.fft.rfft(frames, axis=1))**2
        P += np.stack([S[:, b].sum(1) for b in bands], axis=1); used += 1
    grid, T = table(nh)
    r = 10*np.log10(P + 1e-30); r -= r.max(1, keepdims=True)
    est = np.empty(nfr)
    for i in range(nfr):
        m = r[i] > -55
        est[i] = grid[np.argmin(np.mean(np.abs(T[:, m] - np.maximum(r[i, m], -80)), 1))]
    t = (np.arange(nfr)*H + N/2)/sr
    return t, est, used

if __name__ == '__main__':
    res = {}
    for f, kf, note in files():
        t, est, used = dcw_track(f, note)
        depth = np.median(est[(t > t[-1] - 1.0)])
        # rise speed: slope of the linear part between 20% and 80% of the final depth
        sel = (est > 0.2*depth) & (est < 0.8*depth) & (t < t[-1] - 0.5)
        slope = np.polyfit(t[sel], est[sel], 1)[0] if sel.sum() > 3 else np.nan
        t80 = t[np.argmax(est > 0.9*depth)] if depth > 0.05 else np.nan
        res[(kf, note)] = (depth, slope, t80)
        print('kf=%d note=%3d takes=%d  depth(end)=%.3f  rise slope=%.3f /s  t(90%% of depth)=%.2f s'
              % (kf, note, used, depth, slope, t80), flush=True)
    np.save('dcwkf_table.npy', np.array([[kf, n, *res[(kf, n)]] for (kf, n) in sorted(res)]))
