"""Helpers for the DCA key follow recordings (czenvrec/20260926); analysis in keyfollow2.py."""
import numpy as np, os, re, wave

D = os.path.join(os.path.dirname(__file__), '..', 'czenvrec', '20260926')
DB_PER_CODE = 20*np.log10(2)/12
TICK = 8.96e6/256

def code_of_speed(codes_per_s):
    """Continuous inverse of step(n) = (8 + (n & 7)) << (n >> 3)."""
    s = codes_per_s*(1 << 18)/TICK
    e = np.floor(np.log2(s/8.0))
    return 8*e + (s/2**e - 8)

def files():
    out = []
    for f in os.listdir(D):
        m = re.match(r'dca_r1_24_l1_99_kf_(\d)_note_(\d+)\.wav$', f)
        if m:
            out.append((f, int(m.group(1)), int(m.group(2))))
    return sorted(out, key=lambda r: (r[1], r[2]))

def load(f):
    with wave.open(os.path.join(D, f)) as w:
        nch, sr = w.getnchannels(), w.getframerate()
        x = np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(float).reshape(-1, nch)[:, 0]/32768
    return x, sr

def f0_of(x, sr):
    n = 1 << 16
    seg = x[:n]*np.hanning(min(n, len(x)))
    S = np.abs(np.fft.rfft(seg, n=4*n)); fr = np.fft.rfftfreq(4*n, 1/sr)
    m = fr > 20
    return fr[m][np.argmax(S[m])]
