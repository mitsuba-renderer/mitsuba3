from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

import mitsuba as mi
import drjit as dr

def mesh_laplacian(n_verts, faces, lambda_):
    """
    Compute the index and data arrays of the (combinatorial) Laplacian matrix of
    a given mesh.
    """
    import numpy as np

    # Neighbor indices
    ii = faces[:, [1, 2, 0]].flatten()
    jj = faces[:, [2, 0, 1]].flatten()
    adj = np.unique(np.stack([np.concatenate([ii, jj]), np.concatenate([jj, ii])], axis=0), axis=1)
    adj_values = np.ones(adj.shape[1], dtype=np.float64) * lambda_

    # Diagonal indices, duplicated as many times as the connectivity of each index
    diag_idx = np.stack((adj[0], adj[0]), axis=0)

    diag = np.stack((np.arange(n_verts), np.arange(n_verts)), axis=0)

    # Build the sparse matrix
    idx = np.concatenate((adj, diag_idx, diag), axis=1)
    values = np.concatenate((-adj_values, adj_values, np.ones(n_verts)))

    return idx, values

def _cholesky_solve(solver, b):
    """
    Solve for the right-hand side ``b`` and return the result as a tensor.

    Cholespy writes the solution into its second argument, for which it
    expects a CUDA or host array. On the Metal backend it instead receives
    a host copy, into which it would write the solution unnoticed.
    """
    if dr.backend_v(mi.Float) == dr.JitBackend.Metal:
        import numpy as np
        b_np = np.array(b, dtype=np.float32)
        x_np = np.zeros_like(b_np)
        solver.solve(b_np, x_np)
        return mi.TensorXf(x_np)

    x = dr.empty(mi.TensorXf, shape=b.shape)
    solver.solve(b, x)
    return mi.TensorXf(x)


class SolveCholesky(dr.CustomOp):
    """
    DrJIT custom operator to solve a linear system using a Cholesky factorization.
    """

    def eval(self, solver, u):
        self.solver = solver
        return _cholesky_solve(solver, u)

    def forward(self):
        self.set_grad_out(_cholesky_solve(self.solver, self.grad_in('u')))

    def backward(self):
        self.set_grad_in('u', _cholesky_solve(self.solver, self.grad_out()))

    def name(self):
        return "Cholesky solve"


class LargeSteps():
    """
    Implementation of the algorithm described in the paper "Large Steps in
    Inverse Rendering of Geometry" (Nicolet et al. 2021).

    It consists in computing a latent variable u = (I + λL) v from the vertex
    positions v, where L is the (combinatorial) Laplacian matrix of the input
    mesh. Optimizing these variables instead of the vertex positions allows to
    diffuse gradients on the surface, which helps fight their sparsity.

    This class builds the system matrix (I + λL) for a given mesh and hyper
    parameter λ, and computes its Cholesky factorization.

    It can then convert vertex coordinates back and forth between their
    cartesian and differential representations. Both transformations are
    differentiable, meshes can therefore be optimized by using the differential
    form as a latent variable.
    """
    @classmethod
    def from_mesh(cls, mesh, lambda_=19.0):
        """
        Build a LargeSteps instance from a mesh, using its stored geometric
        topology.

        This consumes the mesh's coarse representation directly: the
        authoritative surface point positions and the faces re-indexed into
        surface point space. The Laplacian therefore stays connected across
        UV seams without comparing position values, and the differential
        form corresponds to the ``positions`` scene parameter.
        """
        import numpy as np
        self = cls.__new__(cls)
        f = np.array(mesh.geometric_faces())
        identity = np.arange(mesh.position_count(), dtype=np.uint32)
        self._build(f, identity, identity, lambda_)
        return self

    def __init__(self, verts, faces, lambda_=19.0):
        """
        Build the system matrix and its Cholesky factorization.

        Coincident vertex coordinates are fused so that duplicates
        introduced by e.g. UV seams or face normals cannot drift apart
        during optimization. Meshes that store a vertex -> surface point
        map should use `from_mesh`, which relies on that stored
        topology instead of value comparisons.

        Args:
            verts: ``(V, 3)`` tensor of vertex coordinates (the ``positions``
                scene parameter).

            faces: ``(F, 3)`` tensor of face indices (the ``faces`` scene
                parameter).

            lambda_: The hyper parameter λ. This controls how much gradients
                are diffused on the surface. this value should increase with
                the tesselation of the mesh.
        """
        import numpy as np

        v = np.asarray(verts)
        f = np.asarray(faces)
        if v.ndim != 2 or v.shape[1] != 3 or f.ndim != 2 or f.shape[1] != 3:
            raise ValueError("LargeSteps(): expected (V, 3) vertex and "
                             "(F, 3) face tensors")

        # Remove duplicates due to e.g. UV seams or face normals.
        # This is necessary to avoid seams opening up during optimisation
        _, index_v, inverse_v = np.unique(v, return_index=True, return_inverse=True, axis=0)
        inverse_v = inverse_v.flatten()

        self._build(inverse_v[f], index_v, inverse_v, lambda_)

    def _build(self, f_unique, index_v, inverse_v, lambda_):
        if mi.variant().endswith('double'):
            from cholespy import CholeskySolverD as CholeskySolver
        else:
            from cholespy import CholeskySolverF as CholeskySolver

        from cholespy import MatrixType
        import numpy as np

        self.index = mi.UInt(index_v)
        self.inverse = mi.UInt(inverse_v)
        self.n_verts = index_v.shape[0]

        # Solver expects matrices without duplicate entries as input, so we need to sum them manually
        indices, values = mesh_laplacian(self.n_verts, f_unique, lambda_)
        indices_unique, inverse_idx = np.unique(indices, axis=1, return_inverse=True)
        inverse_idx = inverse_idx.flatten()

        self.rows = mi.TensorXi(indices_unique[0])
        self.cols = mi.TensorXi(indices_unique[1])
        data = dr.zeros(mi.TensorXd, shape=(indices_unique.shape[1],))

        dr.scatter_reduce(dr.ReduceOp.Add, data.array, mi.Float64(values), mi.UInt(inverse_idx))

        self.solver = CholeskySolver(self.n_verts, self.rows, self.cols, data, MatrixType.COO)
        self.data = mi.TensorXf(data)

    def to_differential(self, v):
        """
        Convert vertex coordinates to their differential form: u = (I + λL) v.

        This method typically only needs to be called once per mesh, to obtain
        the latent variable before optimization.

        Args:
            v: ``(V, 3)`` tensor of vertex coordinates, such as the
                ``positions`` scene parameter (also accepts a ``Point3f`` or
                a flat array).

        Returns:
            ``(N, 3)`` tensor holding the differential form of ``v``, where
            ``N`` is the number of vertices after fusing duplicates.
        """
        # Perform a sparse matrix-vector product
        cols = dr.gather(mi.UInt32, self.index, self.cols.array)  # rows of ``v``
        row_prod = dr.gather(mi.Point3f, mi.Float(dr.ravel(v)), cols) * self.data.array
        u = dr.zeros(mi.Float, 3 * self.n_verts)
        dr.scatter_reduce(dr.ReduceOp.Add, u, row_prod, self.rows.array)

        return mi.TensorXf(u, shape=(self.n_verts, 3))

    def from_differential(self, u):
        """
        Convert differential coordinates back to their cartesian form: v = (I +
        λL)⁻¹ u.

        This is done by solving the linear system (I + λL) v = u using the
        previously computed Cholesky factorization.

        This method is typically called at each iteration of the optimization,
        to update the mesh coordinates before rendering.

        Args:
            u: ``(N, 3)`` tensor holding the differential form of ``v`` (also
                accepts a ``Point3f`` or a flat array).

        Returns:
            ``(V, 3)`` tensor of vertex coordinates, ready to be assigned
            back to the ``positions`` scene parameter.
        """
        u = mi.TensorXf(mi.Float(dr.ravel(u)), shape=(self.n_verts, 3))
        v = dr.custom(SolveCholesky, self.solver, u)

        # Undo the deduplication by picking the row of each original vertex
        return dr.take(v, self.inverse, axis=0)
