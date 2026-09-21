import numpy as np
import pytest
import drjit as dr
import mitsuba as mi


def table(*rows):
    return mi.TensorXf(np.array(rows, dtype=np.float32))


def make(data, input=0.5, **kwargs):
    return mi.load_dict({ 'type': 'lut', 'data': data, 'input': input, **kwargs })


def si_at(uv=(0.5, 0.5)):
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.uv = mi.Point2f(*uv)
    return si


@pytest.mark.parametrize('kwargs, xs, expected', [
    # Entries sit at i / (N - 1), and are interpolated linearly in between
    ({}, [0, 1/3, 2/3, 1, 1/6, 0.5, 0.75], [0, 1, 3, 2, 0.5, 2, 2.75]),
    # Inputs outside the range are clamped
    ({}, [-1, 7], [0, 2]),
    ({ 'input_min': -1, 'input_max': 1 }, [-1, -1/3, 1/3, 1, 0], [0, 1, 3, 2, 2]),
    ({ 'filter_type': 'nearest' }, [0, 0.1, 0.2, 0.4, 0.6, 0.9, 1], [0, 0, 1, 1, 3, 2, 2]),
])
def test01_lookup(variants_vec_backends_once_rgb, kwargs, xs, expected):
    """Evaluate the table [0 1 3 2] at all inputs with one query through a bitmap"""
    n = len(xs)
    data = mi.TensorXf(np.array(xs, dtype=np.float32).reshape(1, n, 1))
    tex = make(table(0, 1, 3, 2), input={ 'type': 'bitmap', 'data': data, 'raw': True,
                                          'filter_type': 'nearest' }, **kwargs)
    si = dr.zeros(mi.SurfaceInteraction3f, n)
    si.uv = mi.Point2f((dr.arange(mi.Float, n) + 0.5) / n, 0.5)
    dr.assert_allclose(tex.eval_1(si), expected, atol=1e-6)


def test02_errors(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match='at least two entries'):
        make(table(1))
    with pytest.raises(RuntimeError, match='must have shape'):
        make(table([0, 0], [1, 1], [2, 2], [3, 3]))
    with pytest.raises(RuntimeError, match='must have shape'):
        make(mi.TensorXf(np.zeros((2, 2, 3), dtype=np.float32)))
    with pytest.raises(RuntimeError, match='"filename" or "data"'):
        mi.load_dict({ 'type': 'lut', 'input': 0.5 })
    with pytest.raises(RuntimeError, match='must exceed'):
        make(table(0, 1), input_min=1, input_max=1)


def test03_shapes(variants_all_rgb):
    """Tensors of shape (N,) and (N, 1) describe the same scalar table"""
    data = np.array([0, 1, 3, 2], dtype=np.float32)
    for data in (mi.TensorXf(data), mi.TensorXf(data.reshape(4, 1))):
        dr.assert_allclose(make(data).eval_1(si_at()), 2)


@pytest.mark.parametrize('channels', [1, 3])
def test04_file(variants_all_rgb, tmp_path, channels):
    """An image file provides the entries and, through metadata, the input range"""
    values = np.linspace(0, 1, 4 * channels, dtype=np.float32).reshape(1, 4, channels)
    bitmap = mi.Bitmap(values)
    bitmap.metadata()['domain_min'] = -1.0
    bitmap.metadata()['domain_max'] = 3.0
    filename = str(tmp_path / 'table.exr')
    bitmap.write(filename)

    tex = mi.load_dict({ 'type': 'lut', 'filename': filename, 'input': 1.0 })
    assert mi.traverse(tex)['data'].shape == (4, channels)
    def entry(v):
        return v.tolist() if channels == 3 else float(v[0])

    # An input of 1 lies at the middle of the range, between entries 1 and 2
    dr.assert_allclose(tex.eval_3(si_at()), entry(0.5 * (values[0, 1] + values[0, 2])),
                       atol=1e-6)

    # Explicit parameters override the metadata
    tex = mi.load_dict({ 'type': 'lut', 'filename': filename, 'input': 1.0,
                         'input_min': 0, 'input_max': 1 })
    dr.assert_allclose(tex.eval_3(si_at()), entry(values[0, 3]), atol=1e-6)

    # Column images work as well
    mi.Bitmap(values.reshape(4, 1, channels)).write(filename)
    tex = mi.load_dict({ 'type': 'lut', 'filename': filename, 'input': 1.0 })
    dr.assert_allclose(tex.eval_3(si_at()), entry(values[0, 3]), atol=1e-6)

    with pytest.raises(RuntimeError, match='both'):
        mi.load_dict({ 'type': 'lut', 'filename': filename, 'input': 1.0,
                       'data': table(0, 1) })


def test05_srgb_file(variant_scalar_rgb, tmp_path):
    """8-bit files are linearized like in the bitmap plugin"""
    filename = str(tmp_path / 'table.png')
    mi.Bitmap(np.array([[[0], [128], [255]]], dtype=np.uint8)).write(filename)
    tex = mi.load_dict({ 'type': 'lut', 'filename': filename, 'input': 0.5 })
    dr.assert_allclose(tex.eval_1(si_at()), mi.math.srgb_to_linear(128 / 255), atol=1e-4)


def test06_color_map(variants_all_rgb):
    """A scalar input selects RGB entries"""
    tex = make(table([1, 0, 0], [0, 1, 0], [0, 0, 1]), input=0.25)
    si = si_at()
    dr.assert_allclose(tex.eval_3(si), [0.5, 0.5, 0])
    dr.assert_allclose(tex.eval(si), [0.5, 0.5, 0])
    dr.assert_allclose(tex.eval_1(si), mi.luminance(mi.Color3f(0.5, 0.5, 0)))

    # A color input is reduced to its monochromatic value before the lookup
    color = { 'type': 'srgb', 'color': [0.2, 0.7, 0.4] }
    tex = make(table([0, 0, 0], [1, 1, 1]), input=color)
    dr.assert_allclose(tex.eval_3(si), mi.Color3f(mi.load_dict(color).eval_1(si)),
                       atol=1e-6)


def test07_curve(variants_all_rgb):
    """A scalar curve applies to each channel of a color input"""
    color = { 'type': 'srgb', 'color': [0.1, 0.5, 1.0] }
    si = si_at()
    tex = make(table(0, 2), input=color, curve=True)
    dr.assert_allclose(tex.eval_3(si), [0.2, 1.0, 2.0], atol=1e-6)
    dr.assert_allclose(tex.eval(si), [0.2, 1.0, 2.0], atol=1e-6)
    dr.assert_allclose(tex.eval_1(si), 2 * mi.load_dict(color).eval_1(si),
                       atol=1e-6)


def test08_rgb_curves(variants_all_rgb):
    """An RGB table in curve mode holds one curve per channel"""
    color = { 'type': 'srgb', 'color': [0.1, 0.5, 1.0] }
    si = si_at()
    tex = make(table([0, 1, 0], [2, 0, 1]), input=color, curve=True)
    dr.assert_allclose(tex.eval_3(si), [0.2, 0.5, 1.0], atol=1e-6)
    dr.assert_allclose(tex.eval(si), [0.2, 0.5, 1.0], atol=1e-6)
    dr.assert_allclose(tex.eval_1(si), mi.luminance(mi.Color3f(0.2, 0.5, 1.0)), atol=1e-6)

    with pytest.raises(RuntimeError, match='scalar entries'):
        tex.eval_1_grad(si)


def test09_grad(variants_all_rgb):
    """eval_1_grad multiplies the segment slope with the input's gradient"""
    data = np.linspace(0, 1, 4 * 4, dtype=np.float32).reshape(4, 4, 1)
    # Loading a bitmap consumes the tensor of its dict, hence a fresh one each time
    bitmap = lambda: { 'type': 'bitmap', 'data': mi.TensorXf(data), 'raw': True,
                       'filter_type': 'bilinear' }
    inner = mi.load_dict(bitmap())
    si = si_at((0.4, 0.6))
    x = inner.eval_1(si)
    x = x[0] if dr.is_array_v(x) else x
    grad_in = inner.eval_1_grad(si)
    assert 0.5 < x < 1

    # Segment slopes of [0 1 3 2] over [0, 1]: 3, 6, -3
    tex = make(table(0, 1, 3, 2), input=bitmap())
    assert tex.is_spatially_varying()
    slope = [3.0, 6.0, -3.0][int(x * 3)]
    dr.assert_allclose(tex.eval_1_grad(si), slope * grad_in, rtol=1e-5)

    # Nearest lookups and clamped inputs have no slope
    tex = make(table(0, 1, 3, 2), input=bitmap(), filter_type='nearest')
    dr.assert_allclose(tex.eval_1_grad(si), [0, 0])
    tex = make(table(0, 1, 3, 2), input=bitmap(), input_min=2, input_max=3)
    dr.assert_allclose(tex.eval_1_grad(si), [0, 0])

    with pytest.raises(RuntimeError, match='scalar entries'):
        make(table([0, 0, 0], [1, 2, 3]), input=bitmap()).eval_1_grad(si)

    # The bumpmap plugin probes eval_1_grad at load time
    mi.load_dict({
        'type': 'bumpmap',
        'height': { 'type': 'lut', 'data': table(0, 1), 'input': bitmap() },
        'bsdf': { 'type': 'diffuse' }
    })


def test10_spectral_color_map(variants_all_spectral):
    """RGB entries of a color map are upsampled and interpolated as spectra"""
    tex = make(table([1, 0, 0], [0, 0, 1]), input=0.25)
    si = si_at()
    si.wavelengths = type(si.wavelengths)(450, 500, 600, 650)

    a = mi.load_dict({ 'type': 'srgb', 'color': [1, 0, 0] }).eval(si)
    b = mi.load_dict({ 'type': 'srgb', 'color': [0, 0, 1] }).eval(si)
    dr.assert_allclose(tex.eval(si), 0.75 * a + 0.25 * b, atol=1e-5)

    # The table keeps the sRGB entries
    dr.assert_allclose(tex.eval_3(si), [0.75, 0, 0.25], atol=1e-6)
    params = mi.traverse(tex)
    dr.assert_allclose(params['data'].array, [1, 0, 0, 0, 0, 1])

    # Updating the table refreshes the cached upsampling coefficients
    params['data'] = table([0, 0, 1], [1, 0, 0])
    params.update()
    dr.assert_allclose(tex.eval(si), 0.75 * b + 0.25 * a, atol=1e-5)

    # Nearest lookups pick one spectrum
    tex = make(table([1, 0, 0], [0, 0, 1]), input=0.25, filter_type='nearest')
    dr.assert_allclose(tex.eval(si), a, atol=1e-6)


def test11_spectral_color_map_grad(variants_all_ad_spectral):
    """Gradients flow from the upsampled spectra into the sRGB entries"""
    tex = make(table([0.2, 0.4, 0.6], [0.8, 0.6, 0.4]), input=0.25)
    params = mi.traverse(tex)
    dr.enable_grad(params['data'])
    params.update()

    si = si_at()
    si.wavelengths = type(si.wavelengths)(450, 500, 600, 650)
    dr.backward(dr.sum(tex.eval(si)))
    grad = dr.grad(params['data']).numpy()
    assert np.all(np.isfinite(grad)) and np.all(np.abs(grad).reshape(2, 3).sum(1) > 0)


def test12_spectral_curves(variants_all_spectral):
    """Curves apply to each wavelength sample, RGB curves to the upsampled result"""
    color = { 'type': 'srgb', 'color': [0.5, 0.5, 0.5] }
    tex = make(table(0, 2), curve=True, input=color)
    si = si_at()
    si.wavelengths = type(si.wavelengths)(450, 500, 600, 650)
    dr.assert_allclose(tex.eval(si), 2 * mi.load_dict(color).eval(si), atol=1e-6)

    tex = make(table([0, 0, 0], [1, 0.5, 2]), curve=True, input=color)
    expected = mi.load_dict({ 'type': 'srgb', 'color': [0.5, 0.25, 1.0] }).eval(si)
    dr.assert_allclose(tex.eval(si), expected, atol=1e-5)


def test13_traverse(variants_all_ad_rgb):
    """The table is exposed as a differentiable tensor that can be replaced"""
    tex = make(table(0, 1), input=0.25)
    params = mi.traverse(tex)
    assert 'input.value' in params
    dr.enable_grad(params['data'])
    params.update()
    dr.backward(tex.eval_1(si_at()))
    dr.assert_allclose(dr.grad(params['data']).array, [0.75, 0.25])

    # Updates may also use a tensor of shape (N,)
    params['data'] = mi.TensorXf(np.array([0, 2, 4], dtype=np.float32))
    params.update()
    dr.assert_allclose(tex.eval_1(si_at()), 1)
