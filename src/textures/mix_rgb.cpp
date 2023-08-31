#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/core/spline.h>
#include <mitsuba/core/math.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>
#include <mutex>


NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-color ramp:

MixRGB texture (:monosp:`mix_rgb`)
---------------------------------

.. pluginparameters::
 :extra-rows: 3

 * - color0
   - |texture| or |float|
   - First RGB input
     |exposed|, |differentiable|

 * - color1
   - |texture| or |float|
   - Second RGB input
     |exposed|, |differentiable|

 * - factor
   - |float|
   - The mixing factor. The value must range from [0,1]
     |exposed|, |differentiable|

 * - mode
   - |string|
   - Specifies the type of mixing performed. Given the mix function defined as 
     `mix(a,b,t) = (1-t) *a + t*b, for colors `a`, 'b` and a factor `t`, the following options 
     are available:

     - ``blend`` (default): `mix(a,b,t)`

     - ``add``: `mix(a, a + b, t)`

     - ``multiply``: `mix(a, a * b, t)`

     - ``subtract``: `mix(a, a - b, t)`

     - ``difference``: `mix(a, abs(a - b), t)`

     - ``exclusion``: `max(mix(a, a + b - 2 * a * b, t), 0)`

     - ``darken``: `mix(a, min(a, b))`

     - ``lighten``: `mix(a, max(a, b))`

This plugin implements color mixing of two input textures given a factor in `[0,1]`.
The mixing modes available are performed in RGB colorspace with the final output at a 
given surface interaction converted to the variant-specified color representation.

.. tabs::
    .. code-tab:: xml
        :name: mix-rgb

        <texture type="mix_rgb">
            <string name="color0" value="texture.png"/>
            <string name="color1" value="0.5"/>
            <float name="factor" value="0.5"/>
            <string name="mode" value="add" />
        </texture>

    .. code-tab:: python

        'type': 'mix_rgb',
        'color0': {
            'type' : 'bitmap',
            'filename' : 'texture.png'
        },
        'color1': {
            'type': 'rgb', 
            'value': [0.5, 0.9, 1.0]
        },
        'factor': 0.3,
        'mode' : 'blend'
*/

template <typename Float, typename Spectrum>
class MixRGB final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    MixRGB(const Properties &props) : Texture(props) {
        m_color0 = props.texture<Texture>("color0");
        m_color1 = props.texture<Texture>("color1");
        m_factor = props.get<ScalarFloat>("factor");

        if (m_factor < 0.f || m_factor > 1.f)
            Throw("Mix factor has value %.2f outside range [0,1]", m_factor);

        std::string mode = props.string("mode", "blend");
        if (mode == "blend")
            m_mode = MixMode::Blend;
        else
            Throw("Invalid mix mode %s", mode.c_str());
    }

    ~MixRGB() {}

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("color0",    m_color0,    +ParamFlags::Differentiable);
        callback->put_parameter("color1",    m_color1,    +ParamFlags::Differentiable);
        callback->put_parameter("factor",    m_factor,    +ParamFlags::Differentiable);
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (dr::none_or<false>(active))
            return dr::zeros<UnpolarizedSpectrum>();

        return eval_mix_colors<UnpolarizedSpectrum>(si, active);
    }

    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (dr::none_or<false>(active))
            return dr::zeros<Float>();

        return eval_mix_colors<Color1f>(si, active).r();
    }

    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active = true) const override {

        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (dr::none_or<false>(active))
            return dr::zeros<Color3f>();

        return eval_mix_colors<Color3f>(si, active);
    }

    ScalarVector2i resolution() const override {
        ScalarVector2i res0 = m_color0->resolution();
        ScalarVector2i res1 = m_color1->resolution();

        if (res0 == res1)
            return res0;
        else
            Throw("Input resolutions (%d, %d) and (%d, %d) are mismatched",
                res0.x(), res0.y(), res1.x(), res1.y());
    }

    bool is_spatially_varying() const override { 
        return m_color0->is_spatially_varying() || m_color1->is_spatially_varying(); 
    }

    std::string to_string() const override {
        std::ostringstream oss, end;
        oss << "MixRGB[" << std::endl
            << "  color0 = " << m_color0 << "," << std::endl
            << "  color1 = " << m_color1 << ","  << std::endl
            << "  factor = " << m_factor << ","  << std::endl
            << "  mode = " << (size_t)m_mode << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

protected:
    /**
     * \brief Evaluates the color mix at the given surface interaction
     *
     */
    template<typename OutputSpectrum>
    MI_INLINE OutputSpectrum eval_mix_colors(const SurfaceInteraction3f &si, Mask active) const {

        Color3f col0 = m_color0->eval_3(si, active);
        Color3f col1 = m_color1->eval_3(si, active);

        auto mix = [t = m_factor](Color3f a, Color3f b) -> Color3f {
            return (1.f - t) * a + t * b;
        };

        Color3f out;
        switch (m_mode) {
            case MixMode::Blend:
                out = mix(col0, col1);
                break;
            case MixMode::Add:
                out = mix(col0, col0 + col1);
                break;
            case MixMode::Multiply:
                out = mix(col0, col0 * col1);
                break;
            case MixMode::Subtract:
                out = mix(col0, col0 - col1);
                break;
            case MixMode::Difference:
                out = mix(col0, dr::abs(col0 - col1));
                break;
            case MixMode::Exclusion:
                out = dr::maximum(mix(col0, col0 + col1 - 2.f * col0 * col1), 0.f);
                break;
            case MixMode::Darken:
                out = mix(col0, dr::minimum(col0, col1));
                break;
            case MixMode::Lighten:
                out = mix(col0, dr::maximum(col0, col1));
                break;
            default:
                Throw("Unsupported mix mode!");
        }

        if constexpr (is_monochromatic_v<OutputSpectrum>)
            return luminance(out);
        else if constexpr (is_spectral_v<OutputSpectrum>)
            return srgb_model_eval<OutputSpectrum>(out, si.wavelengths);
        else
            return out;
    }

protected:
    enum class MixMode { 
        Blend,
        Add,
        Multiply,
        Subtract,
        Difference,
        Exclusion,
        Darken,
        Lighten
    };

    MixMode m_mode = MixMode::Blend;
    ScalarFloat m_factor = 0.5f;

    ref<Texture> m_color0;
    ref<Texture> m_color1;
};

MI_IMPLEMENT_CLASS_VARIANT(MixRGB, Texture)
MI_EXPORT_PLUGIN(MixRGB, "Mix RGB")

NAMESPACE_END(mitsuba)