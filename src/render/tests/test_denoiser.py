import pytest
import mitsuba as mi
import drjit as dr

from mitsuba.scalar_rgb.test.util import fresolver_append_path

###
### Reference images are generated using the `optixDenoiser` example provided
### with the OptiX 7.4 SDK. The example will always output float16 EXRs with an
### alpha channel.
###


def test01_denosier_construct(variants_any_cuda):
    input_res = [33, 18]

    assert (
        "Denoiser[\n  albedo = 0,\n  normals = 0,\n  temporal = 0\n]""" ==
        str(mi.Denoiser(input_res))
    )

    with pytest.raises(Exception) as e:
        mi.Denoiser(input_res, albedo=False, normals=True)
    e.match("The denoiser cannot use normals to guide its process without " +
            "also providing albedo information!")


@fresolver_append_path
def test02_denosier_denoise(variant_cuda_ad_rgb):
    noisy = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/noisy.exr"))
    ref = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/ref.exr"))[...,:3]

    denoiser = mi.Denoiser(noisy.shape[:2])
    denoised = mi.util.convert_to_bitmap(
        denoiser.denoise(noisy), uint8_srgb=False
    )
    denoised = mi.TensorXf(denoised.convert(component_format=mi.Struct.Type.Float16))

    ref_array = ref.array
    denoised_array = denoised.array

    assert dr.allclose(denoised_array, ref_array)


@fresolver_append_path
def test02_denosier_denoise_albedo(variant_cuda_ad_rgb):
    noisy = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/noisy.exr"))
    albedo = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/albedo.exr"))
    ref = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/ref_albedo.exr"))

    denoiser = mi.Denoiser(noisy.shape[:2], True)
    denoised = mi.util.convert_to_bitmap(
        denoiser.denoise(noisy, False, albedo), uint8_srgb=False
    )
    denoised = mi.TensorXf(denoised.convert(component_format=mi.Struct.Type.Float16))

    ref_tensor = mi.TensorXf(ref)[...,:3]
    denoised_tensor = mi.TensorXf(denoised)

    ref_array = ref_tensor.array
    denoised_array = denoised_tensor.array

    assert dr.allclose(denoised_array, ref_array)


@fresolver_append_path
def test02_denosier_denoise_normals(variant_cuda_ad_rgb):
    noisy = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/noisy.exr"))
    albedo = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/albedo.exr"))
    normals = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/normals.exr"))
    ref = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/ref_normals.exr"))

    denoiser = mi.Denoiser(noisy.shape[:2], True, True)

    denoised = mi.convert_to_bitmap(
        denoiser.denoise(noisy, False, albedo, normals), uint8_srgb=False
    )
    denoised = mi.TensorXf(denoised.convert(component_format=mi.Struct.Type.Float16))

    ref_tensor = mi.TensorXf(ref)[...,:3]
    denoised_tensor = mi.TensorXf(denoised)

    ref_array = ref_tensor.array
    denoised_array = denoised_tensor.array

    assert dr.allclose(denoised_array, ref_array)


@fresolver_append_path
def test02_denosier_denoise_normals(variant_cuda_ad_rgb):
    noisy = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/noisy.exr"))
    albedo = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/albedo.exr"))
    normals = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/normals.exr"))
    ref = mi.TensorXf(mi.Bitmap("resources/data/tests/denoiser/ref_normals.exr"))

    denoiser = mi.Denoiser(noisy.shape[:2], True, True)
    denoised = mi.util.convert_to_bitmap(
        denoiser.denoise(noisy, False, albedo, normals), uint8_srgb=False
    )
    denoised = mi.TensorXf(denoised.convert(component_format=mi.Struct.Type.Float16))

    ref_tensor = mi.TensorXf(ref)[...,:3]
    denoised_tensor = mi.TensorXf(denoised)

    ref_array = ref_tensor.array
    denoised_array = denoised_tensor.array

    assert dr.allclose(denoised_array, ref_array)
