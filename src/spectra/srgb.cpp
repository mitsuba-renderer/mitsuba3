#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _spectrum-srgb:

sRGB spectrum (:monosp:`srgb`)
------------------------------

.. pluginparameters::
 :extra-rows: 1

 * - color
   - :paramtype:`color`
   - The corresponding sRGB color value.

 * - value
   - :paramtype:`color`
   - The sRGB color value, exposed for differentiation and parameter updates.
   - |exposed|, |differentiable|

In spectral render modes, this smooth spectrum is the result of the
*spectral upsampling* process :cite:`Jakob2019Spectral` used by the system.
In RGB render modes, this spectrum represents a constant RGB value.
In monochrome modes, this spectrum represents a constant luminance value.

.. tabs::
    .. code-tab:: xml
        :name: srgb

        <spectrum type="srgb">
            <rgb name="color" value="10, 20, 250"/>
        </spectrum>

    .. code-tab:: python

        'type': 'srgb',
        'color': [10, 20, 250]

 */

template <typename Float, typename Spectrum>
class SRGBReflectanceSpectrum final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    SRGBReflectanceSpectrum(const Properties &props) : Texture(props) {
        ScalarColor3f color = props.get<ScalarColor3f>("color");

        if (dr::any(color < 0 || color > 1) && !props.get<bool>("unbounded", false))
            Throw("Invalid RGB reflectance value %s, must be in the range [0, 1]!", color);
        props.mark_queried("unbounded");

        m_color = color;
        dr::make_opaque(m_color);

        if constexpr (is_spectral_v<Spectrum>) {
            m_coeff = SRGBModel<Float, Spectrum>::fetch(color);
            dr::make_opaque(m_coeff);
        }
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("value", m_color, ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        dr::make_opaque(m_color);

        if constexpr (is_spectral_v<Spectrum>) {
            m_coeff = SRGBModel<Float, Spectrum>::fetch(m_color);
            dr::make_opaque(m_coeff);
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>)
            return srgb_model_eval<UnpolarizedSpectrum>(m_coeff, si.wavelengths);
        else if constexpr (is_monochromatic_v<Spectrum>)
            return eval_1(si, active);
        else
            return m_color;
    }

    Color3f eval_3(const SurfaceInteraction3f &/*si*/, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return m_color;
    }

    Float eval_1(const SurfaceInteraction3f & /*it*/, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        if constexpr (is_spectral_v<Spectrum>)
            return dr::mean(srgb_model_mean(m_coeff));
        else if constexpr (is_monochromatic_v<Spectrum>)
            return luminance(m_color);
        else
            return dr::mean(m_color);
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &_si,
                    const Wavelength &sample,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureSample, active);

        if constexpr (is_spectral_v<Spectrum>) {
            // TODO: better sampling strategy
            SurfaceInteraction3f si(_si);
            si.wavelengths = MI_CIE_MIN + (MI_CIE_MAX - MI_CIE_MIN) * sample;
            return { si.wavelengths, eval(si, active) * (MI_CIE_MAX - MI_CIE_MIN) };
        } else {
            DRJIT_MARK_USED(sample);
            UnpolarizedSpectrum value = eval(_si, active);
            return { dr::empty<Wavelength>(), value };
        }
    }

    ScalarFloat max() const override {
        if constexpr (is_spectral_v<Spectrum>)
            return dr::max_nested(srgb_model_mean(m_coeff));
        else if constexpr (is_monochromatic_v<Spectrum>)
            return dr::max_nested(luminance(m_color));
        else
            return dr::max_nested(m_color);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SRGBReflectanceSpectrum[" << std::endl
            << "  value = " << string::indent(m_color) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(SRGBReflectanceSpectrum)
protected:
    /// The sRGB color, which is the authoritative representation
    Color<Float, 3> m_color;

    /// Spectral upsampling coefficients, derived from \ref m_color
    dr::Array<Float, 3> m_coeff;

    MI_TRAVERSE_CB(Texture, m_color, m_coeff)
};

MI_EXPORT_PLUGIN(SRGBReflectanceSpectrum)
NAMESPACE_END(mitsuba)
