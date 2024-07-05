import mitsuba
import pytest
import drjit as dr
import mitsuba as mi
import numpy as np

from mitsuba.scalar_rgb.test.util import fresolver_append_path

@fresolver_append_path
def test01_evaluation(variant_scalar_spectral_polarized):
    # Here we load a small example pBSDF file and evaluate the BSDF for a fixed
    # incident and outgoing position. Any future changes to polarization frames
    # or table interpolation should keep the values below unchanged.
    #
    # For convenience, we use a file where the usual resolution of parameters
    # (phi_d x theta_d x theta_h x wavelengths x Mueller mat. ) was significantly
    # downsampled from (361 x 91 x 91 x 5 x 4 x 4) to (22 x 9 x 9 x 5 x 4 x 4).

    bsdf = mi.load_dict({
        'type': 'measured_polarized',
        'filename': 'resources/data/tests/pbsdf/spectralon_lowres.pbsdf'
    })

    phi_i   = 30 * dr.pi / 180.0
    theta_i = 10 * dr.pi / 180.0
    wi = mi.Vector3f([dr.sin(theta_i) * dr.cos(phi_i),
                      dr.sin(theta_i) * dr.sin(phi_i),
                      dr.cos(theta_i)])

    ctx = mi.BSDFContext()
    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.wi = wi
    si.sh_frame = mi.Frame3f([0, 0, 1])
    si.wavelengths = [500, 500, 500, 500]

    phi_o   = 200 * dr.pi / 180.0
    theta_o = 3 * dr.pi / 180.0
    wi = mi.Vector3f([dr.sin(theta_o) * dr.cos(phi_o),
                      dr.sin(theta_o) * dr.sin(phi_o),
                      dr.cos(theta_o)])

    value = bsdf.eval(ctx, si, wi)

    # Manually extract matrix for first wavelength
    # TODO: An issue with ArrayBase empty base class optimization
    # This means that the data isn't tightly packed
    mtx = mi.ScalarMatrix4f()
    for i in range(0,4):
        for j in range(0,4):
            mtx[i, j] = value[i,j][0]

    ref = [[ 0.17119151, -0.00223141,  0.00754681,  0.00010021],
           [-0.00393003,  0.00427623, -0.00117126, -0.00310079],
           [-0.00424358,  0.00312945, -0.01219576,  0.00086167],
           [ 0.00099006, -0.00345963, -0.00285343, -0.00205485]]

    assert dr.allclose(ref, mtx)
