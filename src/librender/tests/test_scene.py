import pytest

import enoki as ek
import mitsuba
from mitsuba.python.test.util import fresolver_append_path


@fresolver_append_path
def test01_emitter_checks(variant_scalar_rgb):
    from mitsuba.core.xml import load_string

    def check_scene(xml, count, error=None):
        xml = """<scene version="2.0.0">
            {}
        </scene>""".format(xml)
        if error is None:
            scene = load_string(xml)
            assert len(scene.emitters()) == count
        else:
            with pytest.raises(RuntimeError, match='.*{}.*'.format(error)):
                load_string(xml)

    shape_xml = """<shape type="obj">
        <string name="filename" value="resources/data/tests/obj/rectangle_uv.obj"/>
        {}
    </shape>"""


    # Environment emitter
    check_scene('<emitter type="constant"/>', 1)

    # Normal area emitter specified in a shape
    check_scene(shape_xml.format('<emitter type="area"/>'), 1)

    # Area emitter specified at top level, then referenced once
    check_scene('<emitter type="area" id="my_emitter"/>'
                + shape_xml.format('<ref id="my_emitter"/>'), 1)

    # Area emitter specified at top level, then referenced twice
    check_scene('<emitter type="area" id="my_emitter"/>'
                + shape_xml.format('<ref id="my_emitter"/>')
                + shape_xml.format('<ref id="my_emitter"/>'), 2,
                error="can be only be attached to a single shape")

    # Environment emitter, point light (top level), and area emitters
    check_scene('<emitter type="constant"/>'
                + '<emitter type="point"/>'
                + '<emitter type="area" id="my_emitter"/>'
                + shape_xml.format('<emitter type="area" id="my_inner_emitter"/>')
                + shape_xml.format('<ref id="my_emitter"/>'), 4)

@fresolver_append_path
def test02_shapes(variant_scalar_rgb):
    from mitsuba.core.xml import load_string

    """Tests that a Shape is downcasted to a Mesh in the Scene::shapes method if possible"""
    scene = load_string("""
        <scene version="2.0.0">
            <shape type="obj" >
                <string name="filename" value="resources/data/tests/obj/cbox_smallbox.obj"/>
            </shape>
            <shape type="sphere"/>
        </scene>
    """)

    shapes = scene.shapes()
    assert len(shapes) == 2
    assert sum(type(s) == mitsuba.render.Mesh  for s in shapes) == 1
    assert sum(type(s) == mitsuba.render.Shape for s in shapes) == 1


@fresolver_append_path
def test03_shapes_parameters_grad_enabled(variant_gpu_autodiff_rgb):
    from mitsuba.core.xml import load_string
    from mitsuba.python.util import traverse

    scene = load_string("""
        <scene version="2.0.0">
            <shape type="obj" id="box">
                <string name="filename" value="resources/data/tests/obj/cbox_smallbox.obj"/>
            </shape>
            <shape type="sphere"/>
        </scene>
    """)

    # Initial scene should always return False
    assert scene.shapes_grad_enabled() == False

    # Get scene parameters
    params = traverse(scene)

    # Only parameters of the shape should affect the result of that method
    bsdf_param_key = 'box.bsdf.reflectance.value'
    ek.set_requires_gradient(params[bsdf_param_key])
    params.set_dirty(bsdf_param_key)
    params.update()
    assert scene.shapes_grad_enabled() == False

    # When setting one of the shape's param to require gradient, method should return True
    shape_param_key = 'box.vertex_positions_buf'
    ek.set_requires_gradient(params[shape_param_key])
    params.set_dirty(shape_param_key)
    params.update()
    assert scene.shapes_grad_enabled() == True
