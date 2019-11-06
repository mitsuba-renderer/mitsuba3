#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Spectrum implementation (just throws exceptions)
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
std::pair<typename ContinuousSpectrum<Float, Spectrum>::Wavelength, Spectrum>
ContinuousSpectrum<Float, Spectrum>::sample(const SurfaceInteraction3f &, const Spectrum &sample_,
                                            Mask active) const {
    return sample(sample_, active);
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

template <typename Float, typename Spectrum>
Texture3D<Float, Spectrum>::Texture3D(const Properties &props) {
    m_world_to_local = props.transform("to_world", Transform4f()).inverse();
    update_bbox();
}

//! @}
// =======================================================================

NAMESPACE_END(mitsuba)
