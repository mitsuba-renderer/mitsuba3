import drjit as dr
import mitsuba as mi


def test01_create(variant_scalar_rgb):
    phase = mi.load_dict(
        {
            "type": "blendphase",
            "phase1": {"type": "isotropic"},
            "phase2": {"type": "isotropic"},
            "weight": 0.2,
        }
    )
    assert phase is not None
    assert phase.flags() == int(mi.PhaseFunctionFlags.Isotropic)

    phase = mi.load_dict(
        {
            "type": "blendphase",
            "phase1": {"type": "isotropic"},
            "phase2": {"type": "hg"},
            "weight": 0.2,
        }
    )
    assert phase is not None
    assert phase.component_count() == 2
    assert phase.flags(0) == int(mi.PhaseFunctionFlags.Isotropic)
    assert phase.flags(1) == int(mi.PhaseFunctionFlags.Anisotropic)
    assert (
        phase.flags() == mi.PhaseFunctionFlags.Isotropic | mi.PhaseFunctionFlags.Anisotropic
    )


def test02_eval_all(variant_scalar_rgb):
    weight = 0.2
    g = 0.2

    phase_0 = mi.load_dict({"type": "isotropic"})
    phase_1 = mi.load_dict({"type": "hg", "g": g})

    phase = mi.load_dict(
        {
            "type": "blendphase",
            "phase1": phase_0,
            "phase2": phase_1,
            "weight": weight,
        }
    )

    mei = mi.MediumInteraction3f()
    mei.t = 0.1
    mei.p = [0, 0, 0]
    mei.sh_frame = mi.Frame3f([0, 0, 1])
    mei.wi = [0, 0, 1]

    wo = [0, 0, 1]
    ctx = mi.PhaseFunctionContext()

    eval0, pdf0 = phase_0.eval_pdf(ctx, mei, wo)
    eval1, pdf1 = phase_1.eval_pdf(ctx, mei, wo)
    expected_eval = (1 - weight) * eval0 + weight * eval1
    expected_pdf = (1 - weight) * pdf0 + weight * pdf1

    # Evaluate the blend of both components
    eval, pdf = phase.eval_pdf(ctx, mei, wo)
    assert dr.allclose(eval, expected_eval)
    assert dr.allclose(pdf, expected_pdf)


def test03_sample_all(variants_all_rgb):
    weight = 0.2
    g = 0.2

    phase_0 = mi.load_dict({"type": "isotropic"})
    phase_1 = mi.load_dict({"type": "hg", "g": g})

    phase = mi.load_dict(
        {
            "type": "blendphase",
            "phase1": phase_0,
            "phase2": phase_1,
            "weight": weight,
        }
    )

    mei = mi.MediumInteraction3f()
    mei.t = 0.1
    mei.p = [0, 0, 0]
    mei.sh_frame = mi.Frame3f([0, 0, 1])
    mei.wi = [0, 0, 1]

    ctx = mi.PhaseFunctionContext()

    # Sample using two different values of 'sample1' and make sure correct
    # components are chosen.

    # -- Sample above weight: first component (isotropic) is selected
    wo_a, w_a, pdf_a = phase.sample(ctx, mei, 0.3, [0.5, 0.5])
    expected_eval_a, expected_pdf_a = phase.eval_pdf(ctx, mei, wo_a)
    expected_w_a = expected_eval_a / expected_pdf_a
    assert dr.allclose(pdf_a, expected_pdf_a)
    assert dr.allclose(w_a, expected_w_a)

    # -- Sample below weight: second component (HG) is selected
    wo_b, w_b, pdf_b = phase.sample(ctx, mei, 0.1, [0, 0])
    expected_eval_b, expected_pdf_b = phase.eval_pdf(ctx, mei, wo_b)
    expected_w_b = expected_eval_b / expected_pdf_b
    assert dr.allclose(pdf_b, expected_pdf_b)
    assert dr.allclose(w_b, expected_w_b)


def test04_eval_components(variant_scalar_rgb):
    weight = 0.2
    g = 0.2

    phase_0 = mi.load_dict({"type": "isotropic"})
    phase_1 = mi.load_dict({"type": "hg", "g": g})

    phase = mi.load_dict(
        {
            "type": "blendphase",
            "phase1": phase_0,
            "phase2": phase_1,
            "weight": weight,
        }
    )

    mei = mi.MediumInteraction3f()
    mei.t = 0.1
    mei.p = [0, 0, 0]
    mei.sh_frame = mi.Frame3f([0, 0, 1])
    mei.wi = [0, 0, 1]

    wo = [0, 0, 1]
    ctx = mi.PhaseFunctionContext()
    blend_ctx = mi.PhaseFunctionContext()

    # Evaluate the two components separately

    blend_ctx.component = 0
    value0, pdf0 = phase.eval_pdf(blend_ctx, mei, wo)
    expected_value0, expected_pdf0 = phase_0.eval_pdf(ctx, mei, wo)
    assert dr.allclose(value0, (1.0-weight) * expected_value0)
    assert dr.allclose(pdf0, (1.0-weight) * expected_pdf0)

    blend_ctx.component = 1
    value1, pdf1 = phase.eval_pdf(blend_ctx, mei, wo)
    expected_value1, expected_pdf1 = phase_1.eval_pdf(ctx, mei, wo)
    assert dr.allclose(value1, weight * expected_value1)
    assert dr.allclose(pdf1, weight * expected_pdf1)


def test05_sample_components(variant_scalar_rgb):
    weight = 0.2
    g = 0.2

    phase_0 = mi.load_dict({"type": "isotropic"})
    phase_1 = mi.load_dict({"type": "hg", "g": g})

    phase = mi.load_dict(
        {
            "type": "blendphase",
            "phase1": phase_0,
            "phase2": phase_1,
            "weight": weight,
        }
    )

    mei = mi.MediumInteraction3f()
    mei.t = 0.1
    mei.p = [0, 0, 0]
    mei.sh_frame = mi.Frame3f([0, 0, 1])
    mei.wi = [0, 0, 1]

    ctx = mi.PhaseFunctionContext()
    blend_ctx = mi.PhaseFunctionContext()

    # Sample using two different values of 'sample1' and make sure correct
    # components are chosen.

    # -- Select component 0: first component is always sampled
    blend_ctx.component = 0

    wo_a, w_a, pdf_a = phase.sample(blend_ctx, mei, 0.3, [0.5, 0.5])
    expected_eval_a, expected_pdf_a = phase_0.eval_pdf(ctx, mei, wo_a)
    assert dr.allclose(pdf_a, (1.0-weight) * expected_pdf_a)
    assert dr.allclose(w_a, (1.0-weight) * expected_eval_a / expected_pdf_a)

    wo_b, w_b, pdf_b = phase.sample(blend_ctx, mei, 0.1, [0.5, 0.5])
    expected_eval_b, expected_pdf_b = phase_0.eval_pdf(ctx, mei, wo_b)
    assert dr.allclose(pdf_b, (1.0-weight)*expected_pdf_b)
    assert dr.allclose(w_b, (1.0-weight)*expected_eval_b / expected_pdf_b)

    # -- Select component 1: second component is always sampled
    blend_ctx.component = 1

    wo_a, w_a, pdf_a = phase.sample(blend_ctx, mei, 0.3, [0.0, 0.0])
    expected_eval_a, expected_pdf_a = phase_1.eval_pdf(ctx, mei, wo_a)
    assert dr.allclose(pdf_a, weight * expected_pdf_a)
    assert dr.allclose(w_a, weight * expected_eval_a / expected_pdf_a)

    wo_b, w_b, pdf_b = phase.sample(blend_ctx, mei, 0.1, [0.0, 0.0])
    expected_eval_b, expected_pdf_b = phase_1.eval_pdf(ctx, mei, wo_b)
    assert dr.allclose(pdf_b, weight * expected_pdf_b)
    assert dr.allclose(w_b, weight * expected_eval_b / expected_pdf_b)

def test06_chi2_isotropic_hg(variants_vec_backends_once_rgb):
    from mitsuba.chi2 import PhaseFunctionAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = PhaseFunctionAdapter("blendphase", {
        "type": "blendphase",
        "phase1": {"type": "isotropic"},
        "phase2": {"type": "hg", "g": 0.2},
        "weight": 0.2
    })

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()

def test07_chi2_hg_rayleigh(variants_vec_backends_once_rgb):
    from mitsuba.chi2 import PhaseFunctionAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = PhaseFunctionAdapter("blendphase", {
        "type": "blendphase",
        "phase1": {"type": "hg", "g": 0.2},
        "phase2": {"type": "rayleigh"},
        "weight": 0.4
    })

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()