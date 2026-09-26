"""Parse CZ101PresetParam .csv into structured preset parameter records."""
import csv, os, re

CSV_PATH = os.path.join(os.path.dirname(__file__), '..', 'czenvrec', 'presets', 'CZ101PresetParam .csv')

DCO_BASE = 2      # wf1, wf2, then 8*(rate,level), sus, end, keyfollow
DCW_BASE = 23     # 8*(rate,level), sus, end, keyfollow
DCA_BASE = 42     # 8*(rate,level), sus, end
VIB_BASE = 60
OCT_RANGE = 65
DET_SIGN = 66
LINE_SELECT = 70
NOISE = 71
RING = 72


def _num(s):
    s = s.strip()
    if s == '' or s == '-':
        return None
    try:
        return int(s)
    except ValueError:
        return s


def _steps(row, base, n=8):
    out = []
    for i in range(n):
        r = _num(row[base + 2 * i])
        l = _num(row[base + 2 * i + 1])
        out.append((r, l))
    return out


def load():
    with open(CSV_PATH, newline='', encoding='utf-8-sig') as f:
        rows = list(csv.reader(f))
    presets = []
    i = 3
    while i < len(rows):
        row = rows[i]
        if not row or not row[0].strip():
            i += 1
            continue
        row2 = rows[i + 1]
        p = {
            'no': int(row[0]),
            'name': row[1].strip(),
            'vibrato': row[VIB_BASE].strip(),
            'vib_wave': _num(row[VIB_BASE + 1]),
            'vib_delay': _num(row[VIB_BASE + 2]),
            'vib_rate': _num(row[VIB_BASE + 3]),
            'vib_depth': _num(row[VIB_BASE + 4]),
            'octave_range': _num(row[OCT_RANGE]),
            'det_sign': row[DET_SIGN].strip(),
            'det_oct': _num(row[DET_SIGN + 1]),
            'det_note': _num(row[DET_SIGN + 2]),
            'det_fine': _num(row[DET_SIGN + 3]),
            'line_select': row[LINE_SELECT].strip(),
            'noise': row[NOISE].strip(),
            'ring': row[RING].strip(),
            'lines': [],
        }
        for r in (row, row2):
            line = {
                'wf1': _num(r[DCO_BASE]),
                'wf2': _num(r[DCO_BASE + 1]),
                # the two "Key Follow Range" columns are, in order, DCW and DCA key
                # follow (the CZ-101 has no DCO key follow)
                'dco': {'steps': _steps(r, DCO_BASE + 2), 'sus': _num(r[DCO_BASE + 18]),
                        'end': _num(r[DCO_BASE + 19]), 'kf': None},
                'dcw': {'steps': _steps(r, DCW_BASE), 'sus': _num(r[DCW_BASE + 16]),
                        'end': _num(r[DCW_BASE + 17]), 'kf': _num(r[DCO_BASE + 20])},
                'dca': {'steps': _steps(r, DCA_BASE), 'sus': _num(r[DCA_BASE + 16]),
                        'end': _num(r[DCA_BASE + 17]), 'kf': _num(r[DCW_BASE + 18])},
            }
            p['lines'].append(line)
        presets.append(p)
        i += 2
    return presets


def fmt_eg(eg):
    steps = ' '.join('%s/%s' % (('-' if r is None else r), ('-' if l is None else l))
                     for (r, l) in eg['steps'] if r is not None or l is not None)
    return '%-40s sus=%s end=%s kf=%s' % (steps, eg['sus'], eg['end'], eg['kf'])


if __name__ == '__main__':
    for p in load():
        print('=== %2d %s   line=%s ring=%s noise=%s vib=%s  oct=%s detune=%s%s/%s/%s' % (
            p['no'], p['name'], p['line_select'], p['ring'], p['noise'], p['vibrato'],
            p['octave_range'], p['det_sign'], p['det_oct'], p['det_note'], p['det_fine']))
        for li, l in enumerate(p['lines']):
            print('   L%d wf=%s,%s' % (li + 1, l['wf1'], l['wf2']))
            for k in ('dco', 'dcw', 'dca'):
                print('      %s %s' % (k.upper(), fmt_eg(l[k])))
