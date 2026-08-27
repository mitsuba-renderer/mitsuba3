"""
Tests mesh differentiation: derivatives of surface interactions with
respect to rays and mesh parameters, the FollowShape/DetachShape
semantics, and handling of discontinuous attributes.
"""

import numpy as np
import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path, seam_quad, \
    curved_patch


@fresolver_append_path
def rect_scene():
    return mi.load_dict({
        "type": "scene",
        "meshes": {
            "type": "obj",
            "id": "rect",
            "filename": "resources/data/common/meshes/rectangle.obj",
        }
    })


def test01_attach_automatic(variants_all_ad_rgb):
    """Surface interactions are attached exactly when the ray or the shape
    parameters carry gradients. Only the shape's own parameters count:
    gradients on its BSDF do not attach anything."""
    scene = rect_scene()
    shape = scene.shapes()[0]
    assert not shape.parameters_grad_enabled()

    ray = mi.Ray3f(mi.Vector3f(-0.3, -0.3, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    # Not attached if not necessary
    si = pi.compute_surface_interaction(ray)
    assert not dr.grad_enabled(si.t)
    assert not dr.grad_enabled(si.p)

    # Attached if the ray is attached
    dr.enable_grad(ray.o)
    si = pi.compute_surface_interaction(ray)
    assert dr.grad_enabled(si.t)
    assert dr.grad_enabled(si.p)
    assert not dr.grad_enabled(si.n)  # face normal does not depend on the ray

    # Still attached to the ray under DetachShape
    si = pi.compute_surface_interaction(ray,
                                        mi.RayFlags.Default |
                                        mi.RayFlags.DetachShape)
    assert dr.grad_enabled(si.p)
    assert dr.grad_enabled(si.uv)
    assert not dr.grad_enabled(si.n)

    # Attached if the shape parameters are attached, which the parameters
    # of its BSDF are not
    params = mi.traverse(scene)
    for key, expected in [('rect.bsdf.reflectance.value', False),
                          ('rect.positions', True)]:
        dr.enable_grad(params[key])
        params.set_dirty(key)
        params.update()
        assert shape.parameters_grad_enabled() == expected

    dr.disable_grad(ray.o)
    si = pi.compute_surface_interaction(ray)
    assert dr.grad_enabled(si.t)
    assert dr.grad_enabled(si.p)


def test02_ray_derivatives(variants_all_ad_rgb):
    """Derivatives with respect to the ray origin and direction match the
    analytic values, in both traversal directions."""
    scene = rect_scene()
    ray = mi.Ray3f(mi.Vector3f(-0.3, -0.4, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    dr.enable_grad(ray.o)
    dr.enable_grad(ray.d)

    for param, field, expected in [
            # Shifting the ray origin along x moves si.p along x, and si.uv
            # by half the rectangle's parameterization
            (ray.o.x, lambda si: si.p, [1, 0, 0]),
            (ray.o.x, lambda si: si.uv, [0.5, 0]),
            # Shifting the origin along z shortens si.t
            (ray.o.z, lambda si: si.t, -1),
            # Tilting the direction along x moves si.p by the flight distance
            (ray.d.x, lambda si: si.p, [10, 0, 0])]:
        si = pi.compute_surface_interaction(ray)
        dr.forward(param)
        dr.assert_allclose(dr.grad(field(si)), expected)

    # The same relations seen from the other end
    for output, expected in [(lambda si: si.p.x, [1, 0, 0]),
                             (lambda si: si.t, [0, 0, -1])]:
        dr.set_grad(ray.o, 0.0)
        si = pi.compute_surface_interaction(ray)
        dr.backward(output(si))
        dr.assert_allclose(dr.grad(ray.o), expected)


def test03_params_forward(variants_all_ad_rgb):
    """Forward derivatives with respect to transformed vertex positions
    match the analytic values."""
    scene = rect_scene()
    params = mi.traverse(scene)
    key = 'rect.positions'
    positions_initial = mi.Point3f(params[key], flip_axes=True)

    diff_vector = mi.Vector3f(0.0)
    dr.enable_grad(diff_vector)

    def apply_transformation(trafo):
        trafo = trafo(diff_vector)
        params[key] = mi.TensorXf(trafo @ positions_initial, flip_axes=True)
        params.set_dirty(key)
        params.update()

    ray = mi.Ray3f(mi.Vector3f(-0.2, -0.3, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    # A translation along z moves si.t and si.p
    apply_transformation(lambda v: mi.Transform4f().translate(v))
    si = pi.compute_surface_interaction(ray)
    dr.forward(diff_vector.z)
    dr.assert_allclose(dr.grad(si.t), 1)
    dr.assert_allclose(dr.grad(si.p), [0, 0, 1])

    # Translations in the plane move si.uv by half a unit in the
    # opposite direction
    for axis, uv_grad in [('x', [-0.5, 0]), ('y', [0, -0.5])]:
        apply_transformation(lambda v: mi.Transform4f().translate(v))
        si = pi.compute_surface_interaction(ray)
        dr.forward(getattr(diff_vector, axis))
        dr.assert_allclose(dr.grad(si.uv), uv_grad, atol=1e-6)

    # A rotation about z moves si.uv of a corner hit tangentially
    ray = mi.Ray3f(mi.Vector3f(-0.99999, -0.99999, -10.0),
                   mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    apply_transformation(lambda v: mi.Transform4f().rotate([0, 0, 1], v.x))
    si = pi.compute_surface_interaction(ray)
    dr.forward(diff_vector.x)
    du = 0.5 * dr.sin(2 * dr.pi / 360.0)
    dr.assert_allclose(dr.grad(si.uv), [-du, du], atol=1e-6)


def test04_params_backward(variants_all_ad_rgb):
    """Backward derivatives of surface interaction fields produce the
    expected gradients on the position and texture coordinate tensors."""
    scene = rect_scene()
    params = mi.traverse(scene)
    pos_key, uv_key = 'rect.positions', 'rect.texcoords'
    dr.enable_grad(params[pos_key])
    dr.enable_grad(params[uv_key])
    params.set_dirty(pos_key)
    params.set_dirty(uv_key)
    params.update()

    # Hit the upper right corner of the rectangle (the 4th vertex)
    ray = mi.Ray3f(mi.Vector3f(0.99999, 0.99999, -10.0),
                   mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    def compute_si():
        # Rebuild the parameter -> mesh edges consumed by the previous
        # backward pass, mirroring an optimization loop
        params.set_dirty(pos_key)
        params.set_dirty(uv_key)
        params.update()
        return pi.compute_surface_interaction(ray)

    # (output, expected positions gradient)
    pos_cases = [
        # If si.t changes, the 4th vertex should move along the z-axis
        (lambda si: si.t, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1]),
        # If si.p moves along the z-axis, so does the 4th vertex
        (lambda si: si.p.z, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1]),
        # To increase si.dp_du along x, stretch the top edge
        (lambda si: si.dp_du.x, [0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0]),
        # To increase si.dp_du along y, shear the rectangle into a trapezoid
        (lambda si: si.dp_du.y, [0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0]),
        # To increase si.dp_dv along x, shear the rectangle into a trapezoid
        (lambda si: si.dp_dv.x, [0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0]),
        # To increase si.dp_dv along y, stretch the right edge
        (lambda si: si.dp_dv.y, [0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0]),
        # To increase si.n along x, rotate the right edge around the y-axis
        (lambda si: si.n.x, [0, 0, 0, 0, 0, 0, 0, 0, 0.5, 0, 0, -0.5]),
        # To increase si.n along y, rotate the top edge around the x-axis
        (lambda si: si.n.y, [0, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 0, -0.5]),
        # The shading normal responds like the geometric one
        (lambda si: si.sh_frame.n.x,
         [0, 0, 0, 0, 0, 0, 0, 0, 0.5, 0, 0, -0.5]),
        (lambda si: si.sh_frame.n.y,
         [0, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 0, -0.5]),
    ]
    for output, expected in pos_cases:
        dr.set_grad(params[pos_key], 0.0)
        si = compute_si()
        dr.backward(output(si))
        dr.assert_allclose(dr.ravel(dr.grad(params[pos_key])), expected,
                           atol=1e-5)

    uv_cases = [
        # To increase si.uv along x/y, move the uv of the 4th vertex the
        # same way
        (lambda si: si.uv.x, [0, 0, 0, 0, 0, 0, 1, 0]),
        (lambda si: si.uv.y, [0, 0, 0, 0, 0, 0, 0, 1]),
        # To increase si.dp_du along x, shrink the uv along the top edge
        (lambda si: si.dp_du.x, [0, 0, 0, 0, 2, 0, -2, 0]),
        # To increase si.dp_dv along y, shrink the uv along the right edge
        (lambda si: si.dp_dv.y, [0, 0, 0, 2, 0, 0, 0, -2]),
    ]
    for output, expected in uv_cases:
        dr.set_grad(params[uv_key], 0.0)
        si = compute_si()
        dr.backward(output(si))
        dr.assert_allclose(dr.ravel(dr.grad(params[uv_key])), expected,
                           atol=1e-5)


def test05_follow_and_detach_shape(variants_all_ad_rgb):
    """With FollowShape, si.p sticks to the moving surface while si.uv
    stays put. Without it, si.p stays on the ray and si.uv slides. With
    DetachShape the surface does not move at all."""
    scene = rect_scene()
    params = mi.traverse(scene)
    key = 'rect.positions'
    positions_initial = mi.Point3f(params[key], flip_axes=True)

    diff_vector = mi.Vector3f(0.0)
    dr.enable_grad(diff_vector)

    def apply_translation():
        params[key] = mi.TensorXf(
            mi.Transform4f().translate(diff_vector) @ positions_initial,
            flip_axes=True)
        params.set_dirty(key)
        params.update()

    ray = mi.Ray3f(mi.Vector3f(0.2, 0.3, -5.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    cases = [
        (mi.RayFlags.Default, [0, 0, 0], [-0.5, 0]),
        (mi.RayFlags.Default | mi.RayFlags.FollowShape, [1, 0, 0], [0, 0]),
        (mi.RayFlags.Default | mi.RayFlags.DetachShape, [0, 0, 0], [0, 0]),
    ]
    for flags, p_grad, uv_grad in cases:
        apply_translation()
        si = pi.compute_surface_interaction(ray, flags)
        dr.forward(diff_vector.x)
        dr.assert_allclose(dr.grad(si.p), p_grad, atol=1e-5)

        apply_translation()
        si = pi.compute_surface_interaction(ray, flags)
        dr.forward(diff_vector.x)
        dr.assert_allclose(dr.grad(si.uv), uv_grad, atol=1e-5)

    # Derivatives with respect to the ray survive DetachShape, and match
    # what a rectangle that was never attached would have produced
    apply_translation()
    dr.enable_grad(ray.o)
    si = pi.compute_surface_interaction(ray, mi.RayFlags.Default |
                                        mi.RayFlags.DetachShape)
    dr.forward(ray.o.x)
    dr.assert_allclose(dr.grad(si.p), [1, 0, 0], atol=1e-5)
    dr.assert_allclose(dr.grad(si.uv), [0.5, 0], atol=1e-5)


@fresolver_append_path
@pytest.mark.parametrize("wall", [False, True])
def test06_follow_shape_curved(variants_all_ad_rgb, wall):
    """FollowShape through scene.ray_intersect() on a curved mesh: si.p and
    si.t track the moving sphere while its shading normal is invariant under
    the motion. Hits on the static wall stay put."""
    scene_dict = {
        'type': 'scene',
        'sphere': {'type': 'obj', 'id': 'sphere',
                   'filename': 'resources/data/common/meshes/sphere.obj'}}
    if wall:
        scene_dict['wall'] = {
            'type': 'obj', 'id': 'wall',
            'filename': 'resources/data/common/meshes/cbox/back.obj'}
    scene = mi.load_dict(scene_dict)

    params = mi.traverse(scene)
    key = 'sphere.positions'
    positions_initial = mi.Point3f(params[key], flip_axes=True)

    def translate(v):
        """Attach the sphere vertices to a differential translation"""
        theta = mi.Float(0.0)
        dr.enable_grad(theta)
        params[key] = mi.TensorXf(
            mi.Transform4f().translate(mi.Vector3f(v) * theta)
            @ positions_initial, flip_axes=True)
        params.update()
        return theta

    # A wavefront of rays, one per pixel of a small film
    res = 7
    i = dr.arange(mi.UInt32, res * res)
    pos = mi.Vector2f(i % res, i // res) * (2.0 / (res - 1)) - 1.0
    ray = mi.Ray3f(mi.Point3f(pos.x, pos.y, -5), mi.Vector3f(0, 0, 1))
    follow = mi.RayFlags.Default | mi.RayFlags.FollowShape

    # Sideways, the hit point follows the sphere
    theta = translate([0, 1, 0])
    si = scene.ray_intersect(ray, follow, True)
    dr.forward(theta)
    hit_sphere = si.t < 6.0
    dr.eval(hit_sphere)

    dr.assert_allclose(dr.grad(si.p), dr.select(
        hit_sphere, mi.Vector3f(0, 1, 0), mi.Vector3f(0, 0, 0)))

    # Along the ray, si.t follows it
    theta = translate([0, 0, 1])
    si = scene.ray_intersect(ray, follow, True)
    dr.forward(theta)
    dr.assert_allclose(dr.grad(si.t), dr.select(hit_sphere, 1.0, 0.0),
                       atol=1e-5)

    # A followed point keeps its shading normal as it moves across the
    # curvature. forward_to() rather than dr.forward(): the default
    # traversal only keeps gradients of terminal variables, and sh_frame.n
    # is consumed by finalize_surface_interaction().
    theta = translate([1, 0, 0])
    si = scene.ray_intersect(ray, follow, True)
    dr.set_grad(theta, 1.0)
    dr.forward_to(si.sh_frame.n)
    dr.assert_allclose(dr.grad(si.sh_frame.n), 0, atol=1e-5)


def test07_normal_sliding_gradient(variants_all_ad_rgb):
    """Without FollowShape, the interaction slides across the surface, so
    the shading normal of a curved mesh picks up a derivative. It is the
    normal partials driven by the sliding texture coordinates."""
    scene = mi.load_dict({'type': 'scene', 'm': curved_patch()[0]})
    params = mi.traverse(scene)

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    params['m.positions'] += mi.TensorXf(mi.Vector3f(theta, 0, 0),
                                         flip_axes=True)
    params.update()

    ray = mi.Ray3f(mi.Point3f(0.7, 0.3, 1), mi.Vector3f(0, 0, -1))
    si = scene.ray_intersect(
        ray, mi.RayFlags.Default | mi.RayFlags.NormalPartials, True)

    dr.set_grad(theta, 1.0)
    dr.forward_to(si.sh_frame.n, si.uv)
    duv = dr.grad(si.uv)
    assert dr.any(dr.abs(duv) > 0.1)

    # The chain rule through the parameterization reproduces the derivative
    dr.assert_allclose(dr.grad(si.sh_frame.n),
                       dr.detach(duv.x * si.dn_du + duv.y * si.dn_dv),
                       atol=1e-5)


@fresolver_append_path
def test08_differential_motion(variants_vec_rgb):
    """differential_motion() returns si.p in the primal and the surface
    velocity under parameter perturbations in the derivative."""
    if not dr.is_diff_v(mi.Float):
        pytest.skip("Only relevant in AD-enabled variants!")

    mesh = mi.load_dict({
        "type": "ply",
        "filename": "resources/data/common/meshes/bunny_lowres.ply",
    })
    params = mi.traverse(mesh)

    theta = mi.Point3f(0.0)
    dr.enable_grad(theta)
    key = 'positions'
    positions = mi.Point3f(params[key], flip_axes=True)
    translation = mi.Transform4f().translate(
        [theta.x, 2 * theta.y, 3 * theta.z])
    params[key] = mi.TensorXf(translation @ positions, flip_axes=True)
    params.update()

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.prim_index = 0
    si.p = mi.Point3f(1, 0, 0)  # doesn't matter
    si.uv = mi.Point2f(0.5, 0.5)

    p_diff = mesh.differential_motion(si)
    dr.forward(theta)
    v = dr.grad(p_diff)

    dr.assert_allclose(p_diff, si.p)
    dr.assert_allclose(v, [1.0, 2.0, 3.0])


# -------------------------------------------------------------------
# Seam behavior of the coarse parameter levels
# -------------------------------------------------------------------

@pytest.mark.parametrize("level", ["positions", "normals"])
def test09_seam_gradient_aggregation(variants_all_ad_rgb, level):
    """Backward gradients from hits on both sides of a UV seam sum on the
    shared rows of the coarse levels, reached through their index maps."""
    scene = mi.load_dict({'type': 'scene', 'm': seam_quad()})
    params = mi.traverse(scene)
    key = 'm.' + level
    assert params[key].shape == (4, 3)  # N == P for generated normals
    dr.enable_grad(params[key])
    params.set_dirty(key)
    params.update()

    # One ray per triangle, both near the seam diagonal. si.t depends on
    # the vertex z coordinates through the barycentric weights, and the
    # shading normal on the normal rows through the same weights.
    ray = mi.Ray3f(mi.Point3f([0.6, 0.4], [0.4, 0.6], [1, 1]),
                   mi.Vector3f(0, 0, -1))
    si = scene.ray_intersect(ray)
    assert dr.all(si.is_valid())
    dr.backward(dr.sum(si.t if level == "positions" else si.sh_frame.n.x))

    # Each coarse row accumulates the interpolation weights of both hits.
    # For positions that lands on z (dt/dz is negative), for normals on x
    # since all normals are (0, 0, 1).
    expected = ([0, 0, -0.8, 0, 0, -0.2, 0, 0, -0.8, 0, 0, -0.2]
                if level == "positions" else
                [0.8, 0, 0, 0.2, 0, 0, 0.8, 0, 0, 0.2, 0, 0])
    dr.assert_allclose(dr.ravel(dr.grad(params[key])), expected, atol=1e-5)


def test10_seam_forward_no_tearing(variants_all_ad_rgb):
    """Perturbing a shared surface position moves the hit points of both
    incident triangles coherently. The seam does not tear under optimization."""
    m = seam_quad()
    scene = mi.load_dict({'type': 'scene', 'm': m})
    params = mi.traverse(scene)
    key = 'm.positions'

    # Lift only coarse position 0, which both triangles reference through
    # different vertices
    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    dz = dr.select(dr.arange(mi.UInt32, 4) == 0, theta, mi.Float(0))
    params[key] += mi.TensorXf(mi.Vector3f(0, 0, dz), flip_axes=True)
    params.update()

    # One ray per triangle; the surface position 0 has interpolation
    # weight 0.4 at both hit points
    ray = mi.Ray3f(mi.Point3f([0.6, 0.4], [0.4, 0.6], [1, 1]),
                   mi.Vector3f(0, 0, -1))
    si = scene.ray_intersect(ray)
    assert dr.all(si.is_valid())
    assert dr.all(si.prim_index == mi.UInt32(0, 1))
    dr.forward(theta)
    dr.assert_allclose(dr.grad(si.p).z, 0.4, atol=1e-5)


def test11_from_fields_grads(variants_all_ad_rgb):
    """Derivatives propagate between the tensors passed to from_fields()
    and the constructed mesh."""
    positions = mi.TensorXf(np.float32([[0, 0, 0], [1, 0, 0], [0, 1, 0]]))
    dr.enable_grad(positions)

    m = mi.Mesh("tri")
    m.from_fields(faces=[[0, 1, 2]], positions=positions)
    assert m.parameters_grad_enabled()

    scene = mi.load_dict({'type': 'scene', 'm': m})
    ray = mi.Ray3f(mi.Point3f(0.25, 0.25, 1), mi.Vector3f(0, 0, -1))
    si = scene.ray_intersect(ray)
    assert dr.all(si.is_valid())
    dr.backward(dr.sum(si.t))
    dr.assert_allclose(dr.ravel(dr.grad(positions)),
                       [0, 0, -0.5, 0, 0, -0.25, 0, 0, -0.25], atol=1e-5)
