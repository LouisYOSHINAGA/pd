"""Where does the DCA attack start? (czenvrec/20260922, DCA step 1 = (r, 99), release rate 99)

The attack is linear in the level code and the volume is 0.5017 dB per code (eg.cpp kVolume,
with the truncated chip amplitude at the bottom). For every take the whole audible attack
(-55..-3 dB re the sustain) is fitted with that model, code(t) = speed * (t - t0), giving t0 =
the moment the accumulator leaves code 0. With E = (release start - t0) - MIDI gate: if the
attack starts at code 0 a fixed time after note-on, E is the same for every rate; if it
started higher (c_s > 0), t0 would lie before the real start and E would grow with the time
per code (E = E0 + c_s * tau).
"""
import numpy as np, os
from scipy.optimize import least_squares
from dcwkf import load, REC

PERIOD = 1/442.8
HOP = 0.0005

def volume_db(code):
    """Chip volume in dB re code 127 (eg.cpp kVolume: 1024 * 2^((c - 127) / 12) - 0.5 LSB)."""
    a = 1024*2**((np.asarray(code, float) - 127)/12) - 0.5
    return 20*np.log10(np.maximum(a, 1e-3)/1023.5)

def takes(path, gate):
    x, sr = load(path)
    w = int(round(PERIOD*sr)); h = int(HOP*sr)
    c = np.concatenate([[0], np.cumsum(x*x)])
    idx = np.arange((len(x) - w)//h)*h
    db = 10*np.log10((c[idx + w] - c[idx])/w + 1e-14)
    t = (idx + w/2)/sr
    noise = np.percentile(db, 5)
    loud = db > noise + 20
    out = []
    for s in np.where(loud[1:] & ~loud[:-1])[0] + 1:
        if loud[max(0, s - int(1.0/HOP)):s].any() or s + int((gate + 0.5)/HOP) >= len(db):
            continue
        sus = np.median(db[s + int((gate - 1.0)/HOP): s + int((gate - 0.2)/HOP)])
        a0, a1 = s - int(0.5/HOP), s + int((gate - 0.2)/HOP)
        seg, ts = db[a0:a1] - sus, t[a0:a1]
        top = np.argmax(seg > -3)
        m = (seg > max(-55, noise - sus + 10)) & (np.arange(len(seg)) <= top)
        if m.sum() < 5:
            continue
        # initial guess from a line through the dB-linear part
        lm = m & (seg > -45) & (seg < -6)
        slope, icpt = np.polyfit(ts[lm], seg[lm], 1) if lm.sum() > 3 else (1e4, 0)
        speed0 = slope/0.5017; t00 = ts[lm][0] - (seg[lm][0]/0.5017 + 127)/speed0 if lm.sum() > 3 else ts[top]
        fit = least_squares(lambda p: volume_db(np.clip(p[1]*(ts[m] - p[0]), 0, 127)) - seg[m],
                            [t00, speed0], x_scale=[1e-3, speed0])
        t0, speed = fit.x
        after = db[s + int((gate - 0.2)/HOP): s + int((gate + 0.5)/HOP)] - sus
        rel = t[s + int((gate - 0.2)/HOP) + np.argmax(after < -1)]
        out.append((speed, t0, rel, np.sqrt(np.mean(fit.fun**2))))
    return out

if __name__ == '__main__':
    print('rate | codes/s | ms/code | fit rms dB | E = release - t0 - gate [ms] (median, per take)')
    rows = []
    for r in (74, 49, 24):
        tk = takes(os.path.join(REC, '20260922', 'dca_r1_%02d_l1_99.wav' % r), 10.0)
        speed = np.median([v[0] for v in tk])
        E = [(rel - t0 - 10.0)*1e3 for _, t0, rel, _ in tk]
        rows.append((1e3/speed, np.median(E)))
        print('  %2d | %7.1f | %6.3f | %5.2f | %+7.1f   (%s)' % (r, speed, 1e3/speed, np.median([v[3] for v in tk]),
                                                          np.median(E), ' '.join('%+.1f' % e for e in E)))
    tau, E = np.array(rows).T
    cs, E0 = np.polyfit(tau, E, 1)
    print('fit E = E0 + c_s * tau: E0 = %+.1f ms, c_s = %+.2f codes' % (E0, cs))
