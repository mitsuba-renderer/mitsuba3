import pytest

import drjit as dr
import mitsuba as mi


def test01_create(variant_scalar_rgb):
    phase = mi.load_dict(
        {
            "type": "multiphase",
            "phase_0": {"type": "isotropic"},
            "weight_0": 0.2,
            "phase_1": {"type": "isotropic"},
            "weight_1": 0.8,
        }
    )
    assert phase is not None
    assert phase.flags() == int(mi.PhaseFunctionFlags.Isotropic)

    phase = mi.load_dict(
        {
            "type": "multiphase",
            "phase_0": {"type": "isotropic"},
            "weight_0": 0.2,
            "phase_1": {"type": "hg"},
            "weight_1": 0.8,
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

    phase = mi.load_dict(
        {
            "type": "multiphase",
            "phase_0": {"type": "isotropic"},
            "weight_0": weight,
            "phase_1": {"type": "hg", "g": g},
            "weight_1": 1 - weight,
            "use_mis": False,
        }
    )

    mei = mi.MediumInteraction3f()
    mei.t = 0.1
    mei.p = [0, 0, 0]
    mei.sh_frame = mi.Frame3f([0, 0, 1])
    mei.wi = [0, 0, 1]

    wo = [0, 0, 1]
    ctx = mi.PhaseFunctionContext()

    # Evaluate the blend of both components
    expected = weight * dr.inv_four_pi + (1-weight) * dr.inv_four_pi * (1.0 - g) / (
        1.0 + g
    ) ** 2
    value = phase.eval_pdf(ctx, mei, wo)[0]
    assert dr.allclose(value, expected)


def test03_sample_all(variants_all_rgb):
    weight = 0.8
    g = 0.2

    phase = mi.load_dict(
        {
            "type": "multiphase",
            "phase_0": {"type": "isotropic"},
            "weight_0": weight,
            "phase_1": {"type": "hg", "g": g},
            "weight_1": 1 - weight,
            "use_mis": False,
        }
    )

    print(phase)

    mei = mi.MediumInteraction3f()
    mei.t = 0.1
    mei.p = [0, 0, 0]
    mei.sh_frame = mi.Frame3f([0, 0, 1])
    mei.wi = [0, 0, 1]

    ctx = mi.PhaseFunctionContext()

    # Sample using two different values of 'sample1' and make sure correct
    # components are chosen.

    # -- Sample below weight: first component (isotropic) is selected
    expected_a = dr.inv_four_pi
    wo_a, w_a, pdf_a = phase.sample(ctx, mei, 0.3, [0.5, 0.5])
    assert dr.allclose(pdf_a, expected_a)

    # -- Sample above weight: second component (HG) is selected
    expected_b = dr.inv_four_pi * (1 - g) / (1 + g) ** 2
    wo_b, w_b, pdf_b = phase.sample(ctx, mei, 0.9, [0, 0])
    assert dr.allclose(pdf_b, expected_b)


def test04_eval_components(variant_scalar_rgb):
    weight = 0.2
    g = 0.2

    phase = mi.load_dict(
        {
            "type": "multiphase",
            "phase_0": {"type": "isotropic"},
            "weight_0": weight,
            "phase_1": {"type": "hg", "g": g},
            "weight_1": 1 - weight,
        }
    )

    mei = mi.MediumInteraction3f()
    mei.t = 0.1
    mei.p = [0, 0, 0]
    mei.sh_frame = mi.Frame3f([0, 0, 1])
    mei.wi = [0, 0, 1]

    wo = [0, 0, 1]
    ctx = mi.PhaseFunctionContext()

    # Evaluate the two components separately

    ctx.component = 0
    value0, pdf0 = phase.eval_pdf(ctx, mei, wo)
    expected0 = weight * dr.inv_four_pi
    assert dr.allclose(value0, expected0)
    assert dr.allclose(value0, pdf0)

    ctx.component = 1
    value1, pdf1 = phase.eval_pdf(ctx, mei, wo)
    expected1 = (1-weight) * dr.inv_four_pi * (1.0 - g) / (1.0 + g) ** 2
    assert dr.allclose(value1, expected1)
    assert dr.allclose(value1, pdf1)


def test05_sample_components(variant_scalar_rgb):
    weight = 0.2
    g = 0.2

    phase = mi.load_dict(
        {
            "type": "multiphase",
            "phase_0": {"type": "isotropic"},
            "weight_0": weight,
            "phase_1": {"type": "hg", "g": g},
            "weight_1": 1 - weight,
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

    # -- Select component 0: first component is always sampled
    ctx.component = 0

    expected_a = weight * dr.inv_four_pi
    wo_a, w_a, pdf_a = phase.sample(ctx, mei, 0.3, [0.5, 0.5])
    assert dr.allclose(pdf_a, expected_a)

    expected_b = weight * dr.inv_four_pi
    wo_b, w_b, pdf_b = phase.sample(ctx, mei, 0.1, [0.5, 0.5])
    assert dr.allclose(pdf_b, expected_b)

    # -- Select component 1: second component is always sampled
    ctx.component = 1

    expected_a = (1 - weight) * dr.inv_four_pi * (1 - g) / (1 + g) ** 2
    wo_a, w_a, pdf_a = phase.sample(ctx, mei, 0.3, [0.0, 0.0])
    assert dr.allclose(pdf_a, expected_a)

    expected_b = (1 - weight) * dr.inv_four_pi * (1 - g) / (1 + g) ** 2
    wo_b, w_b, pdf_b = phase.sample(ctx, mei, 0.1, [0.0, 0.0])
    assert dr.allclose(pdf_b, expected_b)


@pytest.mark.parametrize("phases,weights", [
    (
        [
            {"type": "isotropic"},
            {"type": "hg", "g": 0.2},
            {"type": "hg", "g": 0.3}
        ],
        [10.0, 20.0, 30.0]
    ),
], ids=["iso+hg_mixed"])
@pytest.mark.parametrize("sample1,phase_idx", [
    (0.1, 0),
    (0.4, 1),
    (0.8, 2),
], ids=["phase_0", "phase_1", "phase_2"])
def test06_sample_all_mis(variant_scalar_rgb, phases, weights, sample1, phase_idx):
    
    mei = mi.MediumInteraction3f()
    mei.t = 0.1
    mei.p = [0, 0, 0]
    mei.sh_frame = mi.Frame3f([0, 0, 1])
    mei.wi = [0, 0, 1]
    
    ctx = mi.PhaseFunctionContext()
    
    phase_objects = [mi.load_dict(p) for p in phases]
    W = sum(weights)
    
    phase = mi.load_dict({
        "type": "multiphase",
        "phase_0": phase_objects[0], "weight_0": weights[0],
        "phase_1": phase_objects[1], "weight_1": weights[1],
        "phase_2": phase_objects[2], "weight_2": weights[2],
    })
    
    wo, w, pdf = phase.sample(ctx, mei, sample1, [0.5, 0.5])
    
    cdf_start = sum(weights[:phase_idx]) / W
    cdf_width = weights[phase_idx] / W
    sample1_adjusted = (sample1 - cdf_start) / cdf_width
    
    wo_expected, val_sampled, pdf_sampled = phase_objects[phase_idx].sample(
        ctx, mei, sample1_adjusted, [0.5, 0.5]
    )
    
    vals, pdfs = [], []
    for i, p in enumerate(phase_objects):
        if i == phase_idx:
            vals.append(val_sampled)
            pdfs.append(pdf_sampled)
        else:
            v, p_pdf = p.eval_pdf(ctx, mei, wo_expected)
            vals.append(v)
            pdfs.append(p_pdf)
    
    pdf_mixture = sum(pdfs[i] * weights[i] for i in range(3))
    val_mixture = sum(
        (vals[i] * pdfs[i] if i == phase_idx else vals[i]) * weights[i]
        for i in range(3)
    )
    
    pdf_expected = pdf_mixture / W
    w_expected = val_mixture / pdf_mixture
    
    assert dr.allclose(wo, wo_expected), f"Direction mismatch"
    assert dr.allclose(pdf, pdf_expected, atol=1e-5), f"PDF = {pdf}, expected {pdf_expected}"
    assert dr.allclose(w, w_expected, atol=1e-5), f"Weight = {w}, expected {w_expected}"


def test_compare_blendphase(variant_scalar_rgb):

    weight = 0.2
    g = 0.2

    phase0 = mi.load_dict({"type": "isotropic"})
    phase1 = mi.load_dict({"type": "hg", "g": g})

    blendphase = mi.load_dict(
        {
            "type": "blendphase",
            "phase_0": phase0,
            "phase_1": phase1,
            "weight": weight,
        }
    )

    multiphase = mi.load_dict(
        {
            "type": "multiphase",
            "phase_0": phase0,
            "weight_0": 1 - weight,
            "phase_1": phase1,
            "weight_1": weight,
            "use_mis": True,  # blendphase returns the mixture pdf
        }
    )

    mei = mi.MediumInteraction3f()
    mei.t = 0.1
    mei.p = [0, 0, 0]
    mei.sh_frame = mi.Frame3f([0, 0, 1])
    mei.wi = [0, 0, 1]

    ctx = mi.PhaseFunctionContext()

    wo_a1, w_a1, pdf_a1 = blendphase.sample(ctx, mei, 0.3, [0.5, 0.5])
    wo_a2, w_a2, pdf_a2 = multiphase.sample(ctx, mei, 0.7, [0.5, 0.5])

    wo_b1, w_b1, pdf_b1 = blendphase.sample(ctx, mei, 0.1, [0, 0])
    wo_b2, w_b2, pdf_b2 = multiphase.sample(ctx, mei, 0.9, [0, 0])

    assert dr.allclose(wo_a1, wo_a2)
    assert dr.allclose(w_a1, w_a2)
    assert dr.allclose(pdf_a1, pdf_a2)

    assert dr.allclose(wo_b1, wo_b2)
    assert dr.allclose(w_b1, w_b2)
    assert dr.allclose(pdf_b1, pdf_b2)


def test08_chi2_isotropic_hg(variants_vec_backends_once_rgb):
    from mitsuba.chi2 import PhaseFunctionAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = PhaseFunctionAdapter("multiphase", {
        "type": "multiphase",
        "phase_0": {"type": "isotropic"},
        "weight_0": 0.8,
        "phase_1": {"type": "hg", "g": 0.2},
        "weight_1": 0.2,
    })

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


@pytest.mark.parametrize("use_mis", [True, False])
def test09_chi2_hg_rayleigh(variants_vec_backends_once_rgb, use_mis):
    from mitsuba.chi2 import PhaseFunctionAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = PhaseFunctionAdapter("multiphase", {
        "type": "multiphase",
        "phase_0": {"type": "hg", "g": 0.2},
        "weight_0": 0.6,
        "phase_1": {"type": "rayleigh"},
        "weight_1": 0.4,
        "use_mis": use_mis,
    })

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()
