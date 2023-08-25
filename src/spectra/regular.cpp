#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/distr_1d.h>
#include <mitsuba/core/string.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _spectrum-regular:

Regular spectrum (:monosp:`regular`)
------------------------------------

.. pluginparameters::
 :extra-rows: 3

 * - wavelength_min
   - |float|
   - Minimum wavelength of the spectral range in nanometers.

 * - wavelength_max
   - |float|
   - Maximum wavelength of the spectral range in nanometers.

 * - values
   - |string|
   - Values of the spectral function at spectral range extremities.
   - |exposed|, |differentiable|

 * - range
   - |string|
   - Spectral emission range.
   - |exposed|, |differentiable|

This spectrum returns linearly interpolated reflectance or emission values from *regularly*
placed samples.

.. tabs::
    .. code-tab:: xml
        :name: regular

        <spectrum type="regular">
            <string name="range" value="400, 700">
            <string name="values" value="0.1, 0.2">
        </spectrum>

    .. code-tab:: python

        'type': 'regular',
        'wavelength_min': 400,
        'wavelength_max': 700,
        'values': '0.1, 0.2'
 */

template <typename Float, typename Spectrum>
class RegularSpectrum final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

public:
    RegularSpectrum(const Properties &props) : Texture(props) {
        ScalarVector2f wavelength_range(
            props.get<ScalarFloat>("wavelength_min"),
            props.get<ScalarFloat>("wavelength_max")
        );

        if (props.type("values") == Properties::Type::String) {
            std::vector<std::string> values_str =
                string::tokenize(props.string("values"), " ,");
            std::vector<ScalarFloat> data;
            data.reserve(values_str.size());

            for (const auto &s : values_str) {
                try {
                    data.push_back(string::stof<ScalarFloat>(s));
                } catch (...) {
                    Throw("Could not parse floating point value '%s'", s);
                }
            }

            m_distr = ContinuousDistribution<Wavelength>(
                wavelength_range, data.data(), data.size()
            );
        } else {
            // Scene/property parsing is in double precision, cast to single precision depending on variant.
            size_t size = props.get<size_t>("size");
            const double *ptr = static_cast<const double*>(props.pointer("values"));

            if constexpr (std::is_same_v<ScalarFloat, double>) {
                m_distr = ContinuousDistribution<Wavelength>(wavelength_range, ptr, size);
            } else {
                std::vector<ScalarFloat> values(size);
                for (size_t i=0; i < size; ++i)
                    values[i] = (ScalarFloat) ptr[i];
                m_distr = ContinuousDistribution<Wavelength>(
                    wavelength_range, values.data(), size);
            }
        }
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("range",  m_distr.range(), +ParamFlags::NonDifferentiable);
        callback->put_parameter("values", m_distr.pdf(),   +ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/) override {
        m_distr.update();
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>)
            return m_distr.eval_pdf(si.wavelengths, active);
        else
            NotImplementedError("eval");
    }

    Wavelength pdf_spectrum(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>)
            return m_distr.eval_pdf_normalized(si.wavelengths, active);
        else
            NotImplementedError("pdf");
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f & /*si*/,
                    const Wavelength &sample, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureSample, active);

        if constexpr (is_spectral_v<Spectrum>)
            return { m_distr.sample(sample, active), m_distr.integral() };
        else {
            DRJIT_MARK_USED(sample);
            NotImplementedError("sample");
        }
    }

    Float mean() const override {
        ScalarVector2f range = m_distr.range();
        return m_distr.integral() / (range[1] - range[0]);
    }

    ScalarVector2f wavelength_range() const override {
        return m_distr.range();
    }

    ScalarFloat spectral_resolution() const override {
        return m_distr.interval_resolution();
    }

    ScalarFloat max() const override {
        return m_distr.max();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RegularSpectrum[" << std::endl
            << "  distr = " << string::indent(m_distr) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    ContinuousDistribution<Wavelength> m_distr;
};

MI_IMPLEMENT_CLASS_VARIANT(RegularSpectrum, Texture)
MI_EXPORT_PLUGIN(RegularSpectrum, "Regular interpolated spectrum")
NAMESPACE_END(mitsuba)
