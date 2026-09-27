"""czenvrec/20260927_4: what sets the high-note DCW cap and the key follow subtraction.

  C1  key 84, KF 0, DCO +12 st held      -> stops like key 96 KF 0 (cap follows the pitch)
  C4  key 84, KF 9, DCO +12 st held      -> stops like key 84 KF 9 (subtraction by the key)
  C3  key 84, KF 0, DCO rate 20 glide +12 -> the cap is that of the current pitch
  E   key 96, DCW (99, 99) then (24, 0) sustain: at KF 0 the output stays at the cap until
      the accumulator falls below it (the cap acts on the output); KF 9 subtracts >= 127
DCW step 1 = (24, 99) sustain for C1, C3, C4. Also measures the DCO glide speed of C3.
"""
import numpy as np, os
from dcwkf import files, load, onsets, table, dcw_track, REC
from dcwkf2 import spectra, hinge, CODES_PER_S

D = os.path.join(REC, '20260927_4')

def track_glide(f, hold=7.9, hop=0.02, nh=7, fmin=1000, fmax=2250):
    """dcw(t) with the harmonic bands following the measured f0 of every frame."""
    x, sr = load(f)
    N = int(0.046*sr); H = int(hop*sr); w = np.hanning(N); nfr = int(hold/hop)
    P = np.zeros((nfr, nh)); F0 = np.zeros(nfr); used = 0
    fr = np.fft.rfftfreq(4*N, 1/sr)
    for o in onsets(x, sr):
        if o + int(hold*sr) + N > len(x): continue
        used += 1
        for j in range(nfr):
            X = np.abs(np.fft.rfft(x[o + j*H: o + j*H + N]*w, 4*N))**2
            m = (fr > fmin) & (fr < fmax)
            f0 = fr[m][np.argmax(X[m])]; F0[j] += f0
            for k in range(nh):
                P[j, k] += X[(fr > (k + 0.7)*f0) & (fr < (k + 1.3)*f0)].sum()
    grid, T = table(nh)
    r = 10*np.log10(P + 1e-30); r -= r.max(1, keepdims=True)
    est = np.array([grid[np.argmin(np.mean(np.abs(T[:, r[i] > -55] - np.maximum(r[i, r[i] > -55], -80)), 1))]
                    for i in range(nfr)])
    return (np.arange(nfr)*H + N/2)/sr, est, F0/used

if __name__ == '__main__':
    l99 = {(kf, n): f for f, kf, n in files()}
    print('stop code with DCO +12 st at key 84 (oscillator at the pitch of key 96):')
    for tag, kf, ref in (('C1', 0, 75.1), ('C4', 9, 68.8)):
        f = os.path.join(D, 'dco_r1_99_l1_66_dcw_r1_24_l1_99_kf_%d_note_84.wav' % kf)
        t, est, _ = dcw_track(f, 96)
        print('   %s KF %d: %.1f   (key 96 KF 0 without DCO: 75.1, key 84 KF %d without DCO: %.1f)'
              % (tag, kf, CODES_PER_S*hinge(t, est, 0.3), kf, 102.3 if kf == 0 else ref))
    t, est, F0 = track_glide(os.path.join(D, 'dco_r1_20_l1_66_dcw_r1_24_l1_99_note_84.wav'))
    print('C3 (DCO glide): stop code %.1f at %.2f s (cap at the note-on pitch would be 102.3)'
          % (CODES_PER_S*hinge(t, est, 0.3), hinge(t, est, 0.3)))
    m = (t > 0.5) & (t < 3.8)
    slope = np.polyfit(t[m], 12*np.log2(F0[m]/(1046.5*1.0032)), 1)[0]
    print('   DCO rate 20 glide: %.2f semitones/s (eg.cpp model: %.2f)' % (slope, 80*8.96e6/256/(12*65536)))
    print('E (key 96): dcw code at 0.5/2/3/3.5/4/5/6/7 s')
    print('   cap on the target:  %s' % ' '.join('%5.1f' % max(0, 75.1 - CODES_PER_S*s) for s in (0.5, 2, 3, 3.5, 4, 5, 6, 7)))
    print('   cap on the output:  %s' % ' '.join('%5.1f' % min(75.1, 127 - CODES_PER_S*s) for s in (0.5, 2, 3, 3.5, 4, 5, 6, 7)))
    for kf in (0, 9):
        f = os.path.join(D, 'dcw_r1_99_l1_99_r2_24_l2_00_r3_99_l3_00_kf_%d_note_96.wav' % kf)
        t, est, _ = dcw_track(f, 96)
        code = est*127
        print('   KF %d measured:     %s' % (kf, ' '.join('%5.1f' % np.median(code[int(s/0.02) - 2:int(s/0.02) + 3])
                                                     for s in (0.5, 2, 3, 3.5, 4, 5, 6, 7))))
