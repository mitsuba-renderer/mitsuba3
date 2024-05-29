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
 :extra-rows: 10

 * - filename
   - |string|
   - Filename of the bitmap to be loaded

 * - bitmap
   - :monosp:`Bitmap object`
   - When creating a Bitmap texture at runtime, e.g. from Python or C++,
     an existing Bitmap image instance can be passed directly rather than
     loading it from the filesystem with :paramtype:`filename`.

 * - filter_type
   - |string|
   - Specifies how pixel values are interpolated and filtered when queried over larger
     UV regions. The following options are currently available:

     - ``bilinear`` (default): perform bilinear interpolation, but no filtering.

     - ``nearest``: disable filtering and interpolation. In this mode, the plugin
       performs nearest neighbor lookups of texture values.

 * - num_band
   - |integer|
   - Specifies the number of color bands used in ColorRamp

* - posx
   - |float|
   - Specifies the position of color bands used in ColorRamp
   - x starts from 0 to num_band - 1
   - Used for interpolation factor

* - colorx
   - |Color3f|
   - Specifies the 3 channels of color bands used in ColorRamp
   - Used for interpolation color results

 * - raw
   - |bool|
   - Should the transformation to the stored color data (e.g. sRGB to linear,
     spectral upsampling) be disabled? You will want to enable this when working
     with bitmaps storing normal maps that use a linear encoding. (Default: false)

 * - to_uv
   - |transform|
   - Specifies an optional 3x3 transformation matrix that will be applied to UV
     values. A 4x4 matrix can also be provided, in which case the extra row and
     column are ignored.
   - |exposed|

 * - accel
   - |bool|
   - Hardware acceleration features can be used in CUDA mode. These features can
     cause small differences as hardware interpolation methods typically have a
     loss of precision (not exactly 32-bit arithmetic). (Default: true)

 * - data
   - |tensor|
   - Tensor array containing the texture data.
   - |exposed|, |differentiable|

This plugin provides an interpolator based on the bitmap texture factor. It currently supports RGB and monochrome modes.
The interpolation for Spectral mode will be added.

.. tabs::
    .. code-tab:: xml
        :name: color-ramp

        <texture type="color_ramp">
            <string name="filename" value="texture.png"/>
            <string name="mode" value="linear"/>
            <float name="pos0" value="0.040000"/>
            <rgb value="0.602237 0.482636 0.000000" name="color0"/>
            <float name="pos1" value="0.084091"/>
            <rgb value="0.019626 0.077920 0.174928" name="color1"/>
        </texture>

    .. code-tab:: python

        'type': 'color_ramp',
        'filename': 'texture.png',
        "mode": "linear",
        "num_band": 2,
        "pos0": 0.0,
        "pos1": 1.0,
        "color0": {
            'type': 'rgb',
            'value': [0.0, 0.0, 0.0]
        },
        "color1": {
            'type': 'rgb',
            'value': [1.0, 1.0, 1.0]
        },

*/

template <typename Float, typename Spectrum>
class ColorRamp final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture, TexturePtr)

    ColorRamp(const Properties &props) : Texture(props) {
        m_num_band = int(props.get<ScalarFloat>("num_band"));

        std::unique_ptr<ScalarFloat[]> m_pos(new ScalarFloat[m_num_band]);
        ScalarFloat *m_pos_ptr = m_pos.get();

        for (int i=0; i < m_num_band; i++) {
            m_color_band.push_back(props.texture<Texture>("color" + std::to_string(i), 0.0f));
            m_pos_ptr[i] = props.get<ScalarFloat>("pos" + std::to_string(i));
        }

        m_color_band_dr = dr::load<DynamicBuffer<TexturePtr>>(m_color_band.data(), m_color_band.size());
        m_pos_dr = dr::load<DynamicBuffer<Float>>(m_pos.get(), m_num_band);

        m_mode = props.string("mode");

        if (props.has_property("factor")) { // Single factor with multiple color (array) interpolation
            if (props.has_property("filename"))
                Throw("Texture factor and single factor for ColorRamp can not be used at the same time!");

            // Factor type
            m_bitmap_factor = props.has_property("input_fac");

            // Single factor
            m_factor = props.get<ScalarFloat>("factor");

        }
        else if (props.has_property("input_fac")) {
            // Factor type
            m_bitmap_factor = props.has_property("input_fac");

            // Load texture as ColorRamp input
            m_input_fac = props.texture<Texture>("input_fac", 0.5f);
        }
        else {
            Throw("Color Ramp should at least have a factor or bitmap as the input!");
        }
    }

    ~ColorRamp() {}

    void traverse(TraversalCallback *callback) override {
        callback->put_object("input_fac", m_input_fac, +ParamFlags::Differentiable);
    }

    // Interpolate RGB
    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (m_bitmap_factor) {
            if (dr::none_or<false>(active))
                return dr::zeros<UnpolarizedSpectrum>();

            if constexpr (is_monochromatic_v<Spectrum>)
                return texture_interpolation<Color1f>(si, active);
            else if constexpr (is_spectral_v<Spectrum>) {
                // This function is equivalent to removing ColorRamp node between bitmap texture and bsdf
                // TODO: Add spectral mode for ColorRamp
                return m_input_fac->eval(si, active);
            }
            else
                return texture_interpolation<Color3f>(si, active);
        }
        else {
            if constexpr (is_monochromatic_v<Spectrum>)
                return single_factor_interpolation<Color1f>(si, active);
            else if constexpr (is_spectral_v<Spectrum>)
                return m_input_fac->eval(si, active);
            else
                return single_factor_interpolation<Color3f>(si, active);
        }
    }

    // Interpolate Float properties such as roughness, specular, etc.
    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active = true) const override {

        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (m_bitmap_factor) {
            if (dr::none_or<false>(active))
                return dr::zeros<Float>();

            if constexpr (is_monochromatic_v<Spectrum>)
                return texture_interpolation<Color1f>(si, active).r();
            else if constexpr (is_spectral_v<Spectrum>)
                return m_input_fac->eval_1(si, active);
            else
                return luminance(texture_interpolation<Color3f>(si, active));
        } else {
            if constexpr (is_monochromatic_v<Spectrum>)
                return single_factor_interpolation<Color1f>(si, active).r();
            else if constexpr (is_spectral_v<Spectrum>)
                return m_input_fac->eval_1(si, active);
            else
                return luminance(single_factor_interpolation<Color3f>(si, active));
        }
    }

    // no color ramp support for bumpmap.cpp
    Vector2f eval_1_grad(const SurfaceInteraction3f &si,
                         Mask active = true) const override {
        Throw("Color Ramp is not supported for bumpmap surface gradient currently");
    }

    // no color ramp support for normalmap.cpp
    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active = true) const override {
        Throw("Color Ramp is not supported for normalmap surface gradient currently");
    }

    std::pair<Point2f, Float>
    sample_position(const Point2f &sample, Mask active = true) const override {
        return m_input_fac->sample_position(sample, active);
    }

    Float pdf_position(const Point2f &pos_, Mask active = true) const override {
        return m_input_fac->pdf_position(pos_, active);
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &_si, const Wavelength &sample,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureSample, active);

        if (dr::none_or<false>(active))
            return { dr::zeros<Wavelength>(), dr::zeros<UnpolarizedSpectrum>() };

        if constexpr (is_spectral_v<Spectrum>) {
            SurfaceInteraction3f si(_si);
            si.wavelengths = MI_CIE_MIN + (MI_CIE_MAX - MI_CIE_MIN) * sample;
            return { si.wavelengths,
                     eval(si, active) * (MI_CIE_MAX - MI_CIE_MIN) };
        } else {
            DRJIT_MARK_USED(sample);
            UnpolarizedSpectrum value = eval(_si, active);
            return { dr::empty<Wavelength>(), value };
        }
    }

    ScalarVector2i resolution() const override {
        return m_input_fac->resolution();
    }

    Float mean() const override {
        return m_input_fac->mean();
    }

    bool is_spatially_varying() const override { return true; }

    std::string to_string() const override {
        std::ostringstream oss, end;

        oss << "ColorRamp[" << std::endl
            << "  name = \"" << m_name << "\"," << std::endl
            << "  input_fac = [";

        end << "]," << std::endl
            << "]";

        std::string str_input_fac = m_input_fac->to_string();

        return oss.str() + str_input_fac + end.str();
    }

    MI_DECLARE_CLASS()

protected:
    // Calculate the weight of cardinal and B-spline interpolation
    // Borrow from Blender
    MI_INLINE void key_curve_position_weights(Float t, Float *data, std::string type) const {
        if(type == "cardinal") {
            Float t2 = t * t;
            Float t3 = t2 * t;
            Float fc = 0.71f;

            data[0] = -fc * t3 + 2.0f * fc * t2 - fc * t;
            data[1] = (2.0f - fc) * t3 + (fc - 3.0f) * t2 + 1.0f;
            data[2] = (fc - 2.0f) * t3 + (3.0f - 2.0f * fc) * t2 + fc * t;
            data[3] = fc * t3 - fc * t2;
        }
        else if(type == "b_spline") {
             Float t2= t * t;
             Float t3= t2 * t;

             data[0]= -0.16666666f * t3 + 0.5f * t2 - 0.5f * t + 0.16666666f;
             data[1]= 0.5f * t3 - t2 + 0.6666666f;
             data[2]= -0.5f * t3 + 0.5f * t2 + 0.5f * t + 0.16666666f;
             data[3]= 0.16666666f * t3;
        }
        else if (type == "cr_spline") {
            Float t2 = t * t;
            Float t3 = t2 * t;
            Float fc = 0.5f;

            data[0] = -fc * t3 + 2.0f * fc * t2 - fc * t;
            data[1] = (2.0f - fc) * t3 + (fc - 3.0f) * t2 + 1.0f;
            data[2] = (fc - 2.0f) * t3 + (3.0f - 2.0f * fc) * t2 + fc * t;
            data[3] = fc * t3 - fc * t2;
        }
    }

    MI_INLINE void key_curve_tangent_weights(Float t, Float *data, std::string type) const {
        if(type == "cardinal") {
            Float t2 = t * t;
            Float fc = 0.71f;

            data[0] = -3.0f * fc * t2 + 4.0f * fc * t - fc;
            data[1] = 3.0f * (2.0f - fc) * t2 + 2.0f * (fc - 3.0f) * t;
            data[2] = 3.0f * (fc - 2.0f) * t2 + 2.0f * (3.0f - 2.0f * fc) * t + fc;
            data[3] = 3.0f * fc * t2 - 2.0f * fc * t;
        }
        else if(type == "b_spline") {
            Float t2 = t * t;

            data[0] = -0.5f * t2 + t - 0.5f;
            data[1] = 1.5f * t2 - t * 2.0f;
            data[2] = -1.5f * t2 + t + 0.5f;
            data[3] = 0.5f * t2;
        }
        else if (type == "cr_spline") {
            Float t2 = t * t;
            Float fc = 0.5f;

            data[0] = -3.0f * fc * t2 + 4.0f * fc * t - fc;
            data[1] = 3.0f * (2.0f - fc) * t2 + 2.0f * (fc - 3.0f) * t;
            data[2] = 3.0f * (fc - 2.0f) * t2 + 2.0f * (3.0f - 2.0f * fc) * t + fc;
            data[3] = 3.0f * fc * t2 - 2.0f * fc * t;
        }
    }

    MI_INLINE void key_curve_normal_weights(Float t, Float *data, std::string type) const {
        if(type == "cardinal") {
            Float fc = 0.71f;

            data[0] = -6.0f * fc * t + 4.0f * fc;
            data[1] = 6.0f * (2.0f - fc) * t + 2.0f * (fc - 3.0f);
            data[2] = 6.0f * (fc - 2.0f) * t + 2.0f * (3.0f - 2.0f * fc);
            data[3] = 6.0f * fc * t - 2.0f * fc;
        }
        else if(type == "b_spline") {
            data[0] = -1.0f * t + 1.0f;
            data[1] = 3.0f * t - 2.0f;
            data[2] = -3.0f * t + 1.0f;
            data[3] = 1.0f * t;
        }
        else if (type == "cr_spline") {
            Float fc = 0.5f;

            data[0] = -6.0f * fc * t + 4.0f * fc;
            data[1] = 6.0f * (2.0f - fc) * t + 2.0f * (fc - 3.0f);
            data[2] = 6.0f * (fc - 2.0f) * t + 2.0f * (3.0f - 2.0f * fc);
            data[3] = 6.0f * fc * t - 2.0f * fc;
        }
    }

    /**
     * \brief Evaluates the color ramp at the given surface interaction
     *
     */
    template<typename color>
    MI_INLINE color texture_interpolation(const SurfaceInteraction3f &si, Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        color factor = m_input_fac->eval(si, active);

        Float fac;
        if constexpr (is_monochromatic_v<Spectrum>)
            fac = factor.r();
        else if constexpr (is_spectral_v<Spectrum>)
            fac = luminance(factor, si.wavelengths);
        else
            fac = luminance(factor);

        if (m_mode == "linear") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex1= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos0 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = dr::select(dr::neq(pos0, pos1), (fac - pos0) / (pos1 - pos0), 0.);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                tex0->eval(si, active),
                tex0->eval(si, active) * (1 - relative_pos) + tex1->eval(si, active) * relative_pos
            );
        }
        else if (m_mode == "ease") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex1= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos0 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = dr::select(dr::neq(pos1, pos0), (fac - pos0) / (pos1 - pos0), 0.0);
            Float ease_fac = relative_pos * relative_pos * (3.0f - 2.0f * relative_pos);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) < fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) > fac,
                tex0->eval(si, active),
                tex0->eval(si, active) * (1 - ease_fac) + tex1->eval(si, active) * ease_fac
            );
        }
        else if (m_mode == "constant") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );
            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );
            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);

            return dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active)->eval(si, active);
        }
        else if (m_mode == "cardinal") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex2= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos2 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex1 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = (fac - pos1) / (pos2 - pos1);
            Float weight[4];
            key_curve_position_weights(relative_pos, weight, m_mode);

            UInt32 lower_boundary_left = dr::select(lower_boundary > 0, lower_boundary - 1, lower_boundary);
            lower_boundary_left = dr::clamp(lower_boundary_left, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary_left, active);

            UInt32 higher_boundary_right = dr::select(higher_boundary < UInt32(m_num_band - 1), higher_boundary + 1, higher_boundary);
            higher_boundary_right = dr::clamp(higher_boundary_right, 0, m_num_band - 1);
            TexturePtr tex3 = dr::gather<TexturePtr>(m_color_band_dr, higher_boundary_right, active);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                tex1->eval(si, active),
                tex0->eval(si, active) * weight[0] +
                    tex1->eval(si, active) * weight[1] +
                    tex2->eval(si, active) * weight[2] +
                    tex3->eval(si, active) * weight[3]
            );
        }
        else if (m_mode == "b_spline") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex2= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos2 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex1 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = (fac - pos1) / (pos2 - pos1);
            Float weight[4];
            key_curve_position_weights(relative_pos, weight, m_mode);

            UInt32 lower_boundary_left = dr::select(lower_boundary > 0, lower_boundary - 1, lower_boundary);
            lower_boundary_left = dr::clamp(lower_boundary_left, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary_left, active);

            UInt32 higher_boundary_right = dr::select(higher_boundary < UInt32(m_num_band - 1), higher_boundary + 1, higher_boundary);
            higher_boundary_right = dr::clamp(higher_boundary_right, 0, m_num_band - 1);
            TexturePtr tex3 = dr::gather<TexturePtr>(m_color_band_dr, higher_boundary_right, active);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                tex1->eval(si, active),
                tex0->eval(si, active) * weight[0] +
                    tex1->eval(si, active) * weight[1] +
                    tex2->eval(si, active) * weight[2] +
                    tex3->eval(si, active) * weight[3]
            );
        }
        else
            Throw("Unknown interpolation type. Please specific interpolation mode (linear, ease, constant, cardinal, b_spline) in ColorRamp!");
    }

    /**
     * \brief Evaluates the color ramp at the given surface interaction
     *
     * Should only be used when there is only single factor for interpolation
     */
    template<typename color>
    MI_INLINE color single_factor_interpolation(const SurfaceInteraction3f &si,Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        if (m_mode == "linear") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= m_factor;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex1= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos0 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = dr::select(dr::neq(pos0, pos1), (m_factor - pos0) / (pos1 - pos0), 0.);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                tex0->eval(si, active),
                tex0->eval(si, active) * (1 - relative_pos) + tex1->eval(si, active) * relative_pos
            );
        }
        else if (m_mode == "ease") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= m_factor;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex1= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos0 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = dr::select(dr::neq(pos1, pos0), (m_factor - pos0) / (pos1 - pos0), 0.0);
            Float ease_fac = relative_pos * relative_pos * (3.0f - 2.0f * relative_pos);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) < m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) > m_factor,
                tex0->eval(si, active),
                tex0->eval(si, active) * (1 - ease_fac) + tex1->eval(si, active) * ease_fac
            );
        }
        else if (m_mode == "constant") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= m_factor;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);

            return dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active)->eval(si, active);
        }
        else if (m_mode == "cardinal") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= m_factor;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex2= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos2 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex1 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = (m_factor - pos1) / (pos2 - pos1);
            Float weight[4];
            key_curve_position_weights(relative_pos, weight, m_mode);

            UInt32 lower_boundary_left = dr::select(lower_boundary > 0, lower_boundary - 1, lower_boundary);
            lower_boundary_left = dr::clamp(lower_boundary_left, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary_left, active);

            UInt32 higher_boundary_right = dr::select(higher_boundary < UInt32(m_num_band - 1), higher_boundary + 1, higher_boundary);
            higher_boundary_right = dr::clamp(higher_boundary_right, 0, m_num_band - 1);
            TexturePtr tex3 = dr::gather<TexturePtr>(m_color_band_dr, higher_boundary_right, active);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                tex1->eval(si, active),
                tex0->eval(si, active) * weight[0] +
                    tex1->eval(si, active) * weight[1] +
                    tex2->eval(si, active) * weight[2] +
                    tex3->eval(si, active) * weight[3]
            );
        }
        else if (m_mode == "b_spline") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= m_factor;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex2= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos2 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex1 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = (m_factor - pos1) / (pos2 - pos1);
            Float weight[4];
            key_curve_position_weights(relative_pos, weight, m_mode);

            UInt32 lower_boundary_left = dr::select(lower_boundary > 0, lower_boundary - 1, lower_boundary);
            lower_boundary_left = dr::clamp(lower_boundary_left, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary_left, active);

            UInt32 higher_boundary_right = dr::select(higher_boundary < UInt32(m_num_band - 1), higher_boundary + 1, higher_boundary);
            higher_boundary_right = dr::clamp(higher_boundary_right, 0, m_num_band - 1);
            TexturePtr tex3 = dr::gather<TexturePtr>(m_color_band_dr, higher_boundary_right, active);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                tex1->eval(si, active),
                tex0->eval(si, active) * weight[0] +
                    tex1->eval(si, active) * weight[1] +
                    tex2->eval(si, active) * weight[2] +
                    tex3->eval(si, active) * weight[3]
            );
        }
        else
            Throw("Unknown interpolation type. Please specific interpolation mode (linear, ease, constant, cardinal, b_spline) in ColorRamp!");
    }

protected:
    ref<Texture> m_input_fac;
    std::string m_name;

    // Single factor
    ScalarFloat m_factor;
    // Bitmap or color interpolator
    bool m_bitmap_factor;
    // Interpolate mode
    std::string m_mode;

    // Multiple color band
    int m_num_band;
    std::vector<ref<Texture>> m_color_band;
    DynamicBuffer<TexturePtr> m_color_band_dr;
    DynamicBuffer<Float> m_pos_dr;
};

MI_IMPLEMENT_CLASS_VARIANT(ColorRamp, Texture)
MI_EXPORT_PLUGIN(ColorRamp, "Color Ramp")

NAMESPACE_END(mitsuba)