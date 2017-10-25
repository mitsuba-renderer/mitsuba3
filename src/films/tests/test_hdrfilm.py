import numpy as np
import os
import pytest

from mitsuba.core import (Bitmap, Struct, ReconstructionFilter, float_dtype)
from mitsuba.core.xml import load_string


def test01_construct():
    # With default reconstruction filter
    film = load_string("""<film version="2.0.0" type="hdrfilm"></film>""")
    assert film is not None
    assert film.reconstruction_filter() is not None

    # With a provided reconstruction filter
    film = load_string("""<film version="2.0.0" type="hdrfilm">
            <rfilter type="gaussian">
                <float name="stddev" value="18.5"/>
            </rfilter>
        </film>""")
    assert film is not None
    assert film.reconstruction_filter().radius() == (4 * 18.5)

    # Certain parameter values are not allowed
    with pytest.raises(RuntimeError):
        load_string("""<film version="2.0.0" type="hdrfilm">
            <string name="component_format" value="uint8"/>
        </film>""")
    with pytest.raises(RuntimeError):
        load_string("""<film version="2.0.0" type="hdrfilm">
            <string name="pixel_format" value="brga"/>
        </film>""")

def test02_crops():
    film = load_string("""<film version="2.0.0" type="hdrfilm">
            <integer name="width" value="32"/>
            <integer name="height" value="21"/>
            <integer name="crop_width" value="11"/>
            <integer name="crop_height" value="5"/>
            <integer name="crop_offset_x" value="2"/>
            <integer name="crop_offset_y" value="3"/>
            <boolean name="high_quality_edges" value="true"/>
            <string name="pixel_format" value="rgba"/>
        </film>""")
    assert film is not None
    assert np.all(film.size() == [32, 21])
    assert np.all(film.crop_size() == [11, 5])
    assert np.all(film.crop_offset() == [2, 3])
    assert film.has_high_quality_edges()
    assert film.has_alpha()

    # Crop size doesn't adjust its size, so an error should be raised if the
    # resulting crop window goes out of bounds.
    incomplete = """<film version="2.0.0" type="hdrfilm">
            <integer name="width" value="32"/>
            <integer name="height" value="21"/>
            <integer name="crop_offset_x" value="30"/>
            <integer name="crop_offset_y" value="20"/>"""
    with pytest.raises(RuntimeError):
        film = load_string(incomplete + "</film>")
    film = load_string(incomplete + """
            <integer name="crop_width" value="2"/>
            <integer name="crop_height" value="1"/>
        </film>""")
    assert film is not None
    assert np.all(film.size() == [32, 21])
    assert np.all(film.crop_size() == [2, 1])
    assert np.all(film.crop_offset() == [30, 20])

def test03_clear_set_add():
    film = load_string("""<film version="2.0.0" type="hdrfilm">
            <integer name="width" value="32"/>
            <integer name="height" value="21"/>
            <string name="pixel_format" value="luminance"/>
            <string name="component_format" value="uint32"/>
        </film>""")
    # Note that the internal storage format is independent from the format
    # which will be developped.
    assert film.bitmap().pixel_format() == Bitmap.ERGB
    assert film.bitmap().component_format() == (
        Struct.EFloat32 if float_dtype == np.float32
                        else Struct.EFloat64)

    b = Bitmap(Bitmap.ERGB, Struct.EFloat, film.bitmap().size())
    n = b.width() * b.height()
    # 0, 1, 2, ...
    ref = 1.05 * np.arange(n).reshape(b.height(), b.width(), 1).astype(float_dtype)
    np.array(b, copy=False)[:] = ref

    film.set_bitmap(b)
    assert np.allclose(np.array(film.bitmap(), copy=False), ref)
    film.add_bitmap(b, multiplier=1.5)
    assert np.allclose(np.array(film.bitmap(), copy=False), (1 + 1.5) * ref, atol=1e-4)
    film.clear()
    assert np.all(np.array(film.bitmap(), copy=False) == 0)
    # This shouldn't affect the original bitmap that was passed.
    assert np.all(np.array(b) == ref)

@pytest.mark.skip("Not implemented")
def test04_develop():
    pass
