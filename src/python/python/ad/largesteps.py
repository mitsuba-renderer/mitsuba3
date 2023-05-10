from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

import mitsuba as mi
import drjit as dr
import numpy as np

def mesh_laplacian(n_verts, faces, lambda_):
    """
    Compute the index and data arrays of the (combinatorial) Laplacian matrix of
    a given mesh.
    """
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

class SolveCholesky(dr.CustomOp):
    """
    DrJIT custom operator to solve a linear system using a Cholesky factorization.
    """

    def eval(self, solver, u):
        self.solver = solver
        x = dr.empty(dr.cuda.TensorXf, shape=u.shape)
        solver.solve(u, x)
        return mi.TensorXf(x)

    def forward(self):
        x = dr.empty(mi.TensorXf, shape=self.grad_in('u').shape)
        self.solver.solve(self.grad_in('u'), x)
        self.set_grad_out(x)

    def backward(self):
        x = dr.empty(dr.cuda.TensorXf, shape=self.grad_out().shape)
        self.solver.solve(self.grad_out(), x)
        self.set_grad_in('u', x)

    def name(self):
        return "Cholesky solve"

class LargeSteps():
    """
    Implementation of the algorithm described in the paper "Large Steps in
    Inverse Rendering of Geometry" (Nicolet et al. 2021)

    It builds the system matrix (I +λL) for a given mesh and hyper parameter λ,
    and computes its Cholesky factorization.

    It can then convert vertex coordinates back and forth between their
    cartesian and differential representations. Both transformations are
    differentiable, which allows optimizing meshes using the differential form
    as a latent variable.
    """
    def __init__(self, verts, faces, lambda_=19.0):
        """
        Build the system matrix and its Cholesky factorization.

        Params
        ------

        verts: mi.Float
            The vertex coordinates of the mesh.

        faces: mi.UInt
            The face indices of the mesh.

        lambda_: Float
            The hyper parameter λ. This controls how much gradients are diffused
            on the surface. this value should increase with the tesselation of
            the mesh.

        """
        if mi.variant().endswith('double'):
            from cholespy import CholeskySolverD as CholeskySolver
        else:
            from cholespy import CholeskySolverF as CholeskySolver

        from cholespy import MatrixType

        v = verts.numpy().reshape((-1,3))
        f = faces.numpy().reshape((-1,3))

        # Remove duplicates due to e.g. UV seams or face normals.
        # This is necessary to avoid seams opening up during optimisation
        v_unique, index_v, inverse_v = np.unique(v, return_index=True, return_inverse=True, axis=0)
        f_unique = inverse_v[f]

        self.index = mi.UInt(index_v)
        self.inverse = mi.UInt(inverse_v)
        self.n_verts = v_unique.shape[0]

        # CHOLMOD expects matrices without duplicate entries as input, so we need to sum them manually
        indices, values = mesh_laplacian(self.n_verts, f_unique, lambda_)
        indices_unique, inverse_idx = np.unique(indices, axis=1, return_inverse=True)

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

        Params
        ------

        v: Float
            The vertex coordinates of the mesh.

        Returns
        -------

        u: Float
            The differential form of v.
        """
        # TODO: support arbitrary components, not just 3

        # Manual matrix-vector multiplication
        v_unique = dr.gather(mi.Point3f, dr.unravel(mi.Point3f, v), self.index)
        row_prod = dr.gather(mi.Point3f, v_unique, self.cols.array) * self.data.array
        u = dr.zeros(mi.Point3f, dr.width(v_unique))
        dr.scatter_reduce(dr.ReduceOp.Add, u, row_prod, self.rows.array)

        return dr.ravel(u)

    def from_differential(self, u):
        """
        Convert differential coordinates back to their cartesian form: v = (I +
        λL)⁻¹ u.

        This method is typically called at each iteration of the optimization,
        to update the mesh coordinates before rendering.

        Params
        ------

        u: Float
            The differential form of v.

        Returns
        -------

        v: Float
            The vertex coordinates of the mesh.
        """
        v_unique = dr.unravel(mi.Point3f, dr.custom(SolveCholesky, self.solver, mi.TensorXf(u, (self.n_verts, 3))).array)
        return dr.ravel(dr.gather(mi.Point3f, v_unique, self.inverse))
