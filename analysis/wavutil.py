import numpy as np, wave, os

REC_DIR = os.path.join(os.path.dirname(__file__), '..', 'czenvrec', 'presets')

def load(path):
    with wave.open(path, 'rb') as w:
        nch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    assert sw == 2
    x = np.frombuffer(raw, dtype='<i2').astype(np.float64) / 32768.0
    if nch > 1:
        x = x.reshape(-1, nch)
    return x, sr

def files():
    return sorted(f for f in os.listdir(REC_DIR) if f.endswith('.wav'))

def preset_no(fname):
    return int(fname.split('_')[1])
