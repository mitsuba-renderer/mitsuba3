#pragma once

#include <mitsuba/core/bitmap.h>
#include <nanogui/texture.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_UI Texture : public nanogui::Texture {
public:
    using Base = nanogui::Texture;
    using Base::Base;

    Texture(const Bitmap *bitmap,
            InterpolationMode min_interpolation_mode = InterpolationMode::Bilinear,
            InterpolationMode mag_interpolation_mode = InterpolationMode::Bilinear,
            WrapMode wrap_mode                       = WrapMode::ClampToEdge);

protected:
    virtual ~Texture();
};

NAMESPACE_END(mitsuba)
