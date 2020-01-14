import mitsuba
import pytest
import enoki as ek

from mitsuba.python.test import variant_scalar


def test01_depolarizer(variant_scalar):
    from mitsuba.render.mueller import depolarizer

    # Remove all polarization, and change intensity based on transmittance value.
    assert ek.allclose(depolarizer(.8) @ [1, .5, .5, .5], [.8, 0, 0, 0])


def test02_rotator(variant_scalar):
    from mitsuba.render.mueller import rotator

    # Linearly polarized horizontal light is rotated by +- 45˚
    assert ek.allclose(rotator( 45 * ek.pi/180) @ [1, 1, 0, 0], [1, 0,  1, 0], atol=1e-5)
    assert ek.allclose(rotator(-45 * ek.pi/180) @ [1, 1, 0, 0], [1, 0, -1, 0], atol=1e-5)


def test03_linear_polarizer(variant_scalar):
    from mitsuba.render.mueller import rotated_element, linear_polarizer

    # Malus' law
    angle = 30 * ek.pi/180
    value_malus = ek.cos(angle)**2
    polarizer = rotated_element(angle, linear_polarizer(1.0))

    stokes_in  = [1, 1, 0, 0]
    stokes_out = polarizer @ stokes_in
    intensity  = stokes_out[0]
    assert ek.allclose(intensity, value_malus)


def test04_specular_reflection(variant_scalar):
    from mitsuba.core import Matrix4f
    from mitsuba.render.mueller import specular_reflection
    import numpy as np

    # Identity matrix (and no phase shift) at perpendicular incidence
    assert ek.allclose(specular_reflection(1, 1.5),   Matrix4f.identity()*0.04)
    assert ek.allclose(specular_reflection(1, 1/1.5), Matrix4f.identity()*0.04)

    # 180 deg phase shift at grazing incidence
    assert ek.allclose(specular_reflection(0, 1.5),   [[1,0,0,0],[0,1,0,0],[0,0,-1,0],[0,0,0,-1]], atol=1e-6)
    assert ek.allclose(specular_reflection(0, 1/1.5), [[1,0,0,0],[0,1,0,0],[0,0,-1,0],[0,0,0,-1]], atol=1e-6)

    # Perfect linear polarization at Brewster's angle
    M = np.zeros((4, 4))
    M[0:2, 0:2] = 0.0739645
    assert ek.allclose(specular_reflection(ek.cos(ek.atan(1/1.5)), 1/1.5), M, atol=1e-6)
    assert ek.allclose(specular_reflection(ek.cos(ek.atan(1.5)), 1.5), M, atol=1e-6)

    # 180 deg phase shift just below Brewster's angle (air -> glass)
    M = specular_reflection(ek.cos(ek.atan(1.5)+1e-4), 1.5)
    assert M[0, 0] > 0 and M[1, 1] > 0 and M[2, 2] < 0 and M[3, 3] < 0
    M = specular_reflection(ek.cos(ek.atan(1/1.5)+1e-4), 1/1.5)
    assert M[0, 0] > 0 and M[1, 1] > 0 and M[2, 2] < 0 and M[3, 3] < 0

    # Complex phase shift in the total internal reflection case
    eta = 1/1.5
    cos_theta_min = ek.sqrt((1 - eta**2) / (1 + eta**2))
    M = specular_reflection(cos_theta_min, eta).numpy()
    assert np.all(M[0:2, 2:4] == 0) and np.all(M[2:4, 0:2] == 0) and ek.allclose(M[0:2, 0:2], np.eye(2), atol=1e-6)
    phi_delta = 4*ek.atan(eta)
    assert ek.allclose(M[2:4, 2:4], [[ek.cos(phi_delta), ek.sin(phi_delta)], [-ek.sin(phi_delta), ek.cos(phi_delta)]])


def test05_specular_transmission(variant_scalar):
    from mitsuba.core import Matrix4f
    from mitsuba.render.mueller import specular_transmission
    import numpy as np

    assert np.allclose(specular_transmission(1, 1.5), Matrix4f.identity() * 0.96, atol=1e-5) #TODO ek.allclose still broken for Matrix4f
    assert np.allclose(specular_transmission(1, 1/1.5), Matrix4f.identity() * 0.96, atol=1e-5)
    assert np.allclose(specular_transmission(1e-7, 1.5), Matrix4f.zero(), atol=1e-5)
    assert np.allclose(specular_transmission(1e-7, 1/1.5), Matrix4f.zero(), atol=1e-5)
    assert np.allclose(specular_transmission(0, 1.5), Matrix4f.zero(), atol=1e-5)
    assert np.allclose(specular_transmission(0, 1/1.5), Matrix4f.zero(), atol=1e-5)

    ref = np.array([[ 0.9260355 , -0.07396451,  0.        ,  0.        ],
                    [-0.07396451,  0.9260355 ,  0.        ,  0.        ],
                    [ 0.        ,  0.        ,  0.92307705,  0.        ],
                    [ 0.        ,  0.        ,  0.        ,  0.92307705]])
    assert ek.allclose(specular_transmission(ek.cos(ek.atan(1/1.5)), 1/1.5), ref)


def test06_rotate_stokes_basis(variant_scalar):
    from mitsuba.render.mueller import stokes_basis, rotate_stokes_basis
    import numpy as np

    # Each Stokes vector describes light polarization only w.r.t. a certain reference
    # frame determined by unit vector perpendicular to the direction of travel.
    #
    # Here we test a duality between rotating the polarization direction (e.g. from
    # linear polarization at 0˚ to 90˚) and rotating the Stokes reference frame
    # in the opposite direction.
    # We start out with horizontally polarized light (Stokes vector [1, 1, 0, 0])
    # and perform several transformations.

    w = np.array([0, 0, 1]) # Optical axis / forward direction

    s_00 = np.array([1, 1, 0, 0]) # Horizontally polarized light
    b_00 = stokes_basis(w) # Corresponding Stokes basis

    def rotate_vector(v, axis, angle):
            return mitsuba.core.Transform4f.rotate(axis, angle).matrix.numpy()[0:3, 0:3] @ v

    # Switch to basis rotated by 90˚.
    # Now polarization should also be at 90˚ (with Stokes vector [1, -1, 0, 0])
    b_90 = rotate_vector(b_00, -w, 90.0)
    s_90 = rotate_stokes_basis(w, b_00, b_90) @ s_00
    assert ek.allclose(s_90, [1.0, -1.0, 0.0, 0.0], atol=1e-3)

    # Switch to basis rotated by +45˚.
    # Now polarization should be at -45˚ (with Stokes vector [1, 0, -1, 0])
    b_45 = rotate_vector(b_00, -w, +45.0)
    s_45 = rotate_stokes_basis(w, b_00, b_45) @ s_00
    assert ek.allclose(s_45, [1.0, 0.0, -1.0, 0.0], atol=1e-3)

    # Switch to basis rotated by -45˚.
    # Now polarization should be at +45˚ (with Stokes vector [1, 0, +1, 0])
    b_45_neg = rotate_vector(b_00, -w, -45.0)
    s_45_neg = rotate_stokes_basis(w, b_00, b_45_neg) @ s_00
    assert ek.allclose(s_45_neg, [1.0, 0.0, +1.0, 0.0], atol=1e-3)


def test07_rotate_mueller_basis(variant_scalar):
    from mitsuba.render.mueller import stokes_basis, rotated_element, rotate_mueller_basis, \
                                       rotate_mueller_basis_collinear, linear_polarizer
    import numpy as np

    # If we have an optical element such as a linear polarizer, its rotation around
    # the optical axis can also be interpreted as a change of both incident and
    # outgoing Stokes reference frames:

    w = np.array([0, 0, 1])       # Optical axis / forward direction

    s_00 = np.array([1, 1, 0, 0]) # Horizontally polarized light
    b_00 = stokes_basis(w)        # Corresponding Stokes basis

    M = linear_polarizer()

    def rotate_vector(v, axis, angle):
            return mitsuba.core.Transform4f.rotate(axis, angle).matrix.numpy()[0:3, 0:3] @ v

    # As reference, rotate the element directly by -45˚
    M_rotated_element = rotated_element(-45 * ek.pi/180, M)

    # Now rotate the two reference bases by 45˚ instead and make sure results are
    # equivalent.
    b_45 = rotate_vector(b_00, -w, +45.0)
    M_rotated_bases_1 = rotate_mueller_basis(M,
                                             w, b_00, b_45,
                                             w, b_00, b_45)
    assert np.allclose(M_rotated_element, M_rotated_bases_1, atol=1e-5)

    # Also test alternative rotation method that assumes collinear incident and
    # outgoing directions.
    M_rotated_bases_2 = rotate_mueller_basis_collinear(M, w, b_00, b_45)
    assert np.allclose(M_rotated_element, M_rotated_bases_2, atol=1e-5)
