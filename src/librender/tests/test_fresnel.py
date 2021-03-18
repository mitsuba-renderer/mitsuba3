import mitsuba
import pytest
import enoki as ek

def test01_fresnel(variant_scalar_rgb):
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
    F, cos_theta_t, _, scale = fresnel(ek.cos(45 * ek.Pi / 180), 1.5)
    cos_theta_t_ref = -ek.cos(28.1255057020557 * ek.Pi / 180)
    F_ref = 0.5 * (0.09201336304552442**2 + 0.3033370452904235**2)
    L = (scale * ek.sqrt(1 - ek.cos(45 * ek.Pi / 180)**2))**2 + cos_theta_t**2
    assert ek.allclose(L, 1)
    assert ek.allclose(cos_theta_t, cos_theta_t_ref)
    assert ek.allclose(F, F_ref)

    # 1.5 -> 1
    F, cos_theta_t, _, _ = fresnel(ek.cos(45 * ek.Pi / 180), 1 / 1.5)
    assert ek.allclose(F, 1)
    assert ek.allclose(cos_theta_t, 0)

    F, cos_theta_t, _, scale = fresnel(ek.cos(10 * ek.Pi / 180), 1 / 1.5)
    cos_theta_t_ref = -ek.cos(15.098086605159006 * ek.Pi / 180)
    F_ref = 0.5 * (0.19046797197779405**2 + 0.20949431963852014**2)
    L = (scale * ek.sqrt(1 - ek.cos(10 * ek.Pi / 180)**2))**2 + cos_theta_t**2
    assert ek.allclose(L, 1)
    assert ek.allclose(cos_theta_t, cos_theta_t_ref)
    assert ek.allclose(F, F_ref)


def test02_fresnel_polarized(variant_scalar_rgb):
    from mitsuba.render import fresnel_polarized

    # Brewster's angle
    angle = ek.cos(56.3099 * ek.Pi / 180)

    a_s, a_p, cos_theta_t, _, scale = fresnel_polarized(angle, 1.5)

    assert ek.allclose(ek.real(a_s*a_s), 0.3846150939598947**2)
    assert ek.allclose(ek.real(a_p*a_p), 0)


def test02_fresnel_polarized_vec(variants_vec_backends_once):
    from mitsuba.core import Float
    from mitsuba.render import fresnel

    cos_theta_i = ek.linspace(Float, -1, 1, 20)
    F, cos_theta_t, _, scale = fresnel(cos_theta_i, 1)
    assert ek.all(F == ek.zero(Float, 20))
    assert ek.allclose(cos_theta_t, -cos_theta_i, atol=5e-7)


def test03_fresnel_conductor(variants_vec_backends_once):
    from mitsuba.core import Float
    from mitsuba.render import fresnel, fresnel_conductor

    # The conductive and diel. variants should agree given a real-valued IOR
    cos_theta_i = ek.cos(ek.linspace(Float, 0, ek.Pi / 2, 20))

    r, cos_theta_t, _, scale = fresnel(cos_theta_i, 1.5)
    r_2 = fresnel_conductor(cos_theta_i, 1.5)
    assert ek.allclose(r, r_2)

    r, cos_theta_t, _, scale = fresnel(cos_theta_i, 1 / 1.5)
    r_2 = fresnel_conductor(cos_theta_i, 1 / 1.5)
    assert ek.allclose(r, r_2)


def test04_snell(variants_vec_backends_once):
    from mitsuba.core import Float
    from mitsuba.render import fresnel

    # Snell's law
    theta_i = ek.linspace(Float, 0, ek.Pi / 2, 20)
    F, cos_theta_t, _, _ = fresnel(ek.cos(theta_i), 1.5)
    theta_t = ek.acos(cos_theta_t)

    assert ek.allclose(ek.sin(theta_i) - 1.5 * ek.sin(theta_t), ek.zero(Float, 20), atol=1e-5)


def test05_amplitudes_external_reflection(variant_scalar_rgb):
    from mitsuba.render import fresnel_polarized

    # External reflection case (air -> glass)
    # Compare with Fig. 4.49 in "Optics, 5th edition" by Eugene Hecht.

    brewster = ek.atan(1.5)

    # Perpendicularly arriving light: check signs and relative phase shift 180 deg.
    a_s, a_p, _, _, _ = fresnel_polarized(1, 1.5)
    assert ek.real(a_s) < 0 and ek.real(a_p) > 0 and ek.imag(a_s) == 0 and ek.imag(a_p) == 0
    assert ek.allclose(ek.abs(ek.arg(a_p) - ek.arg(a_s)), ek.Pi, atol=1e-5)

    # Just below Brewster's angle: same as above still.
    a_s, a_p, _, _, _ = fresnel_polarized(ek.cos(brewster - 0.01), 1.5)
    assert ek.real(a_s) < 0 and ek.real(a_p) > 0 and ek.imag(a_s) == 0 and ek.imag(a_p) == 0
    assert ek.allclose(ek.abs(ek.arg(a_p) - ek.arg(a_s)), ek.Pi)

    # At Brewster's angle: p-polarized light is zero.
    a_s, a_p, _, _, _ = fresnel_polarized(ek.cos(brewster), 1.5)
    assert ek.allclose(ek.real(a_p), 0, atol=1e-5)
    assert ek.real(a_s) < 0 and ek.imag(a_s) == 0 and ek.imag(a_p) == 0

    # Just above Brewster's angle: both amplitudes have the same sign and no more relative phase shift.
    a_s, a_p, _, _, _ = fresnel_polarized(ek.cos(brewster + 0.01), 1.5)
    assert ek.real(a_s) < 0 and ek.real(a_p) < 0 and ek.imag(a_s) == 0 and ek.imag(a_p) == 0
    assert ek.allclose(ek.abs(ek.arg(a_p) - ek.arg(a_s)), 0, atol=1e-5)

    # At grazing angle: both amplitudes go to minus one.
    a_s, a_p, _, _, _ = fresnel_polarized(0, 1/1.5)
    assert ek.allclose(ek.real(a_s), -1, atol=1e-3) and ek.allclose(ek.real(a_p), -1, atol=1e-3)
    assert ek.imag(a_s) == 0 and ek.imag(a_p) == 0
    assert ek.allclose(ek.abs(ek.arg(a_p) - ek.arg(a_s)), 0, atol=1e-5)

def test06_amplitudes_internal_reflection(variant_scalar_rgb):
    from mitsuba.render import fresnel_polarized

    # Internal reflection case, but before TIR (glass -> air)
    # Compare with Fig. 4.50 in "Optics, 5th edition" by Eugene Hecht.

    brewster = ek.atan(1/1.5)
    critical = ek.asin(1/1.5)

    # Perpendicularly arriving light: check signs and relative phase shift 180 deg.
    a_s, a_p, _, _, _ = fresnel_polarized(1, 1/1.5)
    assert ek.real(a_s) > 0 and ek.real(a_p) < 0 and ek.imag(a_s) == 0 and ek.imag(a_p) == 0
    assert ek.allclose(ek.abs(ek.arg(a_p) - ek.arg(a_s)), ek.Pi, atol=1e-5)

    # Just below Brewster's angle: same as above still.
    a_s, a_p, _, _, _ = fresnel_polarized(ek.cos(brewster - 0.01), 1/1.5)
    assert ek.real(a_s) > 0 and ek.real(a_p) < 0 and ek.imag(a_s) == 0 and ek.imag(a_p) == 0
    assert ek.allclose(ek.abs(ek.arg(a_p) - ek.arg(a_s)), ek.Pi, atol=1e-5)

    # At Brewster's angle: p-polarized light is zero.
    a_s, a_p, _, _, _ = fresnel_polarized(ek.cos(brewster), 1/1.5)
    assert ek.allclose(ek.real(a_p), 0, atol=1e-5)
    assert ek.real(a_s) > 0 and ek.imag(a_s) == 0 and ek.imag(a_p) == 0

    # Just above Brewster's angle: both amplitudes have the same sign and no more relative phase shift.
    a_s, a_p, _, _, _ = fresnel_polarized(ek.cos(brewster + 0.01), 1/1.5)
    assert ek.real(a_s) > 0 and ek.real(a_p) > 0 and ek.imag(a_s) == 0 and ek.imag(a_p) == 0
    assert ek.allclose(ek.abs(ek.arg(a_p) - ek.arg(a_s)), 0, atol=1e-5)

    # At critical angle: both amplitudes go to one.
    a_s, a_p, _, _, _ = fresnel_polarized(ek.cos(critical), 1/1.5)
    assert ek.allclose(ek.real(a_s), 1, atol=1e-3) and ek.allclose(ek.real(a_p), 1, atol=1e-3)
    assert ek.imag(a_s) == 0 and ek.imag(a_p) == 0

def test06_phase_tir(variant_scalar_rgb):
    from mitsuba.render import fresnel_polarized

    # Internal reflection case with TIR (glass -> air)
    # Compare with Fig. 1 in
    # "Phase shifts that accompany total internal reflection at a dielectric-
    #  dielectric interface" by R.M.A. Azzam 2004.

    eta = 1/1.5
    critical = ek.asin(eta)

    # At critcal angle: both phases are zero.
    a_s, a_p, _, _, _ = fresnel_polarized(ek.cos(critical), eta)
    assert ek.allclose(ek.arg(a_s), 0.0)
    assert ek.allclose(ek.arg(a_p), 0.0)

    # At grazing angle: both phases are 180 deg.
    a_s, a_p, _, _, _ = fresnel_polarized(0, eta)
    assert ek.allclose(ek.arg(a_s), ek.Pi, atol=1e-5)
    assert ek.allclose(ek.arg(a_p), ek.Pi, atol=1e-5)

    # Location of the maximal phase difference
    # Azzam, Eq. (11)
    cos_theta_max = ek.sqrt((1 - eta**2) / (1 + eta**2))
    # Value of the maximal phase difference
    delta_max = 2*ek.atan((1 - eta**2) / (2*eta))

    a_s, a_p, _, _, _ = fresnel_polarized(cos_theta_max, eta)
    assert ek.allclose(ek.arg(a_p) - ek.arg(a_s), delta_max)
