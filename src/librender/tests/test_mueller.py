import numpy as np
from mitsuba.render.mueller import (
    depolarizer,
    linear_polarizer,
    rotator,
    rotated_element,
    specular_reflection,
    specular_transmission)


def test01_depolarizer():
    assert np.allclose(depolarizer(.8) @ [1, .5, .5, .5], [.8, 0, 0, 0])


def test02_rotator():
    assert np.allclose(rotator( 45 * np.pi/180) @ [1, 1, 0, 0], [1, 0,  1, 0], atol=1e-5)
    assert np.allclose(rotator(-45 * np.pi/180) @ [1, 1, 0, 0], [1, 0, -1, 0], atol=1e-5)

# def test03_linear_polarizer():
#     # Malus' law
#     angle = 30 * np.pi / 180
#     v = np.cos(angle)**2
#     print(v)
#     assert np.allclose(rotated_element(linear_polarizer(angle) @ [1, 1, 0, 0], [1, 0, 0]))


def test04_specular_reflection():
    # Identity matrix (and no phase shift) at perpendicular incidence
    assert np.allclose(specular_reflection(1, 1.5), np.eye(4)*0.04)
    assert np.allclose(specular_reflection(1, 1/1.5), np.eye(4)*0.04)

    # 180 deg phase shift at grazing incidence
    assert np.allclose(specular_reflection(0, 1.5), np.diag([1,1,-1,-1]), atol=1e-6)
    assert np.allclose(specular_reflection(0, 1/1.5), np.diag([1,1,-1,-1]), atol=1e-6)

    # Perfect linear polarization at Brewster's angle
    M = np.zeros((4, 4))
    M[0:2, 0:2] = 0.0739645
    assert np.allclose(specular_reflection(np.cos(np.arctan(1/1.5)), 1/1.5), M, atol=1e-6)
    assert np.allclose(specular_reflection(np.cos(np.arctan(1.5)), 1.5), M, atol=1e-6)

    # 180 deg phase shift just below Brewster's angle (air -> glass)
    M = specular_reflection(np.cos(np.arctan(1.5)+1e-4), 1.5)
    assert M[0, 0] > 0 and M[1, 1] > 0 and M[2, 2] < 0 and M[3, 3] < 0
    M = specular_reflection(np.cos(np.arctan(1/1.5)+1e-4), 1/1.5)
    assert M[0, 0] > 0 and M[1, 1] > 0 and M[2, 2] < 0 and M[3, 3] < 0

    # Complex phase shift in the total internal reflection case
    eta = 1/1.5
    cos_theta_min = np.sqrt((1 - eta**2) / (1 + eta**2))
    M = specular_reflection(cos_theta_min, eta)
    assert np.all(M[0:2, 2:4] == 0) and np.all(M[2:4, 0:2] == 0) and np.allclose(M[0:2, 0:2], np.eye(2), atol=1e-6)
    phi_delta = 4*np.arctan(eta)
    assert np.allclose(M[2:4, 2:4], [[np.cos(phi_delta), np.sin(phi_delta)], [-np.sin(phi_delta), np.cos(phi_delta)]])


def test05_specular_transmission():
    assert np.allclose(specular_transmission(1, 1.5), np.eye(4)*0.96, atol=1e-5)
    assert np.allclose(specular_transmission(1, 1/1.5), np.eye(4)*0.96, atol=1e-5)
    assert np.allclose(specular_transmission(1e-7, 1.5), np.zeros((4, 4)), atol=1e-5)
    assert np.allclose(specular_transmission(1e-7, 1/1.5), np.zeros((4, 4)), atol=1e-5)
    assert np.allclose(specular_transmission(0, 1.5), np.zeros((4, 4)), atol=1e-5)
    assert np.allclose(specular_transmission(0, 1/1.5), np.zeros((4, 4)), atol=1e-5)

    ref = np.array([[ 0.9260355 , -0.07396451,  0.        ,  0.        ],
                    [-0.07396451,  0.9260355 ,  0.        ,  0.        ],
                    [ 0.        ,  0.        ,  0.92307705,  0.        ],
                    [ 0.        ,  0.        ,  0.        ,  0.92307705]])
    assert np.allclose(specular_transmission(np.cos(np.arctan(1/1.5)), 1/1.5), ref)
