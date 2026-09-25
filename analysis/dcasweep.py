"""Loader for the DCA rate/level sweep recordings (czenvrec/20260922)."""
import numpy as np, os, re, wave

SWEEP_DIR = os.path.join(os.path.dirname(__file__), '..', 'czenvrec', '20260922')

def sweep_files(directory=None):
    out = []
    for f in sorted(os.listdir(directory or SWEEP_DIR)):
        m = re.match(r'dca_r1_(\d+)_l1_(\d+)\.wav$', f)
        if m:
            out.append((f, int(m.group(1)), int(m.group(2))))
    return out

def load_raw(fname, directory=None):
    with wave.open(os.path.join(directory or SWEEP_DIR, fname), 'rb') as w:
        nch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        x = np.frombuffer(w.readframes(n), dtype='<i2').astype(np.float64)/32768.0
    return x.reshape(-1, nch), sr

def meansq(x, sr, win_s, hop_s):
    w = max(1, int(round(win_s*sr))); h = max(1, int(round(hop_s*sr)))
    c = np.concatenate([[0.0], np.cumsum(x*x)])
    idx = np.arange((len(x)-w)//h + 1)*h
    return (c[idx+w]-c[idx])/w, h/sr

def takes(fname, win_s=0.005, hop_s=0.0005, pre=0.5, post=None, thr_db=8.0, directory=None):
    """Mean-square envelope of each take, aligned at the note-on (first rise above floor)."""
    X, sr = load_raw(fname, directory)
    x = X[:, 0]
    ms, hop = meansq(x, sr, win_s, hop_s)
    db = 10*np.log10(ms + 1e-14)
    floor = np.percentile(db, 5)
    above = db > floor + thr_db
    # onsets: rising edges preceded by >= 1 s below threshold
    q = int(1.0/hop)
    edges = np.where(above[1:] & ~above[:-1])[0] + 1
    ons = [e for e in edges if e >= q and not above[e-q:e].any()]
    if post is None:
        post = 22.0 if '_r1_01_' in fname else 12.5
    segs = []
    for e in ons:
        a, b = e - int(pre/hop), e + int(post/hop)
        if a >= 0 and b <= len(db):
            segs.append(ms[a:b])
    t = np.arange(-int(pre/hop), int(post/hop))*hop
    return t, np.array(segs), floor, hop, ons
