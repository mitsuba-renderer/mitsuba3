import pytest
import mitsuba as mi
import drjit as dr

from mitsuba.scalar_rgb.test.util import find_resource

###
### DLSS Ray Reconstruction is a temporal, real-time denoiser whose output is
### not reproducible across driver and DLSS library versions. These tests
### therefore only check the plumbing (shapes, error handling, sanity of the
### output) rather than comparing against reference images.
###
### They are skipped unless Mitsuba was compiled with `MI_ENABLE_DLSS` and DLSS
### Ray Reconstruction is actually supported by the driver and the GPU.
###


def skip_if_unavailable():
    if not mi.DLSSDenoiser.is_available():
        pytest.skip("DLSS Ray Reconstruction is not available on this system")


def make_inputs(res, seed=0):
    """Generate a plausible set of noisy inputs of resolution ``res``"""
    width, height = res
    n = width * height

    def random(channels, scale=1.0, offset=0.0):
        rng = mi.PCG32(size=n * channels, initstate=seed * 7919 + channels)
        values = rng.next_float32() * scale + offset
        return mi.TensorXf(values, shape=(height, width, channels))

    def constant(channels, value):
        values = dr.full(mi.Float, value, n * channels)
        return mi.TensorXf(values, shape=(height, width, channels))

    normals = constant(3, 0.0)
    normals[..., 2] = 1.0

    return dict(
        noisy=random(3, 10.0),
        albedo=random(3),
        normals=normals,
        depth=random(1, 10.0, 1.0),
        specular_albedo=random(3),
        roughness=random(1),
        flow=constant(2, 0.0),
    )


def test01_construct(variants_any_cuda):
    skip_if_unavailable()

    denoiser = mi.DLSSDenoiser([64, 48])
    assert str(denoiser) == (
        'DLSSDenoiser[\n  input_size = [64, 48],\n'
        '  output_size = [64, 48],\n  quality = "high"\n]'
    )

    with pytest.raises(Exception, match="too small"):
        mi.DLSSDenoiser([16, 16])

    with pytest.raises(Exception, match="cannot be smaller"):
        mi.DLSSDenoiser([64, 48], [32, 24])

    with pytest.raises(Exception, match="unknown quality mode"):
        mi.DLSSDenoiser([64, 48], [128, 96], "ultra")


def test02_denoise(variant_cuda_ad_rgb):
    skip_if_unavailable()

    res = [64, 48]
    denoiser = mi.DLSSDenoiser(res)
    denoised = denoiser(**make_inputs(res))

    assert denoised.shape == (res[1], res[0], 3)
    assert dr.all(dr.isfinite(denoised.array))
    assert dr.all(denoised.array >= 0.0)


def test03_denoise_upscale(variant_cuda_ad_rgb):
    skip_if_unavailable()

    res, out_res = [64, 48], [128, 96]
    denoiser = mi.DLSSDenoiser(res, out_res)
    denoised = denoiser(**make_inputs(res))

    assert denoised.shape == (out_res[1], out_res[0], 3)
    assert dr.all(dr.isfinite(denoised.array))


def test04_denoise_alpha_is_preserved(variant_cuda_ad_rgb):
    skip_if_unavailable()

    res = [64, 48]
    inputs = make_inputs(res)

    # Append an opaque alpha channel to the noisy input
    noisy, rgb = dr.zeros(mi.TensorXf, (res[1], res[0], 4)), inputs["noisy"]
    for i in range(3):
        noisy[..., i] = rgb[..., i]
    noisy[..., 3] = 1.0
    inputs["noisy"] = noisy

    denoiser = mi.DLSSDenoiser(res)
    denoised = denoiser(**inputs)

    assert denoised.shape == (res[1], res[0], 4)
    assert dr.allclose(mi.TensorXf(denoised)[..., 3].array, 1.0)


def test05_denoise_sequence(variant_cuda_ad_rgb):
    skip_if_unavailable()

    res = [64, 48]
    denoiser = mi.DLSSDenoiser(res)

    # A temporal sequence with a different subpixel offset per frame, DLSS
    # accumulates information across these invocations
    jitters = [[0.0, 0.0], [-0.25, 0.25], [0.25, -0.25], [0.375, 0.125]]
    for i, jitter in enumerate(jitters):
        denoised = denoiser(**make_inputs(res, seed=i), jitter=jitter,
                            reset=(i == 0))
        assert dr.all(dr.isfinite(denoised.array))

    assert denoised.shape == (res[1], res[0], 3)


def test06_validate_input(variant_cuda_ad_rgb):
    skip_if_unavailable()

    res = [64, 48]
    denoiser = mi.DLSSDenoiser(res)
    inputs = make_inputs(res)

    with pytest.raises(Exception, match="does not have this size"):
        denoiser(**dict(inputs, albedo=make_inputs([80, 48])["albedo"]))

    with pytest.raises(Exception, match="exactly 1 channel"):
        denoiser(**dict(inputs, depth=inputs["albedo"]))

    with pytest.raises(Exception, match="exactly 2 channel"):
        denoiser(**dict(inputs, flow=inputs["albedo"]))


def test07_denoise_multichannel_bitmap(variant_cuda_ad_rgb):
    skip_if_unavailable()

    scene = mi.load_file(find_resource("resources/data/scenes/cbox/cbox-rgb.xml"), res=64)
    sensor = scene.sensors()[0]
    integrator = mi.load_dict({
        'type': 'aov',
        'aovs': 'albedo:albedo,sh_normal:sh_normal,depth:depth',
        'img': {
            'type': 'path',
            'max_depth': 6,
        }
    })
    mi.render(scene, spp=2, integrator=integrator, sensor=sensor)
    multichannel = sensor.film().bitmap()

    denoiser = mi.DLSSDenoiser(multichannel.size())
    denoised = denoiser(multichannel, "albedo", "sh_normal", "depth",
                        to_sensor=sensor.world_transform().inverse())

    assert dr.all(denoised.size() == multichannel.size())
    dr.eval(mi.TensorXf(denoised.convert(
        component_format=mi.Struct.Type.Float32)))

    with pytest.raises(Exception, match="Could not find layer"):
        denoiser(multichannel, "albedo", "sh_normal", "missing")
