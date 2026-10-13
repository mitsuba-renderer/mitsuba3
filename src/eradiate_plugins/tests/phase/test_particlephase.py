import time

import pytest
from pytest import fixture
import drjit as dr
import mitsuba as mi
import numpy as np

#################################
# Helper construction functions #
#################################

SCALAR_AND_LLVM_VARIANTS = ["scalar_mono_polarized", "llvm_ad_rgb"]


def storage_types():
    """Returns precision-aware Mitsuba tensor types for the active variant."""
    return mi.TensorXf, mi.TensorXu


def make_phase_grid(
    rng, n_r=10, n_v=10, min_pts=50, max_pts=200, array=None, array_u=None
):
    """Generate a random set of nodes and populate the phase with Henyey-Greenstein phase functions"""

    if array is None or array_u is None:
        default_array, default_array_u = storage_types()
        array = array if array is not None else default_array
        array_u = array_u if array_u is not None else default_array_u

    n_entries = n_r * n_v
    grid_len = rng.integers(min_pts, max_pts, n_entries).astype(np.uint32)
    grid_start = np.concatenate([[0], np.cumsum(grid_len[:-1])]).astype(np.uint32)
    total_pts = int(grid_len.sum())

    nodes = np.empty(total_pts)
    phase_mueller = np.empty((total_pts, 6))

    for i in range(n_entries):
        s, l = int(grid_start[i]), int(grid_len[i])
        cos_vals = np.sort(rng.uniform(-1.0, 1.0, l))
        cos_vals[0], cos_vals[-1] = -1.0, 1.0
        cos_vals = cos_vals.astype(np.float32)
        for k in range(1, l):
            if cos_vals[k] <= cos_vals[k - 1]:
                cos_vals[k] = np.nextafter(cos_vals[k - 1], np.float32(1.0))
        nodes[s : s + l] = cos_vals

        g = rng.uniform(-0.9, 0.9)
        g2 = g * g
        denom = (1.0 + g2 - 2.0 * g * cos_vals) ** 1.5
        m11 = (1.0 - g2) / denom / (4.0 * np.pi)
        phase_mueller[s : s + l, 0] = m11
        phase_mueller[s : s + l, 1] = m11 * 0.1
        phase_mueller[s : s + l, 2] = m11 * 0.9
        phase_mueller[s : s + l, 3] = m11 * 0.8
        phase_mueller[s : s + l, 4] = m11 * 0.05
        phase_mueller[s : s + l, 5] = m11 * 0.8

    grid_start_sentinel = np.append(grid_start, total_pts).astype(np.uint32)

    return (
        array(nodes.reshape(-1, 1)),
        array_u(grid_start_sentinel.reshape(-1, 1)),
        array_u(grid_len.reshape(-1, 1)),
        array(phase_mueller),
    )


def make_rv_volumes(rng, r_eff_bounds, v_eff_bounds, shape=(1, 1, 1)):
    """Build random spatial gridvolumes of r_eff and v_eff within the provided bounds"""

    if r_eff_bounds[0] == r_eff_bounds[1]:
        r_eff_val = np.full(shape, r_eff_bounds[0])
    else:
        r_eff_val = r_eff_bounds[0] + rng.random(shape) * (
            r_eff_bounds[1] - r_eff_bounds[0]
        )
    if v_eff_bounds[0] == v_eff_bounds[1]:
        v_eff_val = np.full(shape, v_eff_bounds[0])
    else:
        v_eff_val = v_eff_bounds[0] + rng.random(shape) * (
            v_eff_bounds[1] - v_eff_bounds[0]
        )

    return (
        {
            "type": "gridvolume",
            "grid": mi.VolumeGrid(r_eff_val),
            "filter_type": "nearest",
        },
        {
            "type": "gridvolume",
            "grid": mi.VolumeGrid(v_eff_val),
            "filter_type": "nearest",
        },
    )


def make_rv_grid(
    n_r=22,
    n_v=3,
    r_bounds=(4.0, 25.0),
    v_bounds=(0.1, 0.2),
    sigma_s_weight=None,
    r_eff_values=None,
    v_eff_values=None,
):
    """Build a grid of r_eff and v_eff points, regularly spaced unless
    explicit (possibly irregularly spaced) values are provided"""

    array, _ = storage_types()

    r_eff_grid = (
        np.asarray(r_eff_values, dtype=float)
        if r_eff_values is not None
        else np.linspace(r_bounds[0], r_bounds[1], n_r)
    )
    v_eff_grid = (
        np.asarray(v_eff_values, dtype=float)
        if v_eff_values is not None
        else np.linspace(v_bounds[0], v_bounds[1], n_v)
    )
    n_r, n_v = len(r_eff_grid), len(v_eff_grid)

    if sigma_s_weight is None:
        sigma_s_weight = np.ones(n_r * n_v)
    sigma_s_weight = np.asarray(sigma_s_weight)

    return (
        array(r_eff_grid.reshape(-1, 1)),
        array(v_eff_grid.reshape(-1, 1)),
        array(sigma_s_weight.reshape(-1, 1)),
    )


def make_interaction(wi=(0, 0, 1), p=(0.5, 0.5, 0.5)):
    """Build a medium interaction with the given incoming direction and position"""
    mei = mi.MediumInteraction3f()
    mei.wi = mi.Vector3f(wi)
    mei.p = mi.Point3f(p)
    mei.sh_frame = mi.Frame3f(mei.wi)
    return mei


def make_query_interactions(rng, n_points, r_bounds, v_bounds, wi=(0, 0, 1)):
    """Build a batch of interactions and their matching (r_eff, v_eff) volumes"""
    rv_volumes = make_rv_volumes(rng, r_bounds, v_bounds, shape=(1, 1, n_points))
    meis = [
        make_interaction(wi=wi, p=(float(rng.random()), 0.5, 0.5))
        for _ in range(n_points)
    ]
    return rv_volumes, meis


def make_phase(
    r_eff_volume,
    v_eff_volume,
    r_eff_grid,
    v_eff_grid,
    nodes,
    phase_mueller,
    grid_start,
    grid_len,
    sigma_s_weight,
):
    """Build a complete phase dictionary"""

    phase = mi.load_dict(
        {
            "type": "particlephase",
            "r_eff_volume": r_eff_volume,
            "v_eff_volume": v_eff_volume,
            "r_eff_grid": r_eff_grid,
            "v_eff_grid": v_eff_grid,
            "nodes": nodes,
            "phase_mueller": phase_mueller,
            "grid_start": grid_start,
            "sigma_s_weight": sigma_s_weight,
        }
    )
    return phase


def build_grid(
    rng,
    n_r,
    n_v,
    r_bounds=(10.0, 11.0),
    v_bounds=(0.1, 0.2),
    sigma_s_weight=None,
    min_pts=10,
    max_pts=11,
    r_eff_values=None,
    v_eff_values=None,
):
    """Build grid parameters (nodes, mueller matrix, and r_eff/v_eff grids)"""

    if r_eff_values is not None:
        n_r = len(r_eff_values)
    if v_eff_values is not None:
        n_v = len(v_eff_values)

    nodes, grid_start, grid_len, phase_mueller = make_phase_grid(
        rng, n_r=n_r, n_v=n_v, min_pts=min_pts, max_pts=max_pts
    )
    r_eff_grid, v_eff_grid, sigma_s_weight = make_rv_grid(
        n_r=n_r,
        n_v=n_v,
        r_bounds=r_bounds,
        v_bounds=v_bounds,
        sigma_s_weight=sigma_s_weight,
        r_eff_values=r_eff_values,
        v_eff_values=v_eff_values,
    )
    return (
        nodes,
        grid_start,
        grid_len,
        phase_mueller,
        r_eff_grid,
        v_eff_grid,
        sigma_s_weight,
    )


def build_phase_function(grid, rv_volumes):
    """Construct a phase function from grid parameters"""

    (
        nodes,
        grid_start,
        grid_len,
        phase_mueller,
        r_eff_grid,
        v_eff_grid,
        sigma_s_weight,
    ) = grid
    r_eff, v_eff = rv_volumes

    phase = make_phase(
        r_eff,
        v_eff,
        r_eff_grid,
        v_eff_grid,
        nodes,
        phase_mueller,
        grid_start,
        grid_len,
        sigma_s_weight,
    )

    return (
        phase,
        r_eff,
        v_eff,
        r_eff_grid,
        v_eff_grid,
        nodes,
        phase_mueller,
        grid_start,
        grid_len,
        sigma_s_weight,
    )


def bilinear_corners(
    r_eff_volume, v_eff_volume, r_eff_grid, v_eff_grid, sigma_s_weight, mei
):
    """Calculate bilinear interpolation weights rescaled by sigma_s from an interaction and r_eff, v_eff volumes"""

    r_eff_grid_np = np.array(r_eff_grid).ravel()
    v_eff_grid_np = np.array(v_eff_grid).ravel()
    sigma_s_weight_np = np.array(sigma_s_weight).ravel()

    n_r = len(r_eff_grid_np)
    n_v = len(v_eff_grid_np)

    r_eff = float(dr.slice(mi.load_dict(r_eff_volume).eval_1(mei), 0))
    v_eff = float(dr.slice(mi.load_dict(v_eff_volume).eval_1(mei), 0))

    if n_r > 1:
        ir = int(
            np.clip(np.searchsorted(r_eff_grid_np, r_eff, side="right") - 1, 0, n_r - 2)
        )
        tr = (r_eff - r_eff_grid_np[ir]) / (r_eff_grid_np[ir + 1] - r_eff_grid_np[ir])
    else:
        ir, tr = 0, 0.0

    if n_v > 1:
        iv = int(
            np.clip(np.searchsorted(v_eff_grid_np, v_eff, side="right") - 1, 0, n_v - 2)
        )
        tv = (v_eff - v_eff_grid_np[iv]) / (v_eff_grid_np[iv + 1] - v_eff_grid_np[iv])
    else:
        iv, tv = 0, 0.0

    ir_hi = ir + 1 if n_r > 1 else ir
    iv_hi = iv + 1 if n_v > 1 else iv

    idx = np.array(
        [
            ir * n_v + iv,
            ir * n_v + iv_hi,
            ir_hi * n_v + iv,
            ir_hi * n_v + iv_hi,
        ]
    )
    w = (
        np.array(
            [
                (1.0 - tr) * (1.0 - tv),
                (1.0 - tr) * tv,
                tr * (1.0 - tv),
                tr * tv,
            ]
        )
        * sigma_s_weight_np[idx]
    )
    w_sum = w.sum()
    if w_sum > 0:
        w = w / w_sum

    return idx, w


def entry_cdf(grid_start_np, grid_len_np, nodes_np, phase_mueller_np, e):
    """Calculate the CDF of a single entry"""

    s, l = int(grid_start_np[e]), int(grid_len_np[e])
    entry_nodes = nodes_np[s : s + l]
    m11 = phase_mueller_np[s : s + l, 0]
    increments = 0.5 * (m11[:-1] + m11[1:]) * np.diff(entry_nodes)
    cdf = np.concatenate(([0.0], np.cumsum(increments)))
    total = cdf[-1]
    if total > 0:
        cdf = cdf / total
    return cdf, total


def union_grid(idx, w, grid_start_np, grid_len_np, nodes_np, lo_bound, hi_bound):
    """Merge two cosine discretizations using a simple union"""

    merged = [lo_bound, hi_bound]
    for e, wgt in zip(idx, w):
        if wgt == 0.0:
            continue
        e = int(e)
        s, l = int(grid_start_np[e]), int(grid_len_np[e])
        entry_nodes = nodes_np[s : s + l]
        mask = (entry_nodes >= lo_bound) & (entry_nodes <= hi_bound)
        merged.extend(entry_nodes[mask].tolist())
    return np.unique(np.array(merged))


def mock_phase(
    r_eff_volume,
    v_eff_volume,
    r_eff_grid,
    v_eff_grid,
    nodes,
    phase_mueller,
    grid_start,
    grid_len,
    sigma_s_weight,
    mei,
):
    """Build a reference phase function via tabphase_polarized, reproducing the bilinearly-blended density at the interaction point"""

    nodes_np = np.array(nodes).ravel()
    grid_start_np = np.array(grid_start).ravel()
    grid_len_np = np.array(grid_len).ravel()
    phase_mueller_np = np.array(phase_mueller).reshape(-1, 6)

    idx, w = bilinear_corners(
        r_eff_volume, v_eff_volume, r_eff_grid, v_eff_grid, sigma_s_weight, mei
    )

    lo_bound = max(nodes_np[int(grid_start_np[int(e)])] for e in idx)
    hi_bound = min(
        nodes_np[int(grid_start_np[int(e)]) + int(grid_len_np[int(e)]) - 1] for e in idx
    )

    grid = union_grid(idx, w, grid_start_np, grid_len_np, nodes_np, lo_bound, hi_bound)

    columns = np.zeros((6, len(grid)))
    for e, wgt in zip(idx, w):
        if wgt == 0.0:
            continue
        e = int(e)
        s, l = int(grid_start_np[e]), int(grid_len_np[e])
        entry_nodes = nodes_np[s : s + l]
        _, total = entry_cdf(grid_start_np, grid_len_np, nodes_np, phase_mueller_np, e)
        norm = 1.0 / total if total > 0 else 0.0
        for c in range(6):
            columns[c] += (
                wgt
                * norm
                * np.interp(grid, entry_nodes, phase_mueller_np[s : s + l, c])
            )

    fmt = lambda v: ",".join(repr(float(x)) for x in v)

    return mi.load_dict(
        {
            "type": "tabphase_polarized",
            "nodes": fmt(grid),
            "m11": fmt(columns[0]),
            "m12": fmt(columns[1]),
            "m22": fmt(columns[2]),
            "m33": fmt(columns[3]),
            "m34": fmt(columns[4]),
            "m44": fmt(columns[5]),
        }
    )


def eval_and_sample(rng, phase, mei):
    """Sample and evaluate the phase function at the same interaction, for pdf-consistency checks"""

    ctx = mi.PhaseFunctionContext(None)

    sample1 = float(rng.random())
    sample2 = mi.Point2f(float(rng.random()), float(rng.random()))

    wo, weight, pdf_sample = phase.sample(ctx, mei, sample1, sample2)
    val, pdf_eval = phase.eval_pdf(ctx, mei, wo)

    return wo, weight, pdf_sample, val, pdf_eval


def check_phase_consistency(
    rng,
    phase,
    r_eff,
    v_eff,
    r_eff_grid,
    v_eff_grid,
    nodes,
    phase_mueller,
    grid_start,
    grid_len,
    sigma_s_weight,
    mei,
):
    """Check sample/eval self-consistency and agreement with the mock_phase reference"""

    wo, weight, pdf_sample, val, pdf_eval = eval_and_sample(rng, phase, mei)

    assert np.isclose(float(dr.slice(pdf_sample, 0)), float(dr.slice(pdf_eval, 0)))

    mock = mock_phase(
        r_eff,
        v_eff,
        r_eff_grid,
        v_eff_grid,
        nodes,
        phase_mueller,
        grid_start,
        grid_len,
        sigma_s_weight,
        mei,
    )
    ctx = mi.PhaseFunctionContext(None)
    val_mock, pdf_mock = mock.eval_pdf(ctx, mei, wo)

    assert np.isclose(
        float(dr.slice(pdf_eval, 0)), float(dr.slice(pdf_mock, 0)), rtol=1e-4
    )
    assert np.allclose(np.array(val), np.array(val_mock), rtol=1e-3, atol=1e-6)


def cell_query_points(r_nodes, v_nodes):
    """Create test evaluation points at the r_eff/v_eff volumes cell centers"""

    points = []
    for i in range(len(r_nodes) - 1):
        r_lo, r_hi = r_nodes[i], r_nodes[i + 1]
        r_mid = 0.5 * (r_lo + r_hi)
        for j in range(len(v_nodes) - 1):
            v_lo, v_hi = v_nodes[j], v_nodes[j + 1]
            v_mid = 0.5 * (v_lo + v_hi)
            points.append((r_mid, v_mid))
            points.append((r_lo, v_mid))
            points.append((r_mid, v_lo))
    return points


def reference_eval_envelope(
    query_nodes, grid_start_np, grid_len_np, phase_mueller_np, nodes_np, n_entries
):
    """Reference implementation of accumulate_envelope: per-node max of the normalized phase value across all entries"""

    values = np.zeros(len(query_nodes))
    for e in range(n_entries):
        s, l = int(grid_start_np[e]), int(grid_len_np[e])
        entry_nodes = nodes_np[s : s + l]
        _, total = entry_cdf(grid_start_np, grid_len_np, nodes_np, phase_mueller_np, e)
        norm = 1.0 / total if total > 0 else 0.0
        m11 = np.interp(query_nodes, entry_nodes, phase_mueller_np[s : s + l, 0])
        values = np.maximum(values, m11 * norm / (2.0 * np.pi))
    return values


############################
# Parametrization fixtures #
############################


@fixture(params=SCALAR_AND_LLVM_VARIANTS)
def variants_scalar_or_llvm(request):
    """Enable one representative scalar variant and one LLVM variant, with conftest's usual per-test cleanup"""
    return request.getfixturevalue(f"variant_{request.param}")


@fixture
def grid_single(variants_scalar_or_llvm, np_rng):
    """Simple single-entry (n_r=n_v=1) grid, 10 node points"""
    return build_grid(np_rng, 1, 1)


@fixture
def rv_volumes_single(np_rng):
    """Simple constant r_eff v_eff volumes"""
    return make_rv_volumes(np_rng, (10.0, 10.0), (0.1, 0.1))


@fixture
def interaction_centrenadir():
    """Basic interaction"""
    return make_interaction(wi=(0, 0, 1), p=(0.5, 0.5, 0.5))


@fixture(
    params=[
        (0, 0, 1),
        (0, 0, -1),
        (1, 0, 0),
        (-1, 0, 0),
        (0, 1, 0),
        (0, -1, 0),
        (1, 1, 1),
        (0.27, -0.64, 0.53),
    ],
    ids=["nadir", "zenith", "+x", "-x", "+y", "-y", "oblique", "generic"],
)
def interaction_directions(request):
    """Create several interactions with different directions"""
    wi = np.array(request.param, dtype=float)
    wi = wi / np.linalg.norm(wi)
    return make_interaction(wi=wi, p=(0.5, 0.5, 0.5))


@fixture
def phase_function_single(grid_single, rv_volumes_single):
    """Basic particlephase function using constant r_eff, v_eff properties"""
    return build_phase_function(grid_single, rv_volumes_single)


@fixture(params=[(2, 1), (1, 2), (2, 2)], ids=["2x1", "1x2", "2x2"])
def grid_dual(request, variants_scalar_or_llvm, np_rng):
    """Build 1 dim and 2 dim interpolation grid parameters"""
    n_r, n_v = request.param
    return build_grid(np_rng, n_r, n_v)


@fixture(params=[(10.0, 0.1), (10.5, 0.15), (11.0, 0.2)], ids=["low", "mid", "high"])
def rv_volumes_dual(request, np_rng):
    """Make several spatial volumes of constant (r_eff, v_eff) values"""
    r_eff_val, v_eff_val = request.param
    return make_rv_volumes(np_rng, (r_eff_val, r_eff_val), (v_eff_val, v_eff_val))


@fixture
def phase_function_dual(grid_dual, rv_volumes_dual):
    """Build phase functions of 1 to 2 dim interpolation grid parameters"""
    return build_phase_function(grid_dual, rv_volumes_dual)


@fixture(
    params=[
        [1.0, 1.0, 1.0, 1.0],
        [1000.0, 1.0, 1.0, 1.0],
        [1.0, 1.0, 1.0, 1000.0],
        [1e-6, 1.0, 1.0, 1.0],
    ],
    ids=["uniform", "corner0_dominant", "corner3_dominant", "corner0_suppressed"],
)
def grid_sigma_s(request, variants_scalar_or_llvm, np_rng):
    """Build sigma_s weights for a 2x2 interpolation grid of (r_eff, v_eff)"""
    return build_grid(np_rng, 2, 2, sigma_s_weight=request.param)


@fixture
def phase_function_sigma_s(grid_sigma_s, np_rng):
    """Build different phase functions with varying sigma_s weights"""
    rv_volumes = make_rv_volumes(np_rng, (10.5, 10.5), (0.15, 0.15))
    return build_phase_function(grid_sigma_s, rv_volumes)


@fixture
def grid_irregular(variants_scalar_or_llvm, np_rng):
    """Build a grid with non-uniformly spaced r_eff and v_eff axes"""
    return build_grid(
        np_rng,
        n_r=None,
        n_v=None,
        r_eff_values=[4.0, 5.0, 7.0, 10.0, 12.5, 25.0],
        v_eff_values=[0.1, 0.11, 0.15, 0.2],
    )


@fixture(
    params=[
        (4.2, 0.105),  # near grid start, off-node
        (8.5, 0.13),  # mid-cell, straddles the irregular spacing change
        (24.0, 0.19),  # near grid end
    ],
    ids=["low", "mid", "high"],
)
def rv_volumes_irregular(request, np_rng):
    """Make several spatial volumes of constant (r_eff, v_eff) values within the irregular grid's range"""
    r_eff_val, v_eff_val = request.param
    return make_rv_volumes(np_rng, (r_eff_val, r_eff_val), (v_eff_val, v_eff_val))


@fixture
def phase_function_irregular(grid_irregular, rv_volumes_irregular):
    """Build a particlephase function over an irregularly-spaced (r_eff, v_eff) grid"""
    return build_phase_function(grid_irregular, rv_volumes_irregular)


#########
# Tests #
#########


def test_instantiate(phase_function_single):
    """Check that a single-entry phase function loads successfully"""
    phase, *_ = phase_function_single
    assert phase is not None


def test_single_directions(phase_function_single, interaction_directions, np_rng):
    """Check sample/eval self-consistency of a single-entry phase function across several incoming directions"""
    check_phase_consistency(np_rng, *phase_function_single, interaction_directions)


def test_dual_directions(phase_function_dual, interaction_directions, np_rng):
    """Check sample/eval self-consistency across 1 and 2 dim (r_eff, v_eff) interpolation grids"""
    check_phase_consistency(np_rng, *phase_function_dual, interaction_directions)


def test_sigma_s_weight(phase_function_sigma_s, interaction_centrenadir, np_rng):
    """Check sample/eval self-consistency under varying per-corner sigma_s weights"""
    check_phase_consistency(np_rng, *phase_function_sigma_s, interaction_centrenadir)


def test_nan_volume_raises(variant_scalar_mono_polarized, np_rng):
    """Check that a NaN r_eff volume raises at construction instead of returning silently wrong values"""
    grid = build_grid(np_rng, 2, 2)
    rv_volumes = make_rv_volumes(np_rng, (float("nan"), float("nan")), (0.15, 0.15))
    with pytest.raises(Exception, match="invalid"):
        build_phase_function(grid, rv_volumes)


@pytest.mark.parametrize(
    "r_eff_val, v_eff_val",
    [(100.0, 0.15), (10.5, 100.0)],
    ids=["r_eff_out_of_range", "v_eff_out_of_range"],
)
def test_out_of_range_volume_raises_at_construction(
    variants_scalar_or_llvm, r_eff_val, v_eff_val, np_rng
):
    """Check that an r_eff/v_eff volume whose range exceeds the dataset's coverage raises at construction, not silently at eval time"""
    grid = build_grid(np_rng, 2, 2)
    rv_volumes = make_rv_volumes(np_rng, (r_eff_val, r_eff_val), (v_eff_val, v_eff_val))
    with pytest.raises(Exception, match="exceeds"):
        build_phase_function(grid, rv_volumes)


def test_irregular_grid_directions(
    phase_function_irregular, interaction_directions, np_rng
):
    """Check sample/eval self-consistency over a non-uniformly spaced (r_eff, v_eff) grid"""
    check_phase_consistency(np_rng, *phase_function_irregular, interaction_directions)


@pytest.mark.parametrize("axis", ["r_eff_values", "v_eff_values"])
def test_grid_axis_not_increasing_raises(variant_scalar_mono_polarized, axis, np_rng):
    """Check that a non-strictly-increasing r_eff_grid or v_eff_grid raises at construction"""
    kwargs = {"n_r": 2, "n_v": 2, axis: [4.0, 6.0, 5.0, 10.0]}
    grid = build_grid(np_rng, **kwargs)
    rv_volumes = make_rv_volumes(np_rng, (5.0, 5.0), (0.15, 0.15))
    with pytest.raises(Exception, match="strictly increasing"):
        build_phase_function(grid, rv_volumes)


def _check_sample_distribution(cos_theta, mean_expected, std_expected, n_query):
    """Check the sampled distribution's per-query mean/std against eval_pdf's own
    marginal density, integrated numerically -- an independent reference, not a
    second sampling method"""

    cur_mean, cur_std = cos_theta.mean(axis=1), cos_theta.std(axis=1)
    good = np.isclose(cur_mean, mean_expected, atol=0.05) & np.isclose(
        cur_std, std_expected, rtol=0.05
    )
    bad = ~good
    print(
        f"stochastic: {int(good.sum())}/{n_query} interactions matched the eval_pdf-integrated "
        f"reference (up to 10 first bad query indices: {np.flatnonzero(bad).tolist()[:10]})"
    )

    assert not bad.any(), (
        f"stochastic: {int(bad.sum())}/{n_query} interactions mismatched the analytic reference"
    )


def _expected_mean_std_scalar(phase, ctx, meis, mu):
    """Numerically integrate eval_pdf's marginal cos_theta density per query
    (scalar variant: one eval_pdf call per grid point)"""
    sin_t = np.sqrt(np.clip(1.0 - mu**2, 0.0, None))
    means = np.empty(len(meis))
    stds = np.empty(len(meis))
    for q, mei in enumerate(meis):
        pdf_vals = np.empty(len(mu))
        for k in range(len(mu)):
            wo = mi.Vector3f(float(sin_t[k]), 0.0, float(-mu[k]))
            _, pdf = phase.eval_pdf(ctx, mei, wo)
            pdf_vals[k] = float(dr.slice(pdf, 0))
        total = np.trapezoid(pdf_vals, mu)
        mean = np.trapezoid(mu * pdf_vals, mu) / total
        std = np.sqrt(np.trapezoid((mu - mean) ** 2 * pdf_vals, mu) / total)
        means[q], stds[q] = mean, std
    return means, stds


def _expected_mean_std_llvm(phase, ctx, mei_batched_grid, n_query, mu):
    """Numerically integrate eval_pdf's marginal cos_theta density per query
    (LLVM variant: one batched eval_pdf call for every query x grid point)"""
    n_grid = len(mu)
    sin_t = np.sqrt(np.clip(1.0 - mu**2, 0.0, None))
    mu_tiled = np.tile(mu, n_query)
    sin_t_tiled = np.tile(sin_t, n_query)
    wo = mi.Vector3f(sin_t_tiled, np.zeros(n_query * n_grid), -mu_tiled)
    _, pdf = phase.eval_pdf(ctx, mei_batched_grid, wo)
    pdf_np = np.array(pdf).reshape(n_query, n_grid)

    total = np.trapezoid(pdf_np, mu, axis=1)
    means = np.trapezoid(mu[None, :] * pdf_np, mu, axis=1) / total
    stds = np.sqrt(
        np.trapezoid((mu[None, :] - means[:, None]) ** 2 * pdf_np, mu, axis=1) / total
    )
    return means, stds


def _sample_distribution_grid(rng):
    """Grid and query points used by test_sample_distribution"""
    r_bounds, v_bounds = (4.0, 25.0), (0.1, 0.2)
    grid = build_grid(
        rng, 22, 2, r_bounds=r_bounds, v_bounds=v_bounds, min_pts=700, max_pts=1000
    )
    n_query = 100
    rv_volumes, meis = make_query_interactions(rng, n_query, r_bounds, v_bounds)
    return grid, rv_volumes, meis, n_query


def _sample_cos_theta_scalar(phase, ctx, meis, n_samples, u_all, phi_all, s1_all):
    """Sample cos_theta one element at a time (scalar variant)"""
    arr = np.empty((len(meis), n_samples))
    for q, mei in enumerate(meis):
        for k in range(n_samples):
            s1 = float(s1_all[q, k])
            s2 = mi.Point2f(float(u_all[q, k]), float(phi_all[q, k]))
            wo, weight, pdf = phase.sample(ctx, mei, s1, s2)
            arr[q, k] = float(dr.slice(-dr.dot(wo, mei.wi), 0))
    return arr


def _sample_cos_theta_llvm(
    phase, ctx, mei_batched, n_query, n_samples, u_all, phi_all, s1_all
):
    """Sample cos_theta in batched calls (LLVM variant)"""
    wo, weight, pdf = phase.sample(
        ctx,
        mei_batched,
        mi.Float(s1_all.reshape(-1)),
        mi.Point2f(u_all.reshape(-1), phi_all.reshape(-1)),
    )
    return np.array(-dr.dot(wo, mei_batched.wi)).reshape(n_query, n_samples)


def _batch_interactions(meis, n_samples_per_query):
    """Tile each query's interaction across its samples (LLVM variant)"""
    n = len(meis) * n_samples_per_query
    px = np.repeat([float(dr.slice(mei.p.x, 0)) for mei in meis], n_samples_per_query)
    py = np.repeat([float(dr.slice(mei.p.y, 0)) for mei in meis], n_samples_per_query)
    pz = np.repeat([float(dr.slice(mei.p.z, 0)) for mei in meis], n_samples_per_query)
    wi0 = meis[0].wi

    mei_batched = mi.MediumInteraction3f()
    mei_batched.wi = mi.Vector3f(
        [float(dr.slice(wi0.x, 0))] * n,
        [float(dr.slice(wi0.y, 0))] * n,
        [float(dr.slice(wi0.z, 0))] * n,
    )
    mei_batched.p = mi.Point3f(px, py, pz)
    mei_batched.sh_frame = mi.Frame3f(mei_batched.wi)
    return mei_batched


def test_sample_distribution(variants_scalar_or_llvm, np_rng):
    """Check the sampled distribution against eval_pdf's own marginal density"""
    rng_local = np.random.default_rng(42)

    grid, rv_volumes, meis, n_query = _sample_distribution_grid(np_rng)
    ctx = mi.PhaseFunctionContext(None)
    phase, *_ = build_phase_function(grid, rv_volumes)

    n_samples_per_query = 4000
    u_all = rng_local.random((n_query, n_samples_per_query))
    phi_all = rng_local.random((n_query, n_samples_per_query))
    s1_all = rng_local.random((n_query, n_samples_per_query))

    is_scalar = variants_scalar_or_llvm.startswith("scalar")
    mei_batched = (
        _batch_interactions(meis, n_samples_per_query) if not is_scalar else None
    )

    t0 = time.perf_counter()
    if is_scalar:
        cos_theta = _sample_cos_theta_scalar(
            phase, ctx, meis, n_samples_per_query, u_all, phi_all, s1_all
        )
    else:
        cos_theta = _sample_cos_theta_llvm(
            phase,
            ctx,
            mei_batched,
            n_query,
            n_samples_per_query,
            u_all,
            phi_all,
            s1_all,
        )
    elapsed = time.perf_counter() - t0
    n_total = n_query * n_samples_per_query
    print(
        f"stochastic: {n_total} samples in {elapsed * 1e3:.2f} ms ({elapsed / n_total * 1e6:.2f} us/sample)"
    )

    n_grid = 200
    mu = np.linspace(-1.0 + 1e-4, 1.0 - 1e-4, n_grid)
    if is_scalar:
        mean_expected, std_expected = _expected_mean_std_scalar(phase, ctx, meis, mu)
    else:
        mei_batched_grid = _batch_interactions(meis, n_grid)
        mean_expected, std_expected = _expected_mean_std_llvm(
            phase, ctx, mei_batched_grid, n_query, mu
        )

    _check_sample_distribution(cos_theta, mean_expected, std_expected, n_query)


@pytest.mark.parametrize(
    "n_r,n_v", [(n_r, n_v) for n_r in range(10, 60, 10) for n_v in range(10, 60, 10)]
)
def test_grid_sizes(variant_scalar_mono_polarized, n_r, n_v, np_rng):
    """Check finite, positive pdf across a set of (r_eff, v_eff) grid shapes"""
    grid = build_grid(np_rng, n_r, n_v)
    *_, r_eff_grid, v_eff_grid, _ = grid

    r_eff_grid_np = np.array(r_eff_grid).ravel()
    v_eff_grid_np = np.array(v_eff_grid).ravel()
    r_eff_val = float(r_eff_grid_np[len(r_eff_grid_np) // 2])
    v_eff_val = float(v_eff_grid_np[len(v_eff_grid_np) // 2])

    rv_volumes = make_rv_volumes(np_rng, (r_eff_val, r_eff_val), (v_eff_val, v_eff_val))
    mei = make_interaction(wi=(0, 0, 1))
    ctx = mi.PhaseFunctionContext(None)

    phase, *_ = build_phase_function(grid, rv_volumes)
    val, pdf = phase.eval_pdf(ctx, mei, mi.Vector3f([0, 0, -1]))
    pdf = float(dr.slice(pdf, 0))
    assert np.isfinite(pdf)
    assert pdf > 0.0


def test_large_grid_sweep(
    variant_scalar_mono_polarized, interaction_directions, np_rng
):
    """Check sample/eval self-consistency at many query points scattered across a 4x4 grid's cells"""
    grid = build_grid(np_rng, 4, 4, r_bounds=(10.0, 14.0), v_bounds=(0.1, 0.2))
    *_, r_eff_grid, v_eff_grid, _ = grid

    r_eff_grid_np = np.array(r_eff_grid).ravel()
    v_eff_grid_np = np.array(v_eff_grid).ravel()
    query_points = cell_query_points(r_eff_grid_np, v_eff_grid_np)

    for r_val, v_val in query_points:
        rv_volumes = make_rv_volumes(np_rng, (r_val, r_val), (v_val, v_val))
        phase_bundle = build_phase_function(grid, rv_volumes)
        check_phase_consistency(np_rng, *phase_bundle, interaction_directions)


def test_get_envl_and_eval_envl(variants_scalar_or_llvm, np_rng):
    """Check get_envelope_nodes/accumulate_envelope against a reference per-node max of the normalized phase value"""
    n_r, n_v = 3, 3
    grid = build_grid(np_rng, n_r, n_v)
    nodes, grid_start, grid_len, phase_mueller, *_ = grid
    rv_volumes = make_rv_volumes(np_rng, (10.5, 10.5), (0.15, 0.15))
    phase, *_ = build_phase_function(grid, rv_volumes)

    nodes_np = np.array(nodes).ravel()
    grid_start_np = np.array(grid_start).ravel()
    grid_len_np = np.array(grid_len).ravel()
    phase_mueller_np = np.array(phase_mueller).reshape(-1, 6)

    nds = phase.get_envelope_nodes()
    nds_np = np.array(nds)
    assert np.allclose(nds_np, np.unique(nodes_np))

    values = dr.zeros(type(nds), len(nds_np))
    phase.accumulate_envelope(nds, values)

    ref_values = reference_eval_envelope(
        nds_np, grid_start_np, grid_len_np, phase_mueller_np, nodes_np, n_r * n_v
    )

    assert np.allclose(np.array(values), ref_values, rtol=1e-4, atol=1e-8)


def test_degenerate_directions(variants_scalar_or_llvm, interaction_directions, np_rng):
    """Check finite, non-negative pdf and agreement with the mock reference when wo is exactly aligned/anti-aligned with wi"""
    mei = interaction_directions
    ctx = mi.PhaseFunctionContext(None)
    grid = build_grid(np_rng, 2, 2)
    rv_volumes = make_rv_volumes(np_rng, (10.5, 10.5), (0.15, 0.15))

    (
        phase,
        r_eff,
        v_eff,
        r_eff_grid,
        v_eff_grid,
        nodes,
        phase_mueller,
        grid_start,
        grid_len,
        sigma_s_weight,
    ) = build_phase_function(grid, rv_volumes)
    mock = mock_phase(
        r_eff,
        v_eff,
        r_eff_grid,
        v_eff_grid,
        nodes,
        phase_mueller,
        grid_start,
        grid_len,
        sigma_s_weight,
        mei,
    )

    for wo in (mi.Vector3f(mei.wi), -mi.Vector3f(mei.wi)):
        val, pdf = phase.eval_pdf(ctx, mei, wo)
        pdf = float(dr.slice(pdf, 0))
        assert np.all(np.isfinite(np.array(val)))
        assert np.isfinite(pdf)
        assert pdf >= 0.0

        val_mock, pdf_mock = mock.eval_pdf(ctx, mei, wo)
        assert np.isclose(pdf, float(dr.slice(pdf_mock, 0)), rtol=1e-4)
        assert np.allclose(np.array(val), np.array(val_mock), rtol=1e-3, atol=1e-6)


def test_chi2(variants_vec_backends_once_rgb, np_rng):
    """Check sample/pdf statistical consistency via a chi-squared test against the spherical domain"""
    n_r, n_v = 2, 2
    r_bounds, v_bounds = (4.0, 25.0), (0.1, 0.2)

    nodes, grid_start, grid_len, phase_mueller = make_phase_grid(
        np_rng,
        n_r=n_r,
        n_v=n_v,
        min_pts=20,
        max_pts=40,
    )
    r_eff_grid, v_eff_grid, sigma_s_weight = make_rv_grid(
        n_r=n_r,
        n_v=n_v,
        r_bounds=r_bounds,
        v_bounds=v_bounds,
    )
    r_eff_volume, v_eff_volume = make_rv_volumes(
        np_rng, r_bounds, v_bounds, shape=(1, 1, 1)
    )

    phase_dict = {
        "type": "particlephase",
        "r_eff_volume": r_eff_volume,
        "v_eff_volume": v_eff_volume,
        "r_eff_grid": r_eff_grid,
        "v_eff_grid": v_eff_grid,
        "nodes": nodes,
        "phase_mueller": phase_mueller,
        "grid_start": grid_start,
        "sigma_s_weight": sigma_s_weight,
    }

    sample_func, pdf_func = mi.chi2.PhaseFunctionAdapter("particlephase", phase_dict)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    result = chi2.run()
    assert result
