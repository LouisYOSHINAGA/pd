"""Python port of the VST oscillator (pd.cpp) for spectrum modelling."""
import numpy as np
PI = np.pi
EPS = PI/64
C = 0.97   # depth at DCW 1.0 (= level code 127), pd.h kDcwMaxDepth

def phase(wf, t, dcw):
    c = C*dcw
    if wf == 1:   # saw
        bp = PI*(1-c); sl = 1/(1-c); sr = 1/(1+c)
        return np.where(t < bp, sl*t, PI + sr*(t-bp))
    if wf == 2:   # square
        bp = PI*(1-c); e = EPS*c; sl = (PI-e)/(PI*(1-c)); sr = EPS/PI
        return np.select([t < bp, t < PI, t < PI+bp],
                         [sl*t, (PI-e)+sr*(t-bp), PI+sl*(t-PI)],
                         (2*PI-e)+sr*(t-(PI+bp)))
    if wf == 3:   # pulse
        bp = PI*c; e = EPS*c; sl = EPS/PI; sr = (PI-e)/(PI*(1-c))
        return np.select([t < bp, t < PI, t < 2*PI-bp],
                         [sl*t, e+sr*(t-bp), PI+sr*(t-PI)],
                         (2*PI-e)+sl*(t-(2*PI-bp)))
    if wf == 4:   # double sine
        bp = PI*(1-c); sl = 2/(1-c); sr = 2/(1+c)
        return np.where(t < bp, sl*t, 2*PI + sr*(t-bp))
    if wf == 5:   # saw pulse
        bp = PI*(1-c); e = EPS*c; sl = (PI-e)/(PI*(1-c)); sr = EPS/(1+c)
        return np.select([t < PI, t < PI+bp], [t, PI+sl*(t-PI)], (2*PI-e)+sr*(t-bp))
    raise ValueError(wf)

def cycle(wf, t, dcw):
    """One oscillator cycle for waveforms 1..8 (6..8: resonance, as in pd.cpp)."""
    if wf <= 5:
        return -np.cos(phase(wf, t, dcw))
    k = 1 + 13.3*dcw   # pd.h kMaxFreqMult
    if wf == 6:
        env = 1 - t/(2*PI - EPS)
    elif wf == 7:
        env = np.where(t < PI, EPS + (1-EPS)/PI*t, EPS + 1 - (1-EPS)/PI*(t-PI))
    else:
        env = np.where(t < PI, 1.0, 1 - (t-PI)/(PI-EPS))
    return env*(-np.cos(k*t) + 1) - 1

def wave(wf1, wf2, dcw, n=4096):
    """One full period (two cycles if a second waveform alternates)."""
    t = np.arange(n)/n*2*PI
    a = cycle(wf1, t, dcw)
    if wf2:
        return np.concatenate([a, cycle(wf2, t, dcw)]), 2
    return a, 1

def harmonics(wf1, wf2, dcw, nharm=16):
    """Power of harmonics of the *base* frequency grid (f0/2 grid if alternating)."""
    y, cyc = wave(wf1, wf2, dcw)
    Y = np.abs(np.fft.rfft(y))**2
    return Y[1:nharm+1]
