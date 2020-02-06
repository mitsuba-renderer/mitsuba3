import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float

from mitsuba.python.test import variant_scalar


def example_rectangle(scale = (1, 1, 1), translate = (0, 0, 0)):
    from mitsuba.core.xml import load_string

    return load_string("""<shape version="2.0.0" type="rectangle">
        <transform name="to_world">
            <scale x="{}" y="{}" z="{}"/>
            <translate x="{}" y="{}" z="{}"/>
        </transform>
    </shape>""".format(scale[0], scale[1], scale[2],
                       translate[0], translate[1], translate[2]))


def test01_create(variant_scalar):
    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    s = example_rectangle()
    assert s is not None
    assert s.primitive_count() == 1
    assert ek.allclose(s.surface_area(), 4.0)


def test02_bbox(variant_scalar):
    from mitsuba.core import Vector3f

    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    sy = 2.5
    for sx in [1, 2, 4]:
        for translate in [Vector3f([1.3, -3.0, 5]),
                          Vector3f([-10000, 3.0, 31])]:
            s = example_rectangle((sx, sy, 1.0), translate)
            b = s.bbox()

            assert ek.allclose(s.surface_area(), sx * sy * 4)

            assert b.valid()
            assert ek.allclose(b.center(), translate)
            assert ek.allclose(b.min, translate - [sx, sy, 0.0])
            assert ek.allclose(b.max, translate + [sx, sy, 0.0])


def test03_ray_intersect(variant_scalar):
    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    from mitsuba.core.xml import load_string
    from mitsuba.core import Ray3f

    # Scalar
    scene = load_string("""<scene version="2.0.0">
        <shape type="rectangle">
            <transform name="to_world">
                <scale x="2" y="0.5" z="1"/>
            </transform>
        </shape>
    </scene>""")

    n = 15
    coords = ek.linspace(Float, -1, 1, n)
    rays = [Ray3f(o=[a, a, 5], d=[0, 0, -1], time=0.0,
                  wavelengths=[]) for a in coords]
    si_scalar = []
    valid_count = 0
    for i in range(n):
        # print(rays[i])
        its_found = scene.ray_test(rays[i])
        si = scene.ray_intersect(rays[i])

        assert its_found == (abs(coords[i]) <= 0.5)
        assert si.is_valid() == its_found
        si_scalar.append(si)
        valid_count += its_found

    assert valid_count == 7

    try:
        mitsuba.set_variant('packet_rgb')
        from mitsuba.core.xml import load_string as load_string_packet
        from mitsuba.core import Ray3f as Ray3fX
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    # Packet
    scene_p = load_string_packet("""<scene version="2.0.0">
        <shape type="rectangle">
            <transform name="to_world">
                <scale x="2" y="0.5" z="1"/>
            </transform>
        </shape>
    </scene>""")

    packet = Ray3fX.zero(n)
    for i in range(n):
        packet[i] = rays[i]

    si_p = scene_p.ray_intersect(packet)
    its_found_p = scene_p.ray_test(packet)

    assert ek.all(si_p.is_valid() == its_found_p)

    for i in range(n):
        assert ek.allclose(si_p.t[i], si_scalar[i].t)
