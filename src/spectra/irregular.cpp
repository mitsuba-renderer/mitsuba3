#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/distr_1d.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _spectrum-irregular:

Irregular spectrum (:monosp:`irregular`)
----------------------------------------

.. pluginparameters::

 * - wavelengths
   - |string|
   - Wavelength values where the function is defined.
   - |exposed|, |differentiable|

 * - values
   - |string|
   - Values of the spectral function at the specified wavelengths.
   - |exposed|, |differentiable|

This spectrum returns linearly interpolated reflectance or emission values from *irregularly*
placed samples.

.. tabs::
    .. code-tab:: xml
        :name: irregular

        <spectrum type="irregular">
            <string name="wavelengths" value="400, 700">
            <string name="values" value="0.1, 0.2">
        </spectrum>

    .. code-tab:: python

        'type': 'irregular',
        'wavelengths': '400, 700',
        'values': '0.1, 0.2'
 */

template <typename Float, typename Spectrum>
class IrregularSpectrum final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

public:
    IrregularSpectrum(const Properties &props) : Texture(props) {
        if (props.has_property("value")) {
            const Properties::Spectrum *spec = props.try_get<Properties::Spectrum>("value");
            if (!spec)
                Throw("Failed to retrieve 'value' property as Properties::Spectrum");

            init(*spec);
        } else {
            // Construct spectrum from separate wavelength and value strings
            Properties::Spectrum spec(
                props.get<std::string_view>("wavelengths"),
                props.get<std::string_view>("values")
            );
            init(spec);
        }
    }

    void init(const Properties::Spectrum &spec) {
        if constexpr (std::is_same_v<ScalarFloat, double>) {
            m_distr = IrregularContinuousDistribution<Wavelength>(
                spec.wavelengths.data(), spec.values.data(), spec.values.size());
        } else {
            std::vector<ScalarFloat> values(spec.values.size()),
                                     wavelengths(spec.wavelengths.size());
            for (size_t i = 0; i < spec.values.size(); ++i) {
                values[i] = (ScalarFloat) spec.values[i];
                wavelengths[i] = (ScalarFloat) spec.wavelengths[i];
            }
            m_distr = IrregularContinuousDistribution<Wavelength>(
                wavelengths.data(), values.data(), values.size());
        }
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("wavelengths", m_distr.nodes(), ParamFlags::Differentiable);
        cb->put("values",      m_distr.pdf(),   ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/) override {
        m_distr.update();
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>)
            return m_distr.eval_pdf(si.wavelengths, active);
        else {
            DRJIT_MARK_USED(si);
            NotImplementedError("eval");
        }
    }

    Wavelength pdf_spectrum(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>)
            return m_distr.eval_pdf_normalized(si.wavelengths, active);
        else {
            DRJIT_MARK_USED(si);
            NotImplementedError("pdf");
        }
    }

    std::pair<Wavelength, UnpolarizedSpectrum> sample_spectrum(const SurfaceInteraction3f & /*si*/,
                                                      const Wavelength &sample,
                                                      Mask active) const override {
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
        oss << "IrregularSpectrum[" << std::endl
            << "  distr = " << string::indent(m_distr) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(IrregularSpectrum)
private:
    IrregularContinuousDistribution<Wavelength> m_distr;

    MI_TRAVERSE_CB(Texture, m_distr)
};

MI_EXPORT_PLUGIN(IrregularSpectrum)
NAMESPACE_END(mitsuba)
