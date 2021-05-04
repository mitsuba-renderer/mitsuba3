#pragma once

#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/optix_api.h>
#include <enoki-jit/optix.h>
#include <enoki/array.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float>
void denoise (Bitmap& noisy, const Bitmap* albedo, const Bitmap* normals) {
    OptixDeviceContext context = jit_optix_context();

    OptixDenoiser* denoiser = new OptixDenoiser();        
    OptixDenoiserSizes* sizes = new OptixDenoiserSizes();
    OptixDenoiserParams* params = new OptixDenoiserParams();
    OptixDenoiserOptions* options = new OptixDenoiserOptions();
    OptixDenoiserModelKind modelKind = OPTIX_DENOISER_MODEL_KIND_AOV;
    options->inputKind = OPTIX_DENOISER_INPUT_RGB_ALBEDO_NORMAL;

    uint32_t scratch_size = 0;
    uint32_t state_size = 0;
    const uint64_t frame_size = noisy.width() * noisy.height();

    OptixImage2D inputs[3] = {};
    OptixImage2D output;

    //TODO check result
    OptixResult result = optixDenoiserCreate(context, options, denoiser); 
    result = optixDenoiserSetModel(denoiser, modelKind, nullptr, 0);
    result = optixDenoiserComputeMemoryResources(denoiser, noisy.width(), noisy.height(), sizes);
    scratch_size = static_cast<uint32_t>(sizes->withoutOverlapScratchSizeInBytes / sizeof(float));

    ek::Array<Float> int_float = ek::zero<Float>(1);
    ek::Array<Float> scratch_float = ek::zero<Float>(scratch_size);
    ek::Array<Float> state_float = ek::zero<Float>(sizes->stateSizeInBytes / sizeof(float));
    ek::Array<Float> noisy_data = ek::zero<Float>(frame_size);
    ek::Array<Float> normals_data = ek::zero<Float>(frame_size);
    ek::Array<Float> albedo_data = ek::zero<Float>(frame_size);
    ek::Array<Float> output_data = ek::zero<Float>(frame_size);
    ek::eval(int_float);
    ek::eval(scratch_float);
    ek::eval(state_float);
    ek::eval(noisy_data);
    ek::eval(normals_data);
    ek::eval(albedo_data);
    ek::eval(output_data);
    CUdeviceptr intensity = (CUdeviceptr) int_float.data();
    CUdeviceptr scratch = (CUdeviceptr) scratch_float.data();
    CUdeviceptr state = (CUdeviceptr) state_float.data();

    inputs[0].data = noisy.data();
    inputs[0].width = noisy.width();
    inputs[0].height = noisy.height();
    inputs[0].format = OPTIX_PIXEL_FORMAT_FLOAT4;
    inputs[0].rowStrideInBytes = noisy.width() * 4 * sizeof(Float);
    inputs[0].pixelStrideInBytes = 4 * sizeof(Float);

    unsigned int nb_channels = 1;
    if(albedo!= nullptr) {
        inputs[1].data = (void*) albedo->data();
        inputs[1].width = albedo->width();
        inputs[1].height = albedo->height();
        inputs[1].format = OPTIX_PIXEL_FORMAT_FLOAT4;
        inputs[1].rowStrideInBytes = albedo->width() * 4 * sizeof(Float);
        inputs[1].pixelStrideInBytes = 4 * sizeof(Float);
        nb_channels++;
    }
    if(normals != nullptr) {
        inputs[2].data = (void*) normals->data();
        inputs[2].width = normals->width();
        inputs[2].height = normals->height();
        inputs[2].format = OPTIX_PIXEL_FORMAT_FLOAT4;
        inputs[2].rowStrideInBytes = normals->width() * 4 * sizeof(Float);
        inputs[2].pixelStrideInBytes = 4 * sizeof(Float);
        nb_channels++;
    }

    output.data = output_data.data();
    output.width = noisy.width();
    output.height = noisy.height();
    output.format = OPTIX_PIXEL_FORMAT_FLOAT4;
    output.rowStrideInBytes = noisy.width() * 4 * sizeof(Float);
    output.pixelStrideInBytes = 4 * sizeof(Float);

    result = optixDenoiserSetup(denoiser, 0, noisy.width(), noisy.height(), state, state_size, scratch, scratch_size);

    params->denoiseAlpha = 0;
    params->hdrIntensity = intensity;
    params->blendFactor  = 0.0f;

    result = optixDenoiserComputeIntensity(denoiser, 0, inputs, intensity, scratch, scratch_size); 
    result = optixDenoiserInvoke(denoiser, 0, params, state, state_size, inputs, nb_channels, 0, 0, &output, scratch, scratch_size);
    if(result != 0)
        return;
    
    Float* data_ptr = (Float*) noisy.data();
    Float* output_ptr = (Float*) output.data;
    for(size_t i = 0 ; i < frame_size ; ++i)
        data_ptr[i] = output_ptr[i];

    optixDenoiserDestroy(denoiser);
    delete options;
    delete params;
    delete sizes;
    delete denoiser;
}

template <typename Float>
void denoise (Bitmap& noisy) {
    denoise<Float>(noisy, nullptr, nullptr);
}

NAMESPACE_END(mitsuba)