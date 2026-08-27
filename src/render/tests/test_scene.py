"""
Tests of scene-level behavior: emitter registration and sampling weights,
shape downcasting, gradient tracking queries, acceleration structure edge
cases, and object lifetime.
"""

import numpy as np
import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


@fresolver_append_path
def test01_emitter_checks(variant_scalar_rgb):
    """Emitters register once each, whether nested in a shape or
    referenced. Attaching one area emitter to two shapes raises an
    error."""
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

    # Area emitter specified in a shape
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
def test02_shapes_downcast(variant_scalar_rgb):
    """Scene::shapes() downcasts each Shape to a Mesh where possible."""
    scene = mi.load_dict({
        "type": "scene",
        "box": {
            "type": "obj",
            "filename": "resources/data/tests/obj/cbox_smallbox.obj"
        },
        "sphere": {"type": "sphere"}
    })

    shapes = scene.shapes()
    assert len(shapes) == 2
    assert sum(type(s) == mi.Mesh for s in shapes) == 1
    assert sum(type(s) == mi.Shape for s in shapes) == 1


@fresolver_append_path
def test03_shapes_parameters_grad_enabled(variants_all_ad_rgb):
    """shapes_grad_enabled() reacts to gradients on shape parameters but
    not on BSDF parameters."""
    scene = mi.load_dict({
        "type": "scene",
        "box": {
            "type": "obj",
            "filename": "resources/data/tests/obj/cbox_smallbox.obj"
        },
        "sphere": {"type": "sphere"}
    })

    # The initial scene should always return False
    assert scene.shapes_grad_enabled() == False

    # Only parameters of the shape should affect the result of that method
    params = mi.traverse(scene)
    dr.enable_grad(params['box.bsdf.reflectance.value'])
    params.update()
    assert scene.shapes_grad_enabled() == False

    # Requiring gradients on one of the shape's parameters flips it to True
    dr.enable_grad(params['box.positions'])
    params.update()
    assert scene.shapes_grad_enabled() == True


@fresolver_append_path
@pytest.mark.parametrize("shadow", [True, False])
def test04_scene_destruction_and_pending_raytracing(variants_vec_rgb, shadow):
    """A pending ray tracing operation keeps the scene alive after the
    last Python reference disappears."""
    from mitsuba import ScalarTransform4f as T

    # Create and raytrace the scene in a function, so that the scene object
    # gets destroyed (attempt) when leaving the function call
    def render():
        scene = mi.load_dict({
            'type': 'scene',
            'integrator': {'type': 'path'},
            'mysensor': {
                'type': 'perspective',
                'to_world': T().look_at(origin=[0, 0, 3], target=[0, 0, 0],
                                        up=[0, 1, 0]),
                'myfilm': {
                    'type': 'hdrfilm',
                    'rfilter': {'type': 'box'},
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

    # The scene would be garbage collected if it weren't "attached" to the
    # pending ray tracing operation
    import gc
    gc.collect()
    gc.collect()

    dr.eval(pi)


@pytest.mark.parametrize("weights", [[1.0, 1.0, 1.0], [1.3, 3.8, 0.0]])
def test05_emitter_pdf(variants_all_backends_once, weights):
    """pdf_emitter() reflects the sampling weights of the scene's
    emitters."""
    scene = mi.load_dict({
        'type': 'scene',
        'shape': {'type': 'sphere',
                  'emitter': {'type': 'area', 'sampling_weight': weights[0]}},
        'emitter_0': {'type': 'point', 'sampling_weight': weights[1]},
        'emitter_1': {'type': 'constant', 'sampling_weight': weights[2]},
    })
    weights = [emitter.sampling_weight() for emitter in scene.emitters()]
    pdf = np.array(weights) / np.sum(weights)
    for i in range(3):
        dr.assert_allclose(scene.pdf_emitter(i), pdf[i])


@pytest.mark.parametrize("weights", [[1.0, 1.0, 1.0], [1.3, 3.8, 0.0]])
def test06_emitter_sampling(variants_all_backends_once, weights):
    """sample_emitter() agrees with a DiscreteDistribution over the
    emitters' sampling weights."""
    sample = 0.75
    scene = mi.load_dict({
        'type': 'scene',
        'shape': {'type': 'sphere',
                  'emitter': {'type': 'area', 'sampling_weight': weights[0]}},
        'emitter_0': {'type': 'point', 'sampling_weight': weights[1]},
        'emitter_1': {'type': 'constant', 'sampling_weight': weights[2]},
    })
    index, weight, reused_sample = scene.sample_emitter(sample)
    distr = mi.DiscreteDistribution(
        [emitter.sampling_weight() for emitter in scene.emitters()])
    ref_index, ref_reused_sample, ref_pmf = distr.sample_reuse_pmf(sample)
    dr.assert_allclose(index, ref_index)
    dr.assert_allclose(weight, 1.0 / ref_pmf)
    dr.assert_allclose(reused_sample, ref_reused_sample)


def test07_emitter_weight_update(variants_all_backends_once):
    """A sampling_weight edit rebuilds the emitter distribution."""
    scene = mi.load_dict({
        'type': 'scene',
        'emitter_0': {'type': 'point', 'sampling_weight': 2.0},
        'emitter_1': {'type': 'constant', 'sampling_weight': 1.0},
        'emitter_2': {'type': 'directional', 'sampling_weight': 0.5},
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
    dr.assert_allclose(index, ref_index)
    dr.assert_allclose(weight, 1.0 / ref_pmf)
    dr.assert_allclose(reused_sample, ref_reused_sample)

    pdf = np.array(weights) / np.sum(weights)
    for i in range(3):
        dr.assert_allclose(scene.pdf_emitter(i), pdf[i])


def test08_scene_bbox_update(variant_scalar_rgb):
    """Moving a shape updates the scene bounding box."""
    scene = mi.load_dict({
        'type': 'scene',
        'sphere': {'type': 'sphere'}
    })

    bbox = scene.bbox()
    params = mi.traverse(scene)
    offset = [-1, -1, -1]
    params['sphere.to_world'] = mi.Transform4f().translate(offset)
    params.update()

    expected = mi.BoundingBox3f(bbox.min + offset, bbox.max + offset)
    assert expected == scene.bbox()


@pytest.mark.parametrize("flags", ["interior", "perimeter", "all"])
def test09_scene_silhouette_bijective(variants_all_ad_rgb, flags):
    """Scene-level silhouette sampling over mixed analytic shapes inverts
    back to the input sample values. Sampling with AllTypes re-warps the
    sample for the type choice, so only single-type queries invert."""
    scene = mi.load_dict({
        'type': 'scene',
        'sphere': {'type': 'sphere'},
        'rectangle': {'type': 'rectangle'},
        'cylinder': {'type': 'cylinder'}
    })

    # Make sure every shape is being differentiated
    params = mi.traverse(scene)
    for key in ('sphere.to_world', 'rectangle.to_world',
                'cylinder.to_world'):
        dr.enable_grad(params[key])
    params.update()

    x = dr.linspace(mi.Float, 1e-6, 1 - 1e-6, 3)
    y = dr.linspace(mi.Float, 1e-6, 1 - 1e-6, 2)
    z = dr.linspace(mi.Float, 1e-6, 1 - 1e-6, 2)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    flag = {"interior": mi.DiscontinuityFlags.InteriorType,
            "perimeter": mi.DiscontinuityFlags.PerimeterType,
            "all": mi.DiscontinuityFlags.AllTypes}[flags]
    ss = scene.sample_silhouette(samples, flag)
    if flags == "all":
        assert dr.all(ss.discontinuity_type !=
                      mi.DiscontinuityFlags.Empty.value)
        return

    out = scene.invert_silhouette_sample(ss)
    valid = ss.is_valid()
    idx = dr.arange(mi.UInt32, dr.width(ss))
    valid_samples = dr.gather(mi.Point3f, samples, idx, valid)
    valid_out = dr.gather(mi.Point3f, out, idx, valid)
    dr.assert_allclose(valid_samples, valid_out, atol=1e-6)


def test10_embree_robust_flag(variants_any_llvm):
    """Rays that graze the shared edge of two triangles only count as hits
    with embree_use_robust_intersections enabled."""
    R = mi.Transform4f().rotate(dr.normalize(mi.Vector3f(1, 1, 1)), 90)
    vertices = mi.Vector3f(
        [0.0, 1.0, 0.0, 1.0], [0.0, 0.0, 1.0, 1.0], [0.0, 0.0, 0.0, 0.0])
    vertices = R @ vertices

    mesh = mi.Mesh("MyMesh")
    mesh.from_fields(
        faces=[[0, 1, 2], [1, 3, 2]], positions=np.array(vertices).T)

    u, v = dr.meshgrid(dr.linspace(mi.Float, 0.05, 0.95, 32),
                       dr.linspace(mi.Float, 0.05, 0.95, 32))
    d = mi.warp.square_to_cosine_hemisphere(mi.Vector2f(u, v))
    ray = R @ mi.Ray3f(mi.Point3f(0.5, 0.5, 0.0) + d, -d)

    scene = mi.load_dict({'type': 'scene', 'mesh': mesh})
    assert dr.any(~scene.ray_intersect(ray).is_valid())

    scene = mi.load_dict({'type': 'scene', 'mesh': mesh,
                          'embree_use_robust_intersections': True})
    assert dr.all(scene.ray_intersect(ray).is_valid())


@fresolver_append_path
def test11_object_multiple_python_repr(variants_vec_rgb):
    """One Object* can be reached through Python objects of different
    types, e.g. as a Mesh via shapes() and as a Shape via shapes_dr()."""
    scene = mi.load_dict({
        "type": "scene",
        "box": {
            "type": "obj",
            "filename": "resources/data/tests/obj/cbox_smallbox.obj"
        },
    })

    box_as_shape = scene.shapes_dr()[0]
    box_as_mesh = scene.shapes()[0]

    assert type(box_as_mesh) == mi.Mesh
    assert type(box_as_shape) == mi.Shape


def test12_many_top_level_analytic_shapes(variants_vec_backends_once_rgb):
    """Same-kind top-level analytic shapes share a BLAS on GPU backends,
    yet si.shape must recover each instance individually."""
    from mitsuba import ScalarTransform4f as T

    n = 8
    d = {'type': 'scene'}
    # Two interleaved kinds (spheres + disks) so the top level builds more
    # than one per-kind BLAS; each shape sits at a distinct x
    for i in range(n):
        x = float(i) - n / 2
        if i % 2 == 0:
            d[f'shape_{i}'] = {'type': 'sphere', 'radius': 0.3,
                               'to_world': T().translate([x, 0, 0])}
        else:
            d[f'shape_{i}'] = {'type': 'disk',
                               'to_world': T().translate([x, 0, 0])
                               @ T().scale(0.3)}
    scene = mi.load_dict(d)
    shapes = scene.shapes()
    assert len(shapes) == n

    cx = dr.arange(mi.Float, n) - n / 2
    ray = mi.Ray3f(o=mi.Point3f(cx, 0, -10), d=mi.Vector3f(0, 0, 1),
                   time=0.0, wavelengths=[])
    si = scene.ray_intersect(ray)

    assert dr.all(si.is_valid())
    dr.assert_allclose(si.p.x, cx, atol=1e-3)
    # Top-level hits carry no instance
    assert dr.all(si.instance == dr.zeros(mi.ShapePtr))
    # The hit shape recovered for ray i must be exactly the i-th shape
    for i in range(n):
        got = dr.gather(mi.ShapePtr, si.shape, mi.UInt32(i))
        assert dr.all(got == shapes[i])
