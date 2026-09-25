"""DCA model: chip EG + volume table, and trace prediction for a preset."""
import numpy as np
import czeg

DB_PER_UNIT = 20*np.log10(2)/12             # 1/12 octave per level code
_I = np.arange(512)
_VT = np.where(_I < 24, 0.0, (1024*2.0**((_I - 508)/48.0) - 0.5)/1024)

def vol_amp(x, table=True):
    x = np.asarray(x, dtype=float)
    if table:
        i = np.clip((x*4).astype(int), 0, 511)
        a = _VT[i]
    else:
        a = 10**(DB_PER_UNIT*(x-127)/20)
    return np.where(x <= 0, 0.0, a)

def line_amps(p, t_off, t_end, dt, rho=1.0, rule='cz', table=True):
    use = {'1': [0], '2': [1], "1+1'": [0, 0], "1+2'": [0, 1]}[p['line_select']]
    out = []
    for li in use:
        r = rho[li] if isinstance(rho, (list, tuple)) else rho
        tt, xx, _ = czeg.simulate(p['lines'][li]['dca'], 'dca', t_off, t_end, rho=r, rule=rule, dt=dt)
        out.append(vol_amp(xx, table))
    return tt, out

def coherent(p):
    return (p['line_select'] == "1+2'" and p['det_oct'] == 0 and p['det_note'] == 0
            and p['det_fine'] == 0)

def predict_ms(p, t, win_s, rho=1.0, t_on=0.0, t_off=5.0, rule='cz', table=True):
    dt = t[1] - t[0]
    tt, amps = line_amps(p, t_off, t[-1] + 1.0, dt, rho, rule, table)
    if coherent(p) and len(amps) == 2:
        ms = (amps[0] + amps[1])**2/2
    else:
        ms = sum(a**2 for a in amps)/2
    k = max(1, int(round(win_s/dt)))
    # silence before the note-on so that the forward window sees the onset
    ms = np.concatenate([np.zeros(k), ms])
    tt = np.concatenate([tt[0] - dt*np.arange(k, 0, -1), tt])
    c = np.concatenate([[0], np.cumsum(ms)])
    m = (c[k:] - c[:-k])/k          # window [t, t+W) like the measurement
    return np.interp(t, tt[:len(m)] + t_on, m, left=0.0, right=m[-1])
