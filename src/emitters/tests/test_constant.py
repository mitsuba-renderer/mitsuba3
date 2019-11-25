import numpy as np
import pytest

from mitsuba.scalar_rgb.core import warp
from mitsuba.scalar_rgb.core.math import InvFourPi, Epsilon
from mitsuba.scalar_rgb.core.xml import load_string
from mitsuba.scalar_rgb.render import PositionSample3f, Interaction3f


def example_emitter(spectrum = "1.0", extra = ""):
    return load_string("""
        <emitter version="2.0.0" type="constant">
            <spectrum name="radiance" value="{}"/>
            {}
        </emitter>
    """.format(spectrum, extra))


@pytest.fixture(scope="module")
def interaction():
    it = Interaction3f()
    it.wavelengths = [1, 1, 1]
    it.p = [-0.5, 0.3, -0.1]  # Some position inside the unit sphere
    it.time = 1.0
    return it


def test01_construct():
    assert not example_emitter().bbox().valid()  # degenerate bounding box


def test02_sample_ray():
    try:
        from mitsuba.scalar_spectral.core.xml import load_string as load_string_spectral
    except ImportError:
        pytest.skip("scalar_spectral mode not enabled")

    e = load_string_spectral("""
            <emitter version="2.0.0" type="constant">
                <spectrum name="radiance" value="3.5"/>
            </emitter>
        """)

    ray, weight = e.sample_ray(0.3, 0.4, [0.1, 0.6], [0.9, 0.24])
    assert np.allclose(ray.time, 0.3) and np.allclose(ray.mint, Epsilon)
    # Ray should point towards the scene
    assert np.sum(ray.d > 0) > 0
    assert np.dot(ray.d, ray.o) < 0
    assert np.all(weight > 0)
    # Wavelengths sampled should be different
    n = len(ray.wavelength)
    assert n > 0 and len(np.unique(ray.wavelength))


def test03_sample_direction(interaction):
    e = example_emitter(spectrum=3.5)
    ds, spectrum = e.sample_direction(interaction, [0.85, 0.13])

    assert np.allclose(ds.pdf, InvFourPi)
    # Ray points towards the scene's bounding sphere
    assert np.any(np.abs(ds.d) > 0)
    assert np.dot(ds.d, [0, 0, 1]) > 0
    assert np.allclose(e.pdf_direction(interaction, ds), InvFourPi)
    assert np.allclose(ds.time, interaction.time)
