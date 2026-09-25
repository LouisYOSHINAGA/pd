import numpy as np, sys, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from scipy.optimize import minimize
from rmsenv import analyse
from wavutil import files
import dcamodel as M

def fit_one(f, rule='bank', table=True, fix_rho=None, t_off=5.0):
    d = analyse(f)
    p = d['preset']
    t, db = d['t'], d['db']
    ms = 10**(db/10)
    floor = np.percentile(ms, 3)
    def pred(ton, rho, g):
        m = M.predict_ms(p, t, d['win_s'], rho=rho, t_on=ton, t_off=t_off, rule=rule, table=table)
        return 10*np.log10(m*10**(g/10) + floor)
    def err(q):
        ton, lrho, g = q
        rho = fix_rho if fix_rho is not None else np.exp(lrho)
        return np.mean(np.abs(db - pred(ton, rho, g)))
    g0 = db.max() - 10*np.log10(max(M.predict_ms(p, t, d['win_s']).max(), 1e-12))
    best = None
    rhos = [0.0] if fix_rho is not None else np.log(np.linspace(0.7, 1.5, 17))
    for lr in rhos:
        for ton0 in np.arange(-0.35, 0.36, 0.02):
            e = err((ton0, lr, g0))
            if best is None or e < best[0]: best = (e, ton0, lr)
    x0 = np.array([best[1], best[2], g0])
    simplex = [x0, x0 + [0.02, 0, 0], x0 + [0, 0.05, 0], x0 + [0, 0, 1.0]]
    res = minimize(err, x0=x0, method='Nelder-Mead',
                   options=dict(xatol=1e-5, fatol=1e-5, maxiter=3000, initial_simplex=simplex))
    ton, lrho, g = res.x
    rho = fix_rho if fix_rho is not None else np.exp(lrho)
    return dict(d=d, p=p, ton=ton, rho=rho, g=g, err=res.fun, pred=pred(ton, rho, g))

if __name__ == '__main__':
    rule = sys.argv[1] if len(sys.argv) > 1 else 'bank'
    fix = float(sys.argv[2]) if len(sys.argv) > 2 else None
    fig, axes = plt.subplots(8, 2, figsize=(20, 30))
    for ax, f in zip(axes.T.ravel(), files()):
        r = fit_one(f, rule, fix_rho=fix)
        p, d = r['p'], r['d']
        print('%2d %-18s ton=%+.3f rho=%.3f  mean|err|=%.2f dB   (kf dcoblk=%s dcwblk=%s)'
              % (p['no'], p['name'], r['ton'], r['rho'], r['err'],
                 p['lines'][0]['dco']['kf'], p['lines'][0]['dcw']['kf']), flush=True)
        top = d['db'].max()
        ax.plot(d['t'], d['db'] - top, lw=0.8, label='rec')
        ax.plot(d['t'], r['pred'] - top, lw=0.8, label='model')
        ax.set_ylim(-80, 3); ax.set_xlim(0, 8); ax.grid(alpha=0.3)
        ax.set_title('%d %s  rho=%.3f err=%.2fdB' % (p['no'], p['name'], r['rho'], r['err']), fontsize=9)
    axes[0, 0].legend()
    plt.tight_layout(); plt.savefig('plots/fit_dca_%s%s.png' % (rule, '' if fix is None else '_fix'), dpi=65)
