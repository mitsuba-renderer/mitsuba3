import numpy as np
import pytest

from mitsuba.core.xml import load_string
from mitsuba.core import Frame3f
from mitsuba.render import BSDFSample3f
from mitsuba.render import BSDF
from mitsuba.render import SurfaceInteraction3f
from mitsuba.core.warp import square_to_uniform_hemisphere
from mitsuba.core.warp import square_to_uniform_sphere
from mitsuba.core.warp import square_to_cosine_hemisphere
from mitsuba.core.math import Pi, InvPi

@pytest.fixture
def interaction():
    nn = [0, 0, 1]
    frame = Frame3f(nn)

    si = SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = nn
    si.sh_frame = frame
    si.wavelengths = [300, 450, 520, 680]
    return si

def test01_create():
    bsdf = load_string("""<bsdf version="2.0.0" type="twosided">
        <bsdf type="diffuse"/>
    </bsdf>""")
    assert bsdf is not None
    bsdf = load_string("""<bsdf version="2.0.0" type="twosided">
        <bsdf type="diffuse"/>
        <bsdf type="roughconductor"/>
    </bsdf>""")
    assert bsdf is not None

def test02_pdf(interaction):
    bsdf = load_string("""<bsdf version="2.0.0" type="twosided">
        <bsdf type="diffuse"/>
    </bsdf>""")

    wi = [0, 0, 1]
    wo = [0, 0, 1]
    bs = BSDFSample3f(interaction, wi, wo)
    p_pdf = bsdf.pdf(bs)
    assert np.allclose(p_pdf, InvPi)

    wi = [0, 0, 1]
    wo = [0, 0, -1]
    bs = BSDFSample3f(interaction, wi, wo)
    p_pdf = bsdf.pdf(bs)
    assert np.allclose(p_pdf, 0.0)

def test03_sample_eval_pdf(interaction):
    bsdf = load_string("""<bsdf version="2.0.0" type="twosided">
        <bsdf type="diffuse">
            <rgb name="reflectance" value="0.1f, 0.1f, 0.1f"/>
        </bsdf>
        <bsdf type="diffuse">
            <rgb name="reflectance" value="0.9f, 0.9f, 0.9f"/>
        </bsdf>
    </bsdf>""")

    n = 5
    for u, v in np.ndindex((n, n)):
        interaction.wi = square_to_uniform_sphere([u / float(n-1),
                                                   v / float(n-1)])
        up = np.dot(interaction.wi, [0, 0, 1]) > 0

        for x, y in np.ndindex((n, n)):
            bs = BSDFSample3f(interaction, [0, 0, 1])

            sample = [x / float(n-1), y / float(n-1)]
            (s_value, s_pdf) = bsdf.sample(bs, sample)
            # multiply by square_to_cosine_hemisphere_theta
            s_value = s_value * (bs.wo[2] * InvPi);
            if not up:
                s_value *= -1

            e_value = bsdf.eval(bs)
            p_pdf   = bsdf.pdf(bs)

            assert np.allclose(s_pdf, p_pdf)
            if p_pdf > 0 :
                assert not np.any(np.isnan(e_value) | np.isnan(s_value))
                assert np.allclose(s_value, e_value, atol=1e-2)
