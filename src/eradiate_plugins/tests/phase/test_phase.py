import drjit as dr
import mitsuba as mi
import numpy as np
import pytest


@pytest.fixture
def hg():
    return mi.load_dict({"type": "hg", "g": 0.6})


@pytest.fixture
def tabphase_nodes():
    return [-1.0, -0.3, 0.4, 1.0]


@pytest.fixture
def tabphase_irregular(tabphase_nodes):
    values = [0.5, 0.8, 1.2, 1.5]
    return mi.load_dict(
        {
            "type": "tabphase_irregular",
            "values": ",".join(map(str, values)),
            "nodes": ",".join(map(str, tabphase_nodes)),
        }
    )


def reference_accumulate_envelope(phase, nodes):
    """Replicate the C++ ``PhaseFunction::accumulate_envelope`` evaluation for a single
    (non-composite) phase function.

    For each node ``mu`` (cos_theta in physics convention, +1 = forward
    scattering), the envelope evaluates the phase value at the reconstructed
    outgoing direction and accumulates the elementwise maximum into a
    zero-initialized buffer. We mirror exactly the direction reconstruction
    used in ``src/render/phase.cpp`` so the comparison is meaningful.
    """
    ctx = mi.PhaseFunctionContext(None)
    mei = mi.MediumInteraction3f()
    mei.wi = mi.Vector3f(0, 0, 1)

    nodes_np = np.array(nodes)
    out = np.zeros(nodes_np.shape[0])
    for i, mu in enumerate(nodes_np):
        sin_theta = np.sqrt(max(0.0, 1.0 - mu * mu))
        wo = mi.Vector3f(sin_theta, 0.0, -mu)
        # value == pdf for the isotropic/hg/tabphase plugins exercized here
        out[i] = phase.eval_pdf(ctx, mei, wo)[1]
    return out


def test_get_envelope_nodes_default(variant_scalar_rgb):
    # A leaf phase function without irregular nodes falls back to the base
    # implementation: 256 nodes spread uniformly over [-1, 1].
    phase = mi.load_dict({"type": "isotropic"})
    nodes = np.array(phase.get_envelope_nodes())

    assert nodes.shape[0] == 256
    # The grid is built in single precision; compare with an absolute tolerance.
    assert np.allclose(nodes, np.linspace(-1, 1, 256), atol=1e-6)
    # Strictly increasing, bounded by [-1, 1].
    assert np.all(np.diff(nodes) > 0)


def test_get_envelope_nodes_tabphase_irregular(
    variant_scalar_rgb, tabphase_nodes, tabphase_irregular
):
    # tabphase_irregular overrides get_envelope_nodes() to expose its own grid.
    assert np.allclose(
        np.array(tabphase_irregular.get_envelope_nodes()), tabphase_nodes
    )


def test_get_envelope_nodes_blendphase_merged(
    variant_scalar_rgb, tabphase_nodes, tabphase_irregular
):
    # A composite phase function merges (sorts + de-duplicates) the node grids
    # of its children. We mix an irregular grid with the default uniform grid
    # so the union contains nodes contributed by both children.

    blend = mi.load_dict(
        {
            "type": "blendphase",
            "weight": 0.5,
            "phase_0": {"type": "isotropic"},
            "phase_1": tabphase_irregular,
        }
    )

    nodes = np.array(blend.get_envelope_nodes())
    expected = np.unique(np.concatenate([np.linspace(-1, 1, 256), tabphase_nodes]))

    # Single-precision grid: compare with an absolute tolerance.
    assert np.allclose(nodes, expected, atol=1e-6)
    # The shared endpoints (-1, 1) collapse: 256 uniform + 2 interior nodes.
    assert nodes.shape[0] == 258
    assert np.all(np.diff(nodes) > 0)


def test_accumulate_envelope_single(variant_scalar_rgb, hg):
    # The envelope of a single phase function is just the phase function
    # evaluated at every node.
    nodes = hg.get_envelope_nodes()
    values = dr.zeros(type(nodes), dr.width(nodes))

    hg.accumulate_envelope(nodes, values)
    assert np.allclose(np.array(values), reference_accumulate_envelope(hg, nodes))


def test_accumulate_envelope_blendphase(variant_scalar_rgb, hg):
    # A composite envelope is the pointwise maximum over its (unweighted)
    # children, regardless of the blend weight.
    iso = mi.load_dict({"type": "isotropic"})
    blend = mi.load_dict(
        {"type": "blendphase", "weight": 0.3, "phase_0": iso, "phase_1": hg}
    )

    nodes = blend.get_envelope_nodes()
    values = dr.zeros(type(nodes), dr.width(nodes))
    blend.accumulate_envelope(nodes, values)

    ref = np.maximum(
        reference_accumulate_envelope(iso, nodes),
        reference_accumulate_envelope(hg, nodes),
    )
    assert np.allclose(np.array(values), ref)


def test_accumulate_envelope_multiphase(variant_scalar_rgb, hg, tabphase_irregular):
    # Same envelope semantics for the N-component mixture.
    iso = mi.load_dict({"type": "isotropic"})
    multi = mi.load_dict(
        {
            "type": "multiphase",
            "phase_0": iso,
            "weight_0": 1.0,
            "phase_1": hg,
            "weight_1": 1.0,
            "phase_2": tabphase_irregular,
            "weight_2": 1.0,
        }
    )

    nodes = multi.get_envelope_nodes()
    values = dr.zeros(type(nodes), dr.width(nodes))
    multi.accumulate_envelope(nodes, values)

    ref = np.maximum.reduce(
        [
            reference_accumulate_envelope(iso, nodes),
            reference_accumulate_envelope(hg, nodes),
            reference_accumulate_envelope(tabphase_irregular, nodes),
        ]
    )
    assert np.allclose(np.array(values), ref)


def _make_ddis_medium(medium_type, phase, ddis_threshold=0.5):
    return mi.load_dict(
        {
            "type": medium_type,
            "sigma_t": 0.5,
            "albedo": 0.8,
            "phase": phase,
            "ddis_threshold": ddis_threshold,
        }
    )


def _ddis_phase_tree(combo, shared):
    """Embed ``shared`` as the medium's phase function, either directly or as
    one branch of a composite phase function."""
    if combo == "hg":
        return shared
    if combo == "blendphase":
        return {
            "type": "blendphase",
            "weight": 0.5,
            "phase_0": shared,
            "phase_1": {"type": "isotropic"},
        }
    if combo == "multiphase":
        return {
            "type": "multiphase",
            "phase_0": shared,
            "weight_0": 1.0,
            "phase_1": {"type": "isotropic"},
            "weight_1": 1.0,
        }
    raise ValueError(combo)


@pytest.mark.parametrize("medium_type", ["piecewise", "eoheterogeneous"])
@pytest.mark.parametrize("phase_combo", ["hg", "blendphase", "multiphase"])
def test_ddis_phase_function_update(
    variant_scalar_mono_double, medium_type, phase_combo
):
    # Two media share a single leaf phase function: one uses it as its direct
    # phase function, the other reaches it only as a child of a composite
    # phase function (except for the "hg" combo, where both use it
    # directly). The layout of the scene relies on the more robust traversal
    # function of `eradiate.traverse`.
    shared = mi.load_dict({"type": "hg", "g": 0.2})

    media = {
        "medium_direct": _make_ddis_medium(medium_type, shared),
        "medium_composite": _make_ddis_medium(
            medium_type, _ddis_phase_tree(phase_combo, shared)
        ),
    }
    scene = mi.load_dict({"type": "scene", **media})

    params = mi.eradiate.traverse(scene)
    g_key = next(k for k in params.keys() if k.endswith(".g"))
    params[g_key] = 0.8
    params.update()

    for name, medium in media.items():
        # Recompute the expected DDIS envelope directly from the medium's
        # (now updated) phase function, and compare against the DDIS phase
        # function's tabulated values exposed as scene parameters.
        phase = medium.phase_function()
        nodes = phase.get_envelope_nodes()
        expected = dr.zeros(type(nodes), dr.width(nodes))
        phase.accumulate_envelope(nodes, expected)

        got = params[f"{name}.ddis_phase_function.values"]
        assert dr.allclose(got, expected), (
            f"DDIS phase function of medium '{name}' was not updated"
        )
