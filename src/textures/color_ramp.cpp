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

ColorRamp texture (:monosp:`color_ramp`)
---------------------------------

.. pluginparameters::
 :extra-rows: 1

 * - input
   - |texture| or |float|
   - Input to color ramp (Default:0.5)
     |exposed|, |differentiable|

 * - mode
   - |string|
   - Denotes the desired interpolation mode. The following options are available:

     - ``linear`` (default): linear interpolation

     - ``constant``: maps to color-band with position equal to the floor of input

     - ``ease``: smoothstep (cubic Hermite) interpolation

     - ``cardinal``: cardinal B-spline with a fixed tension of `0.71`

 * - num_bands
   - |int|
   - The number of color bands
   - |exposed|

 * - pos[x]
   - |float|
   - The x-th color band stop position where x is in the range `[0, num_bands-1]`.
     The position values must range from `[0,1]` and be increasing
   - |exposed|

 * - color[x]
   - |spectrum|
   - The x-th color band RGB value where x is in the range `[0, num_bands -1]`

This plugin provides a mapping of an input texture's relative luminance to colors 
on an RGB gradient.


.. tabs::
    .. code-tab:: xml
        :name: color-ramp

        <texture type="color_ramp">
            <string name="input" value="0.7"/>
            <string name="mode" value="linear"/>
            <float name="pos0" value="0.040000"/>
            <value="0.602237 0.482636 0.000000" name="color0"/>
            <float name="pos1" value="0.084091"/>
            <value="0.019626 0.077920 0.174928" name="color1"/>
        </texture>

    .. code-tab:: python

        'type': 'color_ramp',
        'input': {
            'type' : 'bitmap',
            'filename' : 'texture.png'
        },
        'mode': 'ease',
        'num_bands': 2,
        'pos0': 0.0,
        'pos1': 1.0,
        'color0': [0.0, 0.0, 0.0],
        'color1': [1.0, 1.0, 1.0]
*/

template <typename Float, typename Spectrum>
class ColorRamp final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    ColorRamp(const Properties &props) : Texture(props) {

        // Load interpolation mode
        {
            std::string mode = props.string("mode", "linear");
            if (mode == "linear")
                m_interp_mode = InterpolationMode::Linear;
            else if (mode == "ease")
                m_interp_mode = InterpolationMode::Ease;
            else if (mode == "constant")
                m_interp_mode = InterpolationMode::Constant;
            else if (mode == "cardinal")
                m_interp_mode = InterpolationMode::Cardinal;
            else
                Throw("Invalid interpolation mode %s. Expected one of: "
                      "linear, ease, constant or cardinal", mode.c_str());
        }

        int num_bands = props.get<int>("num_bands");

        if (num_bands <= 0)
            Throw("Num of color bands %d has to be strictly positive");

        // Load colors and positions
        {
            size_t padding = 2;
            std::vector<ScalarFloat> band_pos(num_bands + padding);
            std::vector<ScalarFloat> band_colors(3 * (num_bands + padding));

            auto load_color = [](ScalarFloat* ptr, ScalarColor3f c) {
                *ptr        = c.r();
                *(ptr + 1)  = c.g();
                *(ptr + 2)  = c.b();
            };

            ScalarFloat prev_pos = 0.f;
            for (int i=0; i < num_bands; ++i) {
                ScalarFloat pos = props.get<ScalarFloat>("pos" + std::to_string(i));
                ScalarColor3f color = props.get<ScalarColor3f>("color" + std::to_string(i));

                if (pos < 0.f || pos > 1.f)
                    Throw("Position at index %d has value %.2f outside range [0,1]", i, pos);

                if (pos < prev_pos)
                    Throw("Position at index %d has value %.2f less than previous position "
                          "%.2f however sequence needs to be increasing", i, pos, prev_pos);

                prev_pos = pos;

                band_pos[i+1] = pos;
                load_color(&band_colors[3*(i+1)], color);
            }

            // Left-pad colors
            band_pos[0] = 0.f;
            memcpy(&band_colors[0], &band_colors[3], sizeof(ScalarFloat) * 3);

            // Right-pad colors
            band_pos[num_bands + padding - 1] = 1.f;
            memcpy(&band_colors[3 *(num_bands + padding - 1)], 
                &band_colors[3 * (num_bands + padding - 2)], sizeof(ScalarFloat) * 3);

            m_band_colors = dr::load<FloatStorage>(band_colors.data(), band_colors.size());
            m_band_pos = dr::load<FloatStorage>(band_pos.data(), band_pos.size());
        }

        m_input_texture = props.texture<Texture>("input", 0.5);
    }

    ~ColorRamp() {}

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("input",    m_input_texture,    +ParamFlags::Differentiable);
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (dr::none_or<false>(active))
            return dr::zeros<UnpolarizedSpectrum>();

        return eval_color_ramp<UnpolarizedSpectrum>(si, active);
    }

    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (dr::none_or<false>(active))
            return dr::zeros<Float>();

        return eval_color_ramp<Color1f>(si, active).r();
    }

    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active = true) const override {

        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (dr::none_or<false>(active))
            return dr::zeros<Color3f>();

        return eval_color_ramp<Color3f>(si, active);
    }

    std::pair<Point2f, Float>
    sample_position(const Point2f &sample, Mask active = true) const override {
        return m_input_texture->sample_position(sample, active);
    }

    Float pdf_position(const Point2f &pos_, Mask active = true) const override {
        return m_input_texture->pdf_position(pos_, active);
    }

    ScalarVector2i resolution() const override {
        return m_input_texture->resolution();
    }

    bool is_spatially_varying() const override { 
        return true; 
    }

    std::string to_string() const override {
        std::ostringstream oss, end;
        oss << "ColorRamp[" << std::endl
            << "  input = " << m_input_texture << "," << std::endl
            << "  mode  = " << (size_t)m_interp_mode << ","  << std::endl
            << "  colors = " << m_band_colors << ","  << std::endl
            << "  positions = " << m_band_pos << "," << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

protected:
    /**
     * \brief Evaluates the color ramp at the given surface interaction
     *
     */
    template<typename OutputSpectrum>
    MI_INLINE OutputSpectrum eval_color_ramp(const SurfaceInteraction3f &si, Mask active) const {

        Float input_pos;
        {
            UnpolarizedSpectrum tex_pos = m_input_texture->eval(si, active);
            if constexpr (is_monochromatic_v<Spectrum>)
                input_pos = tex_pos.r();
            else if constexpr (is_spectral_v<Spectrum>)
                input_pos = luminance(tex_pos, si.wavelengths);
            else
                input_pos = luminance(tex_pos);

            input_pos = dr::clamp(input_pos, 0.0f, 1.0f);
        }

        // This includes the left and right padding
        size_t num_bands = m_band_pos.size();

        // We start search at index 1 because we have padded band pos/colors
        // This ensures we don't have to do any index out-of-bounds checks 
        UInt32 upper_band_index = dr::binary_search<UInt32>(1, num_bands - 1, 
            [&](UInt32 idx) DRJIT_INLINE_LAMBDA {  
                return dr::gather<Float>(m_band_pos, idx, active) <= input_pos;
            }
        );
        UInt32 lower_band_index = upper_band_index - 1;

        Float pos0 = dr::gather<Float>(m_band_pos, lower_band_index, active);
        Float pos1 = dr::gather<Float>(m_band_pos, upper_band_index, active);
        Float relative_fac = dr::select(dr::neq(pos0, pos1), 
            (input_pos - pos0) / (pos1 - pos0), 0.f);
        relative_fac = dr::clamp(relative_fac, 0.f, 1.0f);

        Color3f colors[4];
        Float   weights[4];
        {
            colors[1] = dr::gather<Color3f>(m_band_colors, lower_band_index, active);
            colors[2] = dr::gather<Color3f>(m_band_colors, upper_band_index, active);

            if (m_interp_mode == InterpolationMode::Cardinal) {
                // Left control point
                colors[0] = dr::select(lower_band_index > 0,
                    dr::gather<Color3f>(m_band_colors, lower_band_index - 1, active), colors[1]);
                // Right control point
                colors[3] = dr::select(upper_band_index < num_bands - 1,
                    dr::gather<Color3f>(m_band_colors, upper_band_index + 1, active), colors[2]);
            }
        }

        auto mix_colors = [&wavelengths = si.wavelengths](Color3f*colors, Float* weights, size_t count) {
            OutputSpectrum col(0);
            for(size_t i = 0; i < count; ++i) {
                OutputSpectrum curr_color;
                if constexpr (is_monochromatic_v<OutputSpectrum>)
                    curr_color = luminance(colors[i]);
                else if constexpr (is_spectral_v<OutputSpectrum>)
                    curr_color = srgb_model_eval<OutputSpectrum>(colors[i], wavelengths);
                else
                    curr_color = colors[i];
                col += curr_color * weights[i];
            }
            return col;
        };

        switch (m_interp_mode) {
            case InterpolationMode::Linear: {
                weights[1] = 1.f - relative_fac;
                weights[2] = relative_fac;
                return mix_colors(&colors[1], &weights[1], 2);
            } break;
            case InterpolationMode::Ease: {
                Float ease_fac = relative_fac * relative_fac * (3.0f - 2.0f * relative_fac);
                weights[1] = 1.f - ease_fac;
                weights[2] = ease_fac;
                return mix_colors(&colors[1], &weights[1], 2);
            } break;
            case InterpolationMode::Constant:
                weights[1] = 1.f;
                return mix_colors(&colors[1], &weights[1], 1);
            case InterpolationMode::Cardinal: {
                Float weights[4];
                Float t = relative_fac;
                Float t2 = t * t;
                Float t3 = t2 * t;
                Float fc = 0.71f;
                weights[0] = -fc * t3 + 2.0f * fc * t2 - fc * t;
                weights[1] = (2.0f - fc) * t3 + (fc - 3.0f) * t2 + 1.0f;
                weights[2] = (fc - 2.0f) * t3 + (3.0f - 2.0f * fc) * t2 + fc * t;
                weights[3] = fc * t3 - fc * t2;
                return mix_colors(colors, weights, 4);
            } break;
            default:
                Throw("Invalid interpolation mode!");
        };
    }

protected:
    using FloatStorage = DynamicBuffer<Float>;
    enum class InterpolationMode { Linear, Ease, Constant, Cardinal };

    InterpolationMode m_interp_mode = InterpolationMode::Linear;

    ref<Texture> m_input_texture;

    FloatStorage  m_band_colors;
    FloatStorage  m_band_pos;
};

MI_IMPLEMENT_CLASS_VARIANT(ColorRamp, Texture)
MI_EXPORT_PLUGIN(ColorRamp, "Color Ramp")

NAMESPACE_END(mitsuba)