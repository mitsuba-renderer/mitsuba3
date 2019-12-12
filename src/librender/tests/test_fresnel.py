from mitsuba.scalar_rgb.render import fresnel, fresnel_polarized, fresnel_conductor
import numpy as np
import pytest

def test01_fresnel():
    ct_crit = -np.sqrt(1 - 1 / 1.5**2)
    assert np.allclose(fresnel(1, 1.5), (0.04, -1, 1.5, 1 / 1.5))
    assert np.allclose(fresnel(-1, 1.5), (0.04, 1, 1 / 1.5, 1.5))
    assert np.allclose(fresnel(1, 1 / 1.5), (0.04, -1, 1 / 1.5, 1.5))
    assert np.allclose(fresnel(-1, 1 / 1.5), (0.04, 1, 1.5, 1 / 1.5))
    assert np.allclose(fresnel(0, 1.5), (1, ct_crit, 1.5, 1 / 1.5))
    assert np.allclose(fresnel(0, 1 / 1.5), (1, 0, 1 / 1.5, 1.5))

    # Spot check at 45 deg (1 -> 1.5)
    # Source: http://hyperphysics.phy-astr.gsu.edu/hbase/phyopt/freseq.html
    F, cos_theta_t, _, scale = fresnel(np.cos(45 * np.pi / 180), 1.5)
    cos_theta_t_ref = -np.cos(28.1255057020557 * np.pi / 180)
    F_ref = 0.5 * (0.09201336304552442**2 + 0.3033370452904235**2)
    L = (scale * np.sqrt(1 - np.cos(45 * np.pi / 180)**2))**2 + cos_theta_t**2
    assert np.allclose(L, 1)
    assert np.allclose(cos_theta_t, cos_theta_t_ref)
    assert np.allclose(F, F_ref)

    # 1.5 -> 1
    F, cos_theta_t, _, _ = fresnel(np.cos(45 * np.pi / 180), 1 / 1.5)
    assert np.allclose(F, 1)
    assert np.allclose(cos_theta_t, 0)

    F, cos_theta_t, _, scale = fresnel(np.cos(10 * np.pi / 180), 1 / 1.5)
    cos_theta_t_ref = -np.cos(15.098086605159006 * np.pi / 180)
    F_ref = 0.5 * (0.19046797197779405**2 + 0.20949431963852014**2)
    L = (scale * np.sqrt(1 - np.cos(10 * np.pi / 180)**2))**2 + cos_theta_t**2
    assert np.allclose(L, 1)
    assert np.allclose(cos_theta_t, cos_theta_t_ref)
    assert np.allclose(F, F_ref)


def test02_fresnel_polarized():
    # Brewster's angle
    angle = np.cos(56.3099 * np.pi / 180)

    a_s, a_p, cos_theta_t, _, scale = fresnel_polarized(angle, 1.5)
    assert np.allclose(a_s*a_s, 0.3846150939598947**2)
    assert np.allclose(a_p*a_p, 0)

    try:
        from mitsuba.packet_rgb.render import fresnel as fresnel_packet
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    cos_theta_i = np.linspace(-1, 1, 20)
    F, cos_theta_t, _, scale = fresnel_packet(cos_theta_i, 1)
    assert np.all(F == np.zeros(20))
    assert np.allclose(cos_theta_t, -cos_theta_i, atol=5e-7)


def test03_fresnel_conductor():
    try:
        from mitsuba.packet_rgb.render import fresnel as fresnel_packet
        from mitsuba.packet_rgb.render import fresnel_conductor as fresnel_conductor_packet
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    # The conductive and diel. variants should agree given a real-valued IOR
    cos_theta_i = np.cos(np.linspace(0, np.pi / 2, 20))

    r, cos_theta_t, _, scale = fresnel_packet(cos_theta_i, 1.5)
    r_2 = fresnel_conductor_packet(cos_theta_i, 1.5)
    assert np.allclose(r, r_2)

    r, cos_theta_t, _, scale = fresnel_packet(cos_theta_i, 1 / 1.5)
    r_2 = fresnel_conductor_packet(cos_theta_i, 1 / 1.5)
    assert np.allclose(r, r_2)


def test04_snell():
    try:
        from mitsuba.packet_rgb.render import fresnel as fresnel_packet
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")
    # Snell's law
    theta_i = np.linspace(0, np.pi / 2, 20)
    F, cos_theta_t, _, _ = fresnel_packet(np.cos(theta_i), 1.5)
    theta_t = np.arccos(cos_theta_t)
    np.allclose(np.sin(theta_i) - np.sin(theta_t) * 1.5, 0.0)


def test05_phase():
    # 180 deg phase shift for perpendicularly arriving light (air -> glass)
    a_s, a_p, _, _, _ = fresnel_polarized(1, 1.5)
    assert np.real(a_s) < 0 and np.real(a_p) < 0 and np.imag(a_s) == 0 and np.imag(a_p) == 0

    # 180 deg phase shift only for s-polarized light below brewster's angle (air -> glass)
    a_s, a_p, _, _, _ = fresnel_polarized(.1, 1.5)
    assert np.real(a_s) < 0 and np.real(a_p) > 0 and np.imag(a_s) == 0 and np.imag(a_p) == 0

    # No phase shift for perpendicularly arriving light (glass -> air)
    a_s, a_p, _, _, _ = fresnel_polarized(1, 1/1.5)
    assert np.real(a_s) > 0 and np.real(a_p) > 0 and np.imag(a_s) == 0 and np.imag(a_p) == 0

    # 180 deg phase shift only for s-polarized light below brewster's angle (glass -> air)
    b = np.arctan(1/1.5)
    a_s, a_p, _, _, _ = fresnel_polarized(np.cos(b+0.01), 1/1.5)
    assert np.real(a_s) > 0 and np.real(a_p) < 0 and np.imag(a_s) == 0 and np.imag(a_p) == 0


def test06_phase_tir():
    eta = 1/1.5
    # Check phase shift at critical angle (total internal reflection case)
    crit = np.arcsin(eta)
    a_s, a_p, _, _, _ = fresnel_polarized(np.cos(crit), eta)

    assert np.allclose(np.angle(a_s), 0)
    assert np.allclose(np.angle(a_p), np.pi) or np.allclose(np.angle(a_p), -np.pi)

    # Check phase shift at grazing angle (total internal reflection case)
    a_s, a_p, _, _, _ = fresnel_polarized(0, eta)

    assert np.allclose(np.angle(a_s), np.pi) or np.allclose(np.angle(a_s), -np.pi)
    assert np.allclose(np.angle(a_p), 0)

    # Location of minimum phase difference
    cos_theta_min = np.sqrt((1 - eta**2) / (1 + eta**2))
    phi_delta = 4*np.arctan(eta)
    a_s, a_p, _, _, _ = fresnel_polarized(cos_theta_min, eta)
    assert np.allclose(np.angle(a_s) - np.angle(a_p), phi_delta)
