import drjit as dr
import mitsuba as mi
import pytest


def test_instantiation(variant_scalar_rgb):
    b = mi.load_dict({"type": "bilambertian"})
    assert b is not None
    assert b.component_count() == 2
    expected_flags_reflection = (
        mi.BSDFFlags.DiffuseReflection | mi.BSDFFlags.FrontSide | mi.BSDFFlags.BackSide
    )
    expected_flags_transmission = (
        mi.BSDFFlags.DiffuseTransmission
        | mi.BSDFFlags.FrontSide
        | mi.BSDFFlags.BackSide
    )
    assert expected_flags_reflection == b.flags(0)
    assert expected_flags_transmission == b.flags(1)


@pytest.mark.parametrize(
    "r, t",
    [
        (0.2, 0.4),
        (0.4, 0.2),
        (0.1, 0.9),
        (0.9, 0.1),
        (0.4, 0.6),
        (0.6, 0.4),
    ],
)
@pytest.mark.parametrize(
    "wi",
    [
        [0, 0, 1],
        [1, 1, 1],
        [0, 0, -1],
        [1, 1, -1],
    ],
    ids=["front", "front_oblique", "back", "back_oblique"],
)
def test_eval_pdf_vector(variant_llvm_ad_rgb, r, t, wi):
    def sph_to_dir(theta, phi):
        """Map spherical to Euclidean coordinates"""
        st, ct = dr.sincos(theta)
        sp, cp = dr.sincos(phi)
        return mi.Vector3f(cp * st, sp * st, ct)

    ctx = mi.BSDFContext()
    bsdf = mi.load_dict({"type": "bilambertian", "reflectance": r, "transmittance": t})

    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.wi = dr.normalize(mi.ScalarVector3f(wi))
    si.sh_frame = mi.Frame3f(si.n)

    # Create grid in spherical coordinates and map it onto the sphere
    res = 300
    theta_o, phi_o = dr.meshgrid(
        dr.linspace(mi.Float, 0, dr.pi, res),
        dr.linspace(mi.Float, 0, 2 * dr.pi, 2 * res),
    )
    wo = sph_to_dir(theta_o, phi_o)

    # Evaluate BSDF
    is_reflect = dr.eq(
        dr.sign(dr.dot(wo, si.n)),
        dr.sign(dr.dot(si.wi, si.n)),
    )
    v_eval = bsdf.eval(ctx, si, wo=wo)
    eval_expected = dr.select(
        is_reflect,
        r * dr.abs(wo[2]) / dr.pi,
        t * dr.abs(wo[2]) / dr.pi,
    )
    assert dr.allclose(v_eval, eval_expected)

    v_pdf = bsdf.pdf(ctx, si, wo=wo)
    assert dr.allclose(v_pdf, eval_expected / (r + t))


@pytest.mark.parametrize(
    "r, t",
    [
        [0.6, 0.2],
        [0.2, 0.6],
        [0.6, 0.4],
        [0.4, 0.6],
        [0.9, 0.1],
        [0.1, 0.9],
        [1.0, 0.0],
        [0.0, 1.0],
        [0.0, 0.0],
    ],
)
def test_chi2(variant_llvm_ad_rgb, r, t):
    from mitsuba.python.chi2 import BSDFAdapter, ChiSquareTest, SphericalDomain

    xml = f"""
        <spectrum name="reflectance" value="{r}"/>
        <spectrum name="transmittance" value="{t}"/>
    """

    sample_func, pdf_func = BSDFAdapter("bilambertian", xml)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    assert chi2.run()
