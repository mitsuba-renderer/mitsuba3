import numpy as np
import pytest

from mitsuba.core import Frame3f
from mitsuba.core.math import Pi
from mitsuba.core.warp import square_to_uniform_hemisphere, \
                              square_to_cosine_hemisphere
from mitsuba.core.xml import load_string
from mitsuba.render import BSDFSample3f, SurfaceInteraction3f

def test01_create():
    b = load_string("<bsdf version='2.0.0' type='diffuse'></bsdf>")
    assert b is not None

def test02_pdf():
    bsdf = load_string("<bsdf version='2.0.0' type='diffuse'></bsdf>")

    nn = [0, 0, 1]
    frame = Frame3f(nn)

    si = SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = nn
    si.sh_frame = frame

    wi = [0, 0, 1]
    wo = [0, 0, 1]
    bs = BSDFSample3f(si, wi, wo)
    p_pdf = bsdf.pdf(bs)
    assert(np.allclose(p_pdf, 1.0 / Pi))

    wi = [0, 0, 1]
    wo = [0, 0, -1]
    bs = BSDFSample3f(si, wi, wo)
    p_pdf   = bsdf.pdf(bs)
    assert(np.allclose(p_pdf, 0.0))

def test03_sample_eval_pdf():
    bsdf = load_string("<bsdf version='2.0.0' type='diffuse'></bsdf>")

    nn = [0, 0, 1]
    frame = Frame3f(nn)

    si = SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = nn
    si.sh_frame = frame

    N = 5
    for u, v in np.ndindex((N, N)):
        si.wi = square_to_uniform_hemisphere([u / float(N-1), v / float(N-1)])

        for x, y in np.ndindex((N, N)):
            bs = BSDFSample3f(si, [0, 0, 1])

            sample = [x / float(N-1), y / float(N-1)]
            (s_value, s_pdf) = bsdf.sample(bs, sample)

            # multiply by cos_theta
            s_value = s_value * bs.wo[2] / Pi;

            e_value = bsdf.eval(bs)
            p_pdf   = bsdf.pdf(bs)

            assert(np.allclose(s_pdf, p_pdf))
            if p_pdf > 0 :
                assert(np.allclose(s_value, e_value, atol=1e-2))
