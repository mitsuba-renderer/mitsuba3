import mitsuba
import pytest
import enoki as ek

def test01_create(variant_scalar_rgb):
    from mitsuba.core import xml, ScalarTransform4f

    s = xml.load_dict({
        'type' : 'shapegroup',
        'shape_01' : {
            'type' : 'sphere',
            'radius' : 1.0,
            'to_world' : ScalarTransform4f.translate([-2, 0, 0])
        },
        'shape_02' : {
            'type' : 'sphere',
            'radius' : 1.0,
            'to_world' : ScalarTransform4f.translate([2, 0, 0])
        }
    })

    b = s.bbox()
    assert s is not None
    assert s.primitive_count() == 2
    assert s.effective_primitive_count() == 0
    assert s.surface_area() == 0
    assert ek.allclose(b.center(), [0, 0, 0])
    assert ek.allclose(b.min,  [-3, -1, -1])
    assert ek.allclose(b.max,  [3, 1, 1])


def test02_error(variant_scalar_rgb):
    from mitsuba.core import xml

    error = "Nested instancing is not permitted"
    with pytest.raises(RuntimeError, match='.*{}.*'.format(error)):
        xml.load_dict({
            'type' : 'shapegroup',
            'shape_01' : {
                'type' : 'instance',
                'shapegroup' : {
                    'type' : 'shapegroup',
                    'shape_01' : { 'type' : 'sphere', },
                }
            },
        })

    error = "Nested ShapeGroup is not permitted"
    with pytest.raises(RuntimeError, match='.*{}.*'.format(error)):
        xml.load_dict({
            'type' : 'shapegroup',
            'shape_01' : {
                'type' : 'shapegroup',
                'shape' : { 'type' : 'sphere' },
            },
        })

    error = "Instancing of emitters is not supported"
    with pytest.raises(RuntimeError, match='.*{}.*'.format(error)):
        xml.load_dict({
            'type' : 'shapegroup',
            'shape_01' : {
                'type' : 'sphere',
                'emitter' : { 'type' : 'area' }
            },
        })

    error = "Instancing of sensors is not supported"
    with pytest.raises(RuntimeError, match='.*{}.*'.format(error)):
        xml.load_dict({
            'type' : 'shapegroup',
            'shape_01' : {
                'type' : 'sphere',
                'sensor' : { 'type' : 'perspective' }
            },
        })
