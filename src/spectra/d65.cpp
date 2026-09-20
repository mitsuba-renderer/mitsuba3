#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _spectrum-d65:

D65 spectrum (:monosp:`d65`)
----------------------------

.. pluginparameters::
 :extra-rows: 2

 * - color
   - :paramtype:`color`
   - An sRGB color value to be composed with the D65 spectrum.

 * - scale
   - |float|
   - Optional scaling factor applied to the emitted spectrum. (Default: 1.0)

 * - (Nested plugin)
   - :paramtype:`texture`
   - Underlying texture/spectra to be multiplied by D65.
   - |exposed|, |differentiable|

 * - value
   - :paramtype:`color`
   - The sRGB color value, exposed for differentiation and parameter updates.
   - |exposed|, |differentiable|


The CIE Standard Illuminant D65 corresponds roughly to the average midday light
in Europe, also called a daylight illuminant. It is the default emission
spectrum used for light sources in all spectral rendering modes.

The D65 spectrum can be multiplied by a color value specified using the ``color``
parameters.

Alternatively, it is possible to modulate the D65 illuminant with a spectrally
or\and spatially varying signal defined by a nested texture plugin. This is used
in many emitter plugins when the radiance quantity might be driven by a 2D
texture but also needs to be multiplied with the D65 spectrum.

In RGB rendering modes, the D65 illuminant isn't relevant therefore this plugin
expands into another plugin type (e.g. ``uniform``, ``srgb``, ...) as the
product isn't required in this case.

.. tabs::
    .. code-tab:: xml
        :name: d65

        <shape type=".. shape type ..">
            <emitter type="area">
                <spectrum type="d65" />
            </emitter>
        </shape>

    .. code-tab:: python

        'type': '.. shape type ..',
        'emitter': {
            'type': 'area',
            'radiance': { 'type': 'd65', }
        }

 */

template <typename Float, typename Spectrum>
class D65Spectrum final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Texture)
    MI_IMPORT_TYPES()

    D65Spectrum(const Properties &props) : Base(props) {
        m_scale = props.get<ScalarFloat>("scale", 1.f);

        for (auto &prop : props.objects()) {
            Base *texture = prop.try_get<Base>();
            if (!texture)
                Throw("Child object should be a texture object.");
            if (m_nested_texture)
                Throw("Only a single texture child object can be specified.");
            m_nested_texture = texture;
        }

        if (props.has_property("color")) {
            if (m_nested_texture)
                Throw("Color and child texture object shouldn't be specified at "
                      "the same time.");

            ScalarColor3f color = props.get<ScalarColor3f>("color");
            m_color = color;
            dr::make_opaque(m_color);

            if constexpr (is_spectral_v<Spectrum>) {
                ScalarFloat factor = normalization(color);
                m_factor = factor;
                m_coeff = SRGBModel<Float, Spectrum>::fetch(color / factor);
                dr::make_opaque(m_factor, m_coeff);
            }

            m_has_value = true;
        }

        std::vector<double> data;
        data.reserve(MI_CIE_SAMPLES);
        for (size_t i = 0; i < MI_CIE_SAMPLES; ++i)
            data.push_back((double) d65_table[i] * (double) m_scale *
                           (double) MI_CIE_D65_NORMALIZATION);

        Properties props_d65("regular");
        props_d65.set("value",
                      Properties::Spectrum(std::move(data), (double) MI_CIE_MIN,
                                           (double) MI_CIE_MAX));

        m_d65 = (Base *) PluginManager::instance()->create_object<Base>(props_d65);
    }

    void traverse(TraversalCallback *cb) override {
        if (m_nested_texture)
            cb->put("nested_texture", m_nested_texture, ParamFlags::Differentiable);
        if (m_has_value)
            cb->put("value", m_color, ParamFlags::Differentiable);
        cb->put("d65", m_d65, ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        if (!m_has_value)
            return;

        dr::make_opaque(m_color);

        if constexpr (is_spectral_v<Spectrum>) {
            m_factor = normalization(m_color);
            m_coeff = SRGBModel<Float, Spectrum>::fetch(m_color / m_factor);
            dr::make_opaque(m_factor, m_coeff);
        }
    }

    /// Compute a normalization factor for use with the spectral upsampling
    /// model that keeps the brightest RGB component at 50%
    template <typename Color> static dr::value_t<Color> normalization(const Color &color) {
        dr::value_t<Color> factor = dr::max(color) * 2.f;
        return dr::select(factor == 0.f, 1.f, factor);
    }

    std::vector<ref<Object>> expand() const override {
        if constexpr (is_spectral_v<Spectrum>) {
            if (!m_nested_texture && !m_has_value)
                return { (Object *) m_d65.get() };
            return { };
        } else {
            if (m_nested_texture)
                 return { (Object *) m_nested_texture.get() };

            Properties props;
            if (m_has_value) {
                props.set_plugin_name("srgb");
                props.set("color", dr::slice(m_color) * m_scale);
                props.set("unbounded", true);
            } else {
                props.set_plugin_name("uniform");
                props.set("value", m_scale);
            }

            return { (Object *) PluginManager::instance()->create_object<Base>(props) };
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>) {
            UnpolarizedSpectrum d65_val = m_d65->eval(si, active);
            if (m_nested_texture)
                d65_val *= m_nested_texture->eval(si, active);
            else if (m_has_value)
                d65_val *= srgb_model_eval<UnpolarizedSpectrum>(
                               m_coeff, si.wavelengths) * m_factor;
            return d65_val;
        } else {
            DRJIT_MARK_USED(si);
            NotImplementedError("eval");
        }
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &si, const Wavelength &sample,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureSample, active);

        if constexpr (is_spectral_v<Spectrum>) {
            if (m_nested_texture) {
                auto [wav, weight] = m_nested_texture->sample_spectrum(si, sample, active);
                SurfaceInteraction3f si2(si);
                si2.wavelengths = wav;
                return { wav, weight * m_d65->eval(si2, active) };
            } else {
                // TODO: better sampling strategy
                SurfaceInteraction3f si2(si);
                si2.wavelengths = MI_CIE_MIN + (MI_CIE_MAX - MI_CIE_MIN) * sample;
                return { si2.wavelengths, eval(si2, active) * (MI_CIE_MAX - MI_CIE_MIN) };
            }
        } else {
            DRJIT_MARK_USED(si);
            DRJIT_MARK_USED(sample);
            NotImplementedError("sample_spectrum");
        }
    }

    Wavelength pdf_spectrum(const SurfaceInteraction3f &si, Mask active) const override {
        if constexpr (is_spectral_v<Spectrum>) {
            if (m_nested_texture)
                return m_nested_texture->pdf_spectrum(si, active);
            else if (m_has_value)
                return dr::rcp(MI_CIE_MAX - MI_CIE_MIN);
            else
                return Base::pdf_spectrum(si, active);
        } else {
            DRJIT_MARK_USED(si);
            DRJIT_MARK_USED(active);
            NotImplementedError("pdf_spectrum");
        }
    }

    std::pair<Point2f, Float> sample_position(const Point2f &sample, Mask active) const override {
        if (m_nested_texture)
            return m_nested_texture->sample_position(sample, active);
        else
            return Base::sample_position(sample, active);
    }

    Float pdf_position(const Point2f &p, Mask active) const override {
        if (m_nested_texture)
            return m_nested_texture->pdf_position(p, active);
        else
            return Base::pdf_position(p, active);
    }

    Float eval_1(const SurfaceInteraction3f &si, Mask active) const override {
        if (m_nested_texture)
            return m_nested_texture->eval_1(si, active);
        else
            return Base::eval_1(si, active);
    }

    Vector2f eval_1_grad(const SurfaceInteraction3f &si, Mask active) const override {
        if (m_nested_texture)
            return m_nested_texture->eval_1_grad(si, active);
        else
            return Base::eval_1_grad(si, active);
    }

    Color3f eval_3(const SurfaceInteraction3f &si, Mask active) const override {
        if (m_nested_texture)
            return m_nested_texture->eval_3(si, active);
        else
            return Base::eval_3(si, active);
    }

    ScalarVector2i resolution() const override {
        if (m_nested_texture)
            return m_nested_texture->resolution();
        else
            return Base::resolution();
    }

    ScalarFloat spectral_resolution() const override {
        return (MI_CIE_MAX - MI_CIE_MIN) / (MI_CIE_SAMPLES - 1);
    }

    ScalarVector2f wavelength_range() const override {
        if (m_nested_texture)
            return m_nested_texture->wavelength_range();
        else
            return ScalarVector2f(MI_CIE_MIN, MI_CIE_MAX);
    }

    ScalarFloat max() const override {
        if constexpr (is_spectral_v<Spectrum>) {
            if (m_nested_texture)
                return m_nested_texture->max();
            else if (m_has_value)
                return dr::max_nested(srgb_model_mean(m_coeff) * m_factor);
            else
                return 1.f;
        } else {
            NotImplementedError("max");
        }
    }

    bool is_spatially_varying() const override {
        if (m_nested_texture)
            return m_nested_texture->is_spatially_varying();
        else
            return false;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "D65Spectrum[" << std::endl;
        oss << "  scale = " << m_scale << std::endl;
        if (m_nested_texture)
            oss << "  nested_texture = " << string::indent(m_nested_texture) << std::endl;
        if (m_has_value)
            oss << "  value = " << m_color << std::endl;
        oss << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(D65Spectrum)
private:
    /// The sRGB color
    Color<Float, 3> m_color;

    /// Spectral upsampling coefficients
    dr::Array<Float, 3> m_coeff;

    /// Normalization factor for 'm_coeff'
    Float m_factor = 1.f;

    ref<Base> m_nested_texture;
    ref<Base> m_d65;

    ScalarFloat m_scale;

    bool m_has_value = false;

    MI_TRAVERSE_CB(Base, m_color, m_coeff, m_factor, m_nested_texture, m_d65)
};

MI_EXPORT_PLUGIN(D65Spectrum)
NAMESPACE_END(mitsuba)
