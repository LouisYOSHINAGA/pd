"""Same-note comparisons for the DCW key follow recordings (czenvrec/20260927_2).

DCW step 1 = (24, 99) rises 14.95 codes/s, so the level code at which the rise stops is
known from the stop time. The stop time is the knee of a line + flat fit to dcw(t) of
dcwkf.dcw_track (checked against the time at which KF 9 departs from KF 0 at the same
note: agrees within 0.05 s). Also compares KF 5 with KF 0.

Result: the DCW rate is unchanged by key follow; instead the DCW level stops at a lower
value at high notes, even at KF 0 (127 - code ~ 0.024*f at KF 0). KF 1..7 are identical to KF 0 at
notes 72, 84, 96; only KF 8 and 9 lower the level further. check_20260927_3.py shows that
the KF 0 limit is a cap while KF 8/9 subtract from the level (see dcwkf_presets.py).
"""
import numpy as np
from dcwkf import files, load, onsets, dcw_track

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

def hinge(t, y, t0=1.0):
    """Knee time of a curve that rises linearly and then stays flat."""
    m = t > t0; t, y = t[m], y[m]
    best = (np.inf, None)
    for ts in t[5:-3]:
        A = np.stack([np.ones_like(t), np.minimum(t - ts, 0)], 1)
        c = np.linalg.lstsq(A, y, rcond=None)[0]
        best = min(best, (np.sum((A @ c - y)**2), ts))
    return best[1]

def stop_code(f, note):
    """Level code at which the DCW stops; None if still rising, 0 if no DCW at all."""
    t, r = spectra(f, note)
    if np.median(r[-10:, 1]) < -40:                 # pure sine
        return 0.0
    if np.polyfit(t[-15:], r[-15:, 1], 1)[0] > 0.2:  # H2/H1 still rising (dB/s)
        return None
    t, est, _ = dcw_track(f, note)
    return CODES_PER_S*hinge(t, est)

if __name__ == '__main__':
    tabs = {(kf, n): f for f, kf, n in files()}
    notes = sorted(set(n for _, n in tabs))
    print('KF 5 vs KF 0: median |difference| of H2/H1, H3/H1 while H2/H1 > -30 dB [dB]')
    for n in notes:
        t, a = spectra(tabs[(0, n)], n); _, b = spectra(tabs[(5, n)], n)
        m = (a[:, 1] > -30)
        print('   note %3d: %.2f dB' % (n, np.median(np.abs(a[m, 1:3] - b[m, 1:3]))))
    runs = {}
    for f, kf, n in files():
        runs.setdefault((kf, n), []).append(stop_code(f, n))
    kfs = sorted(set(kf for kf, _ in runs)); notes = sorted(set(n for _, n in runs))
    print('level code at which the DCW rise stops (level 99 = code 127; "-": still rising at the')
    print('end of the hold, i.e. above %.0f)' % (CODES_PER_S*7.8))
    print('   kf ' + ''.join('%8d' % n for n in notes))
    for kf in kfs:
        row = ''
        for n in notes:
            v = runs.get((kf, n))
            row += '%8s' % ('' if v is None else '-' if v[0] is None else '%.1f' % v[0])
        print('   %d  ' % kf + row)
    print('(127 - code) / f  [codes/kHz]')
    for kf in kfs:
        row = ''
        for n in notes:
            v = [c for c in runs.get((kf, n), []) if c]
            row += '%8s' % ('%.1f' % ((127 - v[0])/(440*2**((n - 69)/12))*1e3) if v else '')
        print('   %d  ' % kf + row)
