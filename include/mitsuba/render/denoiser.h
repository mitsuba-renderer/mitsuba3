// #prsagma once

#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/platform.h>
#include <mitsuba/render/optix_api.h>
#include <enoki-jit/optix.h>
#include <enoki/array.h>
#include <iostream>
#include <exception>

#define optix_check(call) if(call != 0) {std::cerr << "Optix call failed" << std::endl; return Float(0.0);}                  

NAMESPACE_BEGIN(mitsuba)

template <typename Float>
Float denoise(const Bitmap& noisy, Bitmap* albedo, Bitmap* normals) {
    if constexpr (ek::is_array_v<Float>) {
        OptixDeviceContext context = jit_optix_context();

        OptixDenoiser denoiser = nullptr;  
        OptixDenoiserSizes sizes = {};
        OptixDenoiserParams params = {};
        OptixDenoiserOptions options = {};
        OptixDenoiserModelKind modelKind = albedo == nullptr ? OPTIX_DENOISER_MODEL_KIND_HDR : OPTIX_DENOISER_MODEL_KIND_AOV;
        options.inputKind = albedo == nullptr ? OPTIX_DENOISER_INPUT_RGB : OPTIX_DENOISER_INPUT_RGB_ALBEDO_NORMAL;

        uint32_t scratch_size = 0;
        uint32_t state_size = 0;

        OptixImage2D inputs[3] = {};
        OptixImage2D output;

        optix_check(optixDenoiserCreate(context, &options, &denoiser)); 
        optix_check(optixDenoiserSetModel(denoiser, modelKind, nullptr, 0));
        optix_check(optixDenoiserComputeMemoryResources(denoiser, noisy.width(), noisy.height(), &sizes));
        scratch_size = static_cast<uint32_t>(sizes.withoutOverlapScratchSizeInBytes);
        state_size = static_cast<uint32_t>(sizes.stateSizeInBytes);
        // const uint64_t frame_size = noisy.pixel_count() * noisy.bytes_per_pixel();

        std::cout << scratch_size << std::endl;
        std::cout << state_size << std::endl;

        Float intensity_float = ek::zero<Float>(1);
        Float scratch_float = ek::zero<Float>(scratch_size / sizeof(float));
        Float state_float = ek::zero<Float>(state_size / sizeof(float));
        Float output_data = ek::zero<Float>(noisy.pixel_count() * noisy.channel_count());
        ek::eval(intensity_float, scratch_float, state_float, output_data);
        ek::sync_thread();

        CUdeviceptr intensity = 0;
        CUdeviceptr scratch = 0;
        CUdeviceptr state = 0;
        intensity = intensity_float.data();
        scratch = scratch_float.data();
        state = state_float.data();
        output.data = output_data.data();

        output.width = noisy.width();
        output.height = noisy.height();
        output.format = OPTIX_PIXEL_FORMAT_FLOAT4;
        output.rowStrideInBytes = noisy.width() * noisy.bytes_per_pixel();
        output.pixelStrideInBytes = noisy.bytes_per_pixel();
        DynamicBuffer<Float> noisy_data = ek::load<DynamicBuffer<Float>>(noisy.data(), noisy.pixel_count() * noisy.channel_count());
        inputs[0].data = noisy_data.data();
        inputs[0].width = noisy.width();
        inputs[0].height = noisy.height();
        inputs[0].format = OPTIX_PIXEL_FORMAT_FLOAT4;
        inputs[0].rowStrideInBytes = noisy.width() * noisy.bytes_per_pixel();
        inputs[0].pixelStrideInBytes = noisy.bytes_per_pixel();

        // std::cout << noisy.pixel_format() << std::endl;
        // std::cout << noisy.has_alpha() << std::endl;
        // std::cout << noisy.bytes_per_pixel() << std::endl;
        std::cout << noisy_data << std::endl;
        

        unsigned int nb_channels = 1;
        if(albedo != nullptr) {

            DynamicBuffer<Float> albedo_data = ek::load<DynamicBuffer<Float>>(albedo->data(), albedo->pixel_count() * albedo->channel_count());
            inputs[1].data = albedo_data.data();
            inputs[1].width = albedo->width();
            inputs[1].height = albedo->height();
            inputs[1].format = OPTIX_PIXEL_FORMAT_FLOAT4;
            inputs[1].rowStrideInBytes = albedo->width() * albedo->bytes_per_pixel();
            inputs[1].pixelStrideInBytes = albedo->bytes_per_pixel();
            nb_channels++;
        }
        if(normals != nullptr) {
            DynamicBuffer<Float> normals_data = ek::load<DynamicBuffer<Float>>(normals->data(), normals->pixel_count() * normals->channel_count());
            inputs[2].data = normals_data.data();
            inputs[2].width = normals->width();
            inputs[2].height = normals->height();
            inputs[2].format = OPTIX_PIXEL_FORMAT_FLOAT4;
            inputs[2].rowStrideInBytes = normals->width() * normals->bytes_per_pixel();
            inputs[2].pixelStrideInBytes = normals->bytes_per_pixel();
            nb_channels++;
        }

        optix_check(optixDenoiserSetup(denoiser, 0, noisy.width(), noisy.height(), state, state_size, scratch, scratch_size));

        params.denoiseAlpha = 0;
        params.hdrIntensity = intensity;
        params.blendFactor  = 0.0f;

        optix_check(optixDenoiserComputeIntensity(denoiser, 0, inputs, intensity, scratch, scratch_size)); 
        optix_check(optixDenoiserInvoke(denoiser, 0, &params, state, state_size, inputs, nb_channels, 
                                        0, 0, &output, scratch, scratch_size));

        Float denoised_data = ek::map<Float>(output.data, noisy.pixel_count() * noisy.channel_count());
        // Float denoised_data = ek::map<Float>(output.data, 114688);

        ek::eval();
        ek::sync_thread();

        std::cout << denoised_data << std::endl;
        optix_check(optixDenoiserDestroy(denoiser));
        return denoised_data;
    }
    return Float(0.0);
}

template <typename Float>
Float denoise (const Bitmap& noisy) {
    return denoise<Float>(noisy, nullptr, nullptr);
}

NAMESPACE_END(mitsuba)
