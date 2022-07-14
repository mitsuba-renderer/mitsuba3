import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


@fresolver_append_path
def test01_emitter_checks(variant_scalar_rgb):
    def check_scene(xml, count, error=None):
        xml = """<scene version="3.0.0">
            {}
        </scene>""".format(xml)
        if error is None:
            scene = mi.load_string(xml)
            assert len(scene.emitters()) == count
        else:
            with pytest.raises(RuntimeError, match='.*{}.*'.format(error)):
                mi.load_string(xml)

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
    """Tests that a Shape is downcasted to a Mesh in the Scene::shapes method if possible"""
    scene = mi.load_dict({
        "type" : "scene",
        "box" :  {
            "type" : "obj",
            "filename" : "resources/data/tests/obj/cbox_smallbox.obj"
        },
        "sphere" : {
            "type" : "sphere"
        }
    })

    shapes = scene.shapes()
    assert len(shapes) == 2
    assert sum(type(s) == mi.Mesh  for s in shapes) == 1
    assert sum(type(s) == mi.Shape for s in shapes) == 1


@fresolver_append_path
def test03_shapes_parameters_grad_enabled(variant_cuda_ad_rgb):
    scene = mi.load_dict({
        "type" : "scene",
        "box" :  {
            "type" : "obj",
            "filename" : "resources/data/tests/obj/cbox_smallbox.obj"
        },
        "sphere" : {
            "type" : "sphere"
        }
    })

    # Initial scene should always return False
    assert scene.shapes_grad_enabled() == False

    # Get scene parameters
    params = mi.traverse(scene)

    # Only parameters of the shape should affect the result of that method
    bsdf_param_key = 'box.bsdf.reflectance.value'
    dr.enable_grad(params[bsdf_param_key])
    params.set_dirty(bsdf_param_key)
    params.update()
    assert scene.shapes_grad_enabled() == False

    # When setting one of the shape's param to require gradient, method should return True
    shape_param_key = 'box.vertex_positions'
    dr.enable_grad(params[shape_param_key])
    params.set_dirty(shape_param_key)
    params.update()
    assert scene.shapes_grad_enabled() == True


@fresolver_append_path
@pytest.mark.parametrize("shadow", [True, False])
def test04_scene_destruction_and_pending_raytracing(variants_vec_rgb, shadow):
    from mitsuba import ScalarTransform4f as T

    # Create and raytrace scene in a function, so that the scene object gets
    # destroyed (attempt) when leaving the function call
    def render():
        scene = mi.load_dict({
            'type': 'scene',
            'integrator': { 'type': 'path' },
            'mysensor': {
                'type': 'perspective',
                'to_world': T.look_at(origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0]),
                'myfilm': {
                    'type': 'hdrfilm',
                    'rfilter': { 'type': 'box'},
                    'width': 4,
                    'height': 4,
                    'pixel_format': 'rgba',

                },
                'mysampler': {
                    'type': 'independent',
                    'sample_count': 4,
                },
            },
            'sphere': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/sphere.obj'
            },
            'emitter': {
                'type': 'point',
                'position': [0, 0, 1]
            }
        })

        ray = mi.Ray3f(mi.Point3f(1, 2, 3), mi.Vector3f(4, 5, 6))
        if shadow:
            return scene.ray_test(ray)
        else:
            return scene.ray_intersect_preliminary(ray, coherent=True)

    pi = render()

    # Ensure scene object would be garbage collected if not "attached" to ray tracing operation
    import gc
    gc.collect()
    gc.collect()

    import drjit as dr
    dr.eval(pi)
