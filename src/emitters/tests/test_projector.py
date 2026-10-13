import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


def uses_hw_texture():
    return dr.backend_v(mi.Float) in (dr.JitBackend.CUDA, dr.JitBackend.Metal)


@fresolver_append_path
def test01_sampling_weights(variants_vec_backends_once_rgb):
    rng = mi.PCG32(size=102400)
    sample = mi.Point2f(
        rng.next_float32(),
        rng.next_float32())
    sample_2 = mi.Point2f(
        rng.next_float32(),
        rng.next_float32())

    emitter = mi.load_dict({
        "type" : "envmap",
        'filename': 'resources/data/common/textures/museum.exr',
    })

    # The radiance lookup uses a hardware texture on GPU backends. The texel
    # values are interpolated at full precision, but the bilinear weight within
    # a texel is quantized to 8 fractional bits (1/256 steps) on CUDA.
    rtol = 7e-2 if uses_hw_texture() else 1e-2

    # Test the sample_direction() interface
    si = dr.zeros(mi.SurfaceInteraction3f)
    ds, w = emitter.sample_direction(si, sample)
    si.wi = -ds.d
    w2 = emitter.eval(si) / emitter.pdf_direction(si, ds)
    w3 = emitter.eval_direction(si, ds) / ds.pdf

    assert dr.allclose(w, w2, rtol=rtol)
    assert dr.allclose(w, w3, rtol=rtol)

    # Test the sample_ray() interface
    ray, w = emitter.sample_ray(0, 0, sample, sample_2)
    si.wi = ray.d
    ds.d = -ray.d
    w4 = emitter.eval(si) / emitter.pdf_direction(si, ds) * dr.pi
    assert dr.allclose(w4, w, rtol=rtol)
