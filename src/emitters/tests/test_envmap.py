import mitsuba
import pytest
import enoki as ek
import numpy as np
import tempfile
import os

@pytest.mark.parametrize("iteration", [0, 1, 2])
def test01_chi2(variants_vec_backends_once_rgb, iteration):
    from mitsuba.python.chi2 import ChiSquareTest, EmitterAdapter, SphericalDomain
    from mitsuba.core import Bitmap

    tempdir = tempfile.TemporaryDirectory()
    fname = os.path.join(tempdir.name, 'out.exr')

    if iteration == 0:
        # Sparse image with 1 pixel turned on
        img = np.zeros((100, 10), dtype=np.float32)
        img[40, 5] = 1
    elif iteration == 1:
        # High res constant image
        img = np.full((100, 100), 1, dtype=np.float32)
    elif iteration == 2:
        # Low res constant image
        img = np.full((3, 2), 1, dtype=np.float32)

    Bitmap(img).write(fname)

    xml = f'<string name="filename" value="{fname}"/>'
    sample_func, pdf_func = EmitterAdapter("envmap", xml)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=2,
        ires=32
    )

    assert chi2.run()

# Ensure that sampling weights remain bounded
def test02_sampling_weights(variants_vec_backends_once_rgb):
    from mitsuba.core import PCG32, Point2f, Bitmap
    from mitsuba.render import SurfaceInteraction3f
    from mitsuba.core.xml import load_string

    rng = PCG32(size=102400)
    sample = Point2f(
        rng.next_float32(),
        rng.next_float32())
    sample_2 = Point2f(
        rng.next_float32(),
        rng.next_float32())

    tempdir = tempfile.TemporaryDirectory()
    fname = os.path.join(tempdir.name, 'out.exr')

    # Sparse image with 1 pixel turned on
    img = np.zeros((100, 10), dtype=np.float32)
    img[40, 5] = 1
    Bitmap(img).write(fname)

    emitter = load_string(f'<emitter version="2.0.0" type="envmap">'
                          f'<string name="filename" value="{fname}"/>'
                          f'</emitter>')

    # Test the sample_direction() interface
    si = ek.zero(SurfaceInteraction3f)
    ds, w = emitter.sample_direction(si, sample)
    si.wi = -ds.d
    w2 = emitter.eval(si) / emitter.pdf_direction(si, ds)

    assert ek.allclose(w, w2, rtol=1e-3)
    assert ek.hmin(w[0]) > 0.018 and ek.hmax(w[0]) < 0.02

    # Test the sample_ray() interface
    ray, w = emitter.sample_ray(0, 0, sample, sample_2)
    si.wi = -ray.d
    ds.d = ray.d
    w2 = emitter.eval(si) / emitter.pdf_direction(si, ds) * ek.Pi
    assert ek.allclose(w, w2, rtol=1e-3)
