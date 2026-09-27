"""DCW preset comparison with the estimator bias of the CZ-101 recordings removed.

At A4 the harmonic matching reads the CZ-101 DCW higher than it is (the CZ output is a bit
brighter than the oscillator model at the same level code). The bias is calibrated with
czenvrec/20260927_2 KF 0 key 69 (saw, DCW rising 14.95 codes/s from 0, so the true code is
known at every moment), using the same estimator as compare_dcw.py, and removed from the
CZ-101 curves of the saw presets at A4. VST renders are compared with and without the DCW
key follow.
"""
import copy, os
import numpy as np
from czparam import load
from dcwtrack import track, build_table
from render_all import render, SR
from dcwkf import files, load as load_wav, onsets
from dcwkf2 import CODES_PER_S

_s = open(os.path.join(os.path.dirname(__file__), 'compare_dcw.py')).read()
exec(_s[_s.index('def track_signal'):_s.index('fig, axes')])   # track_signal()

def calibration():
    """(estimated code, true code) pairs for a saw at A4."""
    f = [f for f, kf, n in files() if kf == 0 and n == 69][0]
    x, sr = load_wav(f)
    est_all = []
    for o in onsets(x, sr)[:4]:
        t, est = track_signal(x[o:o + int(7.9*sr)], 440, 1, 0)
        est_all.append(np.interp(np.arange(0.2, 7.6, 0.05), t, est))
    true = CODES_PER_S*np.arange(0.2, 7.6, 0.05)
    est = np.median(est_all, axis=0)*127
    return est, true

CASES = [  # sub, preset no, wf1, wf2, smoothing (beating of 1+1'), shift (rec -> render), tmax
    ('brass_ens1',   1, 1, 0, 0.336, 0.298, 2.5),
    ('brass_ens2',   9, 1, 0, 0.294, 0.256, 2.0),
    ('strings_ens1', 4, 1, 0, 0.336, 0.282, 7.0),
]

if __name__ == '__main__':
    est_c, true_c = calibration()
    print('calibration (saw, A4): estimated -> true code')
    print('   ' + '  '.join('%.0f->%.0f' % (e, t) for e, t in zip(est_c[::20], true_c[::20])))
    order = np.argsort(est_c)
    hi = est_c > np.percentile(est_c, 70)            # above the calibrated range: extend linearly
    slope, icpt = np.polyfit(est_c[hi], true_c[hi], 1)
    to_true = lambda code: np.where(code > est_c.max(), slope*code + icpt,
                                    np.interp(code, est_c[order], true_c[order]))
    P = {p['no']: p for p in load()}
    for sub, no, w1, w2, sm, sh, tmax in CASES:
        t, est, err, tot = track(sub, 440, w1, w2, tmax=min(8.0, tmax + sh + 0.2), smooth_s=sm)
        cz = to_true(est*127)/127
        m = (t - sh > 0.05) & (t - sh < tmax) & (tot > tot.max() - 40)
        out = []
        for label, kf in (('DCW KF on', True), ('DCW KF off', False)):
            p = copy.deepcopy(P[no])
            if not kf:
                for line in p['lines']:
                    line['dcw']['kf'] = 0
            tr, er = track_signal(render(p, tag='new'), 440, w1, w2, smooth_s=sm)
            vst = np.interp(t[m] - sh, tr, er)
            out.append('%s %.3f (raw CZ %.3f)' % (label, np.mean(np.abs(vst - cz[m])), np.mean(np.abs(vst - est[m]))))
        print('%-13s KF %d:  ' % (sub, P[no]['lines'][0]['dcw']['kf']) + '   '.join(out))
