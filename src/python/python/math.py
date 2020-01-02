import enoki as ek

def rlgamma(a, x):
    'Regularized lower incomplete gamma function based on CEPHES'

    eps = 1e-15
    big = 4.503599627370496e15
    biginv = 2.22044604925031308085e-16

    if a < 0 or x < 0:
        raise "out of range"

    if x == 0:
        return 0

    ax = (a * ek.log(x)) - x - ek.lgamma(a)

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

            if c2 / ans2 <= eps:
                break

        return ek.exp(ax) * ans2 / a

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
            error = ek.abs((ans - nextans) / nextans)
            ans = nextans
        else:
            error = 1

        p3 = p2
        p2 = p
        q3 = q2
        q2 = q

        # normalize fraction when the numerator becomes large
        if ek.abs(p) > big:
            p3 *= biginv
            p2 *= biginv
            q3 *= biginv
            q2 *= biginv

        if error <= eps:
            break;

    return 1 - ek.exp(ax) * ans
