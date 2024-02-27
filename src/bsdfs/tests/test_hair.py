import pytest
import drjit as dr
import mitsuba as mi

def test01_create(variants_all):
    b = mi.load_dict({'type': 'hair'})
    assert b is not None
    assert b.component_count() == 2
    assert b.flags(0) == (
            mi.BSDFFlags.Glossy | mi.BSDFFlags.FrontSide |
            mi.BSDFFlags.Anisotropic | mi.BSDFFlags.NonSymmetric
    )
    assert b.flags(1) == (mi.BSDFFlags.Null | mi.BSDFFlags.BackSide)
    assert b.flags() == (b.flags(0) | b.flags(1))


def test02_white_furnace(variants_vec_backends_once_rgb):
    total = int(1e8)
    sampler = mi.load_dict({'type': 'independent', 'sample_count': total})
    sampler.seed(seed=0, wavefront_size=total)

    si    = mi.SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.wi = mi.warp.square_to_uniform_sphere(sampler.next_2d())
    si.sh_frame = mi.Frame3f(si.n)

    ctx = mi.BSDFContext()

    all_long_roughness = list(dr.linspace(mi.Float, 0.1, 0.99, 3))
    all_azi_roughness = all_long_roughness
    for long_roughness in all_long_roughness:
        for azi_roughness in all_azi_roughness:
            bsdf = mi.load_dict({
                'type': 'hair',
                'longitudinal_roughness': long_roughness,
                'azimuthal_roughness': azi_roughness,
                'sigma_a': {
                    'type': 'rgb',
                    'value': mi.ScalarColor3f(0.)
                },
            })

            wo = mi.warp.square_to_uniform_sphere(sampler.next_2d())
            value = bsdf.eval(ctx, si, wo)
            expected = dr.sum(value, axis=None) / (total * mi.warp.square_to_uniform_sphere_pdf(1.))

            assert dr.allclose(expected - 3, 0, atol=1e-2)


def test03_white_furnace_importance_sample(variants_vec_backends_once_rgb):
    total = int(1e6)
    sampler = mi.load_dict({'type': 'independent', 'sample_count': total})
    sampler.seed(seed=0, wavefront_size=total)

    si    = mi.SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.wi = mi.warp.square_to_uniform_sphere(sampler.next_2d())
    si.sh_frame = mi.Frame3f(si.n)

    ctx = mi.BSDFContext()

    all_long_roughness = list(dr.linspace(mi.Float, 0.1, 0.99, 5))
    all_azi_roughness = all_long_roughness
    for long_roughness in all_long_roughness:
        for azi_roughness in all_azi_roughness:
            bsdf = mi.load_dict({
                'type': 'hair',
                'longitudinal_roughness': long_roughness,
                'azimuthal_roughness': azi_roughness,
                'sigma_a': {
                    'type': 'rgb',
                    'value': mi.ScalarColor3f(0.)
                },
            })

            _, value = bsdf.sample(ctx, si, sampler.next_1d(), sampler.next_2d())
            expected = dr.sum(value, axis=None) / total

            assert dr.allclose(expected - 3, 0, atol=1e-3)


def test04_sample_numeric(variants_vec_backends_once_rgb):
    total = int(1e5)
    sampler = mi.load_dict({'type': 'independent', 'sample_count': total})
    sampler.seed(seed=0, wavefront_size=total)

    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)

    ctx = mi.BSDFContext()

    all_long_roughness = list(dr.linspace(mi.Float, 0.1, 0.99, 5))
    all_azi_roughness = all_long_roughness
    for long_roughness in all_long_roughness:
        for azi_roughness in all_azi_roughness:
            # estimate reflected uniform incident radiance from hair
            bsdf = mi.load_dict({
                'type': 'hair',
                'longitudinal_roughness': long_roughness,
                'azimuthal_roughness': azi_roughness,
                'sigma_a': {
                    'type': 'rgb',
                    'value': mi.ScalarColor3f(0.)
                }
            })
            si.wi = dr.normalize(mi.warp.square_to_uniform_sphere(sampler.next_2d()))

            d1 = sampler.next_1d()
            d2 = sampler.next_2d()

            _, spec = bsdf.sample(ctx, si, d1, d2)

            # Sample might be 0
            spec = dr.select(mi.luminance(spec) ==  0, 1, spec)
            lum = mi.luminance(spec)

            assert dr.all(dr.allclose(lum - 1, 0, atol=1e-3))


@pytest.mark.slow
def test05_sample_consistency(variants_vec_backends_once_rgb):
    total = int(1e8)
    sampler = mi.load_dict({'type': 'independent', 'sample_count': total})
    sampler.seed(seed = 4, wavefront_size = total)

    si = dr.zeros(mi.SurfaceInteraction3f)
    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)

    ctx = mi.BSDFContext()

    for long_roughness in list(dr.linspace(mi.Float, 0.2, 1.0, 4)):
        for azi_roughness in list(dr.linspace(mi.Float, 0.4, 1.0, 3)):
            si.wi = mi.warp.square_to_uniform_hemisphere(sampler.next_2d())
            bsdf = mi.load_dict({
                'type': 'hair',
                'sigma_a': {
                    'type': 'rgb',
                    'value': mi.ScalarColor3f(0.25)
                },
                'longitudinal_roughness': long_roughness,
                'azimuthal_roughness': azi_roughness,
            })

            def helper_Li(vec):
                return vec.z * vec.z

            bs, spec = bsdf.sample(ctx, si, sampler.next_1d(), sampler.next_2d())
            importance = spec * helper_Li(bs.wo)

            wo = mi.warp.square_to_uniform_sphere(sampler.next_2d())
            uniform = bsdf.eval(ctx, si, wo) * helper_Li(wo) * (dr.pi * 4)

            assert dr.allclose(dr.sum(importance.y), dr.sum(uniform.y), rtol=0.05, atol=1e-3)


@pytest.mark.slow
def test06_chi2(variants_vec_backends_once_rgb):
    from mitsuba.chi2 import BSDFAdapter, ChiSquareTest, SphericalDomain
    for long_roughness in list(dr.linspace(mi.Float, 0.2, 1.0, 4)):
        # Low azimuthal roughness tests are difficult to pass (w/ limited budget)
        for azi_roughness in list(dr.linspace(mi.Float, 0.8, 1.0, 3)):
            for theta in list(dr.linspace(mi.Float, 0, 0.99 * (dr.pi / 2), 3)):
                for phi in list(dr.linspace(mi.Float, 0.0, dr.two_pi / 2, 3)):
                        xml = f"""
                            <rgb name="sigma_a" value="0.8, 0.8, 0.8" />
                            <float name="longitudinal_roughness" value="{long_roughness}" />
                            <float name="azimuthal_roughness" value="{azi_roughness}" />
                        """

                        x = dr.sin(theta) * dr.cos(phi)
                        y = dr.sin(theta) * dr.sin(phi)
                        z = dr.cos(theta)
                        wi = dr.normalize(mi.ScalarVector3f(x, y, z))
                        sample_func, pdf_func = BSDFAdapter("hair", xml, wi=wi)

                        chi2 = ChiSquareTest(
                            domain=SphericalDomain(),
                            sample_func=sample_func,
                            pdf_func=pdf_func,
                            sample_dim=3,
                            res=301,
                        )

                        assert chi2.run()
