#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/distr_1d.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _spectrum-irregular:

Irregular spectrum (:monosp:`irregular`)
----------------------------------------

This spectrum returns linearly interpolated reflectance or emission values from *irregularly*
placed samples.

 */

template <typename Float, typename Spectrum>
class IrregularSpectrum final : public Texture<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Texture)

public:
    IrregularSpectrum(const Properties &props) : Texture(props) {
        if (props.type("values") == Properties::Type::String) {
            std::vector<std::string> wavelengths_str =
                string::tokenize(props.string("wavelengths"), " ,");
            std::vector<std::string> entry_str, values_str =
                string::tokenize(props.string("values"), " ,");

            if (values_str.size() != wavelengths_str.size())
                Throw("IrregularSpectrum: 'wavelengths' and 'values' parameters must have the same size!");

            std::vector<ScalarFloat> values, wavelengths;
            values.reserve(values_str.size());
            wavelengths.reserve(values_str.size());

            for (size_t i = 0; i < values_str.size(); ++i) {
                try {
                    wavelengths.push_back((ScalarFloat) std::stod(wavelengths_str[i]));
                } catch (...) {
                    Throw("Could not parse floating point value '%s'", wavelengths_str[i]);
                }
                try {
                    values.push_back((ScalarFloat) std::stod(values_str[i]));
                } catch (...) {
                    Throw("Could not parse floating point value '%s'", values_str[i]);
                }
            }

            m_distr = IrregularContinuousDistribution<Wavelength>(
                wavelengths.data(), values.data(), values.size()
            );
        } else {
            size_t size = props.size_("size");
            const ScalarFloat *wavelengths = (ScalarFloat *) props.pointer("wavelengths");
            const ScalarFloat *values = (ScalarFloat *) props.pointer("values");

            m_distr = IrregularContinuousDistribution<Wavelength>(
                wavelengths, values, size
            );
        }
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("wavelengths", m_distr.nodes());
        callback->put_parameter("values", m_distr.pdf());
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/) override {
        m_distr.update();
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>)
            return m_distr.eval_pdf(si.wavelengths, active);
        else {
            ENOKI_MARK_USED(si);
            NotImplementedError("eval");
        }
    }

    Wavelength pdf_spectrum(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>)
            return m_distr.eval_pdf_normalized(si.wavelengths, active);
        else {
            ENOKI_MARK_USED(si);
            NotImplementedError("pdf");
        }
    }

    std::pair<Wavelength, UnpolarizedSpectrum> sample_spectrum(const SurfaceInteraction3f & /*si*/,
                                                      const Wavelength &sample,
                                                      Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureSample, active);

        if constexpr (is_spectral_v<Spectrum>)
            return { m_distr.sample(sample, active), m_distr.integral() };
        else {
            ENOKI_MARK_USED(sample);
            NotImplementedError("sample");
        }
    }

    ScalarFloat mean() const override {
        return m_distr.integral() / (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "IrregularSpectrum[" << std::endl
            << "  distr = " << string::indent(m_distr) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    IrregularContinuousDistribution<Wavelength> m_distr;
};

MTS_IMPLEMENT_CLASS_VARIANT(IrregularSpectrum, Texture)
MTS_EXPORT_PLUGIN(IrregularSpectrum, "Irregular interpolated spectrum")
NAMESPACE_END(mitsuba)
