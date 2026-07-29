"""
Miscellaneous utility functions for tests (common fixtures,
test decorators, etc).
"""

import os
import sys
from functools import wraps
from inspect import signature, _empty

import numpy as np
import pytest
import drjit as dr

def find_resource(fname):
    path = os.path.dirname(os.path.realpath(__file__))
    while True:
        full = os.path.join(path, fname)
        if os.path.exists(full):
            return full
        if path == '' or path == '/':
            raise Exception("find_resource(): could not find \"%s\"" % fname)
        path = os.path.dirname(path)

def fresolver_append_path(func):
    """
    Function decorator that adds the mitsuba project root
    to the mitsuba.FileResolver's search path. This is useful in particular
    for tests that e.g. load scenes, and need to specify paths to resources.

    The file resolver is restored to its previous state once the test's
    execution has finished.
    """

    import mitsuba as mi

    par = os.path.dirname

    # Get the path to the source file from which this function is being called.
    # We previously used inspect.staack() here, which is needlessly expensive.
    caller_filename = sys._getframe(1).f_code.co_filename
    caller_path = par(os.path.realpath(caller_filename))

    # Heuristic to find the project's root directory
    def is_root(path):
        if not path:
            return False
        children = os.listdir(path)
        return ('ext' in children) and ('include' in children) \
               and ('src' in children) and ('resources' in children)
    root_path = caller_path
    while not is_root(root_path) and (par(root_path) != root_path):
        root_path = par(root_path)

    # The @wraps decorator properly sets __name__ and other properties, so that
    # pytest-xdist can keep track of the original test function.
    @wraps(func)
    def f(*args, **kwargs):
        # New file resolver
        fres_old = mi.file_resolver()
        fres = mi.FileResolver(fres_old)

        # Append current test directory and project root to the
        # search path.
        fres.append(caller_path)
        fres.append(root_path)

        mi.set_file_resolver(fres)

        # Run actual function
        res = func(*args, **kwargs)

        # Restore previous file resolver
        mi.set_file_resolver(fres_old)

        return res

    return f


@pytest.fixture
def tmpfile(request, tmpdir_factory):
    """Fixture to create a temporary file"""
    return make_tmpfile(request, tmpdir_factory)

def make_tmpfile(request, tmpdir_factory):
    my_dir = tmpdir_factory.mktemp('tmpdir')
    request.addfinalizer(lambda: my_dir.remove(rec=1))
    path_value = str(my_dir.join('tmpfile'))
    open(path_value, 'a').close()
    return path_value


def check_vectorization(kernel, arg_dims = [], width = 125, atol=1e-6,
                        modes=['llvm', 'cuda', 'llvm_ad', 'cuda_ad']):
    """
    Helper routine which compares evaluations of the vectorized and
    non-vectorized version of a kernel using available variants (e.g. LLVM, CUDA).

    Parameter ``kernel`` (function):
        Function to be evaluated. It's arguments should be annotated if
        ``arg_dims`` is not specified. A kernel can return any drjit supported array
        types (e.g. Float, Vector3f, ...) or a tuple of such arrays.

    Parameter ``arg_dims`` (list(int)):
        Dimensionalities of the function arguments. If not specified, those will be
        deduced from the function argument annotations (if available).

    Parameter ``width`` (int):
       Number of elements to be evaluated at a time for the vectorized call.

    Parameter ``atol`` (float):
       Absolute tolerance for the comparison of the returned values.
    """
    import numpy as np
    import mitsuba as mi

    # Ensure scalar variant is enabled when calling this kernel
    assert mi.variant().startswith('scalar_')

    # List available variants with similar spectral variant
    spectral_variant = mi.variant().replace("scalar", "")
    variants = list(set(mi.variants()) & set([m + spectral_variant for m in modes]))

    if not variants:
        pytest.skip(f"No vectorized variants available")

    # If argument dimensions not provided, look at kernel argument annotations
    if arg_dims == []:
        params = signature(kernel).parameters
        args_types = [params[name].annotation for name in params]
        assert not _empty in args_types, \
               "`kernel` arguments should be annotated, or `arg_dims` should be set."
        arg_dims = [1 if t == float else dr.size_v(t) for t in args_types]

    # Construct random argument arrays
    rng = np.random.default_rng(seed=0)
    args_np = [rng.random(width) if d == 1 else rng.random((width, d)) for d in arg_dims]

    # Evaluate non-vectorized kernel
    from mitsuba import Float, Vector2f, Vector3f
    types = [Float, Vector2f, Vector3f]
    results_scalar = []
    for i in range(width):
        args = [types[arg_dims[j]-1](args_np[j][i]) for j in range(len(args_np))]
        res = kernel(*args)

        if not type(res) in [list, tuple]:
            res = [res]

        if results_scalar == []:
            results_scalar = [[] for i in range(len(res))]

        for i in range(len(res)):
            results_scalar[i].append(res[i])

    results_scalar = [np.array(res) for res in results_scalar]

    # Evaluate and compare vectorized kernel
    for variant in variants:
        # Set variant
        mi.set_variant(variant)
        types = [mi.Float, mi.Vector2f, mi.Vector3f]

        # Cast arguments
        args = [types[arg_dims[i]-1](np.transpose(args_np[i])) for i in range(len(args_np))]

        # Evaluate vectorized kernel
        results_vec = kernel(*args)
        if not type(results_vec) in [list, tuple]:
            results_vec = [results_vec]

        # Compare results
        for i in range(len(results_scalar)):
            assert dr.allclose(results_vec[i], np.transpose(results_scalar[i]), atol=atol)


def unit_triangle(name="tri", **kwargs):
    """The triangle (0,0,0), (1,0,0), (0,1,0), built through from_fields()"""
    import mitsuba as mi
    m = mi.Mesh(name)
    m.from_fields(faces=[[0, 1, 2]],
                  positions=[[0, 0, 0], [1, 0, 0], [0, 1, 0]], **kwargs)
    return m


def quad_corners(seam=False):
    """
    Corner-indexed description of a unit quad in the z=0 plane: two
    triangles (0,1,2) and (0,2,3), returned as a ``(positions,
    corner_vertex, uv)`` triple for ``Mesh.from_corners()``. With ``seam``,
    the second triangle's UVs jump to a separate island along the diagonal.
    """
    import numpy as np
    positions = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]],
                         dtype=np.float32)
    corner_vertex = np.array([0, 1, 2, 0, 2, 3], dtype=np.uint32)
    uv = np.array([[0, 0], [1, 0], [1, 1], [0, 0], [1, 1], [0, 1]],
                  dtype=np.float32)
    if seam:
        uv[3] = (5, 5)
        uv[4] = (6, 6)
    return positions, corner_vertex, uv


def seam_quad_fields():
    """
    The quad from ``quad_corners(seam=True)``, written out by hand in the
    form that ``Mesh.from_fields()`` takes: the two vertices on the seam
    appear twice (6 vertices, 4 positions), and ``position_index`` says
    which position each vertex sits on. Kept independent of the corner
    welder so that ``from_fields()`` tests do not rely on it. Returns
    ``(positions, faces, position_index, uv)``.
    """
    import numpy as np
    coarse = np.float32([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]])
    faces = np.uint32([[0, 1, 2], [3, 4, 5]])
    pidx = np.uint32([0, 1, 2, 0, 2, 3])
    uv = np.float32([[0, 0], [1, 0], [1, 1], [5, 5], [6, 6], [0, 1]])
    return coarse, faces, pidx, uv


def seam_quad():
    """The mesh described by ``seam_quad_fields()``: 4 surface points, 6
    vertices, faces (0,1,2)/(0,2,3) in surface point space"""
    import mitsuba as mi
    coarse, faces, pidx, uv = seam_quad_fields()
    m = mi.Mesh("seam")
    m.from_fields(faces=mi.TensorXu(faces), position_index=pidx,
                  positions=mi.TensorXf(coarse), texcoords=mi.TensorXf(uv))
    return m


# A material that consumes the tangent frame, which makes a mesh spend the
# trailing packed lane on the tangent instead of on n.z
ANISOTROPIC_BSDF = {'type': 'roughconductor', 'alpha_u': 0.1, 'alpha_v': 0.3}


def anisotropic_bsdf():
    """Instance of the ``ANISOTROPIC_BSDF`` description"""
    import mitsuba as mi
    return mi.load_dict(dict(ANISOTROPIC_BSDF))


def assert_uniform_within_groups(values, index):
    """Assert that all rows of ``values`` sharing an ``index`` entry (a
    position or normal group) hold the same value"""
    import numpy as np
    for g in np.unique(index):
        members = values[index == g]
        assert len(members) > 0 and np.allclose(members, members[0])


def assert_valid_tangent_frame(mesh):
    """Assert that a mesh's tangents are unit length and perpendicular to
    the corresponding vertex normals"""
    import numpy as np
    t, n = np.array(mesh.tangents()), vertex_normals(mesh)
    assert np.all(np.isfinite(t))
    assert np.allclose(np.linalg.norm(t, axis=1), 1, atol=1e-6)
    assert np.allclose((t * n).sum(axis=1), 0, atol=1e-6)


def _expand(values, index):
    """Resolve grouped values through an index map (empty means identity)"""
    import numpy as np
    values = np.array(values)
    index = np.array(index)
    return values if index.size == 0 else values[index]


def vertex_positions(mesh):
    """Per-vertex positions of a mesh, resolved through the position map"""
    return _expand(mesh.positions(), mesh.position_index())


def vertex_normals(mesh):
    """Per-vertex normals of a mesh, resolved through the normal map"""
    return _expand(mesh.normals(), mesh.normal_index())


def faces_of(mesh):
    """(F, 3) vertex indices read through the 'faces' parameter"""
    import numpy as np
    import mitsuba as mi
    return np.array(mi.traverse(mesh)['faces'])


def face_records(mesh):
    """(F, 4) vertex indices + BSDF index from the parameter interface"""
    import numpy as np
    import mitsuba as mi
    params = mi.traverse(mesh)
    faces = np.array(params['faces'])
    bsdf_index = np.array(params['bsdf_index'])
    if bsdf_index.size == 0:  # an empty assignment stands for zeros
        bsdf_index = np.zeros(faces.shape[0], dtype=np.uint32)
    return np.column_stack([faces, bsdf_index])


def rows(value, dim):
    """``(N, dim)`` double precision view of a Dr.Jit vector, in any variant"""
    return np.array(value, dtype=np.float64).reshape(dim, -1).T


class CurvedPatch:
    """
    Reference normals and shading frame on a 2-triangle curved patch.
    """

    def __init__(self, p, n, uv, sign):
        self.p, self.uv, self.sign = np.float64(p), np.float64(uv), sign
        self.n = np.float64(n)
        self.n /= np.linalg.norm(self.n, axis=-1, keepdims=True)

    def basis(self, f):
        """Position and texture coordinate edges of a face, plus the
        reciprocal determinant of the latter"""
        e, d = self.p[f, 1:] - self.p[f, 0], self.uv[f, 1:] - self.uv[f, 0]
        return e, d, 1.0 / (d[0, 0] * d[1, 1] - d[0, 1] * d[1, 0])

    @staticmethod
    def interpolate(v, b1, b2):
        """Barycentric interpolation of a face's corner values"""
        b1, b2 = np.atleast_1d(b1)[:, None], np.atleast_1d(b2)[:, None]
        return v[0] * (1 - b1 - b2) + v[1] * b1 + v[2] * b2

    def normal_field(self, f):
        """The unit shading normal of a face as a function of ``(u, v)``"""
        _, d, inv_det = self.basis(f)

        def field(u, v):
            du, dv = u - self.uv[f, 0, 0], v - self.uv[f, 0, 1]
            n = self.interpolate(self.n[f],
                                 (d[1, 1] * du - d[1, 0] * dv) * inv_det,
                                 (d[0, 0] * dv - d[0, 1] * du) * inv_det)
            return self.sign * n / np.linalg.norm(n, axis=-1, keepdims=True)

        return field

    def expected(self, f, b1, b2):
        """The exact fields at barycentric coordinates on the given face"""
        e, d, inv_det = self.basis(f)
        unit = lambda v: v / np.linalg.norm(v, axis=-1, keepdims=True)
        return dict(
            p=self.interpolate(self.p[f], b1, b2),
            n=unit(np.cross(e[0], e[1])),
            sh_n=self.sign * unit(self.interpolate(self.n[f], b1, b2)),
            uv=self.interpolate(self.uv[f], b1, b2),
            dp_du=(d[1, 1] * e[0] - d[0, 1] * e[1]) * inv_det,
            dp_dv=(d[0, 0] * e[1] - d[1, 0] * e[0]) * inv_det,
            flipped=bool(inv_det < 0))


def curved_patch(hard_edge=False, flip_normals=False, face_normals=False):
    """
    Build a curved patch of two triangles with a diagonal from surface point 2
    and 0. Returns a reference model of the associated shading fields.
    The diagonal is a UV seam. The second triangle is mirrored in VU space.
    When ``hard_edge`` is true, the edge is furthermore creased.
    """
    import mitsuba as mi

    def unit(v):
        v = np.float32(v)
        return v / np.linalg.norm(v, axis=-1, keepdims=True)

    positions = np.float32([[0, 0, 0], [1, 0, 0.2], [1, 1, -0.1], [0, 1, 0.3]])
    position_index = np.uint32([0, 1, 2, 0, 2, 3])
    texcoords = np.float32([[0.10, 0.20], [0.70, 0.05], [0.55, 0.80],
                            [2.30, 0.40], [2.05, 0.95], [2.85, 0.75]])
    normals = unit([[-0.3, -0.2, 1], [0.4, -0.1, 1],
                    [0.25, 0.35, 1], [-0.2, 0.3, 1]])

    # The normal groups coincide with the surface points, unless the second
    # face carries its own normals on the shared diagonal
    normal_index = position_index
    if hard_edge:
        normals = np.vstack([normals[[0, 1, 2]],
                             unit([[0.5, 0.45, 1], [-0.45, 0.5, 1]]),
                             normals[[3]]])
        normal_index = np.uint32([0, 1, 2, 3, 4, 5])

    m = mi.Mesh("patch", flip_normals=flip_normals, face_normals=face_normals)
    m.from_fields(faces=[[0, 1, 2], [3, 4, 5]], positions=positions,
                  position_index=position_index, normals=normals,
                  normal_index=normal_index, texcoords=texcoords)

    # 'flip_normals' reverses the winding of the built mesh and negates its
    # shading normals, which the reference model has to follow
    corner = np.uint32([[0, 1, 2], [3, 4, 5]])
    if flip_normals:
        corner = corner[:, ::-1]

    return m, CurvedPatch(positions[position_index[corner]],
                          normals[normal_index[corner]], texcoords[corner],
                          -1.0 if flip_normals else 1.0)


def check_normal_partials(si, normal_field, to_world=None, h=1e-3, rtol=2e-3):
    """
    Check ``si.dn_du``/``si.dn_dv`` against central differences of a normal
    field ``normal_field(u, v)``, and return the largest relative error.
    """
    field = normal_field
    if to_world is not None:
        N = np.linalg.inv(np.array(to_world.matrix, dtype=np.float64)[:3, :3])

        def field(u, v):
            n = normal_field(u, v) @ N
            return n / np.linalg.norm(n, axis=-1, keepdims=True)

    u, v = rows(si.uv, 2).T
    ref = [(field(u + h, v) - field(u - h, v)) / (2 * h),
           (field(u, v + h) - field(u, v - h)) / (2 * h)]

    scale = max(max(np.abs(r).max() for r in ref), 1.0)
    err = max(np.abs(rows(d, 3) - r).max()
              for d, r in zip((si.dn_du, si.dn_dv), ref)) / scale

    assert err < rtol, (f"normal partials are off by {err}:\n"
                        f"  dn_du = {rows(si.dn_du, 3)}, expected {ref[0]}\n"
                        f"  dn_dv = {rows(si.dn_dv, 3)}, expected {ref[1]}")
    return err
