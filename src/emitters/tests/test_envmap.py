import pytest
import drjit as dr
import mitsuba as mi
import tempfile
import os


@pytest.mark.parametrize("iteration", [0, 1, 2])
def test01_chi2(variants_vec_backends_once_rgb, iteration):
    tempdir = tempfile.TemporaryDirectory()
    fname = os.path.join(tempdir.name, 'out.exr')

    if iteration == 0:
        # Sparse image with 1 pixel turned on
        img = dr.zeros(mi.TensorXf, [100, 10, 1])
        img[40, 5] = 1
    elif iteration == 1:
        # High res constant image
        img = dr.full(mi.TensorXf, 1, [100, 100, 1])
    elif iteration == 2:
        # Low res constant image
        img = dr.full(mi.TensorXf, 1, [3, 2, 1])

    mi.Bitmap(img).write(fname)

    xml = f'<string name="filename" value="{fname}"/>' \
           '<boolean name="mis_compensation" value="false"/>'
    sample_func, pdf_func = mi.chi2.EmitterAdapter("envmap", xml)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=2,
        ires=32
    )

    assert chi2.run()

# Ensure that sampling weights remain bounded even in an extremely
# challenging case (envmap zero, with one pixel turned on)
def test02_sampling_weights(variants_vec_backends_once_rgb):
    rng = mi.PCG32(size=102400)
    sample = mi.Point2f(
        rng.next_float32(),
        rng.next_float32())
    sample_2 = mi.Point2f(
        rng.next_float32(),
        rng.next_float32())

    tempdir = tempfile.TemporaryDirectory()
    fname = os.path.join(tempdir.name, 'out.exr')

    # Sparse image with 1 pixel turned on
    img = dr.zeros(mi.TensorXf, [100, 10, 1])
    img[40, 5] = 1
    mi.Bitmap(img).write(fname)

    emitter = mi.load_dict({
        "type" : "envmap",
        "filename" : fname
    })

    # Test the sample_direction() interface
    si = dr.zeros(mi.SurfaceInteraction3f)
    ds, w = emitter.sample_direction(si, sample)
    si.wi = -ds.d
    w2 = emitter.eval(si) / emitter.pdf_direction(si, ds)
    w3 = emitter.eval_direction(si, ds) / ds.pdf

    assert dr.allclose(w, w2, rtol=1e-3)
    assert dr.allclose(w, w3, rtol=1e-3)
    assert dr.min(w[0])[0] > 0.018 and dr.max(w[0])[0] < 0.02

    # Test the sample_ray() interface
    ray, w = emitter.sample_ray(0, 0, sample, sample_2)
    si.wi = ray.d
    ds.d = -ray.d
    w4 = emitter.eval(si) / emitter.pdf_direction(si, ds) * dr.pi
    assert dr.allclose(w4, w, rtol=1e-3)


def test03_load_bitmap(variants_vec_backends_once_rgb):
    rng = mi.PCG32(size=102400)
    sample = mi.Point2f(
        rng.next_float32(),
        rng.next_float32())
    sample_2 = mi.Point2f(
        rng.next_float32(),
        rng.next_float32())

    tempdir = tempfile.TemporaryDirectory()
    fname = os.path.join(tempdir.name, 'out.exr')

    # Sparse image with 1 pixel turned on
    img = dr.zeros(mi.TensorXf, [100, 10, 1])
    img[40, 5] = 1
    bmp = mi.Bitmap(img)
    bmp.write(fname)

    emitter_1 = mi.load_dict({
        "type" : "envmap",
        "filename" : fname
    })

    emitter_2 = mi.load_dict({
        "type" : "envmap",
        "bitmap" : bmp
    })

    # Test the sample_direction() interface
    si = dr.zeros(mi.SurfaceInteraction3f)
    ds, w = emitter_1.sample_direction(si, sample)
    si.wi = -ds.d
    w1 = emitter_1.eval(si)
    w2 = emitter_2.eval(si)

    assert dr.allclose(w1, w2, rtol=1e-3)


def test04_parameters_changed(variants_all):
    import numpy as np

    n_channels = dr.size_v(mi.Spectrum)
    bitmap = mi.Bitmap(np.zeros((191, 23, n_channels), dtype=np.float32))
    emitter = mi.load_dict({
        "type" : "envmap",
        "bitmap" : bitmap
    })

    params = mi.traverse(emitter)
    shape = dr.shape(params['data'])
    params['data'] = dr.ones(mi.TensorXf, shape=shape)
    params.update()

    params = mi.traverse(emitter)
    assert dr.allclose(params['data'], 1)
