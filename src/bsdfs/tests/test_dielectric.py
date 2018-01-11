import numpy as np
import pytest

from mitsuba.core import Frame3f
from mitsuba.core.math import Pi
from mitsuba.core.warp import square_to_uniform_hemisphere, \
                              square_to_cosine_hemisphere
from mitsuba.core.xml import load_string
from mitsuba.render import BSDF, BSDFSample3f, BSDFSample3fX, SurfaceInteraction3f

def example_bsdf(reflectance = 0.5, transmittance = 0.5):
    return load_string("""<bsdf version="2.0.0" type="dielectric">
            <spectrum name="specular_reflectance" value="{}"/>
            <spectrum name="specular_transmittance" value="{}"/>
        </bsdf>""".format(reflectance, transmittance))

def test01_create():
    b = load_string("<bsdf version='2.0.0' type='dielectric'></bsdf>")
    assert b is not None
    b = load_string("""<bsdf version="2.0.0" type="dielectric">
            <float name="ext_ior" value="1.5"/>
            <float name="int_ior" value="0.5"/>
            <rgb name="specular_reflectance" value="0.5, 0.9, 1.0"/>
            <spectrum name="specular_transmittance" value="400:1.2, 500:3.9"/>
        </bsdf>""")

    # Should not accept negative IORs
    with pytest.raises(RuntimeError):
        load_string("""<bsdf version="2.0.0" type="dielectric">
                <float name="int_ior" value="-0.5"/>
            </bsdf>""")

def test02_sample_specific_component():
    """The lobe sampled should respect `bs.type_mask` and `bs.component`."""
    si = SurfaceInteraction3f()
    si.wi = [0.09759001,  0.19518001,  0.97590007]
    wo_reflected   = [-0.09759001,  -0.19518001,  0.97590007]
    wo_transmitted = [-0.06487907, -0.12975813, -0.98942083]
    b = example_bsdf()
    samples = [[0.01, 0.01], [0.9, 0.9]]

    bs = BSDFSample3f(si, [0, 0, 0])
    bs.component = -1
    bs.type_mask = BSDF.EDeltaReflection
    for sample in samples:
        (_, pdf) = b.sample(bs, sample)
        assert bs.sampled_component == 0
        assert bs.sampled_type == BSDF.EDeltaReflection
        assert np.allclose(pdf, 1.0)
        assert bs.eta == 1.0
        assert np.allclose(bs.wo, wo_reflected)

    bs = BSDFSample3f(si, [0, 0, 0])
    bs.component = -1
    bs.type_mask = BSDF.EDeltaTransmission
    for sample in samples:
        (_, pdf) = b.sample(bs, sample)
        assert bs.sampled_component == 1
        assert bs.sampled_type == BSDF.EDeltaTransmission
        assert np.allclose(pdf, 1.0)
        assert np.allclose(bs.wo, wo_transmitted)

    bs = BSDFSample3f(si, [0, 0, 0])
    bs.component = 1
    bs.type_mask = BSDF.EDelta
    for sample in samples:
        (_, pdf) = b.sample(bs, sample)
        assert bs.sampled_component == 1
        assert bs.sampled_type == BSDF.EDeltaTransmission
        assert np.allclose(pdf, 1.0)
        assert np.allclose(bs.wo, wo_transmitted)

    # Allow both lobes
    bs = BSDFSample3f(si, [0, 0, 0])
    bs.component = -1
    bs.type_mask = BSDF.EDelta
    (_, pdf) = b.sample(bs, samples[0])
    assert np.all(bs.sampled_component == 0)
    assert np.all(bs.sampled_type == BSDF.EDeltaReflection)
    assert np.all(pdf < 1.0)
    assert np.allclose(bs.wo, wo_reflected)
    (_, pdf) = b.sample(bs, samples[1])
    assert np.all(bs.sampled_component == 1)
    assert np.all(bs.sampled_type == BSDF.EDeltaTransmission)
    assert np.all(pdf < 1.0)
    assert np.allclose(bs.wo, wo_transmitted)

    # TODO: use a BSDFSample3fX instead once it has been bound
    # bs_all = BSDFSample3f(si, [0, 0, 0])
    # bs_all.component = -1
    # bs_all.type_mask = BSDF.EDelta
    # bs = BSDFSample3fX(2)
    # bs[0] = bs_all
    # bs[1] = bs_all
    # (_, pdf) = b.sample(bs, samples)
    # assert np.all(bs.sampled_component == [0, 1])
    # assert np.all(bs.sampled_type == [BSDF.EDeltaReflection, BSDF.EDeltaTransmission])
    # assert np.all(pdf < 1.0)
    # assert np.all(bs.wo == [wo_reflected, wo_transmitted])
