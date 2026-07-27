import pytest
import drjit as dr
import mitsuba as mi
import numpy as np
import tempfile
import os


def uses_hw_texture():
    return dr.backend_v(mi.Float) in (dr.JitBackend.CUDA, dr.JitBackend.Metal)


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

    # The radiance lookup uses a hardware texture on GPU backends. The texel
    # values are interpolated at full precision, but the bilinear weight within
    # a texel is quantized to 8 fractional bits (1/256 steps) on CUDA.
    rtol = 7e-2 if uses_hw_texture() else 1e-3

    # Test the sample_direction() interface
    si = dr.zeros(mi.SurfaceInteraction3f)
    ds, w = emitter.sample_direction(si, sample)
    si.wi = -ds.d
    w2 = emitter.eval(si) / emitter.pdf_direction(si, ds)
    w3 = emitter.eval_direction(si, ds) / ds.pdf

    assert dr.allclose(w, w2, rtol=rtol)
    assert dr.allclose(w, w3, rtol=rtol)
    if uses_hw_texture():
        # Hardware bilinear weights occasionally over/under-shoot at texel
        # boundaries; the key property is that the weight stays bounded above.
        assert dr.max(w[0])[0] < 0.05
    else:
        assert dr.min(w[0])[0] > 0.018 and dr.max(w[0])[0] < 0.02

    # Test the sample_ray() interface
    ray, w = emitter.sample_ray(0, 0, sample, sample_2)
    si.wi = ray.d
    ds.d = -ray.d
    w4 = emitter.eval(si) / emitter.pdf_direction(si, ds) * dr.pi
    assert dr.allclose(w4, w, rtol=rtol)


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

def test05_parameters_check_invalid_update(variants_vec_backends_once_rgb):
    import numpy as np

    n_channels = dr.size_v(mi.Spectrum)
    bitmap = mi.Bitmap(np.zeros((191, 23, n_channels), dtype=np.float32))
    emitter = mi.load_dict({
        "type" : "envmap",
        "bitmap" : bitmap
    })

    with pytest.raises(RuntimeError) as excinfo:
        params = mi.traverse(emitter)
        shape = dr.shape(params['data'])
        params['data'] = dr.ones(mi.TensorXf, shape=(191, 23, n_channels + 1))
        params.update()
    assert (
        f"Environment map data has {n_channels + 1} channels, expected {n_channels}"
        in str(excinfo.value)
    )


def make_envmap(array):
    return mi.load_dict({
        "type": "envmap",
        "bitmap": mi.Bitmap(np.asarray(array, dtype=np.float32))
    })


def eval_radiance(emitter, u, v):
    """Radiance at latitude-longitude coordinates (u, v), using the same
    direction convention as ``direction_to_uv``. Returns the attached Spectrum."""
    u = np.atleast_1d(np.asarray(u, dtype=np.float64))
    v = np.atleast_1d(np.asarray(v, dtype=np.float64))
    phi, theta = u * 2 * np.pi, v * np.pi
    st, ct = np.sin(theta), np.cos(theta)
    d = np.stack([np.sin(phi) * st, ct, -np.cos(phi) * st], axis=1)
    si = dr.zeros(mi.SurfaceInteraction3f, len(u))
    si.wi = mi.Vector3f(mi.Float(-d[:, 0]), mi.Float(-d[:, 1]), mi.Float(-d[:, 2]))
    return emitter.eval(si)


def eval_envmap(emitter, u, v):
    """``eval_radiance`` as an (N, channels) numpy array."""
    out = np.array(eval_radiance(emitter, u, v))
    return out.reshape(out.shape[0], -1).T


@pytest.mark.parametrize("axis", ["u", "v"])
def test06_pixel_shift(variants_vec_backends_once_rgb, axis):
    """A ramp whose pixel value equals its column (resp. row) index reads back
    the exact texel-center / align-corners coordinate, with zero shift. The u
    axis probes the phi remap, the v axis guards the align-corners v remap."""
    if axis == "u":
        W, H = 16, 8
        img = np.broadcast_to(np.arange(W, dtype=np.float32)[None, :, None],
                              (H, W, 3)).copy()
        # Interior coordinate u*W - 0.5 stays within [0, W-1].
        t = np.linspace(1.0 / W, 1.0 - 1.0 / W, 50)
        u, v, expected = t, np.full_like(t, 0.5), t * W - 0.5
    else:
        W, H = 8, 16
        img = np.broadcast_to(np.arange(H, dtype=np.float32)[:, None, None],
                              (H, W, 3)).copy()
        # Interior row coordinate v*(H-1) avoids the pole clamp.
        t = np.linspace(1.0 / H, 1.0 - 1.0 / H, 50)
        u, v, expected = np.zeros_like(t), t, t * (H - 1)

    got = eval_envmap(make_envmap(img), u, v)[:, 0]
    atol = 5e-2 if uses_hw_texture() else 1e-4
    assert np.max(np.abs(got - expected)) < atol, \
        f"{axis}-shift detected: max |got-expected| = {np.max(np.abs(got - expected))}"


def test07_phi_seam_continuity(variants_vec_backends_once_rgb):
    """A periodic cosine map must evaluate continuously across the phi seam at
    u = 0; a broken halo/wrap would interpolate the wrong columns there."""
    W, H = 64, 8
    col = np.cos(2 * np.pi * np.arange(W) / W).astype(np.float32)
    img = np.broadcast_to(col[None, :, None], (H, W, 3)).copy()

    # Sample a window straddling the seam (negative u wraps to ~1).
    u = np.linspace(-0.1, 0.1, 201)
    u_wrapped = u - np.floor(u)
    got = eval_envmap(make_envmap(img), u, np.full_like(u, 0.5))[:, 0]
    expected = np.cos(2 * np.pi * (u_wrapped * W - 0.5) / W)

    atol = 5e-2 if uses_hw_texture() else 5e-3
    assert np.max(np.abs(got - expected)) < atol, \
        f"phi-seam discontinuity: max |got-expected| = {np.max(np.abs(got - expected))}"


def test08_theta_pole_clamp(variants_vec_backends_once_rgb):
    """The poles clamp (not wrap): exactly at theta=pi the lookup returns the
    bottom row, with no contribution from the opposite (top) row."""
    W, H = 8, 16
    A, B = 10.0, 20.0  # distinct top / bottom row constants
    img = np.zeros((H, W, 3), dtype=np.float32)
    img[0] = A
    img[H - 1] = B
    emitter = make_envmap(img)

    top = eval_envmap(emitter, 0.0, 0.0)[0, 0]
    bottom = eval_envmap(emitter, 0.0, 1.0)[0, 0]

    atol = 0.2 if uses_hw_texture() else 1e-3
    # North pole -> top row constant A
    assert abs(top - A) < atol, f"north pole = {top}, expected {A}"
    # South pole -> bottom row constant B (clamp). A wrap/repeat would blend
    # toward (A+B)/2 = 15, and the previous implementation wrapped to A.
    assert abs(bottom - B) < atol, f"south pole = {bottom}, expected {B} (clamp)"


def test09_data_gradient(variant_llvm_ad_rgb):
    """Forward/reverse-mode gradients of eval w.r.t. the data tensor match
    finite differences, including the halo gradient: perturbing a real edge
    column must affect eval through its opposite halo copy near the seam."""
    W, H = 8, 6
    rng = np.random.default_rng(0)
    img = rng.uniform(0.1, 1.0, (H, W, 3)).astype(np.float32)

    # Evaluate near the seam (u ~ 1) so the bilinear footprint reads storage
    # column W+1, the halo copy of real column 0 (storage column 1).
    u_probe, v_probe = np.array([0.999]), np.array([0.5])

    def eval_value(data_np):
        emitter = make_envmap(img)
        params = mi.traverse(emitter)
        params['data'] = mi.TensorXf(np.ascontiguousarray(data_np))
        params.update()
        return eval_envmap(emitter, u_probe, v_probe)[0, 0]

    # Reference exposed tensor (carries the W+2 halo layout).
    emitter = make_envmap(img)
    params = mi.traverse(emitter)
    data0 = np.array(params['data'])  # (H, W+2, 3)

    # Analytic gradient via reverse mode. We keep an explicit handle to the
    # grad-enabled leaf (as an optimizer would), since the halo refresh makes
    # the post-update exposed tensor a derived node.
    data = mi.TensorXf(np.ascontiguousarray(data0))
    dr.enable_grad(data)
    params['data'] = data
    params.update()
    dr.backward(eval_radiance(emitter, u_probe, v_probe)[0])  # red channel
    grad = np.array(dr.grad(data))

    # Finite-difference a few entries: an interior real texel and the two real
    # edge columns (storage cols 1 and W, whose halo copies sit at W+1 and 0).
    h = 1e-2
    probes = [(2, 4, 0), (3, 1, 0), (1, W, 0)]
    for (y, x, c) in probes:
        dp, dm = data0.copy(), data0.copy()
        dp[y, x, c] += h
        dm[y, x, c] -= h
        fd = (eval_value(dp) - eval_value(dm)) / (2 * h)
        assert np.allclose(fd, grad[y, x, c], atol=1e-3), \
            f"grad mismatch at {(y, x, c)}: AD={grad[y, x, c]}, FD={fd}"


def test10_pad_to(variants_vec_backends_once_rgb):
    bitmap = mi.Bitmap(np.ones((2, 1, 3), dtype=np.float32))
    emitter = mi.load_dict({"type" : "envmap", "bitmap" : bitmap})
    params = mi.traverse(emitter)

    # Initially pads to (3, 2), and then adds 2 to width => (3, 4)
    assert params['data'].shape[:2] == (3, 4)
