#include <mitsuba/ui/texture.h>

NAMESPACE_BEGIN(mitsuba)

static nanogui::Texture::PixelFormat convert_pixel_format(Bitmap::EPixelFormat pf) {
    switch (pf) {
        case Bitmap::EY:    return Texture::PixelFormat::R;
        case Bitmap::EYA:   return Texture::PixelFormat::RA;
        case Bitmap::ERGB:  return Texture::PixelFormat::RGB;
        case Bitmap::ERGBA: return Texture::PixelFormat::RGBA;
        default: Throw("Texture::Texture(): unsupported pixel format '%s'!", pf);
    }
}

static nanogui::Texture::ComponentFormat convert_component_format(Struct::EType cf) {
    switch (cf) {
        case Struct::EUInt8:   return Texture::ComponentFormat::UInt8;
        case Struct::EInt8:    return Texture::ComponentFormat::Int8;
        case Struct::EUInt16:  return Texture::ComponentFormat::UInt16;
        case Struct::EInt16:   return Texture::ComponentFormat::Int16;
        case Struct::EUInt32:  return Texture::ComponentFormat::UInt32;
        case Struct::EInt32:   return Texture::ComponentFormat::Int32;
        case Struct::EFloat16: return Texture::ComponentFormat::Float16;
        case Struct::EFloat32: return Texture::ComponentFormat::Float32;
        default: Throw("Texture::Texture(): unsupported component format '%s'!", cf);
    }
}

Texture::Texture(const Bitmap *bitmap, InterpolationMode min_interpolation_mode,
                 InterpolationMode mag_interpolation_mode, WrapMode wrap_mode)
    : Base(convert_pixel_format(bitmap->pixel_format()),
           convert_component_format(bitmap->component_format()), bitmap->size(),
           min_interpolation_mode, mag_interpolation_mode, wrap_mode) {
    if (convert_pixel_format(bitmap->pixel_format()) != m_pixel_format)
        Throw("Texture::Texture(): pixel format not supported by the hardware!");
    if (convert_component_format(bitmap->component_format()) != m_component_format)
        Throw("Texture::Texture(): component format not supported by the hardware!");
    upload((const uint8_t *)  bitmap->data());
}

Texture::~Texture() { }

NAMESPACE_END(mitsuba)
