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
 :extra-rows: 5

 * - wavelength_min
   - |float|
   - Minimum wavelength of the spectral range in nanometers.

 * - wavelength_max
   - |float|
   - Maximum wavelength of the spectral range in nanometers.

 * - frequency_min
   - |float|
   - Minimum frequency of the spectral range in Hz (alternative to wavelength parameters for acoustic rendering).

 * - frequency_max
   - |float|
   - Maximum frequency of the spectral range in Hz (alternative to wavelength parameters for acoustic rendering).

 * - values
   - |string|
   - Values of the spectral function at spectral range extremities.
   - |exposed|, |differentiable|

 * - range
   - |string|
   - Spectral emission range (wavelengths or frequencies).
   - |exposed|, |differentiable|

This spectrum returns linearly interpolated reflectance or emission values from *regularly*
placed samples. You can specify either wavelengths or frequencies as the domain.

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

    .. code-tab:: python Python (acoustic)
        :name: regular-acoustic

        'type': 'regular',
        'frequency_min': 250,
        'frequency_max': 500,
        'values': '0.1, 0.2'
 */

template <typename Float, typename Spectrum>
class RegularSpectrum final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

public:
    RegularSpectrum(const Properties &props) : Texture(props) {
        if (props.has_property("value")) {
            const Properties::Spectrum *spec = props.try_get<Properties::Spectrum>("value");
            if (!spec)
                Throw("Failed to retrieve 'value' property as Properties::Spectrum");

            if (!spec->is_regular())
                Throw("RegularSpectrum requires regularly spaced wavelengths");

            init(*spec);
        } else {
            bool has_wavelengths = props.has_property("wavelength_min") && props.has_property("wavelength_max");
            bool has_frequencies = props.has_property("frequency_min") && props.has_property("frequency_max");
            if (has_wavelengths && has_frequencies) {
                Throw("Please specify either 'wavelength_min'/'wavelength_max' (for light rendering) or 'frequency_min'/'frequency_max' (for acoustic rendering), but not both.");
            } else if (has_wavelengths) {
                Properties::Spectrum spec(
                    props.get<std::string_view>("values"),
                    props.get<double>("wavelength_min"),
                    props.get<double>("wavelength_max")
                );
                init(spec);
            } else if (has_frequencies) {
                Properties::Spectrum spec(
                    props.get<std::string_view>("values"),
                    props.get<double>("frequency_min"),
                    props.get<double>("frequency_max")
                );
                init(spec);
            } else {
                Throw("Either 'wavelength_min/max' or 'frequency_min/max' property must be specified.");
            }
        }
    }

    void init(const Properties::Spectrum &spec) {
        ScalarVector2d range(
            spec.wavelengths.front(),
            spec.wavelengths.back()
        );

        if constexpr (std::is_same_v<ScalarFloat, double>) {
            m_distr = ContinuousDistribution<Wavelength>(
                range, spec.values.data(), spec.values.size());
        } else {
            std::vector<ScalarFloat> values(spec.values.size());
            for (size_t i = 0; i < spec.values.size(); ++i)
                values[i] = (ScalarFloat) spec.values[i];
            m_distr = ContinuousDistribution<Wavelength>(
                ScalarVector2f(range), values.data(), values.size());
        }
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("range", m_distr.range(), ParamFlags::NonDifferentiable);
        cb->put("values", m_distr.pdf(), ParamFlags::Differentiable);
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

    MI_DECLARE_CLASS(RegularSpectrum)
private:
    ContinuousDistribution<Wavelength> m_distr;

    MI_TRAVERSE_CB(Texture, m_distr)
};

MI_EXPORT_PLUGIN(RegularSpectrum)
NAMESPACE_END(mitsuba)
