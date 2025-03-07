import math
import numpy as np
import drjit as dr
import mitsuba as mi

def adjacency_list(faces, return_inverse=False):
    """
    Computes the adjacency list of the given faces as a COO index array.

    For example:
    ```
    >>> faces = np.array([
            [0, 1, 2],
            [0, 1, 3],
            [0, 2, 3],
            [1, 2, 3],
        ])
    >>> adjacency_list(faces)
    [[0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3]
     [1, 2, 3, 0, 2, 3, 0, 1, 3, 0, 1, 2]]
    ```
    """
    ii = faces[:, [1, 2, 0]].flatten()
    jj = faces[:, [2, 0, 1]].flatten()
    a, inv = np.unique(np.stack([np.concatenate([ii, jj]), np.concatenate([jj, ii])], axis=0), axis=1, return_inverse=True)
    return (a, inv) if return_inverse else a

def combinatorial_laplacian(faces):
    """
    Computes the index and data arrays (COO format) of the combinatorial Laplacian matrix of
    a given set of faces:

    ``w_ij = 1`` if vertices i and j are adjacent

    Parameter ``faces`` (``numpy.ndarray`` of shape ``(F, 3)``):
        Face indices of the mesh.

    Returns ``(numpy.ndarray, numpy.ndarray)``:
        Indices and weights ``w_ij`` of the combinatorial Laplacian matrix.
    """
    idx = adjacency_list(faces)
    val = -np.ones(idx.shape[1], dtype=np.float64)
    return (idx, val)

def __side_lengths(verts, faces):
    """Returns the 3 side lengths for each triangle face"""
    face_verts = verts[faces]
    v0, v1, v2 = face_verts[:, 0], face_verts[:, 1], face_verts[:, 2]

    # Side lengths of each triangle, of shape (F,)
    # A is the side opposite v1, B is opposite v2, and C is opposite v3
    A = np.linalg.norm(v1 - v2, axis=1)
    B = np.linalg.norm(v0 - v2, axis=1)
    C = np.linalg.norm(v0 - v1, axis=1)
    return A, B ,C

def __faces_areas(A, B, C):
    """Returns the face area of each face, given their side lengths (see ``__side_lengths``)"""
    # Heron's formula for triangle face area
    s = 0.5 * (A + B + C)
    return np.sqrt(np.clip(s * (s - A) * (s - B) * (s - C), 1e-12, None))

def __coalesce(idx, val):
    """Returns a coalesced copy of the COO array represented by idx and val"""
    idx_coalesced, idx_inverse = np.unique(idx, axis=1, return_inverse=True)
    idx_inverse = idx_inverse.flatten()
    val_coalesced = dr.zeros(mi.TensorXd, shape=(idx_coalesced.shape[1],))
    dr.scatter_add(val_coalesced.array, mi.Float64(val), mi.UInt(idx_inverse))
    return idx_coalesced, val_coalesced.numpy()

def cotangent_laplacian(verts, faces):
    """
    Computes the index and data arrays (COO format) of the cotangent Laplacian matrix of
    a mesh:

    ``w_ij = 1/2 * (cot(alpha_ij) + cot(beta_ij))`` if vertices i and j are adjacent

    Parameter ``verts`` (``numpy.ndarray`` of shape ``(V, 3)``):
        Vertex coordinates of the mesh.

    Parameter ``faces`` (``numpy.ndarray`` of shape ``(F, 3)``):
        Face indices of the mesh.

    Returns ``(numpy.ndarray, numpy.ndarray)``:
        Indices and weights ``w_ij`` of the cotangent Laplacian matrix.

    Inspired by https://pytorch3d.readthedocs.io/en/latest/_modules/pytorch3d/loss/mesh_laplacian_smoothing.html
    """   
    A, B, C = __side_lengths(verts, faces)
    areas = __faces_areas(A, B, C)

    # cot a = (B^2 + C^2 - A^2) / (4 * area)
    A2, B2, C2 = A * A, B * B, C * C
    cota = (B2 + C2 - A2) / (4 * areas)
    cotb = (A2 + C2 - B2) / (4 * areas)
    cotc = (A2 + B2 - C2) / (4 * areas)
    cot = np.stack([cota, cotb, cotc], axis=1)
    
    # Construct a sparse matrix by basically doing:
    # L[v1, v2] = L[v2, v1] = -cota/2
    # L[v2, v0] = L[v0, v2] = -cotb/2
    # L[v0, v1] = L[v1, v0] = -cotc/2
    ii = faces[:, [1, 2, 0]].flatten()
    jj = faces[:, [2, 0, 1]].flatten()
    idx = np.stack([np.concatenate([ii, jj]), np.concatenate([jj, ii])], axis=0)
    val = np.tile(0.5 * cot.flatten(), 2)
    return __coalesce(idx, -val)

def kernel_laplacian(verts, faces, bandwidth):
    """
    Computes the index and data arrays (COO format) of the kernelized Laplacian matrix of
    a mesh:

    ``w_ij = 1/(4*pi*h^2) * exp(-||x_i-x_j||^2/(4*h))`` if vertices i and j are adjacent,
    where ``h`` is a user-specified bandwidth parameter.

    Parameter ``verts`` (``numpy.ndarray`` of shape ``(V, 3)``):
        Vertex coordinates of the mesh.

    Parameter ``faces`` (``numpy.ndarray`` of shape ``(F, 3)``):
        Face indices of the mesh.

    Parameter ``bandwidth`` (``float`` or ``numpy.ndarray`` of shape ``adj[0].shape``):
        Bandwidth parameter ``h``. Either global or one entry per adjacency entry.
        Typical values range from 0.0001 to 0.01.

    Returns ``(numpy.ndarray, numpy.ndarray)``:
        Indices and weights ``w_ij`` of the kernelized Laplacian matrix.

    Inspired by "Discrete laplace operator on meshed surfaces" (Belkin M. et al.)
    """   
    def adjacency_list_inverse(faces):
        ii = faces[:, [1, 2, 0]].flatten()
        jj = faces[:, [2, 0, 1]].flatten()
        return np.unique(np.stack([np.concatenate([ii, jj]), np.concatenate([jj, ii])], axis=0), axis=1, return_inverse=True)

    def dual_areas(verts, faces, adj, inv):
        """
        Computes the dual area for each edge e_ij: ``(sum of the areas of the two faces adjacent to e_ij)/3``
        """
        A, B, C = __side_lengths(verts, faces)
        face_areas = __faces_areas(A, B, C)
        face_areas = face_areas[np.tile(np.repeat(np.arange(len(faces)), 3), 2)]
        dual = dr.zeros(dtype=mi.Float64, shape=adj[0].shape)
        dr.scatter_add(dual, face_areas, mi.UInt(inv))
        return dual.numpy() / 3

    def kern(h, sqr_dist):
        return np.exp(-sqr_dist/(4.0*h)) / (4*math.pi*h*h)

    adj, inv = adjacency_list_inverse(faces)
    sqr_dists = np.sum(np.square(verts[adj[0]] - verts[adj[1]]), axis=1)
    dual = dual_areas(verts, faces, adj, inv)
    K = -kern(bandwidth, sqr_dists) * dual
    return __coalesce(adj, K)

def adaptive_bandwidth(verts, faces, weight, pilot, power=7):
    """
    Computes an adaptive per-vertex bandwidth parameter ``h`` to be used in a Kernelized
    Laplacian (see ``mi.ad.kernel_laplacian``):

    ``h_i = (C * A_i / (pi * ||Lx_i||^2))^(1/d)``
    Where ``C`` is the ``weight`` parameter, ``Lx_i`` is the pilot Laplacian operator
    and ``d`` is the ``power`` parameter.

    Parameter ``verts`` (``numpy.ndarray`` of shape ``(V, 3)``):
        Vertex coordinates of the mesh.

    Parameter ``faces`` (``numpy.ndarray`` of shape ``(F, 3)``):
        Face indices of the mesh.

    Parameter ``weight`` (``float``):
        Weight parameter ``C``. Controls the overall smoothness of the final mesh: smaller values
        leads to smoother meshes wheras larger values better capture details of the mesh.
        Excessively large values tend to produce unstable optimizations.

    Parameter ``pilot`` (``(verts, faces) -> (idx, values)``):
        Function producing the Laplacian matrix used as a pilot given a set of vertices and a set of faces.

    Parameter ``power`` (``float``):
        Parameter ``d``, typically 7 in practical usage.

    Returns ``(numpy.ndarray of shape (V,))``:
        Per-vertex bandwidth parameter ``h``

    Inspired by "Adaptively weighted discrete Laplacian for inverse rendering" (Hyeonjang A. et al.)
    """
    def dual_areas_i(verts, faces):
        V = verts.shape[0]
        A, B, C = __side_lengths(verts, faces)
        face_areas = __faces_areas(A, B, C)

        dual_i = dr.zeros(dtype=mi.Float64, shape=V)
        face_areas = dr.repeat(mi.Float64(face_areas), 3)
        idx = faces.flatten()
        dr.scatter_add(dual_i, face_areas, mi.UInt(idx))
        return dual_i.numpy() / 3

    def _normalize_rows(idx, val, nv):
        row_sum = dr.zeros(mi.TensorXd, shape=nv)
        dr.scatter_add(row_sum.array, mi.Float64(-val), mi.UInt(idx[0]))
        row_sum = row_sum.numpy()
        return val / row_sum[idx[0]]
    
    def _local_min_max(verts, adj):
        V = len(verts)
        idx = np.concatenate(([np.arange(V), np.arange(V)], adj), axis=1)
        adj_verts = verts[idx[1]]

        # /!\ drjit scatter_reduce(ReduceOp.Min/Max, ...) not supported for CUDA except for Float16 with CC>=90 /!\
        mins = np.full((V, 3), np.inf)
        maxs = np.full((V, 3), -np.inf)
        np.minimum.at(mins, idx[0], adj_verts)
        np.maximum.at(maxs, idx[0], adj_verts)

        range_ = (maxs - mins) * 0.5
        return mins - range_, maxs + range_
    
    def _local_verts_diff(verts, adj, min, max):
        mmrange = 2.0/(max[adj[0]] - min[adj[0]])
        mmrange = np.nan_to_num(mmrange, nan=0, posinf=0, neginf=0)
        return (verts[adj[0]]-verts[adj[1]])*mmrange

    V = verts.shape[0]

    # Local normalized coordinates
    adj = adjacency_list(faces)
    local_min, local_max = _local_min_max(verts, adj)
    g_scale = np.linalg.norm(local_max-local_min, ord=2, axis=1) * 0.1
    local_diff = _local_verts_diff(verts, adj, local_min, local_max)

    # Laplacian matrix (rows normalized)
    idx, gamma = pilot(verts, faces)
    gamma = _normalize_rows(idx, gamma, V)

    # Laplacian values (Lx_i = sum_j { gamma_ij * (x_i - x_j))
    l_vals = gamma[:, np.newaxis] * local_diff
    L = dr.zeros(dtype=mi.Point3f, shape=(3, V))
    dr.scatter_add(L.array, mi.Point3f(l_vals.T), mi.UInt(idx[0]))
    L = L.numpy().T

    # Bandwidth h_i = (C * A_i / (pi * norm(Lx_i)^2))^(1/d)
    dual = dual_areas_i(verts, faces)
    L_sqr_norm = np.sum(np.square(L), axis=1) # norm(Lx_i)^2
    h = np.nan_to_num(
        weight * dual / (np.pi * np.maximum(1e-8, L_sqr_norm)), 
        nan=0, 
        posinf=0, 
        neginf=0)
    return np.maximum(np.power(h, 1/power), 1e-8) * g_scale

def adaptive_laplacian(verts, faces, weight, pilot, power=7):
    """
    Computes the index and data arrays (COO format) of the adaptively weighted kernelized 
    Laplacian matrix of a mesh:

    ``w_ij = sqrt(K(x_i, x_j; h_i) * K(x_j, x_i, h_j))`` 
    where ``K(x_i, x_j; h)`` is the standard kernelized Laplacian entry for ``x_i`` and ``x_j``
    with the bandwidth ``h``. 
    
    An adaptive weight ``h_i`` is computed for each vertex ``x_i``:
    ``h_i = (C * A_i / (pi * ||Lx_i||^2))^(1/d)``
    Where ``C`` is the ``weight`` parameter, ``Lx_i`` is the pilot Laplacian operator
    and ``d`` is the ``power`` parameter.

    Parameter ``verts`` (``numpy.ndarray`` of shape ``(V, 3)``):
        Vertex coordinates of the mesh.

    Parameter ``faces`` (``numpy.ndarray`` of shape ``(F, 3)``):
        Face indices of the mesh.

    Parameter ``weight`` (``float``):
        Weight parameter ``C``. Controls the overall smoothness of the final mesh: smaller values
        leads to smoother meshes wheras larger values better capture details of the mesh.
        Excessively large values tend to produce unstable optimizations.

    Parameter ``pilot`` (``(verts, faces) -> (idx, values)``):
        Function producing the Laplacian matrix used as a pilot given a set of vertices and a set of faces.

    Parameter ``power`` (``float``):
        Parameter ``d``, typically 7 in practical usage.

    Returns ``(numpy.ndarray, numpy.ndarray)``:
        Indices and weights ``w_ij`` of the adaptively weighted kernelized Laplacian matrix.

    Inspired by "Adaptively weighted discrete Laplacian for inverse rendering" (Hyeonjang A. et al.)
    """   
    h = adaptive_bandwidth(verts, faces, weight, pilot, power)

    adj = adjacency_list(faces)    
    ii, Ki = kernel_laplacian(verts, faces, h[adj[0]])
    ij, Kj = kernel_laplacian(verts, faces, h[adj[1]])
    assert np.all(ii == ij)
    return ii, -np.sqrt(Ki*Kj)
