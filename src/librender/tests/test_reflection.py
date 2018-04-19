from mitsuba.render import (fresnel, fresnel_polarized,
                            fresnel_complex_polarized)
import numpy as np


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

    r_s, r_p, cos_theta_t, _, scale = fresnel_polarized(angle, 1.5)
    assert np.allclose(r_s, 0.3846150939598947**2)
    assert np.allclose(r_p, 0)

    cos_theta_i = np.linspace(-1, 1, 20)
    F, cos_theta_t, _, scale = fresnel(cos_theta_i, 1)
    assert np.all(F == np.zeros(20))
    assert np.allclose(cos_theta_t, -cos_theta_i, atol=5e-7)


def test03_fresnel_polarized_complex():
    # The conductive and diel. variants should agree given a real-valued IOR
    cos_theta_i = np.cos(np.linspace(0, np.pi / 2, 20))

    r_s, r_p, cos_theta_t, _, scale = fresnel_polarized(cos_theta_i, 1.5)
    r_s_2, r_p_2 = fresnel_complex_polarized(cos_theta_i, 1.5)
    assert np.allclose(r_s, r_s_2)
    assert np.allclose(r_p, r_p_2)

    r_s, r_p, cos_theta_t, _, scale = fresnel_polarized(cos_theta_i, 1 / 1.5)
    r_s_2, r_p_2 = fresnel_complex_polarized(cos_theta_i, 1 / 1.5)
    assert np.allclose(r_s, r_s_2)
    assert np.allclose(r_p, r_p_2)


def test04_snell():
    # Snell's law
    theta_i = np.linspace(0, np.pi / 2, 20)
    F, cos_theta_t, _, _ = fresnel(np.cos(theta_i), 1.5)
    theta_t = np.arccos(cos_theta_t)
    np.allclose(np.sin(theta_i) - np.sin(theta_t) * 1.5, 0.0)
