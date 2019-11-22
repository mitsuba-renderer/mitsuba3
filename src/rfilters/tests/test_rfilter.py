from mitsuba.scalar_rgb.core.xml import load_string
from mitsuba.scalar_rgb.core import float_dtype
from mitsuba.scalar_rgb.core import Resampler, FilterBoundaryCondition
from pytest import approx
import numpy as np

# Spot-check the various reconstruction filters to prevent code rot

def test01_box():
    f = load_string("<rfilter version='2.0.0' type='box'/>")
    assert f.eval(0.49) == 1 and f.eval(0.51) == 0
    assert f.eval_discretized(0.49) == 1 and f.eval_discretized(0.51) == 0

def test02_gaussian():
    f = load_string("<rfilter version='2.0.0' type='gaussian'/>")
    assert f.eval(0.2) == approx(0.9227, rel = 8e-3)
    assert f.eval_discretized(0.2) == approx(0.9227, rel=8e-3)
    assert f.eval(2.1) == 0 and f.eval_discretized(2.1) == 0

def test03_lanczos():
    f = load_string("<rfilter version='2.0.0' type='lanczos'/>")
    assert f.eval(1.4) == approx(-0.14668, rel = 1e-2)
    assert f.eval_discretized(1.4) == approx(-0.14668, rel=1e-2)
    assert f.eval(3.1) == 0 and f.eval_discretized(3.1) == 0

def test04_mitchell():
    f = load_string("<rfilter version='2.0.0' type='mitchell'/>")
    assert f.eval(0) == approx(0.8888, rel = 1e-3)
    assert f.eval_discretized(0) == approx(0.8888, rel=1e-3)
    assert f.eval(2.1) == 0 and f.eval_discretized(2.1) == 0

def test05_catmullrom():
    f = load_string("<rfilter version='2.0.0' type='catmullrom'/>")
    assert f.eval(0) == approx(0.9765, rel = 5e-2)
    assert f.eval_discretized(0) == approx(0.9765, rel=5e-2)
    assert f.eval(2.1) == 0 and f.eval_discretized(2.1) == 0

def test06_tent():
    f = load_string("<rfilter version='2.0.0' type='tent'/>")
    assert f.eval(0.1) == approx(0.903, rel = 5e-2)
    assert f.eval_discretized(0.1) == approx(0.903, rel=5e-2)
    assert f.eval(1.1) == 0 and f.eval_discretized(1.1) == 0

def test07_resampler_box():
    from mitsuba.scalar_rgb.core import Resampler
    f = load_string("""
        <rfilter version='2.0.0' type='box'>
            <float name='radius' value='0.5'/>
        </rfilter>
    """)

    a = np.linspace(0, 1, 5, dtype=float_dtype)
    b = np.zeros(5, dtype=float_dtype)

    # Resample to same size
    resampler = Resampler(f, 5, 5)
    assert resampler.taps() == 1
    resampler.resample(a, 1, b, 1, 1)

    # Resample to same size
    resampler = Resampler(f, 5, 3)
    b = np.zeros(3, dtype=float_dtype)
    assert resampler.taps() == 2
    resampler.resample(a, 1, b, 1, 1)
    assert np.all(b == [0.125, 0.5, 0.875])

def test08_resampler_boundary_conditions():
    # Use a slightly larger filter
    f = load_string("""
        <rfilter version='2.0.0' type='box'>
            <float name='radius' value='0.85'/>
        </rfilter>
    """)

    a = np.linspace(0, 1, 5, dtype=float_dtype)
    b = np.zeros(3, dtype=float_dtype)

    resampler = Resampler(f, 5, 3)
    resampler.set_boundary_condition(FilterBoundaryCondition.Clamp)
    assert resampler.taps() == 3
    resampler.resample(a, 1, b, 1, 1)
    assert np.allclose(b, [(0 + 0 + 0.25) / 3, 0.5, (0.75 + 1 + 1) / 3])
    resampler.set_boundary_condition(FilterBoundaryCondition.Zero)
    resampler.resample(a, 1, b, 1, 1)
    assert np.allclose(b, [(0 + 0 + 0.25) / 3, 0.5, (0.75 + 1 + 0) / 3])
    resampler.set_boundary_condition(FilterBoundaryCondition.One)
    resampler.resample(a, 1, b, 1, 1)
    assert np.allclose(b, [(1 + 0 + 0.25) / 3, 0.5, (0.75 + 1 + 1) / 3])
    resampler.set_boundary_condition(FilterBoundaryCondition.Repeat)
    resampler.resample(a, 1, b, 1, 1)
    assert np.allclose(b, [(1 + 0 + 0.25) / 3, 0.5, (0.75 + 1 + 0) / 3])
    resampler.set_boundary_condition(FilterBoundaryCondition.Mirror)
    resampler.resample(a, 1, b, 1, 1)
    assert np.allclose(b, [(0.25 + 0 + 0.25) / 3, 0.5, (0.75 + 1 + 0.75) / 3])

def test09_resampler_filter_only():
    def G(x):
        return np.exp(-2.0*x*x) - np.exp(-8.0)

    from mitsuba.scalar_rgb.core import Resampler
    f = load_string("""
        <rfilter version='2.0.0' type='gaussian'/>
    """)

    resampler = Resampler(f, 3, 3)
    resampler.set_boundary_condition(FilterBoundaryCondition.Repeat)
    assert resampler.taps() == 3
    a = np.linspace(0, 1, 3, dtype=float_dtype)
    b = np.zeros(3, dtype=float_dtype)
    resampler.resample(a, 1, b, 1, 1)
    assert b[0] == approx((G(0) * a[0] + G(1) * (a[1] + a[2])) / (G(0) + 2*G(1)))
    assert b[1] == approx((G(0) * a[1] + G(1) * (a[0] + a[2])) / (G(0) + 2*G(1)))
    assert b[2] == approx((G(0) * a[2] + G(1) * (a[0] + a[1])) / (G(0) + 2*G(1)))
