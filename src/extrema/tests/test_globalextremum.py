import mitsuba as mi
import numpy as np
import pytest

from .utils import sample_iter


def _make_grid_volume(values, n):
    data = np.array(values, dtype=float).reshape(n, 1, 1)
    return mi.load_dict(
        {
            "type": "gridvolume",
            "grid": mi.VolumeGrid(data),
            "filter_type": "nearest",
            "accel": False,
        }
    )


def _bounds(extremum):
    # The global structure yields its scaled bounds on any segment, so one
    # `next()` call is enough, even for a ray missing an unbounded bbox.
    ray = mi.Ray3f(o=[0.5, 0.5, -1.0], d=[0, 0, 1])
    segment, _ = extremum.next(extremum.begin(ray, 0.0, 10.0), ray)
    return segment.minorant(), segment.majorant()


def test_build(variant_scalar_rgb):
    volume = _make_grid_volume([1.0, 2.0, 3.0, 4.0], 4)
    extremum = mi.load_dict({"type": "globalextremum"})
    extremum.update_extremum(volume.bbox(), volume)

    assert np.allclose(_bounds(extremum), (1.0, 4.0))


def test_set_scale(variant_scalar_rgb):
    volume = _make_grid_volume([1.0, 2.0, 3.0, 4.0], 4)
    extremum = mi.load_dict({"type": "globalextremum"})
    extremum.update_extremum(volume.bbox(), volume, 2.0)

    assert np.allclose(_bounds(extremum), (2.0, 8.0))


@pytest.mark.parametrize(
    "medium_type", ["heterogeneous", "homogeneous"]
)
def test_update_on_sigma_t_change(variant_scalar_rgb, medium_type):
    n = 4
    before = [1.0, 2.0, 3.0, 4.0]
    after = [5.0, 6.0, 7.0, 8.0]

    volume = _make_grid_volume(before, n)
    medium = mi.load_dict({"type": medium_type, "sigma_t": volume, "albedo": 0.5})

    # Ground truth: an extremum structure built directly from the "after"
    # data, independently of the update mechanism under test.
    ref_volume = _make_grid_volume(after, n)
    ref_medium = mi.load_dict(
        {"type": medium_type, "sigma_t": ref_volume, "albedo": 0.5}
    )
    expected = np.array(_bounds(ref_medium.extremum()))

    params = mi.traverse(medium)
    params["sigma_t.data"] = mi.TensorXf(np.array(after).reshape(n, 1, 1, 1))
    params.update()

    got = np.array(_bounds(medium.extremum()))
    assert np.allclose(got, expected)


@pytest.mark.parametrize(
    "medium_type", ["heterogeneous", "homogeneous"]
)
def test_update_on_scale_change(variant_scalar_rgb, medium_type):
    # A scale-only update must go through `set_scale()` without building
    # the extremum structure from `sigma_t`.
    volume = _make_grid_volume([1.0, 2.0, 3.0, 4.0], 4)
    medium = mi.load_dict(
        {"type": medium_type, "sigma_t": volume, "albedo": 0.5, "scale": 1.0}
    )

    params = mi.traverse(medium)
    params["scale"] = mi.Float(2.0)
    params.update()

    got = np.array(_bounds(medium.extremum()))
    assert np.allclose(got, (2.0, 8.0))


def test_sample_sampled(variant_scalar_rgb):
    # Homogeneous majorant of 3.0 over [1, 4]: segment_ot = 3 * 3 = 9. The
    # domain extends along z to hold the range.
    volume = _make_grid_volume([3.0, 3.0], 2)
    extremum = mi.load_dict({"type": "globalextremum"})
    extremum.update_extremum(mi.BoundingBox3f([0, 0, 0], [1, 1, 5]), volume)

    ray = mi.Ray3f(o=[0.5, 0.5, 0], d=[0, 0, 1])
    distance, leftover_ot = sample_iter(extremum, ray, 1.0, 4.0, target_ot=6.0)

    # target_ot < segment_ot: interaction sampled inside the segment.
    assert np.allclose(distance, 3.0)
    assert np.allclose(leftover_ot, 6.0)


def test_sample_escapes(variant_scalar_rgb):
    # Homogeneous majorant of 3.0 over [1, 4]: segment_ot = 3 * 3 = 9. The
    # domain extends along z to hold the range.
    volume = _make_grid_volume([3.0, 3.0], 2)
    extremum = mi.load_dict({"type": "globalextremum"})
    extremum.update_extremum(mi.BoundingBox3f([0, 0, 0], [1, 1, 5]), volume)

    ray = mi.Ray3f(o=[0.5, 0.5, 0], d=[0, 0, 1])
    distance, leftover_ot = sample_iter(extremum, ray, 1.0, 4.0, target_ot=12.0)

    # target_ot > segment_ot: ray exits the medium before sampling.
    assert np.isinf(distance)
    assert np.allclose(leftover_ot, 3.0)
