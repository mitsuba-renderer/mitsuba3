import numpy as np
import pytest

import mitsuba
from mitsuba.core import MTS_WAVELENGTH_SAMPLES, MTS_WAVELENGTH_MIN, \
                         MTS_WAVELENGTH_MAX, Properties, BoundingBox3f
from mitsuba.core.xml import load_string
from mitsuba.render import Texture3D, Interaction3f, Interaction3fX
from mitsuba.test.util import tmpfile

def test01_constant_construct():
    t3d = load_string("""
        <texture3d type="constant3d" version="2.0.0">
            <transform name="to_world">
                <scale x="2" y="0.2" z="1"/>
            </transform>
            <spectrum name="color" value="500:3.0 600:3.0"/>
        </texture3d>
    """)
    assert t3d is not None
    assert t3d.bbox() == BoundingBox3f([0, 0, 0], [2, 0.2, 1])

def test02_constant_eval():
    color_xml = """
        <spectrum name="color" type="blackbody" {}>
            <float name="temperature" value="3200"/>
        </spectrum>
    """
    color = load_string(color_xml.format('version="2.0.0"'))
    t3d = load_string("""
        <texture3d type="constant3d" version="2.0.0">
            {color}
            {transform}
        </texture3d>
    """.format(color=color_xml.format(''), transform=""))
    assert np.allclose(t3d.mean(), color.mean())
    assert t3d.bbox() == BoundingBox3f([0, 0, 0], [1, 1, 1])

    n_points = 20
    it = Interaction3fX(n_points)
    # Locations inside and outside the unit cube
    it.p = np.concatenate([
        np.random.uniform(size=(n_points//2, 3)),
        3.0 + np.random.uniform(size=(n_points//2, 3))
    ], axis=0)
    it.wavelengths = np.random.uniform(low=MTS_WAVELENGTH_MIN, high=MTS_WAVELENGTH_MAX,
                                       size=(n_points, MTS_WAVELENGTH_SAMPLES))
    active = np.ones(shape=(n_points), dtype=np.bool)
    results = t3d.eval(it, active)
    expected = np.concatenate([
        color.eval(it.wavelengths[:n_points//2, :], active[:n_points//2]),
        np.zeros(shape=(n_points//2, MTS_WAVELENGTH_SAMPLES))
    ], axis=0)
    assert np.allclose(results, expected), "\n{}\nvs\n{}\n".format(results, expected)

