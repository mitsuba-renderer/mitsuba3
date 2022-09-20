import pytest
import drjit as dr
import mitsuba as mi

def example_bsdf(reflectance=0.3, transmittance=0.6):
    return mi.load_dict({
        'type': 'dielectric',
        'specular_reflectance': reflectance,
        'specular_transmittance': transmittance,
        'int_ior': 1.5,
        'ext_ior': 1.0,
    })


def test01_create(variant_scalar_rgb):
    b = mi.load_dict({'type': 'dielectric'})
    assert b is not None
    assert b.component_count() == 2
    assert b.flags(0) == (mi.BSDFFlags.DeltaReflection | mi.BSDFFlags.FrontSide |
                          mi.BSDFFlags.BackSide)
    assert b.flags(1) == (mi.BSDFFlags.DeltaTransmission | mi.BSDFFlags.FrontSide |
                          mi.BSDFFlags.BackSide | mi.BSDFFlags.NonSymmetric)
    assert b.flags() == b.flags(0) | b.flags(1)

    # Should not accept negative IORs
    with pytest.raises(RuntimeError):
        mi.load_dict({'type': 'dielectric', 'int_ior': -0.5})


def test02_sample(variant_scalar_rgb):
    si = mi.SurfaceInteraction3f()
    si.wi = [0, 0, 1]
    bsdf = example_bsdf()

    for i in range(2):
        ctx = mi.BSDFContext(mi.TransportMode.Importance if i == 0
                          else mi.TransportMode.Radiance)

        # Sample reflection
        bs, spec = bsdf.sample(ctx, si, 0, [0, 0])
        assert dr.allclose(spec, [0.3] * 3)
        assert dr.allclose(bs.pdf, 0.04)
        assert dr.allclose(bs.eta, 1.0)
        assert dr.allclose(bs.wo, [0, 0, 1])
        assert bs.sampled_component == 0
        assert bs.sampled_type == +mi.BSDFFlags.DeltaReflection

        # Sample refraction
        bs, spec = bsdf.sample(ctx, si, 0.05, [0, 0])
        if i == 0:
            assert dr.allclose(spec, [0.6] * 3)
        else:
            assert dr.allclose(spec, [0.6 / 1.5**2] * 3)
        assert dr.allclose(bs.pdf, 1 - 0.04)
        assert dr.allclose(bs.eta, 1.5)
        assert dr.allclose(bs.wo, [0, 0, -1])
        assert bs.sampled_component == 1
        assert bs.sampled_type == +mi.BSDFFlags.DeltaTransmission


def test03_sample_reverse(variant_scalar_rgb):
    si = mi.SurfaceInteraction3f()
    si.wi = [0, 0, -1]
    bsdf = example_bsdf()

    for i in range(2):
        ctx = mi.BSDFContext(mi.TransportMode.Importance if i == 0
                          else mi.TransportMode.Radiance)

        # Sample reflection
        bs, spec = bsdf.sample(ctx, si, 0, [0, 0])
        assert dr.allclose(spec, [0.3] * 3)
        assert dr.allclose(bs.pdf, 0.04)
        assert dr.allclose(bs.eta, 1.0)
        assert dr.allclose(bs.wo, [0, 0, -1])
        assert bs.sampled_component == 0
        assert bs.sampled_type == +mi.BSDFFlags.DeltaReflection

        # Sample refraction
        bs, spec = bsdf.sample(ctx, si, 0.05, [0, 0])
        if i == 0:
            assert dr.allclose(spec, [0.6] * 3)
        else:
            assert dr.allclose(spec, [0.6 * 1.5**2] * 3)
        assert dr.allclose(bs.pdf, 1 - 0.04)
        assert dr.allclose(bs.eta, 1 / 1.5)
        assert dr.allclose(bs.wo, [0, 0, 1])
        assert bs.sampled_component == 1
        assert bs.sampled_type == +mi.BSDFFlags.DeltaTransmission


def test04_sample_specific_component(variant_scalar_rgb):
    si = mi.SurfaceInteraction3f()
    si.wi = [0, 0, 1]
    bsdf = example_bsdf()

    for i in range(2):
        for sample in [0.0, 0.5, 1.0]:
            for sel_type in range(2):
                ctx = mi.BSDFContext(mi.TransportMode.Importance if i == 0
                                     else mi.TransportMode.Radiance)

                # Sample reflection
                if sel_type == 0:
                    ctx.type_mask = mi.BSDFFlags.DeltaReflection
                else:
                    ctx.component = 0
                bs, spec = bsdf.sample(ctx, si, sample, [0, 0])
                assert dr.allclose(spec, [0.3 * 0.04] * 3)
                assert dr.allclose(bs.pdf, 1.0)
                assert dr.allclose(bs.eta, 1.0)
                assert dr.allclose(bs.wo, [0, 0, 1])
                assert bs.sampled_component == 0
                assert bs.sampled_type == +mi.BSDFFlags.DeltaReflection

                # Sample refraction
                if sel_type == 0:
                    ctx.type_mask = mi.BSDFFlags.DeltaTransmission
                else:
                    ctx.component = 1
                bs, spec = bsdf.sample(ctx, si, sample, [0, 0])
                if i == 0:
                    assert dr.allclose(spec, [0.6 * (1 - 0.04)] * 3)
                else:
                    assert dr.allclose(spec, [0.6 * (1 - 0.04) / 1.5**2] * 3)
                assert dr.allclose(bs.pdf, 1.0)
                assert dr.allclose(bs.eta, 1.5)
                assert dr.allclose(bs.wo, [0, 0, -1])
                assert bs.sampled_component == 1
                assert bs.sampled_type == +mi.BSDFFlags.DeltaTransmission

    ctx = mi.BSDFContext()
    ctx.component = 3
    bs, spec = bsdf.sample(ctx, si, 0, [0, 0])
    assert dr.all(spec == [0] * 3)


def test05_spot_check(variant_scalar_rgb):
    angle = 80 * dr.pi / 180
    ctx = mi.BSDFContext()
    si = mi.SurfaceInteraction3f()
    wi = [dr.sin(angle), 0, dr.cos(angle)]
    si.wi = wi
    bsdf = example_bsdf()

    bs, spec = bsdf.sample(ctx, si, 0, [0, 0])
    assert dr.allclose(bs.pdf, 0.387704354691473)
    assert dr.allclose(bs.wo, [-dr.sin(angle), 0, dr.cos(angle)])

    bs, spec = bsdf.sample(ctx, si, 1, [0, 0])
    angle = 41.03641052520335 * dr.pi / 180
    assert dr.allclose(bs.pdf, 1 - 0.387704354691473)
    assert dr.allclose(bs.wo, [-dr.sin(angle), 0, -dr.cos(angle)])

    si.wi = bs.wo
    bs, spec = bsdf.sample(ctx, si, 1, [0, 0])
    assert dr.allclose(bs.pdf, 1 - 0.387704354691473)
    assert dr.allclose(bs.wo, wi)


def test06_attached_sampling(variants_all_ad_rgb):
    bsdf = mi.load_dict({'type': 'dielectric'})

    angle = mi.Float(10 * dr.pi / 180)
    dr.enable_grad(angle)

    si    = mi.SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [dr.sin(angle), 0, dr.cos(angle)]
    si.sh_frame = mi.Frame3f(si.n)
    si.wi = si.sh_frame.to_local([0, 0, 1])

    bs, weight = bsdf.sample(mi.BSDFContext(), si, 0, [0, 0])
    assert dr.grad_enabled(weight)
    
    dr.forward(angle)
    assert dr.allclose(dr.grad(weight), 0.008912204764783382)
    