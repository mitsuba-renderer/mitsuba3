#include <mitsuba/core/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Importance spectrum suitable for rendering RGB data
 *
 * Based on "An Improved Technique for Full Spectral Rendering"
 * Radziszewski, Boryczko, and Alda
 *
 * The proposed distribution and sampling technique is clamped to
 * the wavelength range 360nm..830nm
 */
class ImportanceSpectrum final : public ContinuousSpectrum {
public:
    ImportanceSpectrum(const Properties &) { }

    template <typename T>
    MTS_INLINE T eval_impl(T lambda) const {
        auto mask_valid = (lambda >= T(360)) & (lambda <= T(830));

        T tmp = sech(Float(0.0072) * (lambda - Float(538)));

        return select(mask_valid,
            Float(0.003939804229326285) * tmp * tmp,
            zero<T>()
        );
    }

    template <typename T>
    std::tuple<T, T, T> sample_impl(T sample) const {
        T lambda = Float(538) -
                   atanh(Float(0.8569106254698279) -
                         Float(1.8275019724092267) * sample) *
                   Float(138.88888888888889);

        T pdf = eval_impl(lambda);

        return std::make_tuple(lambda, T(Float(1)), pdf);
    }

    Spectrumf  eval(const Spectrumf  &lambda) const override { return eval_impl(lambda); }
    SpectrumfP eval(const SpectrumfP &lambda, const Mask &) const override { return eval_impl(lambda); }

    Spectrumf  pdf(const Spectrumf  &lambda) const override { return eval_impl(lambda); }
    SpectrumfP pdf(const SpectrumfP &lambda, const Mask &) const override { return eval_impl(lambda); }

    std::tuple<Spectrumf, Spectrumf, Spectrumf>
    sample(const Spectrumf &sample) const override {
        return sample_impl<Spectrumf>(sample);
    }

    std::tuple<SpectrumfP, SpectrumfP, SpectrumfP>
    sample(const SpectrumfP &sample, const Mask &) const override {
        return sample_impl<SpectrumfP>(sample);
    }

    Float integral() const override { return 1.0f; }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS(ImportanceSpectrum, ContinuousSpectrum)
MTS_EXPORT_PLUGIN(ImportanceSpectrum, "RGB Camera Importance Spectrum")

NAMESPACE_END(mitsuba)
