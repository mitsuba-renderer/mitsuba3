"""Tests for the majorant of the `heterogeneous` medium."""

import pytest
import drjit as dr
import mitsuba as mi


def make_grid(value):
    import numpy as np
    return mi.VolumeGrid(np.full((4, 4, 4, 1), value, dtype=np.float32))


def make_medium(sigma_t, *, albedo=None, scale=1.0, majorant_factor=None):
    d = {'type': 'heterogeneous', 'scale': scale,
         'sigma_t': {'type': 'gridvolume', 'grid': make_grid(sigma_t)}}
    if albedo is not None:
        d['albedo'] = {'type': 'gridvolume', 'grid': make_grid(albedo)}
    if majorant_factor is not None:
        d['majorant_factor'] = majorant_factor
    return mi.load_dict(d)


def interaction(medium):
    mei = dr.zeros(mi.MediumInteraction3f)
    mei.p = mi.Point3f(0.5, 0.5, 0.5)
    mei.wi = mi.Vector3f(0, 0, 1)
    mei.sh_frame = mi.Frame3f(mei.wi)
    mei.wavelengths = mi.Color0f()
    mei.medium = mi.MediumPtr(medium)
    return mei


def test01_majorant_factor(variants_vec_backends_once_rgb):
    """The factor only raises the majorant; the coefficients stay the same."""
    m0 = make_medium(0.7, albedo=0.4, scale=2.0, majorant_factor=1.0)
    m1 = make_medium(0.7, albedo=0.4, scale=2.0, majorant_factor=1.3)
    mei0, mei1 = interaction(m0), interaction(m1)
    assert dr.allclose(m0.get_majorant(mei0), 2.0 * 0.7)
    assert dr.allclose(m1.get_majorant(mei1), 2.0 * 0.7 * 1.3)
    s0, n0, t0 = m0.get_scattering_coefficients(mei0)
    s1, n1, t1 = m1.get_scattering_coefficients(mei1)
    assert dr.allclose(t0, t1) and dr.allclose(s0, s1)
    assert dr.allclose(n0, 0.0) and dr.allclose(n1, 2.0 * 0.7 * 0.3)
    # Test default value, if left unspecified, the factor is 1.2
    m = make_medium(0.7, scale=2.0)
    assert dr.allclose(m.get_majorant(interaction(m)), 2.0 * 0.7 * 1.2)
    # A factor at or below 1 is kept as given; the constructor only warns
    m = make_medium(0.7, scale=2.0, majorant_factor=0.9)
    assert dr.allclose(m.get_majorant(interaction(m)), 2.0 * 0.7 * 0.9)
