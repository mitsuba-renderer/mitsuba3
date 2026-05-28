import math

import pytest
import drjit as dr
import mitsuba as mi


def bitmap_data(channels=3, height=3, width=4):
    values = (dr.arange(mi.Float, height * width * channels) + 1) / 17.0
    return mi.TensorXf(values, shape=(height, width, channels))


def volume_data(channels=6, z=3, y=4, x=5):
    values = (dr.arange(mi.Float, z * y * x * channels) + 1) / 29.0
    return mi.TensorXf(values, shape=(z, y, x, channels))


def bitmap_field_dict(channels=3, **kwargs):
    result = {
        "type": "bitmap",
        "data": bitmap_data(channels),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }
    result.update(kwargs)
    return result


def grid_field_dict(channels=6, **kwargs):
    result = {
        "type": "gridvolume",
        "data": volume_data(channels),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }
    result.update(kwargs)
    return result


def encoding_dict(plugin="hashgridfield", input_dim=2, out_dim=8, **kwargs):
    result = {
        "type": plugin,
        "input_dim": input_dim,
        "out_dim": out_dim,
        "n_levels": 4,
        "n_features_per_level": 2,
        "base_resolution": 4,
        "per_level_scale": 1.5,
        "hashmap_size": 128,
    }
    result.update(kwargs)
    if plugin == "permutofield":
        result.pop("hashmap_size", None)
    return result


def neural_field_dict(**kwargs):
    result = {
        "type": "neuralfield",
        "domain": "Surface",
        "out_type": "Color3",
        "out_dim": 3,
        "args_dim": 4,
        "encoding": encoding_dict("hashgridfield"),
        "hidden_size": 16,
        "num_layers": 2,
    }
    result.update(kwargs)
    return result


def neuralbsdf_dict(field):
    return {
        "type": "neuralbsdf",
        "reflectance": field,
    }


def surface_interaction(width=1):
    si = dr.zeros(mi.SurfaceInteraction3f, width)
    if width == 1:
        si.uv = mi.Point2f(0.25, 0.75)
    else:
        si.uv = mi.Point2f(
            mi.Float(-0.25, 0.125, 0.5, 0.9, 1.25),
            mi.Float(1.20, 0.375, 0.5, -0.1, 0.75),
        )
    si.p = mi.Point3f(si.uv.x, si.uv.y, 0)
    si.n = mi.Normal3f(0, 0, 1)
    si.wi = mi.Vector3f(0, 0, 1)
    si.sh_frame = mi.Frame3f(si.n)
    return si


def normalized_surface_interaction(width=5):
    si = surface_interaction(width)
    si.uv = mi.Point2f(
        dr.linspace(mi.Float, 0.1, 0.9, width),
        dr.linspace(mi.Float, 0.2, 0.8, width),
    )
    si.p = mi.Point3f(si.uv.x, si.uv.y, 0)
    return si


def interaction(width=1):
    it = dr.zeros(mi.Interaction3f, width)
    if width == 1:
        it.p = mi.Point3f(0.25, 0.5, 0.75)
    else:
        it.p = mi.Point3f(
            mi.Float(-0.25, 0.125, 0.5, 0.9, 1.25),
            mi.Float(1.20, 0.375, 0.5, -0.1, 0.75),
            mi.Float(0.1, 0.25, 0.5, 0.75, 1.4),
        )
    return it


def test01_bitmap_field_ad_gradients_are_finite_and_nonzero(field_ad_rgb_variant):
    field = mi.load_dict(bitmap_field_dict(
        channels=3, filter_type="bilinear", data=bitmap_data(channels=3)
    )).field()
    params = mi.traverse(field)
    dr.enable_grad(params["data"])
    params.update()

    loss = dr.sum(field.eval_color3(surface_interaction(width=5)))
    dr.backward(loss)

    grad = dr.grad(params["data"])
    assert dr.all(dr.isfinite(grad))
    assert dr.any(grad != 0)


def test02_bitmap_field_ad_gradient_matches_bilinear_weights(field_ad_rgb_variant):
    field = mi.load_dict(bitmap_field_dict(
        channels=1,
        data=dr.zeros(mi.TensorXf, shape=(2, 2, 1)),
        filter_type="bilinear",
        wrap_mode="clamp",
    )).field()
    params = mi.traverse(field)
    dr.enable_grad(params["data"])
    params.update()

    si = surface_interaction()
    si.uv = mi.Point2f(0.5, 0.5)
    dr.backward(field.eval_1(si))

    expected = dr.full(mi.TensorXf, 0.25, shape=(2, 2, 1))
    assert dr.allclose(dr.grad(params["data"]), expected, atol=1e-6)


def test03_grid_field_ad_gradient_matches_trilinear_weights(field_ad_rgb_variant):
    field = mi.load_dict(grid_field_dict(
        channels=1,
        data=dr.zeros(mi.TensorXf, shape=(2, 2, 2, 1)),
        filter_type="trilinear",
        wrap_mode="clamp",
    )).field()
    params = mi.traverse(field)
    dr.enable_grad(params["data"])
    params.update()

    it = interaction()
    it.p = mi.Point3f(0.5, 0.5, 0.5)
    dr.backward(field.eval_1(it))

    expected = dr.full(mi.TensorXf, 0.125, shape=(2, 2, 2, 1))
    assert dr.allclose(dr.grad(params["data"]), expected, atol=1e-6)


def test04_bitmap_field_data_can_be_optimized(field_ad_rgb_variant):
    field = mi.load_dict(bitmap_field_dict(
        channels=1,
        data=dr.zeros(mi.TensorXf, shape=(2, 2, 1)),
        filter_type="bilinear",
        wrap_mode="clamp",
    )).field()
    params = mi.traverse(field)
    opt = mi.ad.SGD(lr=0.5, params=params)
    params.update(opt)

    si = surface_interaction()
    si.uv = mi.Point2f(0.5, 0.5)
    target = mi.Float(0.8)

    initial_loss = None
    for _ in range(16):
        value = field.eval_1(si)
        loss = dr.square(value - target)
        if initial_loss is None:
            dr.eval(loss)
            dr.sync_thread()
            initial_loss = float(loss[0])
        dr.backward(loss)
        opt.step()
        params.update(opt)

    final_value = field.eval_1(si)
    final_loss = dr.square(final_value - target)
    dr.eval(final_loss, final_value)
    dr.sync_thread()
    assert float(final_loss[0]) < 0.02 * initial_loss
    assert dr.allclose(final_value, target, atol=0.02)


@pytest.mark.parametrize("plugin", ["hashgridfield", "permutofield"])
def test05_trainable_encoding_field_params_receive_gradients(
    field_ad_rgb_variant, plugin
):
    field = mi.load_dict(encoding_dict(plugin))
    params = mi.traverse(field)
    dr.enable_grad(params["params"])

    si = surface_interaction()
    si.uv = mi.Point2f(0.2, 0.35)
    si.p = mi.Point3f(0.2, 0.35, 0)
    loss = dr.sum(field.eval_n(si, field.out_dim()))
    dr.backward(loss)

    grad = dr.grad(params["params"])
    assert dr.all(dr.isfinite(grad))
    assert dr.any(grad != 0)


def test06_sinusoidalfield_has_coordinate_ad_but_no_trainable_params(field_ad_rgb_variant):
    field = mi.load_dict({
        "type": "sinusoidalfield",
        "input_dim": 2,
        "out_dim": 4,
        "n_frequencies": 1,
        "min_frequency": 1.0,
        "max_frequency": 1.0,
    })
    assert list(mi.traverse(field).keys()) == []

    u = mi.Float(0.125)
    dr.enable_grad(u)
    si = surface_interaction()
    si.uv = mi.Point2f(u, 0.25)
    si.p = mi.Point3f(u, 0.25, 0)
    dr.backward(field.eval_n(si, field.out_dim())[0])

    expected = 2.0 * math.pi * math.cos(2.0 * math.pi * 0.125)
    assert dr.allclose(dr.grad(u), expected, atol=1e-5)


def test07_neuralfield_backpropagates_to_network_and_encoding_params(field_ad_rgb_variant):
    field = mi.load_dict(neural_field_dict())
    params = mi.traverse(field)
    assert "network_weights" in params
    assert "encoding.params" in params

    dr.enable_grad(params["network_weights"])
    dr.enable_grad(params["encoding.params"])

    loss = dr.sum(field.eval_color3(
        normalized_surface_interaction(width=5),
        args=[0.0, 0.0, 0.0, 0.0],
    ))
    dr.backward(loss)

    for key in ["network_weights", "encoding.params"]:
        grad = dr.grad(params[key])
        assert dr.all(dr.isfinite(grad))
        assert dr.any(grad != 0)


def test08_neuralfield_neural_decoder_can_be_optimized(field_ad_rgb_variant):
    field = mi.load_dict({
        "type": "neuralfield",
        "domain": "Surface",
        "out_type": "Color3",
        "out_dim": 3,
        "args_dim": 0,
        "decoder": "neural",
        "hidden_size": 8,
        "num_layers": 1,
        "seed": 3,
    })
    params = mi.traverse(field)
    assert "network_weights" in params
    assert "encoding.params" not in params

    opt = mi.ad.SGD(lr=0.5, params=params)
    params.update(opt)

    si = surface_interaction(width=5)
    target = mi.Color3f(0.25, 0.5, 0.75)

    initial_loss = None
    for _ in range(30):
        value = field.eval_color3(si)
        loss = dr.mean(dr.square(value - target))
        if initial_loss is None:
            dr.eval(loss)
            dr.sync_thread()
            initial_loss = float(loss[0])
        dr.backward(loss)
        opt.step()
        params.update(opt)

    final_loss = dr.mean(dr.square(field.eval_color3(si) - target))
    dr.eval(final_loss)
    dr.sync_thread()
    assert float(final_loss[0]) < 0.01 * initial_loss


def test09_neuralbsdf_backpropagates_to_reflectance_field(field_ad_rgb_variant):
    neural = mi.load_dict(neuralbsdf_dict({
        "type": "bitmap",
        "data": dr.full(mi.TensorXf, 0.4, shape=(2, 2, 3)),
        "raw": True,
        "filter_type": "bilinear",
        "wrap_mode": "clamp",
    }))
    params = mi.traverse(neural)
    key = "reflectance.data"
    assert key in params
    dr.enable_grad(params[key])
    params.update()

    si = surface_interaction()
    si.uv = mi.Point2f(0.5, 0.5)
    wo = mi.Vector3f(0.2, 0.1, math.sqrt(1.0 - 0.2 * 0.2 - 0.1 * 0.1))
    loss = dr.sum(neural.eval(mi.BSDFContext(), si, wo))
    dr.backward(loss)

    grad = dr.grad(params[key])
    assert dr.all(dr.isfinite(grad))
    assert dr.all(grad > 0)
