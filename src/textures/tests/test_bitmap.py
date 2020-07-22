import mitsuba
import pytest

from mitsuba.python.test.util import fresolver_append_path


@fresolver_append_path
@pytest.mark.parametrize('filter_type', ['nearest', 'bilinear'])
@pytest.mark.parametrize('wrap_mode', ['repeat', 'clamp', 'mirror'])
def test01_sample_position(variant_packet_rgb, filter_type, wrap_mode):
    from mitsuba.core.xml import load_string
    from mitsuba.python.chi2 import ChiSquareTest, PlanarDomain
    from mitsuba.core import ScalarBoundingBox2f

    bitmap = load_string("""
    <texture type="bitmap" version="2.0.0">
        <string name="filename" value="resources/data/common/textures/carrot.png"/>
        <string name="filter_type" value="%s"/>
        <string name="wrap_mode" value="%s"/>
    </texture>""" % (filter_type, wrap_mode)).expand()[0]

    chi2 = ChiSquareTest(
        domain=PlanarDomain(ScalarBoundingBox2f([0, 0], [1, 1])),
        sample_func=lambda s: bitmap.sample_position(s)[0],
        pdf_func=lambda p: bitmap.pdf_position(p),
        sample_dim=2,
        res=201,
        ires=8
    )

    assert chi2.run()


@fresolver_append_path
def test02_eval_grad(variant_scalar_rgb):
    # Tests evaluating the texture gradient under different rotation
    from mitsuba.render import SurfaceInteraction3f
    from mitsuba.core.xml import load_string
    from mitsuba.core import Vector2f
    import numpy as np
    import enoki as ek
    delta = 1e-4
    si = SurfaceInteraction3f()
    for u01 in np.random.rand(10, 1):
        angle = 360.0*u01[0]
        bitmap = load_string("""
        <texture type="bitmap" version="2.0.0">
            <string name="filename" value="resources/data/common/textures/noise_8x8.png"/>
            <transform name="to_uv">
                <rotate angle="%f"/>
            </transform>
        </texture>""" % angle).expand()[0]
        for uv in np.random.rand(10, 2):
            si.uv = Vector2f(uv)
            f = bitmap.eval_1(si)
            si.uv = uv + Vector2f(delta, 0)
            fu = bitmap.eval_1(si)
            si.uv = uv + Vector2f(0, delta)
            fv = bitmap.eval_1(si)
            gradient_finite_difference = Vector2f((fu - f)/delta, (fv - f)/delta)
            gradient_analytic = bitmap.eval_1_grad(si)
            assert ek.allclose(0, ek.abs(gradient_finite_difference/gradient_analytic - 1.0), atol = 1e04)