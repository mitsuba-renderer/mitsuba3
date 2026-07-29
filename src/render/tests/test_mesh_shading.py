"""
Tests of what meshes produce at render time: surface interactions, the
shading frame, UV parameterization queries, and area sampling.

Tangent contract: corners split into vertices by (normal group, UV value,
orientation sign) plus any custom attributes. Each vertex accumulates an
angle-weighted average of normalize(dp/du) over its incident triangles.
compute_surface_interaction() only decodes the tangent frame for BSDFs
carrying the NeedsTangents flag.
"""

import numpy as np
import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path, \
    anisotropic_bsdf, assert_valid_tangent_frame, curved_patch, \
    check_normal_partials, rows, ANISOTROPIC_BSDF


# Probe positions in the unit square, four per face of a curved patch, kept
# clear of the diagonal that the two faces share
PATCH_PROBES = [(0.75, 0.25), (0.92, 0.13), (0.55, 0.40), (0.97, 0.62),
                (0.25, 0.75), (0.13, 0.92), (0.40, 0.55), (0.62, 0.97)]


def probe(scene, xy, flags=mi.RayFlags.Default):
    """
    Fire a ray straight down onto a patch in the unit square. Returns the
    index of the face it hits, the barycentric coordinates of the hit, the
    surface interaction and the ray.
    """
    ray = mi.Ray3f(mi.Point3f(float(xy[0]), float(xy[1]), 5.0),
                   mi.Vector3f(0, 0, -1))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)
    # Pin the traversal so that probes of different scenes cannot end up in
    # one kernel, which a single shader binding table could not serve.
    dr.eval(pi)
    assert dr.all(pi.is_valid())
    return (int(np.array(pi.prim_index).ravel()[0]), rows(pi.prim_uv, 2).T,
            pi.compute_surface_interaction(ray, flags), ray)


# -------------------------------------------------------------------
# Surface interactions
# -------------------------------------------------------------------

def test01_ray_intersect_triangle(variants_all_rgb):
    """ray_intersect_triangle() intersects a single triangle without an
    acceleration structure, and reports the barycentric coordinates of the
    hit in ``prim_uv``."""
    p = np.float64([[0, 0, 0], [1, 0, 0], [0.5, 1, 0]])
    mesh = mi.Mesh('')
    mesh.from_fields(faces=[[0, 1, 2]], positions=np.float32(p))

    def hit(o, d=(0, 0, -1)):
        ray = mi.Ray3f(mi.Point3f(*map(float, o)), mi.Vector3f(d))
        return mesh.ray_intersect_triangle(mi.UInt32(0), ray)

    # prim_uv holds (b1, b2), the weights of the 2nd and 3rd vertex
    for b1, b2 in [(0.2, 0.5), (0.6, 0.2), (0.05, 0.9)]:
        q = (1 - b1 - b2) * p[0] + b1 * p[1] + b2 * p[2]
        pi = hit([q[0], q[1], 1.0])
        assert dr.all(pi.is_valid())
        dr.assert_allclose(pi.t, 1, atol=1e-6)
        dr.assert_allclose(pi.prim_uv, [b1, b2], atol=1e-6)

    # A hit from below is reported just the same, with mirrored weights
    pi = hit([0.5, 0.25, -1.0], (0, 0, 1))
    assert dr.all(pi.is_valid())
    dr.assert_allclose(pi.prim_uv, [0.375, 0.25], atol=1e-6)

    # Misses: outside the triangle, behind the ray origin, and parallel to
    # the triangle's plane
    assert dr.none(hit([2.0, 0.5, 1.0]).is_valid())
    assert dr.none(hit([0.5, 0.25, 1.0], (0, 0, 1)).is_valid())
    assert dr.none(hit([0.5, 0.25, 1.0], (1, 0, 0)).is_valid())

    # Only hits within the ray's [mint, maxt] range count
    ray = mi.Ray3f(mi.Point3f(0.5, 0.25, 1.0), mi.Vector3f(0, 0, -1))
    ray.maxt = 0.5
    assert dr.none(mesh.ray_intersect_triangle(mi.UInt32(0), ray).is_valid())


@fresolver_append_path
def test02_eval_parameterization(variants_all_rgb):
    """eval_parameterization() maps UV coordinates back to surface points,
    resolving each island of a seamed parameterization to its own face. It
    also works from within a symbolic emitter sampling call."""
    shape = mi.load_dict({
        "type": "obj",
        "filename": "resources/data/common/meshes/rectangle.obj",
        "emitter": {
            "type": "area",
            "radiance": {"type": "checkerboard"}
        }
    })

    si = shape.eval_parameterization([-0.01, 0.5])
    assert not dr.any(si.is_valid())
    si = shape.eval_parameterization([1.0 - 1e-7, 1.0 - 1e-7])
    assert dr.all(si.is_valid())
    dr.assert_allclose(si.p, [1, 1, 0])
    si = shape.eval_parameterization([1e-7, 1e-7])
    assert dr.all(si.is_valid())
    dr.assert_allclose(si.p, [-1, -1, 0])
    si = shape.eval_parameterization([.2, .3])
    assert dr.all(si.is_valid())
    dr.assert_allclose(si.p, [-.6, -.4, 0])

    # Test with symbolic virtual function call
    if dr.is_jit_v(mi.Float):
        emitter = shape.emitter()
        N = 4
        mask = mi.Bool(False, True, False, True)
        emitters = mi.EmitterPtr(emitter)
        it = dr.zeros(mi.Interaction3f, N)
        it.p = [0, 0, -3]
        it.t = 0
        ds, _ = emitters.sample_direction(it, [0.5, 0.5], mask)
        dr.assert_allclose(ds.uv, dr.select(mask, mi.Point2f(0.5),
                                            mi.Point2f(0.0)))

    # On a patch whose two faces are separate UV islands, each island
    # resolves to its own face and the gap between them to neither
    m, ref = curved_patch()
    b = np.float64([0.3]), np.float64([0.4])
    for f in range(2):
        exp = ref.expected(f, *b)
        si = m.eval_parameterization(mi.Point2f(*map(float, exp['uv'][0])))
        assert dr.all(si.is_valid())
        assert np.allclose(rows(si.p, 3), exp['p'], atol=1e-5)

    assert not dr.any(m.eval_parameterization([1.5, 0.5]).is_valid())


@pytest.mark.parametrize("flip_normals", [False, True])
@pytest.mark.parametrize("tangents", [False, True])
def test03_surface_interaction(variants_all_rgb, flip_normals, tangents):
    """Every field of a surface interaction on a curved patch with a UV
    seam matches an exact model of the mesh."""
    mesh, ref = curved_patch(flip_normals=flip_normals)
    if tangents:
        mesh.set_bsdf(anisotropic_bsdf())
    assert mesh.packs_tangent() == tangents

    scene = mi.load_dict({'type': 'scene', 'm': mesh})
    flags = mi.RayFlags.Default | mi.RayFlags.NormalPartials

    for xy in PATCH_PROBES:
        f, b, si, ray = probe(scene, xy, flags)
        exp = ref.expected(f, *b)

        for name, value, atol in [('p', si.p, 1e-6), ('n', si.n, 1e-6),
                                  ('sh_n', si.sh_frame.n, 2e-5),
                                  ('uv', si.uv, 1e-6),
                                  ('dp_du', si.dp_du, 1e-5),
                                  ('dp_dv', si.dp_dv, 1e-5)]:
            assert np.allclose(rows(value, dr.size_v(value)), exp[name],
                               atol=atol), (name, xy)

        # si.t is consistent with the hit point, and the normal partials
        # with central differences of the interpolated normal field
        dr.assert_allclose(ray(si.t), si.p, atol=1e-6)
        check_normal_partials(si, ref.normal_field(f))

        # The shading frame is orthonormal, and wi is the incident direction
        # expressed in it
        s, t, n = si.sh_frame.s, si.sh_frame.t, si.sh_frame.n
        dr.assert_allclose(dr.norm(s), 1, atol=1e-6)
        dr.assert_allclose(dr.dot(s, n), 0, atol=2e-6)
        dr.assert_allclose(si.to_world(si.wi), -ray.d, atol=1e-6)

        # Only the interpolated tangents track the orientation of the
        # parameterization; the bitangent follows it
        assert dr.all(si.frame_flipped == (exp['flipped'] and tangents))
        dr.assert_allclose(t, dr.select(si.frame_flipped, -1., 1.) *
                           dr.cross(n, s), atol=2e-6)

        if not tangents:
            # Without a material that consumes them, the mesh leaves the
            # tangents undecoded and the frame falls back to a basis that
            # only depends on the shading normal
            dr.assert_allclose(s, mi.Frame3f(n).s, atol=1e-6)


def test04_ray_flags(variants_all_rgb):
    """Fields that the ray flags do not request are left at their zero
    initialization, and a mesh shaded with face normals has no curvature."""
    mesh, ref = curved_patch()
    scene = mi.load_dict({'type': 'scene', 'm': mesh})

    for xy in PATCH_PROBES:
        # Minimal computes the distance, position and geometric normal only
        f, b, si, _ = probe(scene, xy, mi.RayFlags.Minimal)
        exp = ref.expected(f, *b)
        assert np.allclose(rows(si.p, 3), exp['p'], atol=1e-6)
        assert np.allclose(rows(si.n, 3), exp['n'], atol=1e-6)
        for value in (si.uv, si.dp_du, si.dp_dv, si.dn_du, si.dn_dv, si.wi,
                      si.sh_frame.n, si.sh_frame.s, si.sh_frame.t):
            assert not dr.any(value != 0, axis=None)

        # Without NormalPartials the curvature terms stay zero, while the
        # shading normal keeps tracking the interpolated one
        si = probe(scene, xy, mi.RayFlags.Default)[2]
        assert not dr.any((si.dn_du != 0) | (si.dn_dv != 0), axis=None)
        assert dr.any(si.sh_frame.n != si.n, axis=None)

    # A mesh shaded with face normals has neither
    flat, _ = curved_patch(face_normals=True)
    scene = mi.load_dict({'type': 'scene', 'm': flat})
    assert not flat.has_normals()
    flags = mi.RayFlags.Default | mi.RayFlags.NormalPartials
    for xy in PATCH_PROBES:
        si = probe(scene, xy, flags)[2]
        dr.assert_allclose(si.sh_frame.n, si.n)
        assert not dr.any((si.dn_du != 0) | (si.dn_dv != 0), axis=None)


@pytest.mark.parametrize("hard_edge", [False, True])
def test05_seam_crease(variants_all_rgb, hard_edge):
    """A UV seam splits the texture coordinates without creasing the
    shading normal. Authored normals on the same edge do crease it."""
    mesh, _ = curved_patch(hard_edge=hard_edge)
    scene = mi.load_dict({'type': 'scene', 'm': mesh})

    # The two faces share the diagonal from (0, 0) to (1, 1), with the first
    # one below it. Each probe pair straddles the seam.
    eps = 1e-3
    for c in np.linspace(0.2, 0.8, 5):
        f_a, _, a, _ = probe(scene, (c + eps, c - eps))
        f_b, _, b, _ = probe(scene, (c - eps, c + eps))
        assert (f_a, f_b) == (0, 1)

        # The surface is continuous, the parameterization is not: the two
        # faces put their texture coordinates in separate islands
        dr.assert_allclose(a.p, b.p, atol=4 * eps)
        assert dr.all(dr.norm(a.uv - b.uv) > 1)

        angle = dr.rad2deg(dr.acos(dr.clip(
            dr.dot(a.sh_frame.n, b.sh_frame.n), -1, 1)))
        assert dr.all(angle > 10 if hard_edge else angle < 0.5), angle


@pytest.mark.parametrize("mode", ["rotate", "mirror"])
def test06_instanced(variants_all_rgb, mode):
    """The shading frame and the normal partials transform along with an
    instance. A mirroring transform flips the frame's handedness."""
    to_world = (mi.ScalarTransform4f().rotate([0, 0, 1], 90.0)
                if mode == "rotate" else
                mi.ScalarTransform4f().scale([-1, 1, 1]))

    scenes = []
    for instance_trafo in (mi.ScalarTransform4f(), to_world):
        mesh, ref = curved_patch()
        mesh.set_bsdf(anisotropic_bsdf())
        scenes.append(mi.load_dict({
            'type': 'scene',
            'group': {'type': 'shapegroup', 'm': mesh},
            'inst': {'type': 'instance',
                     'group': {'type': 'ref', 'id': 'group'},
                     'to_world': instance_trafo}}))
    trafo = mi.Transform4f(to_world)
    flags = mi.RayFlags.Default | mi.RayFlags.NormalPartials

    # Both transforms map vertical lines onto vertical lines, so the probe
    # of the instanced scene sees the transformed hit point of the original
    for xy in PATCH_PROBES:
        f, _, si, _ = probe(scenes[0], xy, flags)
        q = to_world @ mi.ScalarPoint3f(xy[0], xy[1], 0.0)
        si_i = probe(scenes[1], (q.x, q.y), flags)[2]

        dr.assert_allclose(si_i.p, trafo @ si.p, atol=1e-5)
        dr.assert_allclose(si_i.sh_frame.s,
                           dr.normalize(trafo @ si.sh_frame.s), atol=1e-5)
        assert dr.all(si_i.frame_flipped ==
                      (si.frame_flipped ^ (mode == "mirror")))
        check_normal_partials(si_i, ref.normal_field(f), to_world=to_world)


# -------------------------------------------------------------------
# Generated tangents
# -------------------------------------------------------------------

@pytest.mark.parametrize("split", ["uv_island", "mirrored_uv",
                                   "hard_normal", "attribute"])
def test07_vertex_splits(variant_scalar_rgb, split):
    """A corner splits off its own vertex when its UV value, UV orientation,
    normal group or any custom attribute differs from the one its neighbor
    assigns to the same source vertex. The copies keep sharing one surface
    point, and each accumulates a tangent over its own triangle fan."""
    # Two triangles sharing the edge from source vertex 0 to 1, whose UVs
    # agree at the shared corners unless the case below says otherwise
    positions = np.float32([[0, 0, 0], [1, 0, 0], [0.5, 1, 0], [0.5, -1, 0]])
    corner_vertex = np.uint32([0, 1, 2, 1, 0, 3])
    uv = np.float32([[0, 0], [1, 0], [1, 1], [1, 0], [0, 0], [0, 1]])
    normal_count, kwargs = 4, {}

    if split == "uv_island":
        uv = np.float32([[0, 0], [1, 0], [1, 1], [6, 5], [5, 5], [5, 4]])
    elif split == "mirrored_uv":
        uv = np.float32([[0, 0], [1, 0], [0.5, 1], [1, 0], [0, 0], [0.5, 1]])
    elif split == "hard_normal":
        s = np.float32(np.sqrt(0.5))
        positions[3] = (0.5, -1, 1)
        kwargs['normals'] = np.float32([[0, 0, 1]] * 3 + [[0, -s, s]] * 3)
        normal_count = 6
    else:
        color = np.zeros((6, 3), dtype=np.float32)
        color[3:5] = (1, 0, 0)
        kwargs['attrs'] = {"vertex_color": color}

    m = mi.Mesh("split")
    m.from_corners(positions=positions, corner_vertex=corner_vertex,
                   texcoords=uv, **kwargs)

    # Both shared source vertices split in two, over the same surface points
    assert (m.position_count(), m.vertex_count(), m.normal_count()) \
        == (4, 6, normal_count)
    faces, pidx = np.array(m.faces()), np.array(m.position_index())
    assert faces[0, 0] != faces[1, 1] and pidx[faces[0, 0]] == pidx[faces[1, 1]]
    assert faces[0, 1] != faces[1, 0] and pidx[faces[0, 1]] == pidx[faces[1, 0]]

    # Every case parameterizes u along +x, so all tangents agree
    assert_valid_tangent_frame(m)
    assert np.allclose(np.array(m.tangents()), [1, 0, 0], atol=1e-6)

    if split == "mirrored_uv":
        # Mirrored winding shows up as a sign flip of the UV determinant
        e = np.array(m.texcoords())[faces[:, 1:]] - \
            np.array(m.texcoords())[faces[:, :1]]
        det = e[:, 0, 0] * e[:, 1, 1] - e[:, 0, 1] * e[:, 1, 0]
        assert det[0] > 0 and det[1] < 0


def test08_degenerate_fallback(variant_scalar_rgb):
    """Degenerate triangles contribute nothing. A vertex without any
    valid contribution falls back to a finite direction perpendicular to
    the normal."""
    positions = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]],
                         dtype=np.float32)
    cv = np.array([0, 1, 2, 0, 1, 1], dtype=np.uint32)
    uv = np.array([[0.5, 0.5], [0.5, 0.5], [0.5, 0.5],
                   [0, 0], [1, 0], [1, 0]], dtype=np.float32)

    m = mi.Mesh("degen")
    m.from_corners(positions=positions, corner_vertex=cv, texcoords=uv)

    assert_valid_tangent_frame(m)


def test09_tangent_weighting_convention(variants_all_rgb):
    """Tests the shading tangent computation (MikkTSpace convention) on a
    less regular case to catch regressions."""
    rng = np.random.default_rng(7)
    n_tri = 4
    phi = np.cumsum(0.5 + rng.random(n_tri + 1))
    phi = phi / phi[-1] * 4.5  # open fan, irregular spacing
    r = 0.7 + 0.6 * rng.random(n_tri + 1)
    ring = np.stack([r * np.cos(phi), r * np.sin(phi),
                     0.6 * rng.random(n_tri + 1) - 0.3], axis=-1)
    positions = np.vstack([[[0, 0, 0.1]], ring]).astype(np.float32)

    normals = np.float32(0.8 * rng.random((n_tri + 2, 3)) - 0.4)
    normals[:, 2] = 1.0
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)

    texcoords = (0.3 * positions[:, :2] +
                 0.2 * rng.random((n_tri + 2, 2))).astype(np.float32)
    faces = np.uint32([[0, i + 1, i + 2] for i in range(n_tri)])

    m = mi.Mesh("fan")
    m.from_fields(faces=faces, positions=positions, normals=normals,
                  texcoords=texcoords)

    P, N, UV = (a.astype(np.float64) for a in (positions, normals, texcoords))
    acc = np.zeros((len(P), 3))
    for idx in faces:
        p, uv = P[idx], UV[idx]
        t1, t2 = uv[1] - uv[0], uv[2] - uv[0]
        area2 = t1[0] * t2[1] - t1[1] * t2[0]
        vos = t2[1] * (p[1] - p[0]) - t1[1] * (p[2] - p[0])
        vos *= (1.0 if area2 > 0 else -1.0) / np.linalg.norm(vos)
        for k in range(3):
            n = N[idx[k]]
            t = vos - n * np.dot(n, vos)
            e1, e2 = p[(k + 1) % 3] - p[k], p[(k + 2) % 3] - p[k]
            e1, e2 = e1 - n * np.dot(n, e1), e2 - n * np.dot(n, e2)
            cos_a = np.dot(e1, e2) / (np.linalg.norm(e1) * np.linalg.norm(e2))
            acc[idx[k]] += t * (np.arccos(np.clip(cos_a, -1, 1)) /
                                np.linalg.norm(t))
    ref = acc / np.linalg.norm(acc, axis=1, keepdims=True)

    assert np.allclose(np.array(m.tangents()), ref, atol=1e-5)


def test10_uv_flip_bits(variants_all_rgb):
    """The cached UV orientation bits share a lane with the per-face BSDF
    index, and follow texture coordinate edits without disturbing it."""
    positions = np.float32([[0, 0, 0], [1, 0, 0], [0, 1, 0],
                            [2, 0, 0], [3, 0, 0], [2, 1, 0]])
    uv = np.float32([[0, 0], [1, 0], [0, 1],    # det > 0
                     [0, 0], [0, 1], [1, 0]])   # det < 0
    m = mi.Mesh("mirrored")
    m.from_fields(faces=np.arange(6, dtype=np.uint32).reshape(2, 3),
                  positions=positions, texcoords=uv,
                  normals=np.tile(np.float32([0, 0, 1]), (6, 1)),
                  bsdf_index=np.uint32([3, 5]))
    m.set_bsdf(anisotropic_bsdf())
    assert m.packs_tangent()

    def flipped():
        scene = mi.load_dict({'type': 'scene', 'm': m})
        return [bool(dr.all(scene.ray_intersect(mi.Ray3f(
                    mi.Point3f(x, 0.25, 1), mi.Vector3f(0, 0, -1)
                )).frame_flipped)) for x in (0.25, 2.25)]

    assert flipped() == [False, True]
    assert np.all(np.array(m.bsdf_index()) == [3, 5])
    assert np.all(np.array(mi.traverse(m)['bsdf_index']) == [3, 5])
    assert np.all(np.array(m.faces()) == np.arange(6).reshape(2, 3))

    # Mirroring every texture coordinate swaps both orientations
    params = mi.traverse(m)
    edited = np.array(params['texcoords'])
    edited[:, 0] = 1 - edited[:, 0]
    params['texcoords'] = edited
    params.update()
    assert flipped() == [True, False]
    assert np.all(np.array(m.bsdf_index()) == [3, 5])

    # The BSDF index lane is writable and survives a 'faces' rewrite at an
    # unchanged face count
    params['bsdf_index'] = np.uint32([4, 6])
    params.update()
    assert np.all(np.array(m.bsdf_index()) == [4, 6])
    params['faces'] = np.array(params['faces'])[[1, 0]].copy()
    params.update()
    assert np.all(np.array(m.bsdf_index()) == [4, 6])


@fresolver_append_path
def test11_uv_sphere_tangents(variants_vec_rgb):
    """End to end on a coarse UV sphere: the interpolated tangent tracks the
    longitude, including at the pole where a full turn spreads over a single
    ring of triangles, and it stays continuous across both the face
    boundaries and the wrap seam of the parameterization."""
    m = mi.load_dict({
        'type': 'obj',
        'filename': 'resources/data/tests/scenes/mesh_tangents/meshes/uv_sphere.obj',
        'bsdf': ANISOTROPIC_BSDF})
    assert m.packs_tangent()
    scene = mi.load_dict({'type': 'scene', 'm': m})

    def pairs(v):
        """Consecutive samples along a wavefront"""
        i = dr.arange(mi.UInt32, dr.width(v) - 1)
        return dr.gather(type(v), v, i), dr.gather(type(v), v, i + 1)

    def turn(v):
        """Angle in degrees between consecutive samples of a direction field"""
        a, b = pairs(dr.normalize(v))
        return dr.rad2deg(dr.acos(dr.clip(dr.abs(dr.dot(a, b)), -1, 1)))

    def ring(theta, n=2000):
        """Intersections along a ring of rays at polar angle theta, spanning
        slightly more than a full turn. The azimuth offset keeps the samples
        off the mesh edges."""
        phi = dr.arange(mi.Float, n + 1) * (2 * dr.pi / n) + 0.0031
        st, ct = float(np.sin(theta)), float(np.cos(theta))
        p = mi.Vector3f(st * dr.cos(phi), st * dr.sin(phi), ct)
        si = scene.ray_intersect(mi.Ray3f(o=mi.Point3f(3 * p), d=-p))
        assert dr.all(si.is_valid())
        return si, phi

    # The tangent lies in the surface and follows the longitude, also right
    # next to the pole where a full turn spreads over one ring of triangles
    for theta, tol in [(0.06, 8.0), (0.4, 7.0), (1.2, 3.0)]:
        si, phi = ring(theta)
        s = dr.normalize(si.sh_frame.s)
        assert dr.all(dr.abs(dr.dot(s, dr.normalize(si.sh_frame.n))) < 1e-5)
        angle = dr.rad2deg(dr.acos(dr.clip(dr.abs(dr.dot(
            s, mi.Vector3f(-dr.sin(phi), dr.cos(phi), 0))), -1, 1)))
        assert dr.all(angle < tol), (theta, dr.max(angle))

    # Away from the pole, a ring crosses the wrap seam of the
    # parameterization once, where u steps back by nearly a full period, and
    # 24 columns of faces, where the dp_du basis steps by a full face
    si, _ = ring(1.2)
    u0, u1 = pairs(si.uv.x)
    wrap = dr.abs(u1 - u0) > 0.5
    assert dr.count(wrap) == 1
    assert dr.count(turn(si.dp_du) > 1.0) >= 20

    # The interpolated tangent instead turns by no more than the ring's own
    # sampling step of 360/2000 degrees. The exception is the wrap seam,
    # where the split vertices each average over their own side only, which
    # leaves a jump of roughly half a face.
    step = turn(si.sh_frame.s)
    assert dr.all(dr.select(wrap, 0, step) < 0.3)
    assert dr.max(step) < 8


# -------------------------------------------------------------------
# Area sampling
# -------------------------------------------------------------------

@pytest.mark.parametrize("flip_normals", [False, True])
def test12_sample_position(variants_vec_rgb, flip_normals):
    """sample_position() draws area-uniform samples with interpolated UVs
    and normals. The density equals the reciprocal surface area."""
    # Two triangles of unequal area (0.5 and 2.0) with UVs = 0.5 * xy
    positions = np.float32([[0, 0, 0], [1, 0, 0], [0, 1, 0],
                            [1, 0, 0], [3, 0, 0], [1, 2, 0]])
    uv = (positions[:, :2] * 0.5).astype(np.float32)
    m = mi.Mesh("two", flip_normals=flip_normals)
    m.from_fields(faces=np.arange(6, dtype=np.uint32).reshape(2, 3),
                  positions=positions, texcoords=uv)
    dr.assert_allclose(m.surface_area(), 2.5)

    n = 64
    u = (dr.arange(mi.Float, n) + 0.5) / n
    sample = mi.Point2f(dr.meshgrid(u, u))
    ps = m.sample_position(0.0, sample)

    dr.assert_allclose(ps.pdf, 1.0 / 2.5)
    dr.assert_allclose(m.pdf_position(ps), ps.pdf)
    dr.assert_allclose(ps.uv, mi.Point2f(ps.p.x, ps.p.y) * 0.5, atol=1e-6)
    n_ref = [0, 0, -1] if flip_normals else [0, 0, 1]
    dr.assert_allclose(ps.n, n_ref)

    # The sample mean matches the area-weighted centroid
    cx = (0.5 * (1 / 3) + 2.0 * (5 / 3)) / 2.5
    cy = (0.5 * (1 / 3) + 2.0 * (2 / 3)) / 2.5
    dr.assert_allclose(dr.mean(ps.p.x), cx, atol=0.02)
    dr.assert_allclose(dr.mean(ps.p.y), cy, atol=0.02)
