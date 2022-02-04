from itertools import product

import drjit as dr
import numpy as np


def test_create(variant_scalar_rgb):
    from mitsuba.core import load_dict

    p = load_dict({"type": "tabphase", "values": "0.5, 1.0, 1.5"})
    assert p is not None


def test_eval(variant_scalar_rgb):
    """
    Compare eval() output with a reference implementation written in Python.
    We make sure that the values we use to initialize the plugin are such that
    the phase function has an asymmetric lobe.
    """
    from mitsuba.core import load_dict
    from mitsuba.render import MediumInteraction3f, PhaseFunctionContext

    # Phase function table definition
    ref_y = np.array([0.5, 1.0, 1.5])
    ref_x = np.linspace(-1, 1, len(ref_y))
    ref_integral = np.trapz(ref_y, ref_x)

    def eval(wi, wo):
        # Python implementation used as a reference
        wi = wi.reshape((-1, 3))
        wo = wo.reshape((-1, 3))

        if wi.shape[0] == 1:
            wi = np.broadcast_to(wi, wo.shape)
        if wo.shape[0] == 1:
            wo = np.broadcast_to(wo, wi.shape)

        cos_theta = np.array([np.dot(a, b) for a, b in zip(wi, wo)])
        return 0.5 / np.pi * np.interp(-cos_theta, ref_x, ref_y) / ref_integral

    wi = np.array([[0, 0, -1]])
    thetas = np.linspace(0, np.pi / 2, 16)
    phis = np.linspace(0, np.pi, 16)
    wos = np.array(
        [
            (
                np.sin(theta) * np.cos(phi),
                np.sin(theta) * np.sin(phi),
                np.cos(theta),
            )
            for theta, phi in product(thetas, phis)
        ]
    )
    ref_eval = eval(wi, wos)

    # Evaluate Mitsuba implementation
    tab = load_dict({"type": "tabphase", "values": ", ".join([str(x) for x in ref_y])})
    ctx = PhaseFunctionContext(None)
    mi = MediumInteraction3f()
    mi.wi = wi
    tab_eval = np.zeros_like(ref_eval)
    for i, wo in enumerate(wos):
        tab_eval[i] = tab.eval(ctx, mi, wo)

    # Compare reference and plugin outputs
    assert np.allclose(ref_eval, tab_eval)


def test_chi2(variants_vec_backends_once_rgb):
    from mitsuba.python.chi2 import ChiSquareTest, PhaseFunctionAdapter, SphericalDomain

    sample_func, pdf_func = PhaseFunctionAdapter(
        "tabphase", "<string name='values' value='0.5, 1.0, 1.5'/>"
    )

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    result = chi2.run(0.1)
    chi2._dump_tables()
    assert result


def test_traverse(variant_scalar_rgb):
    from mitsuba.core import load_dict
    from mitsuba.python.util import traverse
    from mitsuba.render import MediumInteraction3f, PhaseFunctionContext

    # Phase function table definition
    ref_y = np.array([0.5, 1.0, 1.5])
    ref_x = np.linspace(-1, 1, len(ref_y))
    ref_integral = np.trapz(ref_y, ref_x)

    # Initialise as isotropic and update with parameters
    phase = load_dict({"type": "tabphase", "values": "1, 1, 1"})
    params = traverse(phase)
    params["values"] = [0.5, 1.0, 1.5]
    params.update()

    # Distribution parameters are updated
    params = traverse(phase)
    assert dr.allclose(params["values"], [0.5, 1.0, 1.5])

    # The plugin itself evaluates consistently
    ctx = PhaseFunctionContext(None)
    mi = MediumInteraction3f()
    mi.wi = np.array([0, 0, -1])
    wo = [0, 0, 1]
    assert dr.allclose(phase.eval(ctx, mi, wo), dr.InvTwoPi * 1.5 / ref_integral)
