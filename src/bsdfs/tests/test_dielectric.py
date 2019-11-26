import numpy as np
import pytest

from mitsuba.scalar_rgb.core.xml import load_string
from mitsuba.scalar_rgb.render import BSDF, BSDFContext, SurfaceInteraction3f, TransportMode, BSDFFlags

def example_bsdf(reflectance=0.3, transmittance=0.6):
    return load_string("""<bsdf version="2.0.0" type="dielectric">
            <spectrum name="specular_reflectance" value="{}"/>
            <spectrum name="specular_transmittance" value="{}"/>
            <float name="int_ior" value="1.5"/>
            <float name="ext_ior" value="1"/>
        </bsdf>""".format(reflectance, transmittance))


def test01_create():
    b = load_string("<bsdf version='2.0.0' type='dielectric'></bsdf>")
    assert b is not None
    assert b.component_count() == 2
    assert b.flags(0) == (BSDFFlags.DeltaReflection | BSDFFlags.FrontSide |
                          BSDFFlags.BackSide)
    assert b.flags(1) == (BSDFFlags.DeltaTransmission | BSDFFlags.FrontSide |
                          BSDFFlags.BackSide | BSDFFlags.NonSymmetric)
    assert b.flags() == b.flags(0) | b.flags(1)

    # Should not accept negative IORs
    with pytest.raises(RuntimeError):
        load_string("""<bsdf version="2.0.0" type="dielectric">
                <float name="int_ior" value="-0.5"/>
            </bsdf>""")


def test02_sample():
    si = SurfaceInteraction3f()
    si.wi = [0, 0, 1]
    bsdf = example_bsdf()

    for i in range(2):
        ctx = BSDFContext(TransportMode.Importance if i == 0 else TransportMode.Radiance)

        # Sample reflection
        bs, spec = bsdf.sample(ctx, si, 0, [0, 0])
        assert np.allclose(spec, [0.3] * 3)
        assert np.allclose(bs.pdf, 0.04)
        assert np.allclose(bs.eta, 1.0)
        assert np.allclose(bs.wo, [0, 0, 1])
        assert bs.sampled_component == 0
        assert bs.sampled_type == +BSDFFlags.DeltaReflection

        # Sample refraction
        bs, spec = bsdf.sample(ctx, si, 0.05, [0, 0])
        if i == 0:
            assert np.allclose(spec, [0.6] * 3)
        else:
            assert np.allclose(spec, [0.6 / 1.5**2] * 3)
        assert np.allclose(bs.pdf, 1 - 0.04)
        assert np.allclose(bs.eta, 1.5)
        assert np.allclose(bs.wo, [0, 0, -1])
        assert bs.sampled_component == 1
        assert bs.sampled_type == +BSDFFlags.DeltaTransmission


def test03_sample_reverse():
    si = SurfaceInteraction3f()
    si.wi = [0, 0, -1]
    bsdf = example_bsdf()

    for i in range(2):
        ctx = BSDFContext(TransportMode.Importance if i == 0 else TransportMode.Radiance)

        # Sample reflection
        bs, spec = bsdf.sample(ctx, si, 0, [0, 0])
        assert np.allclose(spec, [0.3] * 3)
        assert np.allclose(bs.pdf, 0.04)
        assert np.allclose(bs.eta, 1.0)
        assert np.allclose(bs.wo, [0, 0, -1])
        assert bs.sampled_component == 0
        assert bs.sampled_type == +BSDFFlags.DeltaReflection

        # Sample refraction
        bs, spec = bsdf.sample(ctx, si, 0.05, [0, 0])
        if i == 0:
            assert np.allclose(spec, [0.6] * 3)
        else:
            assert np.allclose(spec, [0.6 * 1.5**2] * 3)
        assert np.allclose(bs.pdf, 1 - 0.04)
        assert np.allclose(bs.eta, 1 / 1.5)
        assert np.allclose(bs.wo, [0, 0, 1])
        assert bs.sampled_component == 1
        assert bs.sampled_type == +BSDFFlags.DeltaTransmission


def test04_sample_specific_component():
    si = SurfaceInteraction3f()
    si.wi = [0, 0, 1]
    bsdf = example_bsdf()

    for i in range(2):
        for sample in np.linspace(0, 1, 3):
            for sel_type in range(2):
                ctx = BSDFContext(TransportMode.Importance if i == 0 else TransportMode.Radiance)

                # Sample reflection
                if sel_type == 0:
                    ctx.type_mask = BSDFFlags.DeltaReflection
                else:
                    ctx.component = 0
                bs, spec = bsdf.sample(ctx, si, sample, [0, 0])
                assert np.allclose(spec, [0.3 * 0.04] * 3)
                assert np.allclose(bs.pdf, 1.0)
                assert np.allclose(bs.eta, 1.0)
                assert np.allclose(bs.wo, [0, 0, 1])
                assert bs.sampled_component == 0
                assert bs.sampled_type == +BSDFFlags.DeltaReflection

                # Sample refraction
                if sel_type == 0:
                    ctx.type_mask = BSDFFlags.DeltaTransmission
                else:
                    ctx.component = 1
                bs, spec = bsdf.sample(ctx, si, sample, [0, 0])
                if i == 0:
                    assert np.allclose(spec, [0.6 * (1 - 0.04)] * 3)
                else:
                    assert np.allclose(spec, [0.6 * (1 - 0.04) / 1.5**2] * 3)
                assert np.allclose(bs.pdf, 1.0)
                assert np.allclose(bs.eta, 1.5)
                assert np.allclose(bs.wo, [0, 0, -1])
                assert bs.sampled_component == 1
                assert bs.sampled_type == +BSDFFlags.DeltaTransmission

    ctx = BSDFContext()
    ctx.component = 3
    bs, spec = bsdf.sample(ctx, si, 0, [0, 0])
    assert np.all(spec == [0] * 3)


def test05_spot_check():
    angle = 80 * np.pi / 180
    ctx = BSDFContext()
    si = SurfaceInteraction3f()
    wi = [np.sin(angle), 0, np.cos(angle)]
    si.wi = wi
    bsdf = example_bsdf()

    bs, spec = bsdf.sample(ctx, si, 0, [0, 0])
    assert np.allclose(bs.pdf, 0.387704354691473)
    assert np.allclose(bs.wo, [-np.sin(angle), 0, np.cos(angle)])

    bs, spec = bsdf.sample(ctx, si, 1, [0, 0])
    angle = 41.03641052520335 * np.pi / 180
    assert np.allclose(bs.pdf, 1 - 0.387704354691473)
    assert np.allclose(bs.wo, [-np.sin(angle), 0, -np.cos(angle)])

    si.wi = bs.wo
    bs, spec = bsdf.sample(ctx, si, 1, [0, 0])
    assert np.allclose(bs.pdf, 1 - 0.387704354691473)
    assert np.allclose(bs.wo, wi)
