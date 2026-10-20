"""Envelope rate/level tables of a CZ book (chapter II "pro-data", pp. 96-99), checked against eg.cpp.

The book converts a step to time with "conversion values" read from its tables:
    time [ms] = |level value of the previous step - level value| / R * n * C,
C = 0.2 (DCO), 0.1 (DCW), 0.05 (DCA); a note starts from level 0.
Level values are 512 per level code (DCA, DCW) or per semitone (DCO). R/n is a mantissa 8..15
over a power of two n for slow rates, and an integer with n = 1 for fast ones.

All entries follow one formula (one misprint: DCA rate 81 reads 142 for 152, DCW rate 81 is 304):
    code = 127r/99 (DCO), 119r/99 + 8 (DCW), 119r/99 (DCA)
    R/n  = step(code) / 256, step = (8 + (code & 7)) << (code >> 3), plus 1 << ((code >> 3) - 1)
           from exponent 9 (code >= 72) up; for exponent 8 the half LSB falls below R's integer
           and is not there (DCO rate 50 recording: 78.4 semitones/s, 83.0 with it)
and C is exactly the chip model of eg.cpp: C = units / (256 * 512) chip ticks at 35 kHz with
14 * 2^16 (DCO, per semitone), 14 * 2^15 (DCW) and 14 * 2^14 (DCA) chip units per level code.
"""
import numpy as np

TICK = 8.96e6/256

# rate -> R for n = 1, and rate -> (R, n) otherwise, as printed
DCO_RATE = {
    99: 1984, 98: 1728, 97: 1600, 96: 1472, 95: 1216, 94: 1088, 93: 992, 92: 928, 91: 800, 90: 736,
    89: 672, 88: 544, 87: 496, 86: 464, 85: 432, 84: 368, 83: 336, 82: 304, 81: 248, 80: 232,
    79: 216, 78: 200, 77: 168, 76: 152, 75: 136, 74: 116, 73: 108, 72: 100, 71: 92, 70: 76,
    69: 68, 68: 62, 67: 54, 66: 50, 65: 46, 64: 42, 63: 34, 62: 31, 61: 29, 60: 25,
    59: 23, 58: 21, 57: 19, 56: 15, 55: 14, 54: 13, 53: 11, 52: 10, 51: 9, 50: 8,
    49: (14, 2), 48: (13, 2), 47: (12, 2), 46: (11, 2), 45: (9, 2), 44: (8, 2), 43: (15, 4),
    42: (13, 4), 41: (12, 4), 40: (11, 4), 39: (10, 4), 38: (8, 4), 37: (15, 8), 36: (14, 8),
    35: (12, 8), 34: (11, 8), 33: (10, 8), 32: (9, 8), 31: (15, 16), 30: (14, 16), 29: (13, 16),
    28: (11, 16), 27: (10, 16), 26: (9, 16), 25: (8, 16), 24: (14, 32), 23: (13, 32), 22: (12, 32),
    21: (10, 32), 20: (9, 32), 19: (8, 32), 18: (15, 64), 17: (13, 64), 16: (12, 64), 15: (11, 64),
    14: (9, 64), 13: (8, 64), 12: (15, 128), 11: (14, 128), 10: (12, 128), 9: (11, 128),
    8: (10, 128), 7: (8, 128), 6: (15, 256), 5: (14, 256), 4: (13, 256), 3: (11, 256),
    2: (10, 256), 1: (9, 256), 0: (8, 256)}
DCW_RATE = {
    99: 1984, 98: 1728, 97: 1600, 96: 1472, 95: 1344, 94: 1088, 93: 992, 92: 928, 91: 864, 90: 800,
    89: 672, 88: 608, 87: 544, 86: 496, 85: 464, 84: 400, 83: 368, 82: 336, 81: 304, 80: 272,
    79: 232, 78: 216, 77: 200, 76: 184, 75: 168, 74: 136, 73: 124, 72: 116, 71: 108, 70: 100,
    69: 84, 68: 76, 67: 68, 66: 62, 65: 58, 64: 50, 63: 46, 62: 42, 61: 38, 60: 34,
    59: 29, 58: 27, 57: 25, 56: 23, 55: 21, 54: 17, 53: 15, 52: 14, 51: 13, 50: 12,
    49: 10, 48: 9, 47: 8, 46: (15, 2), 45: (14, 2), 44: (12, 2), 43: (11, 2), 42: (10, 2),
    41: (9, 2), 40: (8, 2), 39: (14, 4), 38: (13, 4), 37: (12, 4), 36: (11, 4), 35: (10, 4),
    34: (8, 4), 33: (15, 8), 32: (14, 8), 31: (13, 8), 30: (12, 8), 29: (10, 8), 28: (9, 8),
    27: (8, 8), 26: (15, 16), 25: (14, 16), 24: (12, 16), 23: (11, 16), 22: (10, 16), 21: (9, 16),
    20: (8, 16), 19: (14, 32), 18: (13, 32), 17: (12, 32), 16: (11, 32), 15: (10, 32), 14: (8, 32),
    13: (15, 64), 12: (14, 64), 11: (13, 64), 10: (12, 64), 9: (10, 64), 8: (9, 64), 7: (8, 64),
    6: (15, 128), 5: (14, 128), 4: (12, 128), 3: (11, 128), 2: (10, 128), 1: (9, 128), 0: (8, 128)}
DCA_RATE = {
    99: 992, 98: 864, 97: 800, 96: 736, 95: 672, 94: 544, 93: 496, 92: 464, 91: 432, 90: 400,
    89: 336, 88: 304, 87: 272, 86: 248, 85: 232, 84: 200, 83: 184, 82: 168, 81: 142, 80: 136,
    79: 116, 78: 108, 77: 100, 76: 92, 75: 84, 74: 68, 73: 62, 72: 58, 71: 54, 70: 50,
    69: 42, 68: 38, 67: 34, 66: 31, 65: 29, 64: 25, 63: 23, 62: 21, 61: 19, 60: 17,
    59: 14, 58: 13, 57: 12, 56: 11, 55: 10, 54: 8, 53: (15, 2), 52: (14, 2), 51: (13, 2),
    50: (12, 2), 49: (10, 2), 48: (9, 2), 47: (8, 2), 46: (15, 4), 45: (14, 4), 44: (12, 4),
    43: (11, 4), 42: (10, 4), 41: (9, 4), 40: (8, 4), 39: (14, 8), 38: (13, 8), 37: (12, 8),
    36: (11, 8), 35: (10, 8), 34: (8, 8), 33: (15, 16), 32: (14, 16), 31: (13, 16), 30: (12, 16),
    29: (10, 16), 28: (9, 16), 27: (8, 16), 26: (15, 32), 25: (14, 32), 24: (12, 32), 23: (11, 32),
    22: (10, 32), 21: (9, 32), 20: (8, 32), 19: (14, 64), 18: (13, 64), 17: (12, 64), 16: (11, 64),
    15: (10, 64), 14: (8, 64), 13: (15, 128), 12: (14, 128), 11: (13, 128), 10: (12, 128),
    9: (10, 128), 8: (9, 128), 7: (8, 128), 6: (15, 256), 5: (14, 256), 4: (12, 256),
    3: (11, 256), 2: (10, 256), 1: (9, 256), 0: (8, 256)}

# level conversion values, as printed, are 512 * these (semitones for the DCO, level codes else)
LEVEL = {
    'dco': lambda l: l/8 if l < 64 else 2*(l - 60),
    'dcw': lambda l: 127*l//99,
    'dca': lambda l: 0 if l == 0 else l + 28,
}
CODE = {'dco': lambda r: 127*r//99, 'dcw': lambda r: 119*r//99 + 8, 'dca': lambda r: 119*r//99}
C_MS = {'dco': 0.2, 'dcw': 0.1, 'dca': 0.05}
UNITS = {'dco': 14*2**16, 'dcw': 14*2**15, 'dca': 14*2**14}     # chip units per level code / semitone

def step(code):
    e = code >> 3
    return ((8 + (code & 7)) << e) + ((1 << (e - 1)) if e >= 9 else 0)

def printed(entry):
    return float(entry) if not isinstance(entry, tuple) else entry[0]/entry[1]

if __name__ == '__main__':
    for kind, table in (('dco', DCO_RATE), ('dcw', DCW_RATE), ('dca', DCA_RATE)):
        bad = []
        for r, entry in sorted(table.items()):
            rn = step(CODE[kind](r))/256
            if printed(entry) != rn:
                bad.append('rate %d printed %s, formula %g' % (r, entry, rn))
        print('%s rate table vs formula: %d entries, mismatches: %s' % (kind, len(table), bad or 'none'))
    # the book's time constant equals the chip model: C = 1 / (35 kHz * 256 / units * 512 / 1000)
    for kind in ('dco', 'dcw', 'dca'):
        c = UNITS[kind]/(256*512)/TICK*1e3
        print('%s: C from the chip model %.4f ms (book %.2f)' % (kind, c, C_MS[kind]))
    # the book's worked example: DCO step 1 = (rate 50, level 66) from level 0
    t = 512*LEVEL['dco'](66)/(step(CODE['dco'](50))/256)*C_MS['dco']
    print('example: DCO rate 50 to level 66 takes %.1f ms (book 153.6 ms)' % t)
