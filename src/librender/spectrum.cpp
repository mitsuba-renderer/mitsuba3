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

std::pair<Spectrumf, Spectrumf>
ContinuousSpectrum::sample(const Spectrumf &) const {
    NotImplementedError("sample");
}

std::pair<SpectrumfP, SpectrumfP> ContinuousSpectrum::sample(const SpectrumfP &,
                                                             MaskP) const {
    NotImplementedError("sample_p");
}

Spectrumf ContinuousSpectrum::pdf(const Spectrumf &) const {
    NotImplementedError("pdf");
}

SpectrumfP ContinuousSpectrum::pdf(const SpectrumfP &, MaskP) const {
    NotImplementedError("pdf_p");
}

Float ContinuousSpectrum::mean() const {
    NotImplementedError("mean");
}

Spectrumf ContinuousSpectrum::eval(const SurfaceInteraction3f &si) const {
    return eval(si.wavelengths);
}

SpectrumfP ContinuousSpectrum::eval(const SurfaceInteraction3fP &si, MaskP active) const {
    return eval(si.wavelengths, active);
}

std::pair<Spectrumf, Spectrumf>
ContinuousSpectrum::sample(const SurfaceInteraction3f &, const Spectrumf &sample_) const {
    return sample(sample_);
}

std::pair<SpectrumfP, SpectrumfP>
ContinuousSpectrum::sample(const SurfaceInteraction3fP &, const SpectrumfP &sample_, MaskP active) const {
    return sample(sample_, active);
}

Spectrumf ContinuousSpectrum::pdf(const SurfaceInteraction3f &si) const {
    return pdf(si.wavelengths);
}

SpectrumfP ContinuousSpectrum::pdf(const SurfaceInteraction3fP &si,
                                   MaskP active) const {
    return pdf(si.wavelengths, active);
}

ref<ContinuousSpectrum> ContinuousSpectrum::D65(Float scale) {
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
