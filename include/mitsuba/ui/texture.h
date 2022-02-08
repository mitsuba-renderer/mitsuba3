#pragma once

#include <mitsuba/core/bitmap.h>
#include <nanogui/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Defines an abstraction for textures that works with
 * OpenGL, OpenGL ES, and Metal.
 *
 * Wraps nanogui::Texture and adds a new constructor for creating
 * textures from \ref mitsuba::Bitmap instances.
 */
class MI_EXPORT_UI GPUTexture : public nanogui::Texture {
public:
    using Base = nanogui::Texture;
    using Base::Base;

    GPUTexture(const Bitmap *bitmap,
            InterpolationMode min_interpolation_mode = InterpolationMode::Bilinear,
            InterpolationMode mag_interpolation_mode = InterpolationMode::Bilinear,
            WrapMode wrap_mode                       = WrapMode::ClampToEdge);

protected:
    virtual ~GPUTexture();
};

NAMESPACE_END(mitsuba)
