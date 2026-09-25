"""RMS-based note envelopes: robust to waveform shape and detune beating."""
import numpy as np, os
from envlib import mono
from wavutil import REC_DIR, files
from czparam import load as load_presets

PERIOD = 8.0
_presets = {p['no']: p for p in load_presets()}
_cache = {}

def preset_of(fname):
    return _presets[int(fname.split('_')[1])]

def detune_semitones(p):
    sign = -1 if p['det_sign'] == '-' else 1
    return sign*(12*p['det_oct'] + p['det_note'] + p['det_fine']/60.0)

def beat_period(p, f0=440.0):
    if p['line_select'] in ('1', '2'):
        return None
    st = detune_semitones(p)
    df = abs(f0*(2**(st/12.0) - 1.0))
    if df < 1e-9 or df > 20.0:
        return None
    return 1.0/df

def _meansq(x, sr, win_s, hop_s):
    w = max(1, int(round(win_s*sr)))
    h = max(1, int(round(hop_s*sr)))
    c = np.concatenate([[0.0], np.cumsum(x.astype(np.float64)**2)])
    n = (len(x)-w)//h + 1
    idx = np.arange(n)*h
    return (c[idx+w]-c[idx])/w, h/sr

def analyse(fname, hop_s=0.001, win_s=None, cache=True):
    key = (fname, hop_s, win_s)
    if cache and key in _cache:
        return _cache[key]
    p = preset_of(fname)
    if win_s is None:
        bp = beat_period(p)
        win_s = bp if bp else 0.01
    x, sr = mono(os.path.join(REC_DIR, fname))
    ms, hop = _meansq(x, sr, win_s, hop_s)
    hz = 1.0/hop
    per = int(round(PERIOD*hz))
    lms = np.log(np.maximum(ms, 1e-12))
    n = len(ms); nn = n//per
    grid = np.arange(nn)*per
    d = int(round(0.05*hz))
    best, phi0 = -1e18, 0
    for phi in range(0, per, 2):
        a, b = phi+grid+d, phi+grid-d
        ok = (b >= 0) & (a < n)
        if ok.sum() < 2: continue
        s = (lms[a[ok]]-lms[b[ok]]).sum()
        if s > best: best, phi0 = s, phi
    ons = phi0 + grid
    W = int(1.2*hz)
    ons = ons[(ons-W >= 0) & (ons+per+W <= n)]
    tmpl = np.median(np.array([lms[i:i+per] for i in ons]), axis=0)
    tc = tmpl - tmpl.mean()
    aligned, shifts = [], []
    for i in ons:
        bestc, bs = -1e18, 0
        for step in (max(1, W//60), 1):
            lo, hi = (-W, W) if step > 1 else (bs-max(1, W//60), bs+max(1, W//60))
            for s in range(lo, hi+1, step):
                seg = lms[i+s:i+s+per]
                if len(seg) != per: continue
                c = np.dot(seg-seg.mean(), tc)
                if c > bestc: bestc, bs = c, s
        seg = ms[i+bs:i+bs+per]
        if len(seg) == per:
            aligned.append(seg); shifts.append(bs)
    aligned = np.array(aligned)
    # reject takes that do not match the median shape
    med = np.median(aligned, axis=0)
    fl = np.percentile(med, 2)*3
    lmed = np.log(np.maximum(med, fl))
    err = np.array([np.mean(np.abs(np.log(np.maximum(a, fl))-lmed)) for a in aligned])
    good = err < max(0.3, 3*np.median(err))
    aligned = aligned[good]
    m = aligned.mean(axis=0)
    t = np.arange(per)*hop
    out = dict(t=t, amp=np.sqrt(m), db=10*np.log10(np.maximum(m, 1e-14)),
               n=len(aligned), win_s=win_s, hop=hop, sr=sr, preset=p, x=x,
               ons=np.array(ons)[good] if len(ons) == len(good) else ons,
               shifts=shifts)
    if cache: _cache[key] = out
    return out

if __name__ == '__main__':
    for f in files():
        d = analyse(f)
        print('%-40s n=%2d win=%.3fs shifts=%s' % (f, d['n'], d['win_s'],
              np.round(np.array(d['shifts'])*d['hop'], 3)[:8]))
