import mitsuba
import pytest
import enoki as ek
import numpy as np

from mitsuba.python.test.util import fresolver_append_path

@fresolver_append_path
def test01_evaluation(variant_scalar_spectral_polarized):
    from mitsuba.core import Vector3f, Frame3f
    from mitsuba.core.xml import load_dict
    from mitsuba.render import BSDFContext, SurfaceInteraction3f

    # Here we load a small example pBSDF file and evaluate the BSDF for a fixed
    # incident and outgoing position. Any future changes to polarization frames
    # or table interpolation should keep the values below unchanged.
    #
    # For convenience, we use a file where the usual resolution of parameters
    # (phi_d x theta_d x theta_h x wavlengths x Mueller mat. ) was significantly
    # downsampled from (361 x 91 x 91 x 5 x 4 x 4) to (22 x 9 x 9 x 5 x 4 x 4).

    bsdf = load_dict({'type': 'measured_polarized',
                      'filename': 'resources/data/tests/pbsdf/spectralon_lowres.pbsdf'})

    phi_i   = 30 * ek.Pi/180
    theta_i = 10 * ek.Pi/180
    wi = Vector3f([ek.sin(theta_i)*ek.cos(phi_i),
                   ek.sin(theta_i)*ek.sin(phi_i),
                   ek.cos(theta_i)])

    ctx = BSDFContext()
    si = SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.wi = wi
    si.sh_frame = Frame3f([0, 0, 1])
    si.wavelengths = [500, 500, 500, 500]

    phi_o   = 200 * ek.Pi/180
    theta_o =  3 * ek.Pi/180
    wi = Vector3f([ek.sin(theta_o)*ek.cos(phi_o),
                   ek.sin(theta_o)*ek.sin(phi_o),
                   ek.cos(theta_o)])

    value = bsdf.eval(ctx, si, wi)
    value = np.array(value)[0,:,:]  # Extract Mueller matrix for one wavelength

    ref = [[ 0.17119151, -0.00223141,  0.00754681,  0.00010021],
           [-0.00393003,  0.00427623, -0.00117126, -0.00310079],
           [-0.00424358,  0.00312945, -0.01219576,  0.00086167],
           [ 0.00099006, -0.00345963, -0.00285343, -0.00205485]]
    assert ek.allclose(ref, value, rtol=1e-4)