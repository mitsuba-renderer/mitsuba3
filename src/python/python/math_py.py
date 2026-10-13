import drjit as dr

def chi2(obs, exp, pool_threshold):
    '''
    Pure-Python/numpy port of ``mitsuba::math::chi2()`` for backends that lack
    float64 (e.g. Metal). Cells with expected frequency below ``pool_threshold``
    are pooled into larger groups before contributing to the statistic.

    Returns ``(statistic, dof, n_pooled_in, n_pooled_out)``.
    '''
    import numpy as np
    obs = np.asarray(obs, dtype=np.float64)
    exp = np.asarray(exp, dtype=np.float64)

    # Cells above the threshold contribute directly (vectorized)
    high = exp >= pool_threshold
    diff = obs[high] - exp[high]
    chsq = float(np.sum(diff * diff / exp[high]))
    dof = int(np.count_nonzero(high))

    # Pool the remaining non-empty cells, preserving the caller's ordering
    low = ~high & ((obs != 0) | (exp != 0))
    pooled_obs = pooled_exp = 0.0
    n_pooled_in = n_pooled_out = 0
    for o, e in zip(obs[low], exp[low]):
        pooled_obs += o
        pooled_exp += e
        n_pooled_in += 1
        if pooled_exp > pool_threshold:
            d = pooled_obs - pooled_exp
            chsq += d * d / pooled_exp
            pooled_obs = pooled_exp = 0.0
            n_pooled_out += 1
            dof += 1

    return chsq, dof - 1, n_pooled_in, n_pooled_out

def rlgamma(a, x):
    'Regularized lower incomplete gamma function based on CEPHES'

    eps = 1e-15
    big = 4.503599627370496e15
    biginv = 2.22044604925031308085e-16

    if a < 0 or x < 0:
        raise "out of range"

    if x == 0:
        return 0

    ax = (a * dr.log(x)) - x - dr.lgamma(a)

    if ax < -709.78271289338399:
        return 1.0 if a < x else 0.0

    if x <= 1 or x <= a:
        r2 = a
        c2 = 1
        ans2 = 1

        while True:
            r2 = r2 + 1
            c2 = c2 * x / r2
            ans2 += c2

            if not (c2 / ans2 > eps):
                break

        return dr.exp(ax) * ans2 / a

    c = 0
    y = 1 - a
    z = x + y + 1
    p3 = 1
    q3 = x
    p2 = x + 1
    q2 = z * x
    ans = p2 / q2

    while True:
        c += 1
        y += 1
        z += 2
        yc = y * c
        p = (p2 * z) - (p3 * yc)
        q = (q2 * z) - (q3 * yc)

        if q != 0:
            nextans = p / q
            error = dr.abs((ans - nextans) / nextans)
            ans = nextans
        else:
            error = 1

        p3 = p2
        p2 = p
        q3 = q2
        q2 = q

        # normalize fraction when the numerator becomes large
        if dr.abs(p) > big:
            p3 *= biginv
            p2 *= biginv
            q3 *= biginv
            q2 *= biginv

        if not (error > eps):
            break

    return 1 - dr.exp(ax) * ans
