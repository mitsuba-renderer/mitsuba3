import mitsuba
import pytest
import enoki as ek
from enoki.scalar import Array4f


def test01_depolarizer(variant_scalar_rgb):
    from mitsuba.render.mueller import depolarizer

    # Remove all polarization, and change intensity based on transmittance value.
    assert ek.allclose(ek.dot(depolarizer(.8), Array4f([1, .5, .5, .5])), [.8, 0, 0, 0])

def test02_rotator(variant_scalar_rgb):
    from mitsuba.render.mueller import rotator

    # Start with horizontally linear polarized light [1,1,0,0] and ..
    # .. rotate the Stokes frame by +45˚
    assert ek.allclose(ek.dot(rotator( 45 * ek.Pi/180), Array4f([1, 1, 0, 0])), [1, 0, -1, 0], atol=1e-5)
    # .. rotate the Stokes frame by -45˚
    assert ek.allclose(ek.dot(rotator(-45 * ek.Pi/180), Array4f([1, 1, 0, 0])), [1, 0, +1, 0], atol=1e-5)



def test03_linear_polarizer(variant_scalar_rgb):
    from mitsuba.render.mueller import rotated_element, linear_polarizer

    # Malus' law
    angle = 30 * ek.Pi/180
    value_malus = ek.cos(angle)**2
    polarizer = rotated_element(angle, linear_polarizer(1.0))

    stokes_in  = Array4f([1, 1, 0, 0])
    stokes_out = ek.dot(polarizer, stokes_in)
    intensity  = stokes_out[0]
    assert ek.allclose(intensity, value_malus)

def test04_linear_polarizer_rotated(variant_scalar_rgb):
    from mitsuba.render.mueller import rotated_element, linear_polarizer, rotator

    # The closed-form expression for a rotated linear polarizer is available
    # in many textbooks and should match the behavior of manually rotating the
    # optical element.
    angle = 33 * ek.Pi/180
    s, c = ek.sin(2*angle), ek.cos(2*angle)
    M_ref = ek.scalar.Matrix4f([[1, c, s, 0],
                                [c, c*c, s*c, 0],
                                [s, s*c, s*s, 0],
                                [0, 0, 0, 0]]) * 0.5
    R = rotator(angle)
    L = linear_polarizer()
    M = rotated_element(angle, L)
    assert ek.allclose(M, M_ref)


def test05_specular_reflection(variant_scalar_rgb):
    from mitsuba.core import Matrix4f
    from mitsuba.render.mueller import specular_reflection, linear_retarder
    import numpy as np

    # Perpendicular incidence: 180 deg phase shift
    assert ek.allclose(specular_reflection(1, 1.5),   linear_retarder(ek.Pi)*0.04)
    assert ek.allclose(specular_reflection(1, 1/1.5), linear_retarder(ek.Pi)*0.04)

    # Just before Brewster's angle: 180 deg phase shift
    M = specular_reflection(ek.cos(ek.atan(1.5) - 0.01), 1.5)
    assert M[0, 0] > 0 and M[1, 1] > 0 and M[2, 2] < 0 and M[3, 3] < 0
    M = specular_reflection(ek.cos(ek.atan(1/1.5) - 0.01), 1/1.5)
    assert M[0, 0] > 0 and M[1, 1] > 0 and M[2, 2] < 0 and M[3, 3] < 0

    # Brewster's angle: perfect linear polarization
    M = np.zeros((4, 4))
    M[0:2, 0:2] = 0.0739645
    assert ek.allclose(specular_reflection(ek.cos(ek.atan(1/1.5)), 1/1.5), M, atol=1e-6)
    assert ek.allclose(specular_reflection(ek.cos(ek.atan(1.5)), 1.5), M, atol=1e-6)

    # Just after Brewster's angle: no phase shift
    M = specular_reflection(ek.cos(ek.atan(1.5) + 0.01), 1.5)
    assert M[0, 0] > 0 and M[1, 1] > 0 and M[2, 2] > 0 and M[3, 3] > 0
    M = specular_reflection(ek.cos(ek.atan(1/1.5) + 0.01), 1/1.5)
    assert M[0, 0] > 0 and M[1, 1] > 0 and M[2, 2] > 0 and M[3, 3] > 0

    # Grazing incidence: identity matrix and no phase shift
    assert ek.allclose(specular_reflection(0, 1.5),   ek.identity(Matrix4f), atol=1e-6)
    assert ek.allclose(specular_reflection(0, 1/1.5), ek.identity(Matrix4f), atol=1e-6)

    # Complex phase shift in the total internal reflection case
    eta = 1/1.5
    cos_theta_max = ek.sqrt((1 - eta**2) / (1 + eta**2))
    M = specular_reflection(cos_theta_max, eta).numpy()
    assert np.all(M[0:2, 2:4] == 0) and np.all(M[2:4, 0:2] == 0) and ek.allclose(M[0:2, 0:2], np.eye(2), atol=1e-6)
    delta_max = 2*ek.atan((1 - eta**2) / (2*eta))
    assert ek.allclose(M[2:4, 2:4], [[ek.cos(delta_max), -ek.sin(delta_max)], [ek.sin(delta_max), ek.cos(delta_max)]], atol=1e-5)


def test06_specular_transmission(variant_scalar_rgb):
    from mitsuba.core import Matrix4f
    from mitsuba.render.mueller import specular_transmission

    assert ek.allclose(specular_transmission(1, 1.5), ek.identity(Matrix4f) * 0.96, atol=1e-5)
    assert ek.allclose(specular_transmission(1, 1/1.5), ek.identity(Matrix4f) * 0.96, atol=1e-5)
    assert ek.allclose(specular_transmission(1e-7, 1.5), ek.zero(Matrix4f), atol=1e-5)
    assert ek.allclose(specular_transmission(1e-7, 1/1.5), ek.zero(Matrix4f), atol=1e-5)
    assert ek.allclose(specular_transmission(0, 1.5), ek.zero(Matrix4f), atol=1e-5)
    assert ek.allclose(specular_transmission(0, 1/1.5), ek.zero(Matrix4f), atol=1e-5)

    ref = ek.scalar.Matrix4f([[ 0.9260355 , -0.07396451,  0.        ,  0.        ],
                              [-0.07396451,  0.9260355 ,  0.        ,  0.        ],
                              [ 0.        ,  0.        ,  0.92307705,  0.        ],
                              [ 0.        ,  0.        ,  0.        ,  0.92307705]])
    assert ek.allclose(specular_transmission(ek.cos(ek.atan(1/1.5)), 1/1.5), ref)


def test07_rotate_stokes_basis(variant_scalar_rgb):
    from mitsuba.core import Transform4f
    from mitsuba.render.mueller import stokes_basis, rotate_stokes_basis

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
    b_00 = stokes_basis(w) # Corresponding Stokes basis

    def rotate_vector(v, axis, angle):
        return Transform4f.rotate(axis, angle) @ v

    # Switch to basis rotated by 90˚.
    b_90 = rotate_vector(b_00, w, 90.0)
    assert ek.allclose(b_90, [0, 1, 0], atol=1e-3)
    # Now polarization should be at 90˚ / vertical (with Stokes vector [1, -1, 0, 0])
    s_90 = ek.dot(rotate_stokes_basis(w, b_00, b_90), s_00)
    assert ek.allclose(s_90, [1.0, -1.0, 0.0, 0.0], atol=1e-3)

    # Switch to basis rotated by +45˚.
    b_45 = rotate_vector(b_00, w, +45.0)
    assert ek.allclose(b_45, [0.70712, 0.70712, 0.0], atol=1e-3)
    # Now polarization should be at -45˚ (with Stokes vector [1, 0, -1, 0])
    s_45 = ek.dot(rotate_stokes_basis(w, b_00, b_45), s_00)
    assert ek.allclose(s_45, [1.0, 0.0, -1.0, 0.0], atol=1e-3)

    # Switch to basis rotated by -45˚.
    b_45_neg = rotate_vector(b_00, w, -45.0)
    assert ek.allclose(b_45_neg, [0.70712, -0.70712, 0.0], atol=1e-3)
    # Now polarization should be at +45˚ (with Stokes vector [1, 0, +1, 0])
    s_45_neg = ek.dot(rotate_stokes_basis(w, b_00, b_45_neg), s_00)
    assert ek.allclose(s_45_neg, [1.0, 0.0, +1.0, 0.0], atol=1e-3)


def test08_rotate_mueller_basis(variant_scalar_rgb):
    from mitsuba.core import Transform4f
    from mitsuba.render.mueller import stokes_basis, rotated_element, rotate_mueller_basis, \
                                       rotate_mueller_basis_collinear, linear_polarizer

    # If we have an optical element such as a linear polarizer, its rotation around
    # the optical axis can also be interpreted as a change of both incident and
    # outgoing Stokes reference frames:

    w = [0, 0, 1] # Optical axis / forward direction

    s_00 = [1, 1, 0, 0] # Horizontally polarized light
    b_00 = stokes_basis(w)  # Corresponding Stokes basis

    M = linear_polarizer()

    def rotate_vector(v, axis, angle):
        return Transform4f.rotate(axis, angle) @ v

    # As reference, rotate the element directly by -45˚
    M_rotated_element = rotated_element(-45 * ek.Pi/180, M)

    # Now rotate the two reference bases by 45˚ instead and make sure results are
    # equivalent.
    # More thorough explanation of what's going on here:
    # Mueller matrix 'M' works in b_00 = [1, 0, 0], and we want to construct
    # another matrix that works in b_45 =[0.707, 0.707, 0] instead.
    # 'rotate_mueller_basis' does exactly that:
    # R(b_00 -> b_45) * M * R(b_45 -> b_00) = R(+45˚) * M * R(-45˚) which is
    # equivalent to rotating the elemnt by -45˚.
    b_45 = rotate_vector(b_00, w, +45.0)
    M_rotated_bases = rotate_mueller_basis(M,
                                           w, b_00, b_45,
                                           w, b_00, b_45)
    assert ek.allclose(M_rotated_element, M_rotated_bases, atol=1e-5)

    # Also test alternative rotation method that assumes collinear incident and
    # outgoing directions.
    M_rotated_bases_aligned = rotate_mueller_basis_collinear(M, w, b_00, b_45)
    assert ek.allclose(M_rotated_element, M_rotated_bases_aligned, atol=1e-5)
