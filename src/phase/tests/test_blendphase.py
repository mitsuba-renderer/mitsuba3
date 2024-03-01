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

    phase = mi.load_dict(
        {
            "type": "blendphase",
            "phase1": {"type": "isotropic"},
            "phase2": {"type": "hg", "g": g},
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

    # Evaluate the blend of both components
    expected = (1.0 - weight) * dr.inv_four_pi + weight * dr.inv_four_pi * (1.0 - g) / (
        1.0 + g
    ) ** 2
    value = phase.eval_pdf(ctx, mei, wo)[0]
    assert dr.allclose(value, expected)


def test03_sample_all(variants_all_rgb):
    weight = 0.2
    g = 0.2

    phase = mi.load_dict(
        {
            "type": "blendphase",
            "phase1": {"type": "isotropic"},
            "phase2": {"type": "hg", "g": g},
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
    expected_a = dr.inv_four_pi
    wo_a, w_a, pdf_a = phase.sample(ctx, mei, 0.3, [0.5, 0.5])
    assert dr.allclose(pdf_a, expected_a)

    # -- Sample below weight: second component (HG) is selected
    expected_b = dr.inv_four_pi * (1 - g) / (1 + g) ** 2
    wo_b, w_b, pdf_b = phase.sample(ctx, mei, 0.1, [0, 0])
    assert dr.allclose(pdf_b, expected_b)


def test04_eval_components(variant_scalar_rgb):
    weight = 0.2
    g = 0.2

    phase = mi.load_dict(
        {
            "type": "blendphase",
            "phase1": {"type": "isotropic"},
            "phase2": {"type": "hg", "g": g},
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

    # Evaluate the two components separately

    ctx.component = 0
    value0, pdf0 = phase.eval_pdf(ctx, mei, wo)
    expected0 = (1 - weight) * dr.inv_four_pi
    assert dr.allclose(value0, expected0)
    assert dr.allclose(value0, pdf0)

    ctx.component = 1
    value1, pdf1 = phase.eval_pdf(ctx, mei, wo)
    expected1 = weight * dr.inv_four_pi * (1.0 - g) / (1.0 + g) ** 2
    assert dr.allclose(value1, expected1)
    assert dr.allclose(value1, pdf1)


def test05_sample_components(variant_scalar_rgb):
    weight = 0.2
    g = 0.2

    phase = mi.load_dict(
        {
            "type": "blendphase",
            "phase1": {"type": "isotropic"},
            "phase2": {"type": "hg", "g": g},
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

    # -- Select component 0: first component is always sampled
    ctx.component = 0

    expected_a = (1 - weight) *  dr.inv_four_pi
    wo_a, w_a, pdf_a = phase.sample(ctx, mei, 0.3, [0.5, 0.5])
    assert dr.allclose(pdf_a, expected_a)

    expected_b = (1 - weight) *  dr.inv_four_pi
    wo_b, w_b, pdf_b = phase.sample(ctx, mei, 0.1, [0.5, 0.5])
    assert dr.allclose(pdf_b, expected_b)

    # -- Select component 1: second component is always sampled
    ctx.component = 1

    expected_a = weight *  dr.inv_four_pi * (1 - g) / (1 + g) ** 2
    wo_a, w_a, pdf_a = phase.sample(ctx, mei, 0.3, [0.0, 0.0])
    assert dr.allclose(pdf_a, expected_a)

    expected_b = weight * dr.inv_four_pi * (1 - g) / (1 + g) ** 2
    wo_b, w_b, pdf_b = phase.sample(ctx, mei, 0.1, [0.0, 0.0])
    assert dr.allclose(pdf_b, expected_b)
