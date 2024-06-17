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

    value = dr.reshape(dr.scalar.TensorXf, dr.ravel(value), shape=(4,4,4))

    value = value[0,:,:]  # Extract Mueller matrix for one wavelength

    ref =  [[0.171191, -0.00224344, 0.00754325, 0.000100205],
            [-0.00392327, 0.00427306, -0.001145, -0.00310216],
            [-0.00424985, 0.00315571, -0.0121926, 0.000856727],
            [0.000990057, -0.00345507, -0.00285894, -0.00205486]]

    assert dr.allclose(ref, value)
