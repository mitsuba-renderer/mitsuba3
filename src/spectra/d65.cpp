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
   - The corresponding sRGB color value.

 * - scale
   - |float|
   - Optional scaling factor applied to the emitted spectrum. (Default: 1.0)

 * - (Nested plugin)
   - :paramtype:`texture`
   - Underlying texture/spectra to be multiplied by D65.
   - |exposed|, |differentiable|

 * - color
   - :paramtype:`color`
   - Spectral upsampling model coefficients of the srgb color value.
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

        auto objects = props.objects(true);
        if (objects.size() > 1)
            Throw("Only a single texture child object can be specified.");
        if (objects.size() == 1) {
            m_nested_texture = dynamic_cast<Base *>(objects[0].second.get());
            if (!m_nested_texture)
                Throw("Child object should be a texture object.");
        }

        if (props.has_property("color")) {
            if (m_nested_texture)
                Throw("Color and child texture object shouldn't be specified at "
                      "the same time.");

            ScalarColor3f color = props.get<ScalarColor3f>("color");

            if constexpr (is_spectral_v<Spectrum>) {
                /* Evaluate the spectral upsampling model. This requires a
                   reflectance value (colors in [0, 1]) which is accomplished here by
                   scaling. We use a color where the highest component is 50%,
                   which generally yields a fairly smooth spectrum. */
                ScalarFloat factor = dr::max(color) * 2.f;
                if (factor != 0.f)
                    color /= factor;
                m_scale *= factor;

                m_value = srgb_model_fetch(color);
            } else {
                m_value = color;
            }

            dr::make_opaque(m_value);
            m_has_value = true;
        }

        Properties props_d65("regular");
        props_d65.set_float("wavelength_min", (Properties::Float) MI_CIE_MIN);
        props_d65.set_float("wavelength_max", (Properties::Float) MI_CIE_MAX);
        props_d65.set_int("size", MI_CIE_SAMPLES);
        Properties::Float data[MI_CIE_SAMPLES];
        for (size_t i = 0; i < MI_CIE_SAMPLES; ++i)
            data[i] = Properties::Float(d65_table[i] * m_scale * ScalarFloat(MI_CIE_D65_NORMALIZATION));
        props_d65.set_pointer("values", (const void *) &data[0]);
        m_d65 = (Base *) PluginManager::instance()->create_object<Base>(props_d65);
    }

    void traverse(TraversalCallback *callback) override {
        if (m_nested_texture)
            callback->put_object("nested_texture", m_nested_texture.get(), +ParamFlags::Differentiable);
        if (m_has_value)
            callback->put_parameter("value", m_value, +ParamFlags::Differentiable);
        callback->put_object("d65", m_d65, +ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        if (m_has_value)
            dr::make_opaque(m_value);
    }

    std::vector<ref<Object>> expand() const override {
        if constexpr (is_spectral_v<Spectrum>) {
            if (!m_nested_texture && !m_has_value)
                return { (Object *) m_d65.get() };
            return { };
        } else {
            if (m_nested_texture)
                 return { (Object *) m_nested_texture.get() };

            if (m_has_value) {
                Properties props("srgb");
                props.set_color("color", dr::slice(m_value) * m_scale);
                props.set_bool("unbounded", true);
                return { (Object *) PluginManager::instance()->create_object<Base>(props) };
            }

            Properties props("uniform");
            props.set_float("value", Properties::Float(m_scale));
            return { (Object *) PluginManager::instance()->create_object<Base>(props) };
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>) {
            UnpolarizedSpectrum d65_val = m_d65->eval(si, active);
            if (m_has_value)
                return d65_val * srgb_model_eval<UnpolarizedSpectrum>(m_value, si.wavelengths);
            else
                return d65_val * m_nested_texture->eval(si, active);
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
            if (m_has_value) {
                // TODO: better sampling strategy
                SurfaceInteraction3f si2(si);
                si2.wavelengths = MI_CIE_MIN + (MI_CIE_MAX - MI_CIE_MIN) * sample;
                return { si2.wavelengths, eval(si2, active) * (MI_CIE_MAX - MI_CIE_MIN) };
            } else {
                auto [wav, weight] = m_nested_texture->sample_spectrum(si, sample, active);
                SurfaceInteraction3f si2(si);
                si2.wavelengths = wav;
                return { wav, weight * m_d65->eval(si2, active) };
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

    Float mean() const override {
        if (m_nested_texture)
            return m_nested_texture->mean();
        else
            return dr::mean(m_value);
    }

    ScalarVector2i resolution() const override {
        if (m_nested_texture)
            return m_nested_texture->resolution();
        else
            return Base::resolution();
    }

    ScalarFloat spectral_resolution() const override {
        return (MI_CIE_MAX - MI_CIE_MAX) / (MI_CIE_SAMPLES - 1);
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
            else
                return dr::max_nested(srgb_model_mean(m_value));
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
            oss << "  value = " << m_value << std::endl;
        oss << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    Color<Float, 3> m_value;
    ref<Base> m_nested_texture;
    ref<Base> m_d65;

    ScalarFloat m_scale;

    bool m_has_value = false;
};

MI_IMPLEMENT_CLASS_VARIANT(D65Spectrum, Texture)
MI_EXPORT_PLUGIN(D65Spectrum, "CIE D65 Spectrum")
NAMESPACE_END(mitsuba)
