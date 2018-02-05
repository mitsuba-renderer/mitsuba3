import numpy as np
import pytest

from mitsuba.core import Frame3f
from mitsuba.core.math import Pi
from mitsuba.core.warp import square_to_uniform_hemisphere, \
                              square_to_cosine_hemisphere
from mitsuba.core.xml import load_string
from mitsuba.render import BSDF, BSDFContext, SurfaceInteraction3f

def test01_create():
    b = load_string("<bsdf version='2.0.0' type='diffuse'></bsdf>")
    assert b is not None
    assert b.component_count() == 1
    assert b.flags(0) == (BSDF.EDiffuseReflection | BSDF.EFrontSide)
    assert b.flags() == b.flags(0)

def test02_pdf():
    bsdf = load_string("<bsdf version='2.0.0' type='diffuse'></bsdf>")

    si    = SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.wi = [0, 0, 1]
    si.sh_frame = Frame3f(si.n)

    p_pdf = bsdf.pdf(si, BSDFContext(), wo=[0, 0, 1])
    assert(np.allclose(p_pdf, 1.0 / Pi))

    p_pdf   = bsdf.pdf(si, BSDFContext(), wo=[0, 0, -1])
    assert(np.allclose(p_pdf, 0.0))

def test03_sample_eval_pdf():
    bsdf = load_string("<bsdf version='2.0.0' type='diffuse'></bsdf>")

    si = SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = Frame3f(si.n)

    N = 5
    for u, v in np.ndindex((N, N)):
        si.wi = square_to_uniform_hemisphere([u / float(N-1), v / float(N-1)])

        for x, y in np.ndindex((N, N)):
            sample2 = [x / float(N-1), y / float(N-1)]
            (bs, s_value) = bsdf.sample(si, BSDFContext(), 0.5, sample2)

            if np.any(s_value) > 0 :
                # Multiply by cos_theta
                s_value *= bs.wo[2] / Pi;

                e_value = bsdf.eval(si, BSDFContext(), bs.wo)
                p_pdf   = bsdf.pdf(si, BSDFContext(), bs.wo)

                assert(np.allclose(s_value, e_value, atol=1e-2))
                assert(np.all(np.isclose(bs.pdf, p_pdf) | np.allclose(s_value, 0.0)))
                assert not np.any(np.isnan(e_value) | np.isnan(s_value))
            # Otherwise, sampling failed and we can't rely on bs.wo.
