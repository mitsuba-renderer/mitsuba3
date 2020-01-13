import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float

@pytest.fixture()
def variant():
    mitsuba.set_variant('scalar_rgb')

def test01_fresnel(variant):
    from mitsuba.render import fresnel

    ct_crit = -ek.sqrt(1 - 1 / 1.5**2)
    assert ek.allclose(fresnel(1, 1.5), (0.04, -1, 1.5, 1 / 1.5))
    assert ek.allclose(fresnel(-1, 1.5), (0.04, 1, 1 / 1.5, 1.5))
    assert ek.allclose(fresnel(1, 1 / 1.5), (0.04, -1, 1 / 1.5, 1.5))
    assert ek.allclose(fresnel(-1, 1 / 1.5), (0.04, 1, 1.5, 1 / 1.5))
    assert ek.allclose(fresnel(0, 1.5), (1, ct_crit, 1.5, 1 / 1.5))
    assert ek.allclose(fresnel(0, 1 / 1.5), (1, 0, 1 / 1.5, 1.5))

    # Spot check at 45 deg (1 -> 1.5)
    # Source: http://hyperphysics.phy-astr.gsu.edu/hbase/phyopt/freseq.html
    F, cos_theta_t, _, scale = fresnel(ek.cos(45 * ek.pi / 180), 1.5)
    cos_theta_t_ref = -ek.cos(28.1255057020557 * ek.pi / 180)
    F_ref = 0.5 * (0.09201336304552442**2 + 0.3033370452904235**2)
    L = (scale * ek.sqrt(1 - ek.cos(45 * ek.pi / 180)**2))**2 + cos_theta_t**2
    assert ek.allclose(L, 1)
    assert ek.allclose(cos_theta_t, cos_theta_t_ref)
    assert ek.allclose(F, F_ref)

    # 1.5 -> 1
    F, cos_theta_t, _, _ = fresnel(ek.cos(45 * ek.pi / 180), 1 / 1.5)
    assert ek.allclose(F, 1)
    assert ek.allclose(cos_theta_t, 0)

    F, cos_theta_t, _, scale = fresnel(ek.cos(10 * ek.pi / 180), 1 / 1.5)
    cos_theta_t_ref = -ek.cos(15.098086605159006 * ek.pi / 180)
    F_ref = 0.5 * (0.19046797197779405**2 + 0.20949431963852014**2)
    L = (scale * ek.sqrt(1 - ek.cos(10 * ek.pi / 180)**2))**2 + cos_theta_t**2
    assert ek.allclose(L, 1)
    assert ek.allclose(cos_theta_t, cos_theta_t_ref)
    assert ek.allclose(F, F_ref)


def test02_fresnel_polarized(variant):
    from mitsuba.render import fresnel_polarized

    # Brewster's angle
    angle = ek.cos(56.3099 * ek.pi / 180)

    a_s, a_p, cos_theta_t, _, scale = fresnel_polarized(angle, 1.5)
    assert ek.allclose(a_s*a_s, 0.3846150939598947**2)
    assert ek.allclose(a_p*a_p, 0)

    try:
        mitsuba.set_variant("packet_rgb")
        from mitsuba.render import fresnel
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    cos_theta_i = ek.linspace(Float, -1, 1, 20)
    F, cos_theta_t, _, scale = fresnel(cos_theta_i, 1)
    assert ek.all(F == np.zeros(20))
    assert ek.allclose(cos_theta_t, -cos_theta_i, atol=5e-7)


def test03_fresnel_conductor(variant):
    from mitsuba.render import fresnel, fresnel_polarized, fresnel_conductor

    try:
        from mitsuba.render import fresnel, fresnel_conductor
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    # The conductive and diel. variants should agree given a real-valued IOR
    cos_theta_i = ek.cos(ek.linspace(Float, 0, ek.pi / 2, 20))

    r, cos_theta_t, _, scale = fresnel(cos_theta_i, 1.5)
    r_2 = fresnel_conductor(cos_theta_i, 1.5)
    assert ek.allclose(r, r_2)

    r, cos_theta_t, _, scale = fresnel(cos_theta_i, 1 / 1.5)
    r_2 = fresnel_conductor(cos_theta_i, 1 / 1.5)
    assert ek.allclose(r, r_2)


def test04_snell(variant):
    from mitsuba.render import fresnel, fresnel_polarized, fresnel_conductor

    try:
        from mitsuba.packet_rgb.render import fresnel
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")
    # Snell's law
    theta_i = ek.linspace(Float, 0, ek.pi / 2, 20)
    F, cos_theta_t, _, _ = fresnel(ek.cos(theta_i), 1.5)
    theta_t = np.arccos(cos_theta_t)
    ek.allclose(ek.sin(theta_i) - ek.sin(theta_t) * 1.5, 0.0)


def test05_phase(variant):
    import numpy as np
    from mitsuba.render import fresnel_polarized

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
    a_s, a_p, _, _, _ = fresnel_polarized(ek.cos(b+0.01), 1/1.5)
    assert np.real(a_s) > 0 and np.real(a_p) < 0 and np.imag(a_s) == 0 and np.imag(a_p) == 0


def test06_phase_tir(variant):
    import numpy as np
    from mitsuba.render import fresnel_polarized

    eta = 1/1.5
    # Check phase shift at critical angle (total internal reflection case)
    crit = np.arcsin(eta)
    a_s, a_p, _, _, _ = fresnel_polarized(ek.cos(crit), eta)

    assert ek.allclose(np.angle(a_s), 0)
    assert ek.allclose(np.angle(a_p), ek.pi) or ek.allclose(np.angle(a_p), -ek.pi)

    # Check phase shift at grazing angle (total internal reflection case)
    a_s, a_p, _, _, _ = fresnel_polarized(0, eta)

    assert ek.allclose(np.angle(a_s), ek.pi) or ek.allclose(np.angle(a_s), -ek.pi)
    assert ek.allclose(np.angle(a_p), 0)

    # Location of minimum phase difference
    cos_theta_min = ek.sqrt((1 - eta**2) / (1 + eta**2))
    phi_delta = 4*np.arctan(eta)
    a_s, a_p, _, _, _ = fresnel_polarized(cos_theta_min, eta)
    assert ek.allclose(np.angle(a_s) - np.angle(a_p), phi_delta)
