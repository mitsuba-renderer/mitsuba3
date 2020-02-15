import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import UInt32


def test01_create(variant_scalar_rgb):
    from mitsuba.render import BSDFFlags
    from mitsuba.core.xml import load_string

    bsdf = load_string("""<bsdf version="2.0.0" type="blendbsdf">
        <bsdf type="diffuse"/>
        <bsdf type="diffuse"/>
        <spectrum name="weight" value="0.2"/>
    </bsdf>""")
    assert bsdf is not None
    assert bsdf.component_count() == 2
    assert bsdf.flags(0) == BSDFFlags.DiffuseReflection | BSDFFlags.FrontSide
    assert bsdf.flags(1) == BSDFFlags.DiffuseReflection | BSDFFlags.FrontSide
    assert bsdf.flags() == bsdf.flags(0) | bsdf.flags(1)

    bsdf = load_string("""<bsdf version="2.0.0" type="blendbsdf">
        <bsdf type="roughconductor"/>
        <bsdf type="diffuse"/>
        <spectrum name="weight" value="0.2"/>
    </bsdf>""")
    assert bsdf is not None
    assert bsdf.component_count() == 2
    assert bsdf.flags(0) == BSDFFlags.GlossyReflection | BSDFFlags.FrontSide
    assert bsdf.flags(1) == BSDFFlags.DiffuseReflection | BSDFFlags.FrontSide
    assert bsdf.flags() == bsdf.flags(0) | bsdf.flags(1)


def test02_eval_all(variant_scalar_rgb):
    from mitsuba.core import Frame3f
    from mitsuba.render import BSDFFlags, BSDFContext, SurfaceInteraction3f
    from mitsuba.core.xml import load_string
    from mitsuba.core.math import InvPi

    weight = 0.2

    bsdf = load_string("""<bsdf version="2.0.0" type="blendbsdf">
        <bsdf type="diffuse">
            <spectrum name="reflectance" value="0.0"/>
        </bsdf>
        <bsdf type="diffuse">
            <spectrum name="reflectance" value="1.0"/>
        </bsdf>
        <spectrum name="weight" value="{}"/>
    </bsdf>""".format(weight))

    si = SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = Frame3f(si.n)
    si.wi = [0, 0, 1]

    wo = [0, 0, 1]
    ctx = BSDFContext()

    # Evaluate the blend of both components
    expected = (1 - weight) * 0.0 * InvPi + weight * 1.0 * InvPi
    value    = bsdf.eval(ctx, si, wo)
    assert ek.allclose(value, expected)


def test03_eval_components(variant_scalar_rgb):
    from mitsuba.core import Frame3f
    from mitsuba.render import BSDFFlags, BSDFContext, SurfaceInteraction3f
    from mitsuba.core.xml import load_string
    from mitsuba.core.math import InvPi

    weight = 0.2

    bsdf = load_string("""<bsdf version="2.0.0" type="blendbsdf">
        <bsdf type="diffuse">
            <spectrum name="reflectance" value="0.0"/>
        </bsdf>
        <bsdf type="diffuse">
            <spectrum name="reflectance" value="1.0"/>
        </bsdf>
        <spectrum name="weight" value="{}"/>
    </bsdf>""".format(weight))

    si = SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = Frame3f(si.n)
    si.wi = [0, 0, 1]

    wo = [0, 0, 1]
    ctx = BSDFContext()

    # Evaluate the two components separately

    ctx.component = 0
    value0 = bsdf.eval(ctx, si, wo)
    expected0 = (1-weight) * 0.0*InvPi
    assert ek.allclose(value0, expected0)

    ctx.component = 1
    value1 = bsdf.eval(ctx, si, wo)
    expected1 = weight * 1.0*InvPi
    assert ek.allclose(value1, expected1)


def test04_sample_all(variant_scalar_rgb):
    from mitsuba.core import Frame3f
    from mitsuba.render import BSDFFlags, BSDFContext, SurfaceInteraction3f
    from mitsuba.core.xml import load_string
    from mitsuba.core.math import InvPi

    weight = 0.2

    bsdf = load_string("""<bsdf version="2.0.0" type="blendbsdf">
        <bsdf type="diffuse">
            <spectrum name="reflectance" value="0.0"/>
        </bsdf>
        <bsdf type="diffuse">
            <spectrum name="reflectance" value="1.0"/>
        </bsdf>
        <spectrum name="weight" value="{}"/>
    </bsdf>""".format(weight))

    si = SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = Frame3f(si.n)
    si.wi = [0, 0, 1]

    ctx = BSDFContext()

    # Sample using two different values of 'sample1' and make sure correct
    # components are chosen.

    expected_a = 1.0    # InvPi & weight will cancel out with sampling pdf
    bs_a, weight_a = bsdf.sample(ctx, si, 0.1, [0.5, 0.5])
    assert ek.allclose(weight_a, expected_a)

    expected_b = 0.0    # InvPi & weight will cancel out with sampling pdf
    bs_b, weight_b = bsdf.sample(ctx, si, 0.3, [0.5, 0.5])
    assert ek.allclose(weight_b, expected_b)


def test05_sample_components(variant_scalar_rgb):
    from mitsuba.core import Frame3f
    from mitsuba.render import BSDFFlags, BSDFContext, SurfaceInteraction3f
    from mitsuba.core.xml import load_string
    from mitsuba.core.math import InvPi

    weight = 0.2

    bsdf = load_string("""<bsdf version="2.0.0" type="blendbsdf">
        <bsdf type="diffuse">
            <spectrum name="reflectance" value="0.0"/>
        </bsdf>
        <bsdf type="diffuse">
            <spectrum name="reflectance" value="1.0"/>
        </bsdf>
        <spectrum name="weight" value="{}"/>
    </bsdf>""".format(weight))

    si = SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = Frame3f(si.n)
    si.wi = [0, 0, 1]

    ctx = BSDFContext()

    # Sample specific components separately using two different values of 'sample1'
    # and make sure the desired component is chosen always.

    ctx.component = 0

    expected_a = (1-weight)*0.0    # InvPi will cancel out with sampling pdf, but still need to apply weight
    bs_a, weight_a = bsdf.sample(ctx, si, 0.1, [0.5, 0.5])
    assert ek.allclose(weight_a, expected_a)

    expected_b = (1-weight)*0.0    # InvPi will cancel out with sampling pdf, but still need to apply weight
    bs_b, weight_b = bsdf.sample(ctx, si, 0.3, [0.5, 0.5])
    assert ek.allclose(weight_b, expected_b)

    ctx.component = 1

    expected_a = weight*1.0    # InvPi will cancel out with sampling pdf, but still need to apply weight
    bs_a, weight_a = bsdf.sample(ctx, si, 0.1, [0.5, 0.5])
    assert ek.allclose(weight_a, expected_a)

    expected_b = weight*1.0    # InvPi will cancel out with sampling pdf, but still need to apply weight
    bs_b, weight_b = bsdf.sample(ctx, si, 0.3, [0.5, 0.5])
    assert ek.allclose(weight_b, expected_b)
