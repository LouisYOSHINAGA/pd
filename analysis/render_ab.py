"""Level-matched A/B WAVs of the 16 presets: CZ-101 recording vs VST (offline harness).

For every preset one take of the recording and a VST render with the same note (A4), gate
(5 s) and the CZ-101's tuning (master tune +10 cents) are aligned at the point where they reach
-20 dB re their maximum, and the VST is scaled to the RMS of the recording over the first
second. Writes analysis/render/ab/NN_name_{cz,vst,ab}.wav (44.1 kHz, 16 bit; "ab" = CZ-101,
1 s of silence, VST). Nothing here is committed (analysis/render/ is ignored).
"""
import numpy as np, os, wave
from czparam import load
from render_all import render, SR
from rmsenv import analyse
from wavutil import files

OUT = os.path.join(os.path.dirname(__file__), 'render', 'ab')
PRE, LENGTH, TAKE = 0.3, 7.5, 1

def rms_db(y, win_s=0.0025, hop_s=0.0005):
    w = int(win_s*SR); h = int(hop_s*SR)
    c = np.concatenate([[0], np.cumsum(y*y)])
    idx = np.arange((len(y) - w)//h)*h
    return idx, 10*np.log10((c[idx + w] - c[idx])/w + 1e-14)

def align(y, search_s=0.8):
    """Sample index where y first reaches -20 dB re its maximum within search_s."""
    idx, db = rms_db(y[:int(search_s*SR)])
    return idx[np.argmax(db > db.max() - 20)]

def excerpt(y, at):
    s = at - int(PRE*SR)
    seg = y[max(0, s): s + int(LENGTH*SR)]
    if s < 0:
        seg = np.concatenate([np.zeros(-s), seg])
    return np.pad(seg, (0, int(LENGTH*SR) - len(seg)))

def write(path, y):
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((np.clip(y, -1, 1)*32767).astype('<i2').tobytes())

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    P = {p['no']: p for p in load()}
    for f in files():
        no = int(f.split('_')[1])
        name = os.path.basename(f)[len('cz101_'):].replace('_vanilla', '').replace('.wav', '')
        d = analyse(f, hop_s=0.001)
        x = d['x']
        i0 = int(round((d['ons'][TAKE] + d['shifts'][TAKE])*d['hop']*SR))
        cz_raw = x[max(0, i0 - int(0.5*SR)): i0 + int(7.5*SR)]
        cz = excerpt(cz_raw, align(cz_raw))
        vst_raw = np.concatenate([np.zeros(int(0.5*SR)), render(P[no], tag='new', gate=5.0, total=7.5, tune=10)])
        vst = excerpt(vst_raw, align(vst_raw))
        ref = slice(int(PRE*SR), int((PRE + 1.0)*SR))
        vst *= np.sqrt(np.mean(cz[ref]**2)/np.mean(vst[ref]**2))
        peak = max(np.abs(cz).max(), np.abs(vst).max())
        if peak > 0.99:
            cz, vst = cz*0.99/peak, vst*0.99/peak
        base = os.path.join(OUT, name)
        write(base + '_cz.wav', cz)
        write(base + '_vst.wav', vst)
        write(base + '_ab.wav', np.concatenate([cz, np.zeros(SR), vst]))
        print('wrote', base + '_{cz,vst,ab}.wav')
