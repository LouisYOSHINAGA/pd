"""Sounding gate length on the CZ-101: attack start -> release start, per take.

The MIDI gate of the recordings is exact (5.000 s for the presets, 10.000 s for the
sweeps), so any excess is the difference between note-off and note-on latency.
"""
import numpy as np, os, wave

ROOT = os.path.join(os.path.dirname(__file__), '..', 'czenvrec')

def load(path):
    with wave.open(path) as w:
        nch, sr = w.getnchannels(), w.getframerate()
        x = np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(float).reshape(-1, nch)[:, 0]/32768
    return x, sr

def peak_env(x, sr, blk_s=0.0002):
    b = max(1, int(blk_s*sr)); n = len(x)//b
    return np.abs(x[:n*b]).reshape(n, b).max(1), b/sr

def gates(path, gate, period):
    """Returns per-take (onset time, release start time) in seconds."""
    x, sr = load(path)
    e, dt = peak_env(x, sr)
    # rolling max over one waveform period: the per-cycle peak
    k = max(1, int(round(period/dt)))
    from numpy.lib.stride_tricks import sliding_window_view
    pk = sliding_window_view(np.concatenate([e, np.zeros(k - 1)]), k).max(1)
    noise = np.percentile(pk, 5)
    loud = pk > max(noise*8, pk.max()*0.003)
    on = np.where(loud[1:] & ~loud[:-1])[0] + 1
    ons = [i for i in on if not loud[max(0, i - int(1.0/dt)):i].any()]
    out = []
    for i in ons:
        j0 = i + int((gate - 1.0)/dt); j1 = i + int((gate + 0.5)/dt)
        if j1 >= len(pk): continue
        seg = pk[j0:j1]
        sus = np.median(seg[:int(0.8/dt)])
        # onset: first sample above the noise, searching back from the detected edge
        a = i
        while a > 0 and e[a] > noise*3: a -= 1
        # release start: first period-peak more than 1 dB below the sustain peak
        r = np.where(seg < sus*10**(-1/20))[0]
        r = r[r > int(0.8/dt)]
        if len(r) == 0: continue
        rel = j0 + r[0] - k          # the period-max lags by up to one period
        out.append((a*dt, rel*dt))
    return np.array(out)

if __name__ == '__main__':
    cases = [
        ('20260922/dca_r1_99_l1_99.wav', 10.0, 1/442.8, 'sweep r99 l99'),
        ('20260922/dca_r1_99_l1_74.wav', 10.0, 1/442.8, 'sweep r99 l74'),
        ('20260922/dca_r1_99_l1_49.wav', 10.0, 1/442.8, 'sweep r99 l49'),
        ('presets/cz101_06_elec_organ_vanilla.wav', 5.0, 1/441.4, 'Elec. Organ (r99 attack, r99 release)'),
        ('presets/cz101_07_flute_vanilla.wav', 5.0, 1/883, 'Flute (r99 attack, r66 release)'),
        ('presets/cz101_03_violin_vanilla.wav', 5.0, 1/883, 'Violin (r99 attack, r56 release)'),
        ('presets/cz101_08_synth_bass_vanilla.wav', 5.0, 1/220.7, 'Synth Bass (r99 attack, r61 release)'),
    ]
    for rel, gate, period, lab in cases:
        g = gates(os.path.join(ROOT, rel), gate, period)
        d = (g[:, 1] - g[:, 0] - gate)*1000
        print('%-40s takes=%2d  excess gate [ms]: median %+6.1f  min %+6.1f  max %+6.1f  (%s)'
              % (lab, len(d), np.median(d), d.min(), d.max(), ' '.join('%.1f' % v for v in d[:10])))
