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
    params.update()
    assert scene.shapes_grad_enabled() == False

    # When setting one of the shape's param to require gradient, method should return True
    shape_param_key = 'box.vertex_positions'
    dr.enable_grad(params[shape_param_key])
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
                'to_world': T().look_at(origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0]),
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


def test05_test_uniform_emitter_pdf(variants_all_backends_once):
    scene = mi.load_dict({
        'type': 'scene',
        'shape': {'type': 'sphere', 'emitter': {'type':'area'}},
        'emitter_0': {'type':'point', 'sampling_weight': 1.0},
        'emitter_1': {'type':'constant', 'sampling_weight': 1.0},
    })
    assert dr.allclose(scene.pdf_emitter(0), 1.0 / 3.0)
    assert dr.allclose(scene.pdf_emitter(1), 1.0 / 3.0)
    assert dr.allclose(scene.pdf_emitter(2), 1.0 / 3.0)


def test06_test_nonuniform_emitter_pdf(variants_all_backends_once):
    import numpy as np

    weights = [1.3, 3.8, 0.0]
    scene = mi.load_dict({
        'type': 'scene',
        'shape': {'type': 'sphere', 'emitter': {'type':'area', 'sampling_weight': weights[0]}},
        'emitter_0': {'type':'point', 'sampling_weight': weights[1]},
        'emitter_1': {'type':'constant', 'sampling_weight': weights[2]},
    })
    weights = [emitter.sampling_weight() for emitter in scene.emitters()]
    pdf = np.array(weights) / np.sum(weights)
    assert dr.allclose(scene.pdf_emitter(0), pdf[0])
    assert dr.allclose(scene.pdf_emitter(1), pdf[1])
    assert dr.allclose(scene.pdf_emitter(2), pdf[2])


def test07_test_uniform_emitter_sampling(variants_all_backends_once):
    scene = mi.load_dict({
        'type': 'scene',
        'shape': {'type': 'sphere', 'emitter': {'type':'area', 'sampling_weight': 1.0}},
        'emitter_0': {'type':'point', 'sampling_weight': 1.0},
        'emitter_1': {'type':'constant', 'sampling_weight': 1.0},
    })
    sample = 0.6
    index, weight, reused_sample = scene.sample_emitter(sample)
    assert dr.allclose(index, 1)
    assert dr.allclose(weight, 3.0)
    assert dr.allclose(reused_sample, sample * 3 - 1)


def test08_test_nonuniform_emitter_sampling(variants_all_backends_once):
    sample = 0.75
    weights = [1.3, 3.8, 0.0]
    scene = mi.load_dict({
        'type': 'scene',
        'shape': {'type': 'sphere', 'emitter': {'type':'area', 'sampling_weight': weights[0]}},
        'emitter_0': {'type':'point', 'sampling_weight': weights[1]},
        'emitter_1': {'type':'constant', 'sampling_weight': weights[2]},
    })
    index, weight, reused_sample = scene.sample_emitter(sample)
    distr = mi.DiscreteDistribution([emitter.sampling_weight() for emitter in scene.emitters()])
    ref_index, ref_reused_sample, ref_pmf = distr.sample_reuse_pmf(sample)
    assert dr.allclose(index, ref_index)
    assert dr.allclose(weight, 1.0 / ref_pmf)
    assert dr.allclose(reused_sample, ref_reused_sample)


def test09_test_emitter_sampling_weight_update(variants_all_backends_once):
    import numpy as np

    weights = [2.0, 1.0, 0.5]
    scene = mi.load_dict({
        'type': 'scene',
        'emitter_0': {'type':'point', 'sampling_weight': weights[0]},
        'emitter_1': {'type':'constant', 'sampling_weight': weights[1]},
        'emitter_2': {'type':'directional', 'sampling_weight': weights[2]},
    })

    params = mi.traverse(scene)
    params['emitter_0.sampling_weight'] = 0.8
    params['emitter_1.sampling_weight'] = 0.05
    params['emitter_2.sampling_weight'] = 1.2
    params.update()

    sample = 0.75
    weights = [emitter.sampling_weight() for emitter in scene.emitters()]
    distr = mi.DiscreteDistribution(weights)
    index, weight, reused_sample = scene.sample_emitter(sample)
    ref_index, ref_reused_sample, ref_pmf = distr.sample_reuse_pmf(sample)
    assert dr.allclose(index, ref_index)
    assert dr.allclose(weight, 1.0 / ref_pmf)
    assert dr.allclose(reused_sample, ref_reused_sample)

    pdf = np.array(weights) / np.sum(weights)
    assert dr.allclose(scene.pdf_emitter(0), pdf[0])
    assert dr.allclose(scene.pdf_emitter(1), pdf[1])
    assert dr.allclose(scene.pdf_emitter(2), pdf[2])


def test10_test_scene_bbox_update(variant_scalar_rgb):
    scene = mi.load_dict({
        'type': 'scene',
        "sphere" : {
            "type" : "sphere"
        }
    })

    bbox = scene.bbox()
    params = mi.traverse(scene)
    offset = [-1, -1, -1]
    params['sphere.to_world'] = mi.Transform4f().translate(offset)
    params.update()

    expected = mi.BoundingBox3f(bbox.min + offset, bbox.max + offset)
    assert expected == scene.bbox()


def test11_sample_silhouette_bijective(variants_vec_rgb):
    scene = mi.load_dict({
        'type': 'scene',
        'sphere' : {
            'type' : 'sphere'
        },
        'rectangle' : {
            'type' : 'rectangle'
        },
        'cylinder' : {
            'type' : 'cylinder'
        }
    })

    params = mi.traverse(scene)
    key_sphere = 'sphere.to_world'
    key_rectangle = 'rectangle.to_world'
    key_cylinder = 'cylinder.to_world'

    # Make sure every shape is being differentiated
    dr.enable_grad(params[key_sphere])
    dr.enable_grad(params[key_rectangle])
    dr.enable_grad(params[key_cylinder])
    params.update()

    x = dr.linspace(mi.Float, 1e-6, 1-1e-6, 3)
    y = dr.linspace(mi.Float, 1e-6, 1-1e-6, 2)
    z = dr.linspace(mi.Float, 1e-6, 1-1e-6, 2)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    # Only interior
    ss = scene.sample_silhouette(samples, mi.DiscontinuityFlags.InteriorType)
    out = scene.invert_silhouette_sample(ss)
    valid = ss.is_valid()
    valid_samples = dr.gather(mi.Point3f, samples, dr.arange(mi.UInt32, dr.width(ss)), valid)
    valid_out = dr.gather(mi.Point3f, out, dr.arange(mi.UInt32, dr.width(ss)), valid)
    assert dr.allclose(valid_samples, valid_out, atol=1e-6)

    ## Only perimeter
    ss = scene.sample_silhouette(samples, mi.DiscontinuityFlags.PerimeterType)
    out = scene.invert_silhouette_sample(ss)
    valid = ss.is_valid()
    valid_samples = dr.gather(mi.Point3f, samples, dr.arange(mi.UInt32, dr.width(ss)), valid)
    valid_out = dr.gather(mi.Point3f, out, dr.arange(mi.UInt32, dr.width(ss)), valid)
    assert dr.allclose(valid_samples, valid_out, atol=1e-6)

    # Both types
    ss = scene.sample_silhouette(samples, mi.DiscontinuityFlags.AllTypes)
    out = scene.invert_silhouette_sample(ss)
    assert dr.all(ss.discontinuity_type != mi.DiscontinuityFlags.Empty.value)
    assert dr.allclose(valid_samples, valid_out, atol=1e-6)


def test_enable_embree_robust_flag(variants_any_llvm):

    # We intersect rays against two adjacent triangles. The rays hit exactly
    # the edge between two triangles, which Embree will not count as an
    # intersection if the "robust" flag is not set.
    R = mi.Transform4f().rotate(dr.normalize(mi.Vector3f(1, 1, 1)), 90)
    vertices = mi.Vector3f(
        [0.0, 1.0, 0.0, 1.0], [0.0, 0.0, 1.0, 1.0], [0.0, 0.0, 0.0, 0.0])
    vertices = R @ vertices

    mesh = mi.Mesh("MyMesh", 4, 2)
    params = mi.traverse(mesh)
    params['vertex_positions'] = dr.ravel(vertices)
    params['faces'] = [0, 1, 2, 1, 3, 2]
    params.update()

    u, v = dr.meshgrid(dr.linspace(mi.Float, 0.05, 0.95, 32),
                       dr.linspace(mi.Float, 0.05, 0.95, 32))
    d = mi.warp.square_to_cosine_hemisphere(mi.Vector2f(u, v))
    ray = R @ mi.Ray3f(mi.Point3f(0.5, 0.5, 0.0) + d, -d)

    scene = mi.load_dict({'type': 'scene', 'mesh': mesh})
    assert dr.any(~scene.ray_intersect(ray).is_valid())

    scene = mi.load_dict({'type': 'scene', 'mesh': mesh,
                          'embree_use_robust_intersections': True})
    assert dr.all(scene.ray_intersect(ray).is_valid())
