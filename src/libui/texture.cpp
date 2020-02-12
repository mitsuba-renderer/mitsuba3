#include <mitsuba/ui/texture.h>

NAMESPACE_BEGIN(mitsuba)

static nanogui::Texture::PixelFormat convert_pixel_format(Bitmap::PixelFormat pf) {
    switch (pf) {
        case Bitmap::PixelFormat::Y:    return nanogui::Texture::PixelFormat::R;
        case Bitmap::PixelFormat::YA:   return nanogui::Texture::PixelFormat::RA;
        case Bitmap::PixelFormat::RGB:  return nanogui::Texture::PixelFormat::RGB;
        case Bitmap::PixelFormat::RGBA: return nanogui::Texture::PixelFormat::RGBA;
        default: Throw("Texture::Texture(): unsupported pixel format '%s'!", pf);
    }
}

static Bitmap::PixelFormat convert_pixel_format(nanogui::Texture::PixelFormat pf) {
    switch (pf) {
        case nanogui::Texture::PixelFormat::R:    return Bitmap::PixelFormat::Y;
        case nanogui::Texture::PixelFormat::RA:   return Bitmap::PixelFormat::YA;
        case nanogui::Texture::PixelFormat::RGB:  return Bitmap::PixelFormat::RGB;
        case nanogui::Texture::PixelFormat::RGBA: return Bitmap::PixelFormat::RGBA;
        default: Throw("Texture::Texture(): unsupported pixel format '%i'!", (int) pf);
    }
}

static nanogui::Texture::ComponentFormat convert_component_format(Struct::Type cf) {
    switch (cf) {
        case Struct::Type::UInt8:   return nanogui::Texture::ComponentFormat::UInt8;
        case Struct::Type::Int8:    return nanogui::Texture::ComponentFormat::Int8;
        case Struct::Type::UInt16:  return nanogui::Texture::ComponentFormat::UInt16;
        case Struct::Type::Int16:   return nanogui::Texture::ComponentFormat::Int16;
        case Struct::Type::UInt32:  return nanogui::Texture::ComponentFormat::UInt32;
        case Struct::Type::Int32:   return nanogui::Texture::ComponentFormat::Int32;
        case Struct::Type::Float16: return nanogui::Texture::ComponentFormat::Float16;
        case Struct::Type::Float32: return nanogui::Texture::ComponentFormat::Float32;
        default: Throw("Texture::Texture(): unsupported component format '%s'!", cf);
    }
}

static Struct::Type convert_component_format(nanogui::Texture::ComponentFormat cf) {
    switch (cf) {
        case nanogui::Texture::ComponentFormat::UInt8:   return Struct::Type::UInt8;
        case nanogui::Texture::ComponentFormat::Int8:    return Struct::Type::Int8;
        case nanogui::Texture::ComponentFormat::UInt16:  return Struct::Type::UInt16;
        case nanogui::Texture::ComponentFormat::Int16:   return Struct::Type::Int16;
        case nanogui::Texture::ComponentFormat::UInt32:  return Struct::Type::UInt32;
        case nanogui::Texture::ComponentFormat::Int32:   return Struct::Type::Int32;
        case nanogui::Texture::ComponentFormat::Float16: return Struct::Type::Float16;
        case nanogui::Texture::ComponentFormat::Float32: return Struct::Type::Float32;
        default: Throw("Texture::Texture(): unsupported component format '%i'!", (int) cf);
    }
}

GPUTexture::GPUTexture(const Bitmap *bitmap, InterpolationMode min_interpolation_mode,
                 InterpolationMode mag_interpolation_mode, WrapMode wrap_mode)
    : Base(convert_pixel_format(bitmap->pixel_format()),
           convert_component_format(bitmap->component_format()), Vector<int, 2>(bitmap->size()),
           min_interpolation_mode, mag_interpolation_mode, wrap_mode) {
    ref<const Bitmap> source = bitmap;
    if (convert_pixel_format(bitmap->pixel_format()) != m_pixel_format ||
        convert_component_format(bitmap->component_format()) != m_component_format) {
        source = bitmap->convert(convert_pixel_format(m_pixel_format),
                                 convert_component_format(m_component_format),
                                 bitmap->srgb_gamma());
    }
    upload((const uint8_t *) source->data());
}

GPUTexture::~GPUTexture() { }

NAMESPACE_END(mitsuba)
