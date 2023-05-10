from pathlib import Path

import mitsuba as mi
import numpy as np
import pytest

from eradiate.kernel import write_binary_grid3d


def gridvol_constant(basepath: Path, data: np.typing.ArrayLike):
    filename = basepath / "gridvol_const.vol"
    data = np.array(data)
    write_binary_grid3d(filename, data)
    return filename


def test_construct(variant_scalar_rgb, tmp_path):
    # Construct without parameters
    volume = mi.load_dict({"type": "sphericalcoordsvolume"})
    assert volume is not None

    # Construct with rmin > rmax:
    with pytest.raises(Exception):
        mi.load_dict({"type": "sphericalcoordsvolume", "rmin": 0.8, "rmax": 0.2})

    # Construct with all parameters set to typical values
    gridvol_filename = gridvol_constant(tmp_path, data=[[[1.0]]])
    volume = mi.load_dict(
        {
            "type": "sphericalcoordsvolume",
            "rmin": 0.0,
            "rmax": 1.0,
            "fillmin": 0.0,
            "fillmax": 0.0,
            "volume": {"type": "gridvolume", "filename": str(gridvol_filename)},
        }
    )
    assert volume is not None


@pytest.mark.parametrize(
    "point,result",
    [
        ([0.9, 0.0, 0.0], 1.0),
        ([0.0, 0.0, 0.1], 3.0),
        ([0.0, 0.5, 0.0], 2.0),
    ],
)
def test_eval_basic(variant_scalar_rgb, tmp_path, point, result):
    gridvol_filename = gridvol_constant(tmp_path, data=[[[2.0]]])
    volume = mi.load_dict(
        {
            "type": "sphericalcoordsvolume",
            "rmin": 0.2,
            "rmax": 0.8,
            "fillmin": 3.0,
            "fillmax": 1.0,
            "volume": {"type": "gridvolume", "filename": str(gridvol_filename)},
        }
    )

    it = mi.Interaction3f()
    it.p = point

    assert volume.eval(it) == result


def test_eval_advanced(variant_scalar_rgb, tmp_path):
    data = np.broadcast_to(
        np.arange(1, 7, 1, dtype=float).reshape((-1, 1, 1, 1)), (6, 2, 2, 3)
    )
    gridvol_filename = gridvol_constant(tmp_path, data=data)
    volume = mi.load_dict(
        {
            "type": "sphericalcoordsvolume",
            "rmin": 0.0,
            "rmax": 1.0,
            "fillmin": 3.0,
            "fillmax": 1.0,
            "volume": {
                "type": "gridvolume",
                "filename": str(gridvol_filename),
                "filter_type": "nearest",
            },
        }
    )

    # have one point in each sector of the spherical volume
    # since the atan2 method maps [-pi, pi] to [0, 1], we choose this
    # order of inputs
    phis = np.array([-175, -115, -55, 5, 65, 125])  # degree
    results = np.empty((len(phis), 3))

    for i, phi in enumerate(phis):
        p = [0.5 * np.cos(np.deg2rad(phi)), 0.5 * np.sin(np.deg2rad(phi)), 0.0]
        it = mi.Interaction3f()
        it.p = p
        results[i, :] = volume.eval(it)

    expected = np.broadcast_to(
        np.array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0]).reshape((-1, 1)),
        (6, 3),
    )
    assert np.all(results == expected)
