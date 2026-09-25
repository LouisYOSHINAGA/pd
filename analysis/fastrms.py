"""1 ms RMS (take-averaged mean square) around a fast event; crossing times of dB levels."""
import numpy as np
from rmsenv import analyse
from wavutil import files
from dcasweep import load_raw, takes

def event_trace(x, sr, starts, span=0.3, win=0.0005):
    w = int(win*sr); acc = None
    for a in starts:
        seg = x[a: a + int(span*sr)]
        c = np.concatenate([[0], np.cumsum(seg*seg)])
        ms = (c[w:] - c[:-w])/w
        acc = ms if acc is None else acc[:len(ms)] + ms[:len(acc)]
    t = np.arange(len(acc))/sr + win/2
    return t, 10*np.log10(acc/len(starts) + 1e-14)

def crossings(t, db, top, levels, after=0.0, rising=True):
    out = []
    for lv in levels:
        m = (t >= after) & ((db > top + lv) if rising else (db < top + lv))
        i = np.where(m)[0]
        out.append(t[i[0]] if len(i) else np.nan)
    return np.array(out)

def preset_starts(sub, t_rel):
    f = [x for x in files() if sub in x][0]
    d = analyse(f, hop_s=0.001)
    return d['x'], d['sr'], [int(round((o+s)*d['hop']*d['sr'])) + int(t_rel*d['sr']) for o, s in zip(d['ons'], d['shifts'])]

if __name__ == '__main__':
    LV = [-40, -30, -20, -10, -3, -1]
    print('ATTACKS (time from -40 dB crossing, ms):      ', LV)
    for sub, lab in [('vibraphone', 'L1 r99 0->99, L2 r99 0->77'), ('elec_organ', 'L1 r99 0->99, L2 r99 0->84'),
                     ('fairy', 'r99 0->99'), ('percussion', 'r99 0->99 both'), ('elec_piano', 'L1 r94 0->99')]:
        x, sr, st = preset_starts(sub, -0.1)
        t, db = event_trace(x, sr, st)
        top = np.median(db[(t > 0.15) & (t < 0.2)]) if sub not in ('percussion', 'fairy', 'vibraphone') else db.max()
        c = crossings(t, db, top, LV)
        print('  %-11s %-28s' % (sub, lab), np.round((c - c[0])*1000, 2))
    for f, r in [('dca_r1_99_l1_99.wav', 99), ('dca_r1_74_l1_99.wav', 74)]:
        X, sr = load_raw(f); x = X[:, 0]
        tt, S, fl, hop, ons = takes(f)
        st = [int(o*hop*sr) - int(0.1*sr) for o in ons]
        t, db = event_trace(x, sr, st)
        top = np.median(db[(t > 0.2) & (t < 0.28)])
        c = crossings(t, db, top, LV)
        print('  %-11s %-28s' % ('sweep', 'r%d 0->99' % r), np.round((c - c[0])*1000, 2))
