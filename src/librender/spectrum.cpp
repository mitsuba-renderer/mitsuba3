#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name ContinuousSpectrum implementations
// =======================================================================

template <typename Float, typename Spectrum>
ContinuousSpectrum<Float, Spectrum>::~ContinuousSpectrum() { }

template <typename Float, typename Spectrum>
Spectrum ContinuousSpectrum<Float, Spectrum>::eval(const Wavelength &, Mask) const {
    NotImplementedError("eval");
}

template <typename Float, typename Spectrum>
std::pair<typename ContinuousSpectrum<Float, Spectrum>::Wavelength, Spectrum>
ContinuousSpectrum<Float, Spectrum>::sample(const Wavelength &, Mask) const {
    NotImplementedError("sample");
}

template <typename Float, typename Spectrum>
Spectrum ContinuousSpectrum<Float, Spectrum>::pdf(const Wavelength &, Mask) const {
    NotImplementedError("pdf");
}

template <typename Float, typename Spectrum>
Float ContinuousSpectrum<Float, Spectrum>::mean() const {
    NotImplementedError("mean");
}

template <typename Float, typename Spectrum>
Spectrum ContinuousSpectrum<Float, Spectrum>::eval(const SurfaceInteraction3f &si,
                                                   Mask active) const {
    return eval(si.wavelengths, active);
}

template <typename Float, typename Spectrum>
Vector<Float, 3> ContinuousSpectrum<Float, Spectrum>::eval3(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval3");
}

template <typename Float, typename Spectrum>
Float ContinuousSpectrum<Float, Spectrum>::eval1(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval1");
}

template <typename Float, typename Spectrum>
std::pair<typename ContinuousSpectrum<Float, Spectrum>::Wavelength,
            Spectrum> ContinuousSpectrum<Float, Spectrum>::sample(const SurfaceInteraction3f &,
                                                                  const Spectrum &, Mask) const {
    NotImplementedError("sample");
}

template <typename Float, typename Spectrum>
Spectrum ContinuousSpectrum<Float, Spectrum>::pdf(const SurfaceInteraction3f &si,
                                                  Mask active) const {
    return pdf(si.wavelengths, active);
}

template <typename Float, typename Spectrum>
ref<ContinuousSpectrum<Float, Spectrum>> ContinuousSpectrum<Float, Spectrum>::D65(ScalarFloat scale) {
    if constexpr (is_monochrome_v<Spectrum>) {
        Properties props("uniform");
        // Should output 1 by default.
        props.set_float("value", scale / (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN));
        auto obj = PluginManager::instance()->create_object<ContinuousSpectrum>(props);
        return (ContinuousSpectrum *) (obj.get());
    } else {
        Properties props("d65");
        props.set_float("value", scale);
        auto obj =
            PluginManager::instance()->create_object<ContinuousSpectrum>(props);
        return (ContinuousSpectrum *) (obj->expand()[0].get());
    }
}

//! @}
// =======================================================================


// =======================================================================
//! @{ \name Texture implementations
// =======================================================================

template <typename Float, typename Spectrum>
Texture<Float, Spectrum>::~Texture() {}

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Texture3D implementations
// =======================================================================

template <typename Float, typename Spectrum>
Texture3D<Float, Spectrum>::Texture3D(const Properties &props) {
    m_world_to_local = props.transform("to_world", ScalarTransform4f()).inverse();
    update_bbox();
}

template <typename Float, typename Spectrum>
std::pair<Spectrum, typename Texture3D<Float, Spectrum>::Vector3f>
Texture3D<Float, Spectrum>::eval_gradient(const Interaction3f & /*it*/,
                                          Mask /*active*/) const {
    NotImplementedError("eval_gradient");
}

template <typename Float, typename Spectrum>
Float Texture3D<Float, Spectrum>::max() const { NotImplementedError("max"); }

//! @}
// =======================================================================

MTS_INSTANTIATE_OBJECT(ContinuousSpectrum)
MTS_INSTANTIATE_OBJECT(Texture)
MTS_INSTANTIATE_OBJECT(Texture3D)
NAMESPACE_END(mitsuba)
