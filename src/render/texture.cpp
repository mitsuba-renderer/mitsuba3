#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Texture implementations
// =======================================================================

MI_VARIANT Texture<Float, Spectrum>::Texture(const Properties &props)
    : m_id(props.id()) { }

MI_VARIANT Texture<Float, Spectrum>::~Texture() { }

MI_VARIANT typename Texture<Float, Spectrum>::UnpolarizedSpectrum
Texture<Float, Spectrum>::eval(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval");
}

MI_VARIANT std::pair<typename Texture<Float, Spectrum>::Wavelength,
                      typename Texture<Float, Spectrum>::UnpolarizedSpectrum>
Texture<Float, Spectrum>::sample_spectrum(const SurfaceInteraction3f &,
                                          const Wavelength &, Mask) const {
    NotImplementedError("sample_spectrum");
}

MI_VARIANT typename Texture<Float, Spectrum>::Wavelength
Texture<Float, Spectrum>::pdf_spectrum(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("pdf_spectrum");
}

MI_VARIANT Float Texture<Float, Spectrum>::eval_1(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval_1");
}

MI_VARIANT typename Texture<Float, Spectrum>::Vector2f
Texture<Float, Spectrum>::eval_1_grad(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval_1_grad");
}

MI_VARIANT typename Texture<Float, Spectrum>::Color3f
Texture<Float, Spectrum>::eval_3(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval_3");
}

MI_VARIANT Float
Texture<Float, Spectrum>::mean() const {
    NotImplementedError("mean");
}

MI_VARIANT
std::pair<typename Texture<Float, Spectrum>::Point2f, Float>
Texture<Float, Spectrum>::sample_position(const Point2f &sample,
                                          Mask) const {
    return { sample, 1.f };
}

MI_VARIANT
Float Texture<Float, Spectrum>::pdf_position(const Point2f &, Mask) const {
    return 1;
}

MI_VARIANT ref<Texture<Float, Spectrum>>
Texture<Float, Spectrum>::D65(ScalarFloat scale) {
    Properties props("d65");
    props.set_float("scale", Properties::Float(scale));
    ref<Texture> texture = PluginManager::instance()->create_object<Texture>(props);
    std::vector<ref<Object>> children = texture->expand();
    if (!children.empty())
        return (Texture *) children[0].get();
    return texture;
}

MI_VARIANT ref<Texture<Float, Spectrum>>
Texture<Float, Spectrum>::D65(ref<Texture> texture) {
    if constexpr (!is_spectral_v<Spectrum>) {
        return texture;
    } else {
        std::vector<std::string> plugins = {
            "srgb", "bitmap", "checkerboard", "mesh_attribute"
        };
        if (string::contains(plugins, texture->class_()->name())) {
            Properties props("d65");
            props.set_object("nested", ref<Object>(texture));
            ref<Texture> texture2 = PluginManager::instance()->create_object<Texture>(props);
            std::vector<ref<Object>> children = texture2->expand();
            if (!children.empty())
                return (Texture *) children[0].get();
            return texture2;
        }
        return texture;
    }
}

MI_VARIANT typename Texture<Float, Spectrum>::ScalarVector2i
Texture<Float, Spectrum>::resolution() const {
    return ScalarVector2i(1, 1);
}

MI_VARIANT typename Texture<Float, Spectrum>::ScalarFloat
Texture<Float, Spectrum>::spectral_resolution() const {
    NotImplementedError("spectral_resolution");
}

MI_VARIANT typename Texture<Float, Spectrum>::ScalarVector2f
Texture<Float, Spectrum>::wavelength_range() const {
    return ScalarVector2f(MI_CIE_MIN, MI_CIE_MAX);
}

MI_VARIANT typename Texture<Float, Spectrum>::ScalarFloat
Texture<Float, Spectrum>::max() const {
    NotImplementedError("max");
}

//! @}
// =======================================================================

MI_IMPLEMENT_CLASS_VARIANT(Texture, Object, "texture")

MI_INSTANTIATE_CLASS(Texture)
NAMESPACE_END(mitsuba)
