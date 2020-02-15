import pytest
import enoki as ek


def example_bsdf(reflectance=0.3, transmittance=0.6):
    from mitsuba.core.xml import load_string
    return load_string("""<bsdf version="2.0.0" type="dielectric">
            <spectrum name="specular_reflectance" value="{}"/>
            <spectrum name="specular_transmittance" value="{}"/>
            <float name="int_ior" value="1.5"/>
            <float name="ext_ior" value="1"/>
        </bsdf>""".format(reflectance, transmittance))


def test01_create(variant_scalar_rgb):
    from mitsuba.render import BSDFFlags
    from mitsuba.core.xml import load_string

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


def test02_sample(variant_scalar_rgb):
    from mitsuba.render import BSDFContext, SurfaceInteraction3f, \
        TransportMode, BSDFFlags

    si = SurfaceInteraction3f()
    si.wi = [0, 0, 1]
    bsdf = example_bsdf()

    for i in range(2):
        ctx = BSDFContext(TransportMode.Importance if i == 0
                          else TransportMode.Radiance)

        # Sample reflection
        bs, spec = bsdf.sample(ctx, si, 0, [0, 0])
        assert ek.allclose(spec, [0.3] * 3)
        assert ek.allclose(bs.pdf, 0.04)
        assert ek.allclose(bs.eta, 1.0)
        assert ek.allclose(bs.wo, [0, 0, 1])
        assert bs.sampled_component == 0
        assert bs.sampled_type == +BSDFFlags.DeltaReflection

        # Sample refraction
        bs, spec = bsdf.sample(ctx, si, 0.05, [0, 0])
        if i == 0:
            assert ek.allclose(spec, [0.6] * 3)
        else:
            assert ek.allclose(spec, [0.6 / 1.5**2] * 3)
        assert ek.allclose(bs.pdf, 1 - 0.04)
        assert ek.allclose(bs.eta, 1.5)
        assert ek.allclose(bs.wo, [0, 0, -1])
        assert bs.sampled_component == 1
        assert bs.sampled_type == +BSDFFlags.DeltaTransmission


def test03_sample_reverse(variant_scalar_rgb):
    from mitsuba.render import BSDFContext, SurfaceInteraction3f, \
        TransportMode, BSDFFlags

    si = SurfaceInteraction3f()
    si.wi = [0, 0, -1]
    bsdf = example_bsdf()

    for i in range(2):
        ctx = BSDFContext(TransportMode.Importance if i == 0
                          else TransportMode.Radiance)

        # Sample reflection
        bs, spec = bsdf.sample(ctx, si, 0, [0, 0])
        assert ek.allclose(spec, [0.3] * 3)
        assert ek.allclose(bs.pdf, 0.04)
        assert ek.allclose(bs.eta, 1.0)
        assert ek.allclose(bs.wo, [0, 0, -1])
        assert bs.sampled_component == 0
        assert bs.sampled_type == +BSDFFlags.DeltaReflection

        # Sample refraction
        bs, spec = bsdf.sample(ctx, si, 0.05, [0, 0])
        if i == 0:
            assert ek.allclose(spec, [0.6] * 3)
        else:
            assert ek.allclose(spec, [0.6 * 1.5**2] * 3)
        assert ek.allclose(bs.pdf, 1 - 0.04)
        assert ek.allclose(bs.eta, 1 / 1.5)
        assert ek.allclose(bs.wo, [0, 0, 1])
        assert bs.sampled_component == 1
        assert bs.sampled_type == +BSDFFlags.DeltaTransmission


def test04_sample_specific_component(variant_scalar_rgb):
    from mitsuba.render import BSDFContext, SurfaceInteraction3f, \
        TransportMode, BSDFFlags

    si = SurfaceInteraction3f()
    si.wi = [0, 0, 1]
    bsdf = example_bsdf()

    for i in range(2):
        for sample in [0.0, 0.5, 1.0]:
            for sel_type in range(2):
                ctx = BSDFContext(TransportMode.Importance if i == 0
                                  else TransportMode.Radiance)

                # Sample reflection
                if sel_type == 0:
                    ctx.type_mask = BSDFFlags.DeltaReflection
                else:
                    ctx.component = 0
                bs, spec = bsdf.sample(ctx, si, sample, [0, 0])
                assert ek.allclose(spec, [0.3 * 0.04] * 3)
                assert ek.allclose(bs.pdf, 1.0)
                assert ek.allclose(bs.eta, 1.0)
                assert ek.allclose(bs.wo, [0, 0, 1])
                assert bs.sampled_component == 0
                assert bs.sampled_type == +BSDFFlags.DeltaReflection

                # Sample refraction
                if sel_type == 0:
                    ctx.type_mask = BSDFFlags.DeltaTransmission
                else:
                    ctx.component = 1
                bs, spec = bsdf.sample(ctx, si, sample, [0, 0])
                if i == 0:
                    assert ek.allclose(spec, [0.6 * (1 - 0.04)] * 3)
                else:
                    assert ek.allclose(spec, [0.6 * (1 - 0.04) / 1.5**2] * 3)
                assert ek.allclose(bs.pdf, 1.0)
                assert ek.allclose(bs.eta, 1.5)
                assert ek.allclose(bs.wo, [0, 0, -1])
                assert bs.sampled_component == 1
                assert bs.sampled_type == +BSDFFlags.DeltaTransmission

    ctx = BSDFContext()
    ctx.component = 3
    bs, spec = bsdf.sample(ctx, si, 0, [0, 0])
    assert ek.all(spec == [0] * 3)


def test05_spot_check(variant_scalar_rgb):
    from mitsuba.render import BSDFContext, SurfaceInteraction3f

    angle = 80 * ek.pi / 180
    ctx = BSDFContext()
    si = SurfaceInteraction3f()
    wi = [ek.sin(angle), 0, ek.cos(angle)]
    si.wi = wi
    bsdf = example_bsdf()

    bs, spec = bsdf.sample(ctx, si, 0, [0, 0])
    assert ek.allclose(bs.pdf, 0.387704354691473)
    assert ek.allclose(bs.wo, [-ek.sin(angle), 0, ek.cos(angle)])

    bs, spec = bsdf.sample(ctx, si, 1, [0, 0])
    angle = 41.03641052520335 * ek.pi / 180
    assert ek.allclose(bs.pdf, 1 - 0.387704354691473)
    assert ek.allclose(bs.wo, [-ek.sin(angle), 0, -ek.cos(angle)])

    si.wi = bs.wo
    bs, spec = bsdf.sample(ctx, si, 1, [0, 0])
    assert ek.allclose(bs.pdf, 1 - 0.387704354691473)
    assert ek.allclose(bs.wo, wi)
