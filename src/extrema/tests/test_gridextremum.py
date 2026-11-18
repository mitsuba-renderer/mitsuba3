import mitsuba as mi
import numpy as np
import pytest

from .utils import sample_iter


def generate_extremum_grid(
    volume_grid,
    extremum_res,
    filter_type,
    transform=None,
):
    if transform is None:
        transform = mi.ScalarAffineTransform4f()

    volume = mi.load_dict(
        {
            "type": "gridvolume",
            "grid": volume_grid,
            "filter_type": filter_type,
            "accel": False,
            "to_world": transform,
        }
    )
    extremum_struct = mi.load_dict({"type": "gridextremum", "resolution": extremum_res})
    extremum_struct.update_extremum(volume.bbox(), volume)

    extremum_grid = mi.traverse(extremum_struct)["data"].numpy()
    extremum_grid = extremum_grid.reshape(
        extremum_res.z, extremum_res.y, extremum_res.x, 2
    )
    extremum_grid = extremum_grid.transpose(2, 1, 0, 3)
    return extremum_struct, extremum_grid


def test_build_high_res(variant_scalar_rgb):

    n_x = 4
    n_y = 8
    n_z = 3
    n_prod = n_x * n_y * n_z
    data = np.linspace(1, n_prod, n_prod).reshape(n_x, n_y, n_z)
    volume_grid = mi.VolumeGrid(data.transpose(2, 1, 0))

    extremum_resolution = mi.ScalarVector3i(n_x, n_y, n_z)
    _, extremum_grid = generate_extremum_grid(
        volume_grid, extremum_resolution, "nearest"
    )

    assert np.allclose(data, extremum_grid[:, :, :, 0])
    assert np.allclose(data, extremum_grid[:, :, :, 1])


def test_build_half_res(variant_scalar_rgb):

    n_x = 4
    n_y = 8
    n_z = 4
    n_prod = n_x * n_y * n_z
    data = np.linspace(1, n_prod, n_prod).reshape(n_x, n_y, n_z)
    volume_grid = mi.VolumeGrid(data.transpose(2, 1, 0))

    extremum_resolution = mi.ScalarVector3i(2, 4, 2)
    _, extremum_grid = generate_extremum_grid(
        volume_grid, extremum_resolution, "nearest"
    )

    assert np.allclose(data[::2, ::2, ::2], extremum_grid[:, :, :, 0])
    assert np.allclose(data[1::2, 1::2, 1::2], extremum_grid[:, :, :, 1])


def test_build_not_multiple(variant_scalar_rgb):
    n_x = 4
    n_y = 9
    n_z = 1
    n_prod = n_x * n_y * n_z
    data = np.linspace(1, n_prod, n_prod).reshape(n_x, n_y, n_z)
    volume_grid = mi.VolumeGrid(data.transpose(2, 1, 0))

    extremum_resolution = mi.ScalarVector3i(3, 4, 1)
    _, extremum_grid = generate_extremum_grid(
        volume_grid, extremum_resolution, "nearest"
    )

    minorant_reference = np.array(
        [1.0, 3.0, 5.0, 7.0, 10.0, 12.0, 14.0, 16.0, 19.0, 21.0, 23.0, 25.0]
    ).reshape(3, 4, 1)
    majorant_reference = np.array(
        [12.0, 14.0, 16.0, 18.0, 21.0, 23.0, 25.0, 27.0, 30.0, 32.0, 34.0, 36.0]
    ).reshape(3, 4, 1)

    assert np.allclose(minorant_reference, extremum_grid[:, :, :, 0])
    assert np.allclose(majorant_reference, extremum_grid[:, :, :, 1])


def test_build_trilinear(variant_scalar_rgb):
    n_x = 3
    n_y = 6
    n_z = 1
    n_prod = n_x * n_y * n_z
    data = np.linspace(1, n_prod, n_prod).reshape(n_x, n_y, n_z)
    volume_grid = mi.VolumeGrid(data.transpose(2, 1, 0))

    extremum_resolution = mi.ScalarVector3i(3, 6, 1)
    _, extremum_grid = generate_extremum_grid(
        volume_grid, extremum_resolution, "trilinear"
    )

    minorant_reference = np.array(
        [
            1.0,
            1.0,
            2.0,
            3.0,
            4.0,
            5.0,
            1.0,
            1.0,
            2.0,
            3.0,
            4.0,
            5.0,
            7.0,
            7.0,
            8.0,
            9.0,
            10.0,
            11.0,
        ]
    ).reshape(3, 6, 1)
    majorant_reference = np.array(
        [
            8.0,
            9.0,
            10.0,
            11.0,
            12.0,
            12.0,
            14.0,
            15.0,
            16.0,
            17.0,
            18.0,
            18.0,
            14.0,
            15.0,
            16.0,
            17.0,
            18.0,
            18.0,
        ]
    ).reshape(3, 6, 1)

    assert np.allclose(minorant_reference, extremum_grid[:, :, :, 0])
    assert np.allclose(majorant_reference, extremum_grid[:, :, :, 1])


def test_build_rotated(variant_scalar_rgb):
    n_x = 3
    n_y = 4
    n_z = 1
    n_prod = n_x * n_y * n_z
    data = np.linspace(1, n_prod, n_prod).reshape(n_x, n_y, n_z)
    volume_grid = mi.VolumeGrid(data.transpose(2, 1, 0))

    extremum_resolution = mi.ScalarVector3i(3, 4, 1)
    _, extremum_grid = generate_extremum_grid(
        volume_grid,
        extremum_resolution,
        "nearest",
        transform=mi.ScalarAffineTransform4f.rotate([0, 0, 1], 180),
    )

    assert np.allclose(data.squeeze(), extremum_grid[:, :, :, 1].squeeze())


def test_build_scaled(variant_scalar_rgb):
    pass


def test_build_resolution_clamped_to_volume(variant_scalar_rgb):
    # A fixed resolution requested finer than the volume's own resolution
    # along some axis is not a valid extremum grid (it wouldn't be coarser
    # than what it summarizes), so that axis must be clamped down to match
    # the volume's resolution. TODO: Come back to this if change to world
    # coordinates.
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
            "type": "gridextremum",
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


@pytest.mark.parametrize("medium_type", ["heterogeneous"])
def test_update_on_sigma_t_change(variant_scalar_rgb, medium_type):
    n = 4
    resolution = mi.ScalarVector3i(1, 1, n)
    before = [1.0, 2.0, 3.0, 4.0]
    after = [5.0, 6.0, 7.0, 8.0]

    volume = _make_grid_volume(before, n)
    extremum = mi.load_dict({"type": "gridextremum", "resolution": resolution})
    medium = _make_medium(medium_type, volume, extremum)

    # Ground truth: an extremum structure built directly from the "after"
    # data, independently of the update mechanism under test.
    ref_volume = _make_grid_volume(after, n)
    ref_extremum = mi.load_dict({"type": "gridextremum", "resolution": resolution})
    ref_extremum.update_extremum(ref_volume.bbox(), ref_volume)
    expected = np.array(mi.traverse(ref_extremum)["data"])

    params = mi.traverse(medium)
    params["sigma_t.data"] = mi.TensorXf(np.array(after).reshape(n, 1, 1, 1))
    params.update()

    got = np.array(mi.traverse(medium.extremum())["data"])
    assert np.allclose(got, expected)


def _make_x_volume(values, n, wrap_mode="clamp", to_world=None):
    # Radial/X resolution must be the grid's fastest (last) axis.
    data = np.array(values, dtype=float).reshape(1, 1, n)
    d = {
        "type": "gridvolume",
        "grid": mi.VolumeGrid(data),
        "filter_type": "nearest",
        "accel": False,
        "wrap_mode": wrap_mode,
    }
    if to_world is not None:
        d["to_world"] = to_world
    return mi.load_dict(d)


def _make_x_extremum(bbox, volume, n):
    extremum = mi.load_dict(
        {
            "type": "gridextremum",
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
    distance, leftover_ot = sample_iter(extremum, ray, 0.0, 1.0, target_ot=1.6)

    assert np.allclose(distance, 0.775)
    assert np.allclose(leftover_ot, 0.1)


def test_sample_tight_escapes(variant_scalar_rgb):
    # Same setup as `test_sample_tight_sampled`,
    # total ot = 0.25 * (1 + 2 + 3 + 4) = 2.5, never reached.
    volume = _make_x_volume([1.0, 2.0, 3.0, 4.0], 4)
    extremum = _make_x_extremum(volume.bbox(), volume, 4)

    ray = mi.Ray3f(o=[0, 0.5, 0.5], d=[1, 0, 0])
    distance, leftover_ot = sample_iter(extremum, ray, 0.0, 1.0, target_ot=3.0)

    assert np.isinf(distance)
    assert np.allclose(leftover_ot, 0.5)


@pytest.mark.parametrize(
    "wrap_mode,expected_distance,expected_leftover",
    [
        # Cells [1,2,3,4] tile again past x=1, so the target is spent reaching the
        # 3rd repeated cell (value 3) before landing 0.1 into the 4th (value 4).
        ("repeat", 1.775, 0.1),
        # Past x=1 the pattern reflects to [4,3,2,1], so the
        # target lands directly in the first (value 4) mirrored cell.
        ("mirror", 1.45, 0.6),
        # Past x=1 the whole remainder is one merged constant-majorant segment
        # segment at the edge value (4). Leftover ot reported is the full amount
        # needed when entering that merged segment, i.e. (4.1 - 2.5 = 1.6).
        ("clamp", 1.4, 1.6),
    ],
)
def test_sample_non_tight_sampled(
    variant_scalar_rgb, wrap_mode, expected_distance, expected_leftover
):
    # Domain twice as wide as the volume's own [0, 1] bbox along x: the
    # second half is only reachable through wrap_mode-driven indexing.
    volume = _make_x_volume([1.0, 2.0, 3.0, 4.0], 4, wrap_mode=wrap_mode)
    domain = mi.BoundingBox3f([0, 0, 0], [2, 2, 2])
    extremum = _make_x_extremum(domain, volume, 4)

    ray = mi.Ray3f(o=[0, 0.5, 0.5], d=[1, 0, 0])
    distance, leftover_ot = sample_iter(extremum, ray, 0.0, 2.0, target_ot=4.1)

    assert np.allclose(distance, expected_distance)
    assert np.allclose(leftover_ot, expected_leftover)


@pytest.mark.parametrize("wrap_mode", ["clamp", "repeat", "mirror"])
def test_sample_rotated_axis_aligned(variant_scalar_rgb, wrap_mode):
    # Volume rotated 90 degrees about z: its local x-variation now runs
    # along world y. Domain is volume.bbox(), so the ray never leaves it and
    # wrap_mode has no effect.
    to_world = mi.ScalarAffineTransform4f.rotate([0, 0, 1], 90)
    volume = _make_x_volume(
        [1.0, 2.0, 3.0, 4.0], 4, wrap_mode=wrap_mode, to_world=to_world
    )
    extremum = _make_x_extremum(volume.bbox(), volume, 4)

    ray = mi.Ray3f(o=[-0.5, 0, 0.5], d=[0, 1, 0])
    distance, leftover_ot = sample_iter(extremum, ray, 0.0, 1.0, target_ot=1.6)

    assert np.allclose(distance, 0.775)
    assert np.allclose(leftover_ot, 0.1)


@pytest.mark.parametrize(
    "wrap_mode,expected_distance,expected_leftover",
    [
        # The 2 gap cells both clip to cell 0 into one 0.75-long segment.
        ("clamp", 1.0 + 0.35 / 3.0, 0.35),
        # The gap cells wrap to the last 2 real cells (values 3, 4).
        ("repeat", 0.4625, 0.85),
        # The gap cells reflect to values (2, 1).
        ("mirror", 1.0 + 0.1 / 3.0, 0.1),
    ],
)
def test_sample_rotated(
    variant_scalar_rgb, wrap_mode, expected_distance, expected_leftover
):
    # Rotated 45 degrees about z, so the local x-variation runs diagonally
    # in world space. The domain is the tightest axis-aligned box around the
    # leaving a triangular gap between the box and the volume's actual footprint.
    # Start on the domain edge, at local x=-0.5, 2 cells before the real
    # data starts at x=0 heading inward.
    to_world = mi.ScalarAffineTransform4f.rotate([0, 0, 1], 45)
    volume = _make_x_volume(
        [1.0, 2.0, 3.0, 4.0], 4, wrap_mode=wrap_mode, to_world=to_world
    )
    extremum = _make_x_extremum(volume.bbox(), volume, 4)

    ray = mi.Ray3f(
        o=to_world @ mi.ScalarPoint3f(-0.5, 0.5, 0.5),
        d=to_world @ mi.ScalarVector3f(1, 0, 0),
    )
    distance, leftover_ot = sample_iter(extremum, ray, 0.0, 1.5, target_ot=1.6)

    assert np.allclose(distance, expected_distance)
    assert np.allclose(leftover_ot, expected_leftover)
