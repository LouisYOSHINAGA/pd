"""Hypothesised CZ-101 envelope model (uPD933 chip EG driven by the CPU).

Chip side: per-sample (TICK Hz) accumulator moving toward a target by `step`
per tick; step = (8 + (n & 7)) << (n >> 3).  Target = L7 << SHIFT.
CPU side: display value (0..99) -> chip rate code n and 7-bit level L7.
"""
import numpy as np

TICK = 8.96e6/256          # 35 kHz
SHIFT = 18

def step_of(n):
    n = int(n)
    return (8 + (n & 7)) << (n >> 3)

def rate_code(r, kind='dca', rule='bank'):
    x = 1.25*r
    if rule == 'bank':
        return int(np.round(x))           # numpy: half to even
    if rule == 'up':
        return int(np.floor(x + 0.5))
    if rule == 'floor':
        return int(np.floor(x))
    raise ValueError(rule)

def level_code(l, kind='dca'):
    if kind == 'dca':
        return 0 if l == 0 else l + 28
    if kind == 'dcw':
        return int(round(l*127/99))
    if kind == 'dco':
        return l if l < 64 else l + 4
    raise ValueError(kind)

def simulate(eg, kind, t_off, t_end, rho=1.0, rule='bank', dt=1e-4):
    """Return times and accumulator value (in L7 units) of one EG.

    eg: dict(steps=[(rate, level)...], sus=int|None, end=int)
    """
    steps = [s for s in eg['steps'] if s[0] is not None]
    end = eg['end']
    sus = eg['sus']
    x = 0.0
    t = 0.0
    out_t, out_x = [0.0], [0.0]
    def speed(r):
        return rho*step_of(rate_code(r, kind, rule))*TICK/(1 << SHIFT)
    def target(i):
        if i == end - 1:
            return 0.0
        return float(level_code(steps[i][1], kind))
    def run(i, t, x, tstop):
        """run step i from (t,x) until done or tstop; return (t,x,done)."""
        tgt = target(i)
        v = speed(steps[i][0])
        if v <= 0:
            return tstop, x, False
        dur = abs(tgt - x)/v
        if t + dur <= tstop:
            return t + dur, tgt, True
        return tstop, x + np.sign(tgt - x)*v*(tstop - t), False
    i = 0
    released = False
    halted = False
    while t < t_end and not halted:
        if not released and t >= t_off:
            released = True
            i = (sus if (sus is not None and sus < end) else end - 1)
            # jump to the step after the sustain point (0-based: sus)
        tstop = t_off if (not released) else t_end
        # hold at sustain
        if not released and sus is not None and i == sus:
            out_t.append(t_off); out_x.append(x); t = t_off
            continue
        t, x, done = run(i, t, x, tstop)
        out_t.append(t); out_x.append(x)
        if done:
            if i == end - 1:
                halted = True
            else:
                i += 1
    out_t.append(t_end); out_x.append(x)
    tt = np.arange(0, t_end, dt)
    return tt, np.interp(tt, out_t, out_x), (out_t, out_x)
