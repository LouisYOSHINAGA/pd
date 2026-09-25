"""Experiments of czenvrec/20260925 (see readme.txt there).

A) Synth Bass (preset 8) with DCA step 2 rate re-entered on the panel (21 -> 22 -> 21):
   the decay must be unchanged if panel entry and factory data share one conversion.
B) Initialized patch, DCA step 1 = (24, 99): attack time compared with 20260922.
C) Same as B with the DCW key follow re-entered (0 -> 1 -> 0).
Also compares the harmonic content at DCW = 0 of presets and of the initialized patch; the
latter matches waveform 8 (Resonance III) at DCW 0, which is not a pure sine.
"""
import numpy as np, os
from dcasweep import load_raw, takes
from rmsenv import analyse
import pdosc

D0922 = os.path.join(os.path.dirname(__file__), '..', 'czenvrec', '20260922')
D0925 = os.path.join(os.path.dirname(__file__), '..', 'czenvrec', '20260925')

def decay_slope(x, sr, starts, a=0.6, b=4.6):
    w = int(0.02*sr); out = []
    for s in starts:
        seg = x[s + int(a*sr): s + int(b*sr)]
        n = len(seg)//w; ms = (seg[:n*w].reshape(n, w)**2).mean(1)
        out.append(np.polyfit((np.arange(n) + 0.5)*w/sr, 10*np.log10(ms), 1)[0])
    return np.array(out)

def attack_time(fname, directory):
    t, S, fl, hop, ons = takes(fname, win_s=0.004, hop_s=0.0005, pre=0.3, post=3.0, directory=directory)
    db = 10*np.log10(S.mean(0) + 1e-14); db -= np.median(db[(t > 2.0) & (t < 2.9)])
    c = [t[np.where(db > a)[0][0]] for a in (-50, -1)]
    return c[1] - c[0], len(S)

def harmonics(x, sr, starts, t0, f_lo=300, f_hi=600, nh=4, n=32768):
    acc = 0
    for s in starts:
        seg = x[s + int(t0*sr): s + int(t0*sr) + n]
        if len(seg) == n:
            acc = acc + np.abs(np.fft.rfft(seg*np.hanning(n)))**2
    fr = np.fft.rfftfreq(n, 1/sr)
    f0 = fr[np.argmax(acc*((fr > f_lo) & (fr < f_hi)))]
    P = [acc[(fr > k*f0 - 8) & (fr < k*f0 + 8)].sum() for k in range(1, nh + 1)]
    return [10*np.log10(p/P[0]) for p in P[1:]]

if __name__ == '__main__':
    X, sr = load_raw('cz101_preset_09_step2_r2_re21.wav', D0925)
    t, S, fl, hop, ons = takes('cz101_preset_09_step2_r2_re21.wav', post=9.0, directory=D0925)
    sa = decay_slope(X[:, 0], sr, [int(o*hop*sr) for o in ons])
    d = analyse('cz101_08_synth_bass_vanilla.wav', hop_s=0.001)
    s0 = decay_slope(d['x'], d['sr'], [int(round((o+s)*d['hop']*d['sr'])) for o, s in zip(d['ons'], d['shifts'])])
    print('A) Synth Bass DCA step 2 (rate 21) decay: re-entered %.3f dB/s, original %.3f dB/s' % (sa.mean(), s0.mean()))
    for lab, dd, f in (('B) 20260922', D0922, 'dca_r1_24_l1_99.wav'), ('B) 20260925', D0925, 'dca_r1_24_l1_99.wav'),
                       ('C) DCW KF re-entered', D0925, 'dca_r1_24_l1_99_dcw_kf_re0.wav')):
        tt, n = attack_time(f, dd)
        print('%s: initialized patch, DCA step 1 rate 24: -50 -> -1 dB in %.3f s (%d takes)' % (lab, tt, n))
    print('   (factory-preset law: rate 24 -> code 30, ~6.5 s; observed ~0.97 s = code 52)')
    print('harmonics H2..H4 relative to H1 [dB]:')
    for f, lab, lo, hi, t0 in (('cz101_10_vibraphone_vanilla.wav', 'Vibraphone L1 saw, DCW -> 0', 300, 600, 1.0),
                               ('cz101_05_elec_piano_vanilla.wav', 'Elec. Piano L1 saw, DCW 0', 300, 600, 1.0),
                               ('cz101_15_whistle_vanilla.wav', 'Whistle square, DCW 0', 1500, 2000, 2.0)):
        d = analyse(f, hop_s=0.001)
        st = [int(round((o+s)*d['hop']*d['sr'])) for o, s in zip(d['ons'], d['shifts'])]
        print('   %-34s %s' % (lab, ' '.join('%6.1f' % v for v in harmonics(d['x'], d['sr'], st, t0, lo, hi))))
    for name, dd, f in (('20260922', D0922, 'dca_r1_99_l1_99.wav'), ('20260925', D0925, 'dca_r1_24_l1_99.wav'),
                        ('20260925 C', D0925, 'dca_r1_24_l1_99_dcw_kf_re0.wav')):
        X, sr = load_raw(f, dd); t, S, fl, hop, ons = takes(f, directory=dd)
        print('   %-34s %s' % ('initialized patch (%s), DCW 0' % name,
                                ' '.join('%6.1f' % v for v in harmonics(X[:, 0], sr, [int(o*hop*sr) for o in ons], 3.0))))
    h = pdosc.harmonics(8, 0, 0.0, 4)
    print('   %-34s %s' % ('model: waveform 8 (Resonance III), DCW 0', ' '.join('%6.1f' % v for v in 10*np.log10(h[1:]/h[0]))))
