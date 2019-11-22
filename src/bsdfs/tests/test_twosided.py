import numpy as np
import pytest

from mitsuba.scalar_rgb.core import Frame3f
from mitsuba.scalar_rgb.core.math import Pi, InvPi
from mitsuba.scalar_rgb.core.warp import square_to_uniform_sphere
from mitsuba.scalar_rgb.core.xml import load_string
from mitsuba.scalar_rgb.render import BSDF, BSDFContext, SurfaceInteraction3f, BSDFFlags

@pytest.fixture(scope="module")
def interaction():
    si = SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = Frame3f(si.n)
    si.wavelengths = [300, 450, 520]
    return si


def test01_create():
    bsdf = load_string("""<bsdf version="2.0.0" type="twosided">
        <bsdf type="diffuse"/>
    </bsdf>""")
    assert bsdf is not None
    assert bsdf.component_count() == 2
    assert bsdf.flags(0) == BSDFFlags.DiffuseReflection | BSDFFlags.FrontSide
    assert bsdf.flags(1) == BSDFFlags.DiffuseReflection | BSDFFlags.BackSide
    assert bsdf.flags() == bsdf.flags(0) | bsdf.flags(1)

    bsdf = load_string("""<bsdf version="2.0.0" type="twosided">
        <bsdf type="roughconductor"/>
        <bsdf type="diffuse"/>
    </bsdf>""")
    assert bsdf is not None
    assert bsdf.component_count() == 2
    assert bsdf.flags(0) == BSDFFlags.GlossyReflection  | BSDFFlags.FrontSide
    assert bsdf.flags(1) == BSDFFlags.DiffuseReflection | BSDFFlags.BackSide
    assert bsdf.flags() == bsdf.flags(0) | bsdf.flags(1)


def test02_pdf(interaction):
    bsdf = load_string("""<bsdf version="2.0.0" type="twosided">
        <bsdf type="diffuse"/>
    </bsdf>""")

    interaction.wi = [0, 0, 1]
    ctx = BSDFContext()
    p_pdf = bsdf.pdf(ctx, interaction, [0, 0, 1])
    assert np.allclose(p_pdf, InvPi)

    p_pdf = bsdf.pdf(ctx, interaction, [0, 0, -1])
    assert np.allclose(p_pdf, 0.0)


def test03_sample_eval_pdf(interaction):
    bsdf = load_string("""<bsdf version="2.0.0" type="twosided">
        <bsdf type="diffuse">
            <rgb name="reflectance" value="0.1, 0.1, 0.1"/>
        </bsdf>
        <bsdf type="diffuse">
            <rgb name="reflectance" value="0.9, 0.9, 0.9"/>
        </bsdf>
    </bsdf>""")

    n = 5
    ctx = BSDFContext()
    for u, v in np.ndindex((n, n)):
        interaction.wi = square_to_uniform_sphere([u / float(n-1),
                                                   v / float(n-1)])
        up = np.dot(interaction.wi, [0, 0, 1]) > 0

        for x, y in np.ndindex((n, n)):
            sample = [x / float(n-1), y / float(n-1)]
            (bs, s_value) = bsdf.sample(ctx, interaction, 0.5, sample)

            if np.any(s_value > 0):
                # Multiply by square_to_cosine_hemisphere_theta
                s_value *= bs.wo[2] * InvPi
                if not up:
                    s_value *= -1

                e_value = bsdf.eval(ctx, interaction, bs.wo)
                p_pdf   = bsdf.pdf(ctx, interaction, bs.wo)

                assert np.allclose(s_value, e_value, atol=1e-2)
                assert np.allclose(bs.pdf, p_pdf)
                assert not np.any(np.isnan(e_value) | np.isnan(s_value))
            # Otherwise, sampling failed and we can't rely on bs.wo.
