"""Extremum structure traversal (``begin``/``next``) and medium tracking."""

import drjit as dr
import mitsuba as mi
import numpy as np
import pytest

from .utils import walk_iter


def _grid_volume(resolution, seed=0):
    rng = np.random.default_rng(seed)
    data = (rng.random(resolution) + 0.1).astype(np.float32)
    return mi.load_dict(
        {
            "type": "gridvolume",
            "grid": mi.VolumeGrid(data.transpose(2, 1, 0)),
            "filter_type": "nearest",
            "accel": False,
        }
    )


def _extremum(volume, kind, extremum_res):
    d = {"type": kind}
    if extremum_res is not None:
        d["resolution"] = mi.ScalarVector3i(*extremum_res)
    extremum = mi.load_dict(d)
    extremum.update_extremum(volume.bbox(), volume)
    return extremum


def _rays():
    """Oblique rays plus axis-aligned, inside-start and missing ones."""
    rng = np.random.default_rng(1)
    out = []
    for _ in range(8):
        o = rng.random(3) * 2.0 - 0.5
        d = rng.random(3) * 2.0 - 1.0
        out.append((o, d / np.linalg.norm(d)))
    out += [
        ([-0.5, 0.3, 0.7], [1.0, 0.0, 0.0]),
        ([1.5, 0.3, 0.7], [-1.0, 0.0, 0.0]),
        ([0.25, 0.25, 0.25], [0.0, 0.0, 1.0]),
        ([0.5, -1.0, 0.5], [0.0, 1.0, 0.0]),
        ([2.0, 2.0, 2.0], [1.0, 1.0, 1.0]),
    ]
    return [
        mi.Ray3f(o=mi.Point3f(*map(float, o)), d=mi.Vector3f(*map(float, d)))
        for o, d in out
    ]


def _structures():
    """``(name, structure, rays, bbox)`` for the structure plugins."""
    return [
        (
            "global",
            _extremum(_grid_volume((4, 4, 4)), "globalextremum", None),
            _rays(),
            (0.0, 1.0),
        ),
        (
            "grid",
            _extremum(_grid_volume((4, 5, 3)), "gridextremum", (2, 3, 3)),
            _rays(),
            (0.0, 1.0),
        ),
    ]


def test_structure_segments_tile(variant_scalar_rgb):
    """
    Tests that all structures segments that tile perfectly.
    """
    for name, extremum, rays, _ in _structures():
        for ray in rays:
            segments = walk_iter(extremum, ray, 0.0, 4.0)
            if not segments:
                continue

            assert segments[0][0] >= 0.0, name
            assert segments[-1][1] <= 4.0, name
            for before, after in zip(segments[:-1], segments[1:]):
                assert before[1] == after[0], (name, ray)


@pytest.mark.parametrize(
    "o,d,maxt,length",
    [
        ([0.5, 0.5, 0.5], [1, 0, 0], dr.inf, 0.5),
        ([-1.0, 0.5, 0.5], [1, 0, 0], 1.25, 0.25),
        ([2.0, 0.5, 0.5], [1, 0, 0], dr.inf, 0.0),
        ([-2.0, 0.5, 0.5], [1, 0, 0], 1.0, 0.0),
        ([-1.0, 5.0, 0.5], [1, 0, 0], dr.inf, 0.0),
    ],
    ids=["inside", "clipped", "behind", "beyond_maxt", "miss"],
)
def test_track_range(variant_scalar_rgb, o, d, maxt, length):
    """Test that the tracking function properly covers the medium's box range.
    Uses transmittance as a proxy of distance, since it is exact with constant
    media."""
    medium = _constant(2.0, 0.5)
    ray = mi.Ray3f(mi.Ray3f(o=mi.Point3f(o), d=mi.Vector3f(d)), maxt)
    sample = _tracking_sample(0)
    tr = medium.sample_transmittance(ray, sample, 0)

    assert dr.allclose(tr, dr.exp(-2.0 * length))


def _tracking_sample(seed):
    """First free-flight sample, drawn from the generator of stream ``seed``."""
    rng = mi.PCG32()
    rng.seed(initseq=mi.UInt64(seed))
    return rng.next_float32()


def _track_test(medium, ray, seed, tracking):
    """``(distance, weight, transmittance, pdf_ratio)`` of the medium method
    that ``tracking`` names, ``None`` where it does not produce a field:
    ``delta`` is ``sample_real_interaction``, ``ratio`` is
    ``sample_transmittance_pdf`` (``weight`` is its throughput) and ``rrt`` is
    ``sample_transmittance``."""
    sample = _tracking_sample(seed)
    if tracking == "delta":
        ms, weight = medium.sample_real_interaction(ray, sample, 0)
        return ms.mei.t, weight, None, ms.pdf_ratio
    if tracking == "ratio":
        tr, throughput, pdf_ratio = medium.sample_transmittance_pdf(ray, sample, 0)
        return None, throughput, tr, pdf_ratio
    tr = medium.sample_transmittance(ray, sample, 0)
    return None, None, tr, None


TRACKING = ["delta", "ratio", "rrt"]


def _constant(sigma_t, albedo):
    return mi.load_dict(
        {
            "type": "heterogeneous",
            "sigma_t": {"type": "constvolume", "value": sigma_t},
            "albedo": albedo,
            "has_spectral_extinction": False,
        }
    )


def _step_grid(**props):
    """sigma_t 1 on x in [0, 0.5] and 2 on [0.5, 1] (y, z in [0, 1]), under
    a global majorant of 2: null collisions on the first half. ``props`` are
    added to the medium's properties."""
    return mi.load_dict(
        {
            "type": "heterogeneous",
            "sigma_t": {
                "type": "gridvolume",
                "grid": mi.VolumeGrid(mi.TensorXf([1.0, 2.0], shape=(1, 1, 2, 1))),
                "filter_type": "nearest",
            },
            "albedo": 0.8,
            "has_spectral_extinction": False,
            "extremum": {"type": "globalextremum"},
            **props,
        }
    )


# name: (medium, ray origin, [(t, optical depth over [0, t])]) for a ray
# along +x; the last checkpoint is past the medium.
TRACKING_CASES = {
    "grid": (_step_grid, (-1.0, 0.5, 0.5), [(1.5, 0.5), (2.0, 1.5)]),
}


def _tracking_case(name):
    """``(medium, ray, checkpoints)`` of ``TRACKING_CASES[name]``."""
    medium, o, checkpoints = TRACKING_CASES[name]
    ray = mi.Ray3f(o=mi.Point3f(o), d=mi.Vector3f(1.0, 0.0, 0.0))
    return medium(), ray, checkpoints


def _track_seeds(medium, ray, n, tracking):
    """``(distance, weight, transmittance, pdf_ratio)`` of ``_track_test``
    over seeds ``0..n-1`` as arrays, spectra reduced to their first channel,
    ``None`` where ``_track_test`` gives ``None``: one wide call, or one call
    per seed in scalar variants."""
    if dr.is_jit_v(mi.Float):
        ray = dr.gather(mi.Ray3f, ray, dr.zeros(mi.UInt32, n))
        rec = _track_test(medium, ray, dr.arange(mi.UInt32, n), tracking)
        return [np.array(rec[0])] + [
            None if rec[i] is None else np.array(rec[i][0]) for i in (1, 2, 3)
        ]
    recs = [_track_test(medium, ray, seed, tracking) for seed in range(n)]
    return [np.array([r[0] for r in recs])] + [
        None if recs[0][i] is None else np.array([r[i][0] for r in recs])
        for i in (1, 2, 3)
    ]


@pytest.mark.parametrize("case", TRACKING_CASES)
@pytest.mark.parametrize("tracking", TRACKING)
def test_track_unbiased(variants_all_rgb, case, tracking):
    """Ratio tracking estimates the transmittance ``exp(-ot)``; delta tracking
    samples real collisions with CDF ``1 - exp(-ot(t))`` and weighs them by
    the albedo. Means agree within 5 standard errors over 100k lanes. Grey
    majorants leave ``pdf_ratio`` at one."""
    n, z = 100_000, 5.0
    medium, ray, checkpoints = _tracking_case(case)
    distance, weight, tr, pdf_ratio = _track_seeds(medium, ray, n, tracking)
    if tracking != "rrt":
        assert np.allclose(pdf_ratio, 1.0, rtol=1e-5)

    if tracking != "delta":
        expected = np.exp(-checkpoints[-1][1])
        assert abs(tr.mean() - expected) < z * tr.std() / np.sqrt(n) + 1e-6
    else:
        for t, ot in checkpoints:
            p = 1.0 - np.exp(-ot)
            assert abs(np.mean(distance < t) - p) < z * np.sqrt(p * (1 - p) / n), t
        real = np.isfinite(distance)
        assert np.allclose(weight[real], 0.8, rtol=1e-5)
        assert np.allclose(weight[~real], 1.0, rtol=1e-5)


@pytest.mark.parametrize("tracking", ["ratio", "rrt"])
def test_ratio_track_russian_roulette(variants_all_rgb, tracking):
    """With an ``rr_threshold`` above the transmittance exp(-1.5) of the step
    grid, Russian roulette kills more estimates than ratio tracking alone,
    and the mean still agrees with exp(-1.5) within 5 standard errors."""
    n, z = 100_000, 5.0
    ray = mi.Ray3f(o=mi.Point3f(-1.0, 0.5, 0.5), d=mi.Vector3f(1.0, 0.0, 0.0))
    tr = _track_seeds(_step_grid(rr_threshold=0.5), ray, n, tracking)[2]
    tr_ref = _track_seeds(_step_grid(), ray, n, tracking)[2]

    assert np.mean(tr == 0.0) > np.mean(tr_ref == 0.0)
    assert abs(tr.mean() - np.exp(-1.5)) < z * tr.std() / np.sqrt(n)


@pytest.mark.parametrize("case", TRACKING_CASES)
@pytest.mark.parametrize("tracking", TRACKING)
def test_track_vcall_matches_host(variants_vec_rgb, case, tracking):
    """Tracking through a ``MediumPtr`` vcall, lanes diverging, against the
    host pointer: same draws, same result."""
    n = 1024
    medium, ray, _ = _tracking_case(case)
    ray = dr.gather(mi.Ray3f, ray, dr.zeros(mi.UInt32, n))
    seed = dr.arange(mi.UInt32, n)
    expected = _track_test(medium, ray, seed, tracking)
    actual = _track_test(dr.full(mi.MediumPtr, medium, n), ray, seed, tracking)
    for e, a in zip(expected, actual):
        assert dr.all(e == a, axis=None)
