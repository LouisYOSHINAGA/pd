"""Same-note comparisons for the DCW key follow recordings (czenvrec/20260927_2).

DCW step 1 = (24, 99) rises 14.95 codes/s, so the level code at which the rise stops is
known from the stop time. The stop time is found by comparing H2/H1 and H3/H1 with the
note 36 KF 0 take (never stops within the hold), shifted by a per-note offset fitted over
1..3 s (removes the output frequency response). Also compares KF 5 with KF 0.

Result: the DCW rate is unchanged by key follow; instead the DCW level is limited at high
notes, even at KF 0. The limit fits  1 - 0.9025*code/127 = k*f  (f = oscillator frequency,
0.9025 = full-scale depth of the oscillator model), k ~ 2.25e-4 s at KF 0/5, ~4.8e-4 s at KF 9.
"""
import numpy as np
from dcwkf import files, load, onsets

CODES_PER_S = (8 + 6) * 8 * (8.96e6/256) / (1 << 18)     # rate 24 -> code 30 -> step 112

def spectra(f, note, hold=7.9, hop_s=0.02):
    x, sr = load(f)
    f0 = 440.0*2**((note - 69)/12)*1.0032
    nh = int(min(10, 12000//f0))
    N = int(max(0.046, 8/f0)*sr); H = int(hop_s*sr)
    w = np.hanning(N); fr = np.fft.rfftfreq(N, 1/sr); bw = min(f0*0.3, 60)
    bands = [(fr > k*f0 - bw) & (fr < k*f0 + bw) for k in range(1, nh + 1)]
    nfr = int(hold/hop_s); P = np.zeros((nfr, nh))
    for o in onsets(x, sr):
        if o + int(hold*sr) + N > len(x): continue
        frames = np.stack([x[o + j*H: o + j*H + N] for j in range(nfr)])*w
        S = np.abs(np.fft.rfft(frames, axis=1))**2
        P += np.stack([S[:, b].sum(1) for b in bands], axis=1)
    r = 10*np.log10(P + 1e-30)
    return (np.arange(nfr)*H + N/2)/sr, r - r[:, :1]          # harmonics relative to H1

def stop_times(tabs):
    S = {k: spectra(f, k[1]) for k, f in tabs.items()}
    t = S[(0, 36)][0]
    ref = S[(0, 36)][1]
    sm = lambda y: np.convolve(y, np.ones(9)/9, mode='same')
    out = {}
    for key, (_, r) in S.items():
        ts, late = [], []
        for h in (1, 2):
            fit = (t > 1.0) & (t < 3.0)
            g = sm(ref[:, h]) + np.mean(r[fit, h] - ref[fit, h])
            plateau = np.median(r[-10:, h])
            ts.append(np.interp(plateau, g[10:-5], t[10:-5]))
            late.append(np.polyfit(t[-20:], r[-20:, h], 1)[0])
        out[key] = (np.mean(ts), max(late) > 0.2, r[-10:, 1].mean())
    return out

if __name__ == '__main__':
    tabs = {(kf, n): f for f, kf, n in files()}
    notes = sorted(set(n for _, n in tabs))
    print('KF 5 vs KF 0: median |difference| of H2/H1, H3/H1 while H2/H1 > -30 dB [dB]')
    for n in notes:
        t, a = spectra(tabs[(0, n)], n); _, b = spectra(tabs[(5, n)], n)
        m = (a[:, 1] > -30)
        print('   note %3d: %.2f dB' % (n, np.median(np.abs(a[m, 1:3] - b[m, 1:3]))))
    st = stop_times(tabs)
    print('level code at which the DCW rise stops ("-": still rising at the end of the hold)')
    print('   kf ' + ''.join('%8d' % n for n in notes))
    for kf in (0, 5, 9):
        row = ''
        for n in notes:
            ts, rising, h2 = st[(kf, n)]
            row += '       -' if rising else ('    sine' if h2 < -40 else '%8.1f' % (CODES_PER_S*ts))
        print('   %d  ' % kf + row)
    print('k = (1 - 0.9025*code/127) / f  [1e-4 s]')
    for kf in (0, 5, 9):
        row = ''
        for n in notes:
            ts, rising, h2 = st[(kf, n)]
            f = 440*2**((n - 69)/12)
            row += '       -' if rising or h2 < -40 else '%8.2f' % ((1 - 0.9025*CODES_PER_S*ts/127)/f*1e4)
        print('   %d  ' % kf + row)
