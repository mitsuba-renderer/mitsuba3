#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Texture implementations
// =======================================================================

MTS_VARIANT Texture<Float, Spectrum>::Texture(const Properties &props)
    : m_id(props.id()) { }

MTS_VARIANT Texture<Float, Spectrum>::~Texture() { }

MTS_VARIANT typename Texture<Float, Spectrum>::UnpolarizedSpectrum
Texture<Float, Spectrum>::eval(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval");
}

MTS_VARIANT std::pair<typename Texture<Float, Spectrum>::Wavelength,
                      typename Texture<Float, Spectrum>::UnpolarizedSpectrum>
Texture<Float, Spectrum>::sample(const SurfaceInteraction3f &, const Wavelength&, Mask) const {
    NotImplementedError("sample");
}

MTS_VARIANT typename Texture<Float, Spectrum>::Wavelength
Texture<Float, Spectrum>::pdf(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("pdf");
}

MTS_VARIANT Float Texture<Float, Spectrum>::eval_1(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval_1");
}

MTS_VARIANT typename Texture<Float, Spectrum>::Color3f
Texture<Float, Spectrum>::eval_3(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval_3");
}

MTS_VARIANT typename Texture<Float, Spectrum>::ScalarFloat
Texture<Float, Spectrum>::mean() const {
    NotImplementedError("mean");
}

MTS_VARIANT ref<Texture<Float, Spectrum>>
Texture<Float, Spectrum>::D65(ScalarFloat scale) {
    Properties props(is_spectral_v<Spectrum> ? "d65" : "uniform");
    props.set_float("value", scale);
    ref<Texture> texture = PluginManager::instance()->create_object<Texture>(props);
    std::vector<ref<Object>> children = texture->expand();
    if (!children.empty())
        return (Texture *) children[0].get();
    return texture;
}

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Texture3D implementations
// =======================================================================

MTS_VARIANT Texture3D<Float, Spectrum>::Texture3D(const Properties &props) {
    m_world_to_local = props.transform("to_world", ScalarTransform4f()).inverse();
    update_bbox();
}

MTS_VARIANT std::pair<Spectrum, typename Texture3D<Float, Spectrum>::Vector3f>
Texture3D<Float, Spectrum>::eval_gradient(const Interaction3f & /*it*/,
                                          Mask /*active*/) const {
    NotImplementedError("eval_gradient");
}

MTS_VARIANT typename Texture3D<Float, Spectrum>::ScalarFloat
Texture3D<Float, Spectrum>::max() const { NotImplementedError("max"); }

//! @}
// =======================================================================

MTS_IMPLEMENT_CLASS_VARIANT(Texture, Object, "texture")
MTS_IMPLEMENT_CLASS_VARIANT(Texture3D, Object, "texture3d")

MTS_INSTANTIATE_CLASS(Texture)
MTS_INSTANTIATE_CLASS(Texture3D)
NAMESPACE_END(mitsuba)
