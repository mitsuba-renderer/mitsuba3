import numpy as np
import pytest

from mitsuba.core import Frame3f
from mitsuba.core.math import Pi
from mitsuba.core.warp import square_to_uniform_hemisphere, \
                              square_to_cosine_hemisphere
from mitsuba.core.xml import load_string
from mitsuba.render import BSDF, BSDFContext, SurfaceInteraction3f, \
                           SurfaceInteraction3fX

def example_bsdf(reflectance = 0.5, transmittance = 0.5):
    return load_string("""<bsdf version="2.0.0" type="dielectric">
            <spectrum name="specular_reflectance" value="{}"/>
            <spectrum name="specular_transmittance" value="{}"/>
        </bsdf>""".format(reflectance, transmittance))

def test01_create():
    b = load_string("<bsdf version='2.0.0' type='dielectric'></bsdf>")
    assert b is not None
    assert b.component_count() == 2
    assert b.flags(0) == (BSDF.EDeltaReflection | BSDF.EFrontSide | BSDF.EBackSide)
    assert b.flags(1) == (BSDF.EDeltaTransmission | BSDF.EFrontSide
                          | BSDF.EBackSide | BSDF.ENonSymmetric)
    assert b.flags() == b.flags(0) | b.flags(1)

    b = load_string("""<bsdf version="2.0.0" type="dielectric">
            <float name="ext_ior" value="1.5"/>
            <float name="int_ior" value="0.5"/>
            <rgb name="specular_reflectance" value="0.5, 0.9, 1.0"/>
            <spectrum name="specular_transmittance" value="400:1.2, 500:3.9"/>
        </bsdf>""")
    assert b is not None

    # Should not accept negative IORs
    with pytest.raises(RuntimeError):
        load_string("""<bsdf version="2.0.0" type="dielectric">
                <float name="int_ior" value="-0.5"/>
            </bsdf>""")

def test02_sample_specific_component():
    """The lobe sampled should respect `bs.type_mask` and `bs.component`."""
    si = SurfaceInteraction3f()
    si.t = 0.88
    si.wi = [0.09759001,  0.19518001,  0.97590007]
    wo_reflected   = [-0.09759001,  -0.19518001,  0.97590007]
    wo_transmitted = [-0.06487907, -0.12975813, -0.98942083]

    b = example_bsdf()
    ctx = BSDFContext()
    samples1 = [0.01, 0.9, 0.55]
    samples2 = 0.5 * np.ones(shape=(3, 2))  # Not used by this BSDF

    ctx.component = np.uint32(-1)
    ctx.type_mask = BSDF.EDeltaReflection
    for sample in samples1:
        (bs, _) = b.sample(si, ctx, sample, samples2[0])
        assert bs.sampled_component == 0
        assert bs.sampled_type == BSDF.EDeltaReflection
        assert np.allclose(bs.pdf, 1.0)
        assert bs.eta == 1.0
        assert np.allclose(bs.wo, wo_reflected)

    ctx.component = np.uint32(-1)
    ctx.type_mask = BSDF.EDeltaTransmission
    for sample in samples1:
        (bs, _) = b.sample(si, ctx, sample, samples2[0])
        assert bs.sampled_component == 1
        assert bs.sampled_type == BSDF.EDeltaTransmission
        assert np.allclose(bs.pdf, 1.0)
        assert np.allclose(bs.wo, wo_transmitted)

    ctx.component = 1
    ctx.type_mask = BSDF.EDelta
    for sample in samples1:
        (bs, _) = b.sample(si, ctx, sample, samples2[0])
        assert bs.sampled_component == 1
        assert bs.sampled_type == BSDF.EDeltaTransmission
        assert np.allclose(bs.pdf, 1.0)
        assert np.allclose(bs.wo, wo_transmitted)

    # Allow both lobes
    ctx.component = np.uint32(-1)
    ctx.type_mask = BSDF.EDelta
    (bs, _) = b.sample(si, ctx, samples1[0], samples2[0])
    assert np.all(bs.sampled_component == 0)
    assert np.all(bs.sampled_type == BSDF.EDeltaReflection)
    assert np.all(bs.pdf < 1.0)
    assert np.allclose(bs.wo, wo_reflected)
    (bs, _) = b.sample(si, ctx, samples1[1], samples2[0])
    assert np.all(bs.sampled_component == 1)
    assert np.all(bs.sampled_type == BSDF.EDeltaTransmission)
    assert np.all(bs.pdf < 1.0)
    assert np.allclose(bs.wo, wo_transmitted)

    # Same, but with a vectorized call
    six = SurfaceInteraction3fX(3)
    for i in range(len(six)):
        six[i] = si

    (bs, _) = example_bsdf().sample(six, ctx, samples1, samples2)
    assert np.all(bs.sampled_component == [0, 1, 1])
    assert np.all(bs.sampled_type == [BSDF.EDeltaReflection, BSDF.EDeltaTransmission,
                                      BSDF.EDeltaTransmission])
    assert np.all(bs.pdf < 1.0)
    assert np.allclose(bs.wo, [wo_reflected, wo_transmitted, wo_transmitted])
