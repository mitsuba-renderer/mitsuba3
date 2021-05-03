#pragma once

#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/optix_api.h>

NAMESPACE_BEGIN(mitsuba)

void denoise (Bitmap& noisy, const Bitmap* albedo, const Bitmap* normals) {
    noisy.data();
    albedo->data();
    normals->data();
}

void denoise (Bitmap& noisy) {
    denoise(noisy, nullptr, nullptr);
}

NAMESPACE_END(mitsuba)