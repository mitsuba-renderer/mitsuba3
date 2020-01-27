import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float

from mitsuba.python.test import variant_scalar, variant_spectral


def example_emitter(spectrum = "1.0", extra = ""):
    from mitsuba.core.xml import load_string

    return load_string("""
        <emitter version="2.0.0" type="constant">
            <spectrum name="radiance" value="{}"/>
            {}
        </emitter>
    """.format(spectrum, extra))


def test01_construct(variant_scalar):
    assert not example_emitter().bbox().valid()  # degenerate bounding box


def test02_sample_ray(variant_spectral):
    from mitsuba.core.math import InvFourPi, RayEpsilon
    from mitsuba.core.xml import load_string
    import numpy as np

    e = load_string("""
            <emitter version="2.0.0" type="constant">
                <spectrum name="radiance" value="3.5"/>
            </emitter>
        """)

    ray, weight = e.sample_ray(0.3, 0.4, [0.1, 0.6], [0.9, 0.24])

    assert ek.allclose(ray.time, 0.3) and ek.allclose(ray.mint, RayEpsilon)
    # Ray should point towards the scene
    assert ek.count(ray.d > 0) > 0
    assert ek.dot(ray.d, ray.o) < 0
    assert ek.all(weight > 0)
    # Wavelengths sampled should be different
    n = len(ray.wavelengths)
    assert n > 0 and len(np.unique(ray.wavelengths))


def test03_sample_direction(variant_scalar):
    from mitsuba.core.math import InvFourPi, RayEpsilon
    from mitsuba.render import Interaction3f

    it = Interaction3f()
    it.wavelengths = []
    it.p = [-0.5, 0.3, -0.1]  # Some position inside the unit sphere
    it.time = 1.0

    e = example_emitter(spectrum=3.5)
    ds, spectrum = e.sample_direction(it, [0.85, 0.13])

    assert ek.allclose(ds.pdf, InvFourPi)
    # Ray points towards the scene's bounding sphere
    assert ek.any(ek.abs(ds.d) > 0)
    assert ek.dot(ds.d, [0, 0, 1]) > 0
    assert ek.allclose(e.pdf_direction(it, ds), InvFourPi)
    assert ek.allclose(ds.time, it.time)
