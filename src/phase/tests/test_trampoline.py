import pytest
import drjit as dr
import mitsuba as mi


def test01_create_and_eval(variants_vec_rgb):
    class MyIsotropicPhaseFunction(mi.PhaseFunction):
        def __init__(self, props):
            mi.PhaseFunction.__init__(self, props)
            self.m_flags = mi.PhaseFunctionFlags.Isotropic

        def sample(self, ctx, mei, sample1, sample2, active=True):
            wo = mi.warp.square_to_uniform_sphere(sample2)
            pdf = mi.warp.square_to_uniform_sphere_pdf(wo)
            return (wo, 1.0, pdf)

        def eval_pdf(self, ctx, mei, wo, active=True):
            pdf = mi.warp.square_to_uniform_sphere_pdf(wo)
            return pdf, pdf

        def to_string(self):
            return "MyIsotropicPhaseFunction[]"

    mi.register_phasefunction("myisotropic", lambda props: MyIsotropicPhaseFunction(props))

    p = mi.load_dict({'type': 'myisotropic'})
    assert p is not None

    assert mi.has_flag(p.m_flags, mi.PhaseFunctionFlags.Isotropic)
    assert not mi.has_flag(p.m_flags, mi.PhaseFunctionFlags.Anisotropic)
    assert not mi.has_flag(p.m_flags, mi.PhaseFunctionFlags.Microflake)

    ctx = mi.PhaseFunctionContext()
    mei = mi.MediumInteraction3f()
    theta = dr.linspace(mi.Float, 0, dr.pi / 2, 4)
    ph = dr.linspace(mi.Float, 0, dr.pi, 4)

    wo = [dr.sin(theta), 0, dr.cos(theta)]
    v_eval, v_pdf = p.eval_pdf(ctx, mei, wo)

    assert dr.allclose(v_pdf, 1.0 / (4 * dr.pi))
