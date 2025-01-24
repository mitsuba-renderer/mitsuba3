from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

import mitsuba as mi
import drjit as dr

class SolveCholesky(dr.CustomOp):
    """
    DrJIT custom operator to solve a linear system using a Cholesky factorization.
    """

    def eval(self, solver, u):
        self.solver = solver
        x = dr.empty(mi.TensorXf, shape=u.shape)
        solver.solve(u, x)
        return mi.TensorXf(x)

    def forward(self):
        x = dr.empty(mi.TensorXf, shape=self.grad_in('u').shape)
        self.solver.solve(self.grad_in('u'), x)
        self.set_grad_out(x)

    def backward(self):
        x = dr.empty(mi.TensorXf, shape=self.grad_out().shape)
        self.solver.solve(self.grad_out(), x)
        self.set_grad_in('u', x)

    def name(self):
        return "Cholesky solve"


class ShapeOptimizer:
    """
    Implementation of the algorithm described in the paper "Large Steps in
    Inverse Rendering of Geometry" (Nicolet et al. 2021).

    It consists in computing a latent variable u = (I + λL) v from the vertex
    positions v, where L is a Laplacian matrix of the input mesh. 
    Optimizing these variables instead of the vertex positions allows to
    diffuse gradients on the surface, which helps fight their sparsity.

    This class builds the system matrix (I + λL) for a given mesh and hyper
    parameter λ, and computes its Cholesky factorization.

    It can then convert vertex coordinates back and forth between their
    cartesian and differential representations. Both transformations are
    differentiable, meshes can therefore be optimized by using the differential
    form as a latent variable.
    """
    def __init__(self, verts, faces, lambda_, alpha, laplacian):
        """
        Build the system matrix and its Cholesky factorization.

        Parameter ``verts`` (``mitsuba.Float``):
            Vertex coordinates of the mesh.

        Parameter ``faces`` (``mitsuba.UInt``):
            Face indices of the mesh.

        Parameter ``lambda_`` (``float``):
            The hyper parameter λ. This controls how much gradients are diffused
            on the surface. this value should increase with the tesselation of
            the mesh.

        Parameter ``alpha`` (``float``):
            Alternative hyper parameter, used to compute the parameterization 
            matrix as ((1-alpha) * I + alpha * L). 
            It has precedence over ``lambda_``.

        Parameter ``laplacian`` (``(numpy.ndarray, numpy.ndarray) -> (numpy.ndarray, numpy.ndarray)``):
            Function used to compute the Laplacian matrix used to build the system matrix (I + λL).

            It should expect two parameters: vertex positions as a numpy array of shape ``(V, 3)`` 
            and face indices as a numpy array of shape ``(F, 3)``.

            It should return the index and data arrays (COO format) of a Laplacian matrix 
            associated to these vertex and face sets.
        """
        if mi.variant().endswith('double'):
            from cholespy import CholeskySolverD as CholeskySolver
        else:
            from cholespy import CholeskySolverF as CholeskySolver

        from cholespy import MatrixType
        import numpy as np

        v = verts.numpy().reshape((-1,3))
        f = faces.numpy().reshape((-1,3))

        # Remove duplicates due to e.g. UV seams or face normals.
        # This is necessary to avoid seams opening up during optimisation
        v_unique, index_v, inverse_v = np.unique(v, return_index=True, return_inverse=True, axis=0)
        inverse_v = inverse_v.flatten()
        f_unique = inverse_v[f]

        self.index = mi.UInt(index_v)
        self.inverse = mi.UInt(inverse_v)
        self.n_verts = v_unique.shape[0]

        # Compute laplacian matrix
        self.lap_idx, self.lap_val = laplacian(v_unique, f_unique)
        idx, val = np.copy(self.lap_idx), np.copy(self.lap_val)

        # Multiply by lambda or alpha
        if alpha is None:
            val *= lambda_
        else:
            if alpha < 0.0 or alpha >= 1.0:
                raise ValueError(f"Invalid value for alpha: {alpha} : it should take values between 0 (included) and 1 (excluded)")
            val *= alpha

        # Add diagonal entries ((1-alpha)I - sum of the row)
        idx = np.concatenate((
            idx, 
            [idx[0], idx[0]], 
            [np.arange(self.n_verts), np.arange(self.n_verts)]
        ), axis=1)
        val = np.concatenate((
            val, 
            -val, 
            np.ones((self.n_verts,)) * (1.0 if alpha is None else (1 - alpha)),
        ))
        idx_unique, inverse_idx = np.unique(idx, axis=1, return_inverse=True)
        inverse_idx = inverse_idx.flatten()
        data = dr.zeros(mi.TensorXd, shape=(idx_unique.shape[1],))
        dr.scatter_add(data.array, mi.Float64(val), mi.UInt(inverse_idx))

        self.rows = mi.TensorXi(idx_unique[0])
        self.cols = mi.TensorXi(idx_unique[1])
        self.solver = CholeskySolver(self.n_verts, self.rows, self.cols, data, MatrixType.COO)
        self.data = mi.TensorXf(data)

    def to_differential(self, v):
        """
        Convert vertex coordinates to their differential form: u = (I + λL) v.

        This method typically only needs to be called once per mesh, to obtain
        the latent variable before optimization.

        Parameter ``v`` (``mitsuba.Float``):
            Vertex coordinates of the mesh.

        Returns ``mitsuba.Float``:
            Differential form of v.
        """

        # Manual matrix-vector multiplication
        v_unique = dr.gather(mi.Point3f, dr.unravel(mi.Point3f, v), self.index)
        row_prod = dr.gather(mi.Point3f, v_unique, self.cols.array) * self.data.array
        u = dr.zeros(mi.Point3f, dr.width(v_unique))
        dr.scatter_add(u, row_prod, self.rows.array)
        return dr.ravel(u)

    def from_differential(self, u):
        """
        Convert differential coordinates back to their cartesian form: v = (I +
        λL)⁻¹ u.

        This is done by solving the linear system (I + λL) v = u using the
        previously computed Cholesky factorization.

        This method is typically called at each iteration of the optimization,
        to update the mesh coordinates before rendering.

        Parameter ``u`` (``mitsuba.Float``):
            Differential form of v.

        Returns ``mitsuba.Float`:
            Vertex coordinates of the mesh.
        """
        v_unique = dr.unravel(mi.Point3f, dr.custom(SolveCholesky, self.solver, mi.TensorXf(u, (self.n_verts, 3))).array)
        return dr.ravel(dr.gather(mi.Point3f, v_unique, self.inverse))

class LargeSteps(ShapeOptimizer):
    """
    Specific ``ShapeOptimizer`` using a combinatorial Laplacian.
    """

    def __init__(self, verts, faces, lambda_=19.0, alpha=None):
        laplacian = lambda _, f: mi.ad.combinatorial_laplacian(f)
        super().__init__(verts, faces, lambda_=lambda_, alpha=alpha, laplacian=laplacian)
