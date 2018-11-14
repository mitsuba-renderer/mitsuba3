#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Spectrum implementation (just throws exceptions)
// =======================================================================

ContinuousSpectrum::~ContinuousSpectrum() { }

Spectrumf ContinuousSpectrum::eval(const Spectrumf &) const {
    NotImplementedError("eval");
}

SpectrumfP ContinuousSpectrum::eval(const SpectrumfP &, MaskP) const {
    NotImplementedError("eval_p");
}

#if defined(MTS_ENABLE_AUTODIFF)
SpectrumfD ContinuousSpectrum::eval(const SpectrumfD &, MaskD) const {
    NotImplementedError("eval_d");
}
#endif

std::pair<Spectrumf, Spectrumf>
ContinuousSpectrum::sample(const Spectrumf &) const {
    NotImplementedError("sample");
}

std::pair<SpectrumfP, SpectrumfP> ContinuousSpectrum::sample(const SpectrumfP &,
                                                             MaskP) const {
    NotImplementedError("sample_p");
}

#if defined(MTS_ENABLE_AUTODIFF)
std::pair<SpectrumfD, SpectrumfD> ContinuousSpectrum::sample(const SpectrumfD &,
                                                             MaskD) const {
    NotImplementedError("sample_d");
}
#endif

Spectrumf ContinuousSpectrum::pdf(const Spectrumf &) const {
    NotImplementedError("pdf");
}

SpectrumfP ContinuousSpectrum::pdf(const SpectrumfP &, MaskP) const {
    NotImplementedError("pdf_p");
}


#if defined(MTS_ENABLE_AUTODIFF)
SpectrumfD ContinuousSpectrum::pdf(const SpectrumfD &, MaskD) const {
    NotImplementedError("pdf_d");
}
#endif

Float ContinuousSpectrum::mean() const {
    NotImplementedError("mean");
}

Spectrumf ContinuousSpectrum::eval(const SurfaceInteraction3f &si) const {
    return eval(si.wavelengths);
}

SpectrumfP ContinuousSpectrum::eval(const SurfaceInteraction3fP &si, MaskP active) const {
    return eval(si.wavelengths, active);
}

#if defined(MTS_ENABLE_AUTODIFF)
SpectrumfD ContinuousSpectrum::eval(const SurfaceInteraction3fD &si, MaskD active) const {
    return eval(si.wavelengths, active);
}
#endif

std::pair<Spectrumf, Spectrumf>
ContinuousSpectrum::sample(const SurfaceInteraction3f &, const Spectrumf &sample_) const {
    return sample(sample_);
}

std::pair<SpectrumfP, SpectrumfP>
ContinuousSpectrum::sample(const SurfaceInteraction3fP &, const SpectrumfP &sample_, MaskP active) const {
    return sample(sample_, active);
}

#if defined(MTS_ENABLE_AUTODIFF)
std::pair<SpectrumfD, SpectrumfD>
ContinuousSpectrum::sample(const SurfaceInteraction3fD &, const SpectrumfD &sample_, MaskD active) const {
    return sample(sample_, active);
}
#endif

Spectrumf ContinuousSpectrum::pdf(const SurfaceInteraction3f &si) const {
    return pdf(si.wavelengths);
}

SpectrumfP ContinuousSpectrum::pdf(const SurfaceInteraction3fP &si, MaskP active) const {
    return pdf(si.wavelengths, active);
}

#if defined(MTS_ENABLE_AUTODIFF)
SpectrumfD ContinuousSpectrum::pdf(const SurfaceInteraction3fD &si, MaskD active) const {
    return pdf(si.wavelengths, active);
}
#endif

ref<ContinuousSpectrum> ContinuousSpectrum::D65(Float scale, bool monochrome) {
    if (unlikely(monochrome)) {
        Properties props("uniform");
        // Should output 1 by default.
        props.set_float("value", scale / (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN));
        auto obj = PluginManager::instance()->create_object<ContinuousSpectrum>(props);
        return (ContinuousSpectrum *) (obj.get());
    }

    Properties props("d65");
    props.set_float("value", scale);
    auto obj =
        PluginManager::instance()->create_object<ContinuousSpectrum>(props);
    return (ContinuousSpectrum *) (obj->expand()[0].get());
}

//! @}
// =======================================================================

MTS_IMPLEMENT_CLASS_ALIAS(ContinuousSpectrum, "spectrum", Object)
NAMESPACE_END(mitsuba)
