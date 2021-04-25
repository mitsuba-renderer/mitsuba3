import enoki as ek


def test01_create(variant_scalar_rgb):
    from mitsuba.render import PhaseFunctionFlags
    from mitsuba.core.xml import load_dict

    phase = load_dict(
        {
            "type": "blendphase",
            "phase1": {"type": "isotropic"},
            "phase2": {"type": "isotropic"},
            "weight": 0.2,
        }
    )
    assert phase is not None
    assert phase.flags() == int(PhaseFunctionFlags.Isotropic)

    phase = load_dict(
        {
            "type": "blendphase",
            "phase1": {"type": "isotropic"},
            "phase2": {"type": "hg"},
            "weight": 0.2,
        }
    )
    assert phase is not None
    assert phase.component_count() == 2
    assert phase.flags(0) == int(PhaseFunctionFlags.Isotropic)
    assert phase.flags(1) == int(PhaseFunctionFlags.Anisotropic)
    assert (
        phase.flags() == PhaseFunctionFlags.Isotropic | PhaseFunctionFlags.Anisotropic
    )


def test02_eval_all(variant_scalar_rgb):
    from mitsuba.core import Frame3f
    from mitsuba.render import PhaseFunctionContext, MediumInteraction3f
    from mitsuba.core.xml import load_dict

    weight = 0.2
    g = 0.2

    phase = load_dict(
        {
            "type": "blendphase",
            "phase1": {"type": "isotropic"},
            "phase2": {"type": "hg", "g": g},
            "weight": weight,
        }
    )

    mi = MediumInteraction3f()
    mi.t = 0.1
    mi.p = [0, 0, 0]
    mi.sh_frame = Frame3f([0, 0, 1])
    mi.wi = [0, 0, 1]

    wo = [0, 0, 1]
    ctx = PhaseFunctionContext(None)

    # Evaluate the blend of both components
    expected = (1.0 - weight) * ek.InvFourPi + weight * ek.InvFourPi * (1.0 - g) / (
        1.0 + g
    ) ** 2
    value = phase.eval(ctx, mi, wo)
    assert ek.allclose(value, expected)


def test03_sample_all(variant_scalar_rgb):
    from mitsuba.core import Frame3f
    from mitsuba.render import PhaseFunctionContext, MediumInteraction3f
    from mitsuba.core.xml import load_dict

    weight = 0.2
    g = 0.2

    phase = load_dict(
        {
            "type": "blendphase",
            "phase1": {"type": "isotropic"},
            "phase2": {"type": "hg", "g": g},
            "weight": weight,
        }
    )

    mi = MediumInteraction3f()
    mi.t = 0.1
    mi.p = [0, 0, 0]
    mi.sh_frame = Frame3f([0, 0, 1])
    mi.wi = [0, 0, 1]

    ctx = PhaseFunctionContext(None)

    # Sample using two different values of 'sample1' and make sure correct
    # components are chosen.

    # -- Sample above weight: first component (isotropic) is selected
    expected_a = ek.InvFourPi
    wo_a, pdf_a = phase.sample(ctx, mi, 0.3, [0.5, 0.5])
    assert ek.allclose(pdf_a, expected_a)

    # -- Sample below weight: second component (HG) is selected
    expected_b = ek.InvFourPi * (1 - g) / (1 + g) ** 2
    wo_b, pdf_b = phase.sample(ctx, mi, 0.1, [0, 0])
    assert ek.allclose(pdf_b, expected_b)


def test04_eval_components(variant_scalar_rgb):
    from mitsuba.core import Frame3f
    from mitsuba.render import PhaseFunctionContext, MediumInteraction3f
    from mitsuba.core.xml import load_dict

    weight = 0.2
    g = 0.2

    phase = load_dict(
        {
            "type": "blendphase",
            "phase1": {"type": "isotropic"},
            "phase2": {"type": "hg", "g": g},
            "weight": weight,
        }
    )

    mi = MediumInteraction3f()
    mi.t = 0.1
    mi.p = [0, 0, 0]
    mi.sh_frame = Frame3f([0, 0, 1])
    mi.wi = [0, 0, 1]

    wo = [0, 0, 1]
    ctx = PhaseFunctionContext(None)

    # Evaluate the two components separately

    ctx.component = 0
    value0 = phase.eval(ctx, mi, wo)
    expected0 = (1 - weight) * ek.InvFourPi
    assert ek.allclose(value0, expected0)

    ctx.component = 1
    value1 = phase.eval(ctx, mi, wo)
    expected1 = weight * ek.InvFourPi * (1.0 - g) / (1.0 + g) ** 2
    assert ek.allclose(value1, expected1)


def test05_sample_components(variant_scalar_rgb):
    from mitsuba.core import Frame3f
    from mitsuba.render import PhaseFunctionContext, MediumInteraction3f
    from mitsuba.core.xml import load_dict

    weight = 0.2
    g = 0.2

    phase = load_dict(
        {
            "type": "blendphase",
            "phase1": {"type": "isotropic"},
            "phase2": {"type": "hg", "g": g},
            "weight": weight,
        }
    )

    mi = MediumInteraction3f()
    mi.t = 0.1
    mi.p = [0, 0, 0]
    mi.sh_frame = Frame3f([0, 0, 1])
    mi.wi = [0, 0, 1]

    ctx = PhaseFunctionContext(None)

    # Sample using two different values of 'sample1' and make sure correct
    # components are chosen.

    # -- Select component 0: first component is always sampled
    ctx.component = 0

    expected_a = (1 - weight) *  ek.InvFourPi
    wo_a, pdf_a = phase.sample(ctx, mi, 0.3, [0.5, 0.5])
    assert ek.allclose(pdf_a, expected_a)

    expected_b = (1 - weight) *  ek.InvFourPi
    wo_b, pdf_b = phase.sample(ctx, mi, 0.1, [0.5, 0.5])
    assert ek.allclose(pdf_b, expected_b)

    # -- Select component 1: second component is always sampled
    ctx.component = 1

    expected_a = weight *  ek.InvFourPi * (1 - g) / (1 + g) ** 2
    wo_a, pdf_a = phase.sample(ctx, mi, 0.3, [0.0, 0.0])
    assert ek.allclose(pdf_a, expected_a)

    expected_b = weight * ek.InvFourPi * (1 - g) / (1 + g) ** 2
    wo_b, pdf_b = phase.sample(ctx, mi, 0.1, [0.0, 0.0])
    assert ek.allclose(pdf_b, expected_b)