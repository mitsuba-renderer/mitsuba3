import mitsuba
import pytest
import enoki as ek


def test01_create(variant_scalar_rgb):
    from mitsuba.core.xml import load_string
    from mitsuba.core import BoundingBox3f

    c = load_string("<sensor version='2.0.0' type='perspective'></sensor>")
    assert c is not None
    assert ek.allclose(c.near_clip(), 0.01)

    c = load_string("""
        <sensor version='2.0.0' type='perspective'>
            <float name='near_clip' value='1'/>
            <float name='far_clip' value='35.4'/>
            <float name='focus_distance' value='15'/>
            <float name='fov' value='34'/>

            <transform name="to_world">
                <translate x="1" y="0" z="1.5"/>
            </transform>
        </sensor>
    """)
    assert ek.allclose(c.near_clip(), 1)
    assert ek.allclose(c.far_clip(), 35.4)
    assert ek.allclose(c.focus_distance(), 15)

    wt = c.world_transform().eval(0)
    vt = wt.inverse()
    assert ek.allclose((wt.matrix - ek.scalar.Matrix4f([
        [1, 0, 0,   1],
        [0, 1, 0,   0],
        [0, 0, 1, 1.5],
        [0, 0, 0,   1]
    ])), ek.scalar.Matrix4f.zero())
    assert ek.allclose((wt.matrix - ek.scalar.Matrix4f([
        [1, 0, 0,   1],
        [0, 1, 0,   0],
        [0, 0, 1, 1.5],
        [0, 0, 0,   1]
    ])), ek.scalar.Matrix4f.zero())
    assert ek.allclose((wt.inverse_transpose - ek.scalar.Matrix4f([
        [ 1, 0,    0,   0],
        [ 0, 1,    0,   0],
        [ 0, 0,    1,   0],
        [-1, 0, -1.5,   1]
    ])), ek.scalar.Matrix4f.zero())
    assert ek.allclose((vt.matrix - ek.scalar.Matrix4f([
        [1, 0, 0,  -1],
        [0, 1, 0,   0],
        [0, 0, 1,-1.5],
        [0, 0, 0,   1]
    ])), ek.scalar.Matrix4f.zero())
    assert ek.allclose((vt.inverse_transpose - ek.scalar.Matrix4f([
        [1, 0,   0,  0],
        [0, 1,   0,  0],
        [0, 0,   1,  0],
        [1, 0, 1.5,  1]
    ])), ek.scalar.Matrix4f.zero())

    assert ek.allclose(c.shutter_open_time(), 0)
    assert not c.needs_aperture_sample()
    assert c.bbox() == BoundingBox3f([1, 0, 1.5], [1, 0, 1.5])

    # Aspect should be the same as a default film
    assert c.film() is not None


def test02_sample_rays(variant_scalar_rgb):
    from mitsuba.core.xml import load_string

    c = load_string("<sensor version='2.0.0' type='perspective'></sensor>")
    (ray, spec_weight) = c.sample_ray(0.0, 0.6, [0, 0], [0, 0])
    assert ray is not None
    assert ek.all(spec_weight > 0)


def test03_needs_time_sample(variant_scalar_rgb):
    from mitsuba.core.xml import load_string

    c = load_string("""
        <sensor version='2.0.0' type='perspective'>
            <float name='shutter_open' value='1.5'/>
            <float name='shutter_close' value='5'/>
        </sensor>
    """)
    assert ek.allclose(c.shutter_open(), 1.5)
    assert ek.allclose(c.shutter_open_time(), 5 - 1.5)
