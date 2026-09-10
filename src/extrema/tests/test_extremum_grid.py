import mitsuba as mi
import numpy as np
import pytest


def test_build_resolution_clamped_to_volume(variant_scalar_rgb):
    # A fixed resolution requested finer than the volume's own resolution
    # along some axis is not a valid extremum grid (it wouldn't be coarser
    # than what it summarizes), so that axis must be clamped down to match
    # the volume's resolution.
    n_x, n_y, n_z = 8, 8, 4
    n_prod = n_x * n_y * n_z
    data = np.linspace(1, n_prod, n_prod).reshape(n_x, n_y, n_z)
    volume_grid = mi.VolumeGrid(data.transpose(2, 1, 0))

    volume = mi.load_dict(
        {
            "type": "gridvolume",
            "grid": volume_grid,
            "filter_type": "nearest",
            "accel": False,
        }
    )

    extremum_struct = mi.load_dict(
        {
            "type": "extremum_grid",
            "resolution": mi.ScalarVector3i(n_x, n_y * 2, n_z),
        }
    )
    extremum_struct.update_extremum(volume.bbox(), volume)

    resolution = np.array(mi.traverse(extremum_struct)["resolution"])
    assert np.array_equal(resolution, [n_x, n_y, n_z])


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


def _make_medium(medium_type, volume, extremum):
    return mi.load_dict(
        {
            "type": medium_type,
            "sigma_t": volume,
            "albedo": 0.5,
            "extremum": extremum,
        }
    )


def test_update_on_sigma_t_change(variant_scalar_rgb):
    n = 4
    resolution = mi.ScalarVector3i(1, 1, n)
    before = [1.0, 2.0, 3.0, 4.0]
    after = [5.0, 6.0, 7.0, 8.0]

    volume = _make_grid_volume(before, n)
    extremum = mi.load_dict({"type": "extremum_grid", "resolution": resolution})
    medium = _make_medium("heterogeneous", volume, extremum)

    # Build the ground truth directly from the "after" data.
    ref_volume = _make_grid_volume(after, n)
    ref_extremum = mi.load_dict({"type": "extremum_grid", "resolution": resolution})
    ref_extremum.update_extremum(ref_volume.bbox(), ref_volume)

    params = mi.traverse(medium)
    params["sigma_t.data"] = mi.TensorXf(np.array(after).reshape(n, 1, 1, 1))
    params.update()

    ray = mi.Ray3f(o=[0.5, 0.5, 0], d=[0, 0, 1])
    expected = ref_extremum.sample_test(ray, 0.0, 1.0, target_ot=6.0)
    got = medium.extremum().sample_test(ray, 0.0, 1.0, target_ot=6.0)
    assert np.allclose(got, expected)


def _make_x_volume(values, n, to_world=None):
    # Radial/X resolution must be the grid's fastest (last) axis.
    data = np.array(values, dtype=float).reshape(1, 1, n)
    d = {
        "type": "gridvolume",
        "grid": mi.VolumeGrid(data),
        "filter_type": "nearest",
        "accel": False,
    }
    if to_world is not None:
        d["to_world"] = to_world
    return mi.load_dict(d)


def _make_x_extremum(bbox, volume, n):
    extremum = mi.load_dict(
        {
            "type": "extremum_grid",
            "resolution": mi.ScalarVector3i(n, 1, 1),
        }
    )
    extremum.update_extremum(bbox, volume)
    return extremum


def test_sample_tight_sampled(variant_scalar_rgb):
    # 4 cells of width 0.25 along x, values [1, 2, 3, 4], domain == volume
    # bbox. Sampled inside the 4th cell: ot(cell1..3) = 0.25 + 0.5 + 0.75,
    # leaving 0.1 of the 1.6 target.
    volume = _make_x_volume([1.0, 2.0, 3.0, 4.0], 4)
    extremum = _make_x_extremum(volume.bbox(), volume, 4)

    ray = mi.Ray3f(o=[0, 0.5, 0.5], d=[1, 0, 0])
    distance, leftover_ot = extremum.sample_test(ray, 0.0, 1.0, target_ot=1.6)

    assert np.allclose(distance, 0.775)
    assert np.allclose(leftover_ot, 0.1)


def test_sample_tight_escapes(variant_scalar_rgb):
    # Same setup as `test_sample_tight_sampled`,
    # total ot = 0.25 * (1 + 2 + 3 + 4) = 2.5, never reached.
    volume = _make_x_volume([1.0, 2.0, 3.0, 4.0], 4)
    extremum = _make_x_extremum(volume.bbox(), volume, 4)

    ray = mi.Ray3f(o=[0, 0.5, 0.5], d=[1, 0, 0])
    distance, leftover_ot = extremum.sample_test(ray, 0.0, 1.0, target_ot=3.0)

    assert np.isinf(distance)
    assert np.allclose(leftover_ot, 0.5)


def test_sample_non_tight_sampled(variant_scalar_rgb):
    # Domain twice as wide as the volume's own [0, 1] bbox along x: the
    # second half is only reachable through the extremum's edge-clamped
    # indexing, which is the only supported behavior past the volume's data.
    volume = _make_x_volume([1.0, 2.0, 3.0, 4.0], 4)
    domain = mi.BoundingBox3f([0, 0, 0], [2, 2, 2])
    extremum = _make_x_extremum(domain, volume, 4)

    ray = mi.Ray3f(o=[0, 0.5, 0.5], d=[1, 0, 0])
    distance, leftover_ot = extremum.sample_test(ray, 0.0, 2.0, target_ot=4.1)

    assert np.allclose(distance, 1.4)
    assert np.allclose(leftover_ot, 0.6)


def test_sample_rotated_axis_aligned(variant_scalar_rgb):
    # Volume rotated 90 degrees about z: its local x-variation now runs
    # along world y. Domain is volume.bbox(), so the ray never leaves it.
    to_world = mi.ScalarAffineTransform4f.rotate([0, 0, 1], 90)
    volume = _make_x_volume([1.0, 2.0, 3.0, 4.0], 4, to_world=to_world)
    extremum = _make_x_extremum(volume.bbox(), volume, 4)

    ray = mi.Ray3f(o=[-0.5, 0, 0.5], d=[0, 1, 0])
    distance, leftover_ot = extremum.sample_test(ray, 0.0, 1.0, target_ot=1.6)

    assert np.allclose(distance, 0.775)
    assert np.allclose(leftover_ot, 0.1)


def test_sample_rotated(variant_scalar_rgb):
    # Rotated 45 degrees about z, so the local x-variation runs diagonally
    # in world space. The domain is the tightest axis-aligned box around the
    # volume's actual footprint, leaving a triangular gap between the box
    # and the volume. Start on the domain edge, at local x=-0.5, 2 cells
    # before the real data starts at x=0 heading inward.
    to_world = mi.ScalarAffineTransform4f.rotate([0, 0, 1], 45)
    volume = _make_x_volume([1.0, 2.0, 3.0, 4.0], 4, to_world=to_world)
    extremum = _make_x_extremum(volume.bbox(), volume, 4)

    ray = mi.Ray3f(
        o=to_world @ mi.ScalarPoint3f(-0.5, 0.5, 0.5),
        d=to_world @ mi.ScalarVector3f(1, 0, 0),
    )
    distance, leftover_ot = extremum.sample_test(ray, 0.0, 1.5, target_ot=1.6)

    # The 2 gap cells both clip to cell 0 into one 0.75-long segment.
    assert np.allclose(distance, 1.0 + 0.35 / 3.0)
    assert np.allclose(leftover_ot, 0.35)
