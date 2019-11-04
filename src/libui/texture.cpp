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

static Bitmap::EPixelFormat convert_pixel_format(nanogui::Texture::PixelFormat pf) {
    switch (pf) {
        case Texture::PixelFormat::R:    return Bitmap::EY;
        case Texture::PixelFormat::RA:   return Bitmap::EYA;
        case Texture::PixelFormat::RGB:  return Bitmap::ERGB;
        case Texture::PixelFormat::RGBA: return Bitmap::ERGBA;
        default: Throw("Texture::Texture(): unsupported pixel format '%i'!", (int) pf);
    }
}

static nanogui::Texture::ComponentFormat convert_component_format(FieldType cf) {
    switch (cf) {
        case FieldType::UInt8:   return Texture::ComponentFormat::UInt8;
        case FieldType::Int8:    return Texture::ComponentFormat::Int8;
        case FieldType::UInt16:  return Texture::ComponentFormat::UInt16;
        case FieldType::Int16:   return Texture::ComponentFormat::Int16;
        case FieldType::UInt32:  return Texture::ComponentFormat::UInt32;
        case FieldType::Int32:   return Texture::ComponentFormat::Int32;
        case FieldType::Float16: return Texture::ComponentFormat::Float16;
        case FieldType::Float32: return Texture::ComponentFormat::Float32;
        default: Throw("Texture::Texture(): unsupported component format '%s'!", cf);
    }
}

static FieldType convert_component_format(nanogui::Texture::ComponentFormat cf) {
    switch (cf) {
        case Texture::ComponentFormat::UInt8:   return FieldType::UInt8;
        case Texture::ComponentFormat::Int8:    return FieldType::Int8;
        case Texture::ComponentFormat::UInt16:  return FieldType::UInt16;
        case Texture::ComponentFormat::Int16:   return FieldType::Int16;
        case Texture::ComponentFormat::UInt32:  return FieldType::UInt32;
        case Texture::ComponentFormat::Int32:   return FieldType::Int32;
        case Texture::ComponentFormat::Float16: return FieldType::Float16;
        case Texture::ComponentFormat::Float32: return FieldType::Float32;
        default: Throw("Texture::Texture(): unsupported component format '%i'!", (int) cf);
    }
}

Texture::Texture(const Bitmap *bitmap, InterpolationMode min_interpolation_mode,
                 InterpolationMode mag_interpolation_mode, WrapMode wrap_mode)
    : Base(convert_pixel_format(bitmap->pixel_format()),
           convert_component_format(bitmap->component_format()), bitmap->size(),
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

Texture::~Texture() { }

NAMESPACE_END(mitsuba)
