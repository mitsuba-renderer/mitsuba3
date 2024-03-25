import pytest
import drjit as dr
import mitsuba as mi

from drjit.scalar import Array4f


def test01_depolarizer(variant_scalar_rgb):
    # Remove all polarization, and change intensity based on transmittance value.
    assert dr.allclose(mi.mueller.depolarizer(.8) @ Array4f([1, .5, .5, .5]), [.8, 0, 0, 0])


def test02_rotator(variant_scalar_rgb):
    # Start with horizontally linear polarized light [1,1,0,0] and ..
    # .. rotate the Stokes frame by +45˚
    assert dr.allclose(mi.mueller.rotator( 45 * dr.pi/180) @ Array4f([1, 1, 0, 0]), [1, 0, -1, 0], atol=1e-5)
    # .. rotate the Stokes frame by -45˚
    assert dr.allclose(mi.mueller.rotator(-45 * dr.pi/180) @ Array4f([1, 1, 0, 0]), [1, 0, +1, 0], atol=1e-5)


def test03_linear_polarizer(variant_scalar_rgb):
    # Malus' law
    angle = 30 * dr.pi/180
    value_malus = dr.cos(angle)**2
    polarizer = mi.mueller.rotated_element(angle, mi.mueller.linear_polarizer(1.0))

    stokes_in  = Array4f([1, 1, 0, 0])
    stokes_out = polarizer @ stokes_in
    intensity  = stokes_out[0]
    assert dr.allclose(intensity, value_malus)


def test04_linear_polarizer_rotated(variant_scalar_rgb):
    # The closed-form expression for a rotated linear polarizer is available
    # in many textbooks and should match the behavior of manually rotating the
    # optical element.
    angle = 33 * dr.pi/180
    s, c = dr.sin(2*angle), dr.cos(2*angle)
    M_ref = dr.scalar.Matrix4f([[1, c, s, 0],
                                [c, c*c, s*c, 0],
                                [s, s*c, s*s, 0],
                                [0, 0, 0, 0]]) * 0.5
    R = mi.mueller.rotator(angle)
    L = mi.mueller.linear_polarizer()
    M = mi.mueller.rotated_element(angle, L)
    assert dr.allclose(M, M_ref)


def test05_specular_reflection(variant_scalar_rgb):
    import numpy as np

    # Perpendicular incidence: 180 deg phase shift
    assert dr.allclose(mi.mueller.specular_reflection(1, 1.5),   mi.mueller.linear_retarder(dr.pi)*0.04)
    assert dr.allclose(mi.mueller.specular_reflection(1, 1/1.5), mi.mueller.linear_retarder(dr.pi)*0.04)

    # Just before Brewster's angle: 180 deg phase shift
    M = mi.mueller.specular_reflection(dr.cos(dr.atan(1.5) - 0.01), 1.5)
    assert M[0, 0] > 0 and M[1, 1] > 0 and M[2, 2] < 0 and M[3, 3] < 0
    M = mi.mueller.specular_reflection(dr.cos(dr.atan(1/1.5) - 0.01), 1/1.5)
    assert M[0, 0] > 0 and M[1, 1] > 0 and M[2, 2] < 0 and M[3, 3] < 0

    # Brewster's angle: perfect linear polarization
    M = dr.zeros(mi.TensorXf, [4, 4])
    M = np.zeros((4, 4))
    M[0:2, 0:2] = 0.0739645
    assert dr.allclose(mi.mueller.specular_reflection(dr.cos(dr.atan(1/1.5)), 1/1.5), M, atol=1e-6)
    assert dr.allclose(mi.mueller.specular_reflection(dr.cos(dr.atan(1.5)), 1.5), M, atol=1e-6)

    # Just after Brewster's angle: no phase shift
    M = mi.mueller.specular_reflection(dr.cos(dr.atan(1.5) + 0.01), 1.5)
    assert M[0, 0] > 0 and M[1, 1] > 0 and M[2, 2] > 0 and M[3, 3] > 0
    M = mi.mueller.specular_reflection(dr.cos(dr.atan(1/1.5) + 0.01), 1/1.5)
    assert M[0, 0] > 0 and M[1, 1] > 0 and M[2, 2] > 0 and M[3, 3] > 0

    # Grazing incidence: identity matrix and no phase shift
    assert dr.allclose(mi.mueller.specular_reflection(0, 1.5),   dr.identity(mi.Matrix4f), atol=1e-6)
    assert dr.allclose(mi.mueller.specular_reflection(0, 1/1.5), dr.identity(mi.Matrix4f), atol=1e-6)

    # Complex phase shift in the total internal reflection case
    eta = 1/1.5
    cos_theta_max = dr.sqrt((1 - eta**2) / (1 + eta**2))
    M = mi.mueller.specular_reflection(cos_theta_max, eta).numpy()
    assert np.all(M[0:2, 2:4] == 0) and np.all(M[2:4, 0:2] == 0) and dr.allclose(M[0:2, 0:2], np.eye(2), atol=1e-6)
    delta_max = 2*dr.atan((1 - eta**2) / (2*eta))
    assert dr.allclose(M[2:4, 2:4], [[dr.cos(delta_max), -dr.sin(delta_max)], [dr.sin(delta_max), dr.cos(delta_max)]], atol=1e-5)


def test06_specular_transmission(variant_scalar_rgb):
    assert dr.allclose(mi.mueller.specular_transmission(1, 1/1.5), dr.identity(mi.Matrix4f) * 0.96, atol=1e-5)
    assert dr.allclose(mi.mueller.specular_transmission(1e-7, 1.5), dr.zeros(mi.Matrix4f), atol=1e-5)
    assert dr.allclose(mi.mueller.specular_transmission(1e-7, 1/1.5), dr.zeros(mi.Matrix4f), atol=1e-5)
    assert dr.allclose(mi.mueller.specular_transmission(0, 1.5), dr.zeros(mi.Matrix4f), atol=1e-5)
    assert dr.allclose(mi.mueller.specular_transmission(0, 1/1.5), dr.zeros(mi.Matrix4f), atol=1e-5)

    ref = dr.scalar.Matrix4f([[ 0.9260355 , -0.07396451,  0.        ,  0.        ],
                              [-0.07396451,  0.9260355 ,  0.        ,  0.        ],
                              [ 0.        ,  0.        ,  0.92307705,  0.        ],
                              [ 0.        ,  0.        ,  0.        ,  0.92307705]])
    assert dr.allclose(mi.mueller.specular_transmission(dr.cos(dr.atan(1/1.5)), 1/1.5), ref)


def test07_rotate_stokes_basis(variant_scalar_rgb):
    # Each Stokes vector describes light polarization only w.r.t. a certain reference
    # frame determined by unit vector perpendicular to the direction of travel.
    #
    # Here we test a duality between rotating the polarization direction (e.g. from
    # linear polarization at 0˚ to 90˚) and rotating the Stokes reference frame
    # in the opposite direction.
    # We start out with horizontally polarized light (Stokes vector [1, 1, 0, 0])
    # and perform several transformations.

    w = [0, 0, 1] # Optical axis / forward direction

    s_00 = Array4f([1, 1, 0, 0]) # Horizontally polarized light
    b_00 = mi.mueller.stokes_basis(w) # Corresponding Stokes basis

    def rotate_vector(v, axis, angle):
        return mi.Transform4f().rotate(axis, angle) @ v

    # Switch to basis rotated by 90˚.
    b_90 = rotate_vector(b_00, w, 90.0)
    assert dr.allclose(b_90, [0, 1, 0], atol=1e-3)
    # Now polarization should be at 90˚ / vertical (with Stokes vector [1, -1, 0, 0])
    s_90 = mi.mueller.rotate_stokes_basis(w, b_00, b_90) @ s_00
    assert dr.allclose(s_90, [1.0, -1.0, 0.0, 0.0], atol=1e-3)

    # Switch to basis rotated by +45˚.
    b_45 = rotate_vector(b_00, w, +45.0)
    assert dr.allclose(b_45, [0.70712, 0.70712, 0.0], atol=1e-3)
    # Now polarization should be at -45˚ (with Stokes vector [1, 0, -1, 0])
    s_45 = mi.mueller.rotate_stokes_basis(w, b_00, b_45) @ s_00
    assert dr.allclose(s_45, [1.0, 0.0, -1.0, 0.0], atol=1e-3)

    # Switch to basis rotated by -45˚.
    b_45_neg = rotate_vector(b_00, w, -45.0)
    assert dr.allclose(b_45_neg, [0.70712, -0.70712, 0.0], atol=1e-3)
    # Now polarization should be at +45˚ (with Stokes vector [1, 0, +1, 0])
    s_45_neg = mi.mueller.rotate_stokes_basis(w, b_00, b_45_neg) @ s_00
    assert dr.allclose(s_45_neg, [1.0, 0.0, +1.0, 0.0], atol=1e-3)


def test08_rotate_mueller_basis(variant_scalar_rgb):
    # If we have an optical element such as a linear polarizer, its rotation around
    # the optical axis can also be interpreted as a change of both incident and
    # outgoing Stokes reference frames:

    w = [0, 0, 1] # Optical axis / forward direction

    s_00 = [1, 1, 0, 0] # Horizontally polarized light
    b_00 = mi.mueller.stokes_basis(w)  # Corresponding Stokes basis

    M = mi.mueller.linear_polarizer()

    def rotate_vector(v, axis, angle):
        return mi.Transform4f().rotate(axis, angle) @ v

    # As reference, rotate the element directly by -45˚
    M_rotated_element = mi.mueller.rotated_element(-45 * dr.pi/180, M)

    # Now rotate the two reference bases by 45˚ instead and make sure results are
    # equivalent.
    # More thorough explanation of what's going on here:
    # Mueller matrix 'M' works in b_00 = [1, 0, 0], and we want to construct
    # another matrix that works in b_45 =[0.707, 0.707, 0] instead.
    # 'rotate_mueller_basis' does exactly that:
    # R(b_00 -> b_45) * M * R(b_45 -> b_00) = R(+45˚) * M * R(-45˚) which is
    # equivalent to rotating the element by -45˚.
    b_45 = rotate_vector(b_00, w, +45.0)
    M_rotated_bases = mi.mueller.rotate_mueller_basis(M,
                                                      w, b_00, b_45,
                                                      w, b_00, b_45)
    assert dr.allclose(M_rotated_element, M_rotated_bases, atol=1e-5)

    # Also test alternative rotation method that assumes collinear incident and
    # outgoing directions.
    M_rotated_bases_aligned = mi.mueller.rotate_mueller_basis_collinear(M, w, b_00, b_45)
    assert dr.allclose(M_rotated_element, M_rotated_bases_aligned, atol=1e-5)


def test09_circular(variant_scalar_rgb):
    # Test a few simple outcomes of polarization due to circular polarizers.
    L = mi.mueller.left_circular_polarizer()
    R = mi.mueller.right_circular_polarizer()

    # Unpolarized light is converted into left/right circularly polarized light.
    dr.allclose(L @ Array4f([1, 0, 0, 0]), Array4f([0.5, 0, 0, -0.5]))
    dr.allclose(R @ Array4f([1, 0, 0, 0]), Array4f([0.5, 0, 0, +0.5]))

    # Linear polarization is also converted to circular polarization.
    dr.allclose(L @ Array4f([1, 1, 0, 0]), Array4f([0.5, 0, 0, -0.5]))
    dr.allclose(R @ Array4f([1, 1, 0, 0]), Array4f([0.5, 0, 0, +0.5]))

    # Light that is already circularly polarized is unchanged.
    dr.allclose(L @ Array4f([1, 0, 0, -1]), Array4f([0.5, 0, 0, -1]))
    dr.allclose(R @ Array4f([1, 0, 0, +1]), Array4f([0.5, 0, 0, +1]))
