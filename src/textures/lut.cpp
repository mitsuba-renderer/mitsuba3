#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <drjit/tensor.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-lut:

Lookup table texture (:monosp:`lut`)
------------------------------------

.. pluginparameters::

 * - input
   - |texture|
   - Texture to be transformed by the lookup table.
   - |exposed|, |differentiable|

 * - filename
   - |string|
   - Filename of an image holding the table. Its pixels in scanline order
     form the entries, and its channel count selects between a scalar
     (``Y``) and an RGB table (``RGB``). Alpha channels are dropped. Images
     with 8-bit sRGB-encoded data are converted to linear values.

 * - data
   - |tensor|
   - Alternative to :paramtype:`filename` when instantiating the plugin from
     Python: a Dr.Jit tensor of shape ``(N,)``, ``(N, 1)`` or ``(N, 3)``.
   - |exposed|, |differentiable|

 * - input_min, input_max
   - |float|
   - These values denote the range of the input texture. Inputs outside of
     this range are clamped. An image file may provide defaults through the
     ``domain_min`` and ``domain_max`` metadata entries. (Default: 0 and 1)

 * - filter_type
   - |string|
   - Specifies how the table is read between entries: ``linear`` interpolates
     the two enclosing entries, and ``nearest`` returns the closest one.
     (Default: ``linear``)

 * - curve
   - |bool|
   - Apply the table to each channel of the input separately (see below).
     (Default: |false|)

This texture maps the value of another texture through a table of :math:`N`
scalar or RGB entries :math:`t_0, \ldots, t_{N-1}`. Given the input range
:math:`[a, b]` specified by ``input_min`` and ``input_max``, entry :math:`t_i`
is the value of the table at the input

.. math::

    x_i = a + \frac{i}{N - 1} \, (b - a).

By default, the plugin evaluates the input as a monochromatic quantity and
uses it to look up an entry. An RGB table then acts as a *color map* that
assigns a color to each input value. In spectral variants, RGB entries are
upsampled to smooth spectra (like in the :ref:`bitmap <texture-bitmap>`
plugin) before they are interpolated.

With ``curve`` set to |true|, the plugin operates in *curve mode* and looks
up every channel of the input separately. A scalar table then specifies one
curve that applies to every channel (or to every wavelength sample in
spectral variants), while an RGB table specifies a separate curve per
channel. Spectral variants evaluate the latter on the RGB representation of
the input and upsample the result.

.. tabs::
    .. code-tab:: xml
        :name: lut-texture

        <texture type="lut">
            <string name="filename" value="colormap.exr"/>
            <texture type="bitmap" name="input">
                <string name="filename" value="mask.png"/>
            </texture>
        </texture>

    .. code-tab:: python

        'type': 'lut',
        'data': mi.TensorXf([[0, 0, 0.5], [1, 0.5, 0], [1, 1, 1]]),
        'input': { 'type': 'bitmap', 'filename': 'mask.png' }

 */

template <typename Float, typename Spectrum>
class LUTTexture final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    LUTTexture(const Properties &props) : Texture(props) {
        m_input = props.get_texture<Texture>("input");
        m_curve = props.get<bool>("curve", false);

        std::string_view filter_type = props.get<std::string_view>("filter_type", "linear");
        m_nearest = filter_type == "nearest";
        if (!m_nearest && filter_type != "linear")
            Throw("Invalid filter type \"%s\", must be \"linear\" or \"nearest\"",
                  filter_type);

        double input_min = 0.0, input_max = 1.0;
        if (props.has_property("filename")) {
            if (props.has_property("data"))
                Throw("Cannot specify both \"filename\" and \"data\"");
            m_values = load_image(props.get<std::string_view>("filename"),
                                  input_min, input_max);
        } else if (props.has_property("data")) {
            m_values = props.get_any<TensorXf>("data");
        } else {
            Throw("The table must be given as \"filename\" or \"data\"");
        }

        m_input_min = props.get<ScalarFloat>("input_min", (ScalarFloat) input_min);
        m_input_max = props.get<ScalarFloat>("input_max", (ScalarFloat) input_max);
        if (!(m_input_max > m_input_min))
            Throw("\"input_max\" (%f) must exceed \"input_min\" (%f)",
                  m_input_max, m_input_min);

        prepare_table(m_values);
        update_coefficients();
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("input", m_input,  ParamFlags::Differentiable);
        cb->put("data",  m_values, ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (keys.empty() || string::contains(keys, "data")) {
            prepare_table(m_values);
            update_coefficients();
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>) {
            if (m_curve) {
                if (rgb_table())
                    return srgb_model_eval<UnpolarizedSpectrum>(
                        SRGBModel<Float, Spectrum>::fetch(eval_3(si, active)), si.wavelengths);

                UnpolarizedSpectrum result = m_input->eval(si, active);
                for (size_t i = 0; i < dr::size_v<UnpolarizedSpectrum>; ++i)
                    result[i] = lookup<Float>(result[i], active);
                return result;
            }

            Float x = m_input->eval_1(si, active);
            if (!rgb_table())
                return UnpolarizedSpectrum(lookup<Float>(x, active));

            // Upsampling is nonlinear, so the entries are converted to
            // spectra before they are interpolated
            return lookup<Color3f>(m_coeffs, x, active, [&](const Color3f &c) {
                return srgb_model_eval<UnpolarizedSpectrum>(c, si.wavelengths);
            });
        } else if constexpr (is_monochromatic_v<Spectrum>) {
            return UnpolarizedSpectrum(eval_1(si, active));
        } else {
            return eval_3(si, active);
        }
    }

    Float eval_1(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (m_curve && rgb_table())
            return luminance(eval_3(si, active));

        Float x = m_input->eval_1(si, active);
        if (!rgb_table())
            return lookup<Float>(x, active);
        return luminance(lookup<Color3f>(x, active));
    }

    Color3f eval_3(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (m_curve) {
            Color3f c = m_input->eval_3(si, active);
            if (rgb_table()) {
                // Channel k of the input is looked up in column k
                Color3f result;
                for (size_t k = 0; k < 3; ++k)
                    result[k] = lookup<Color3f>(c[k], active)[k];
                return result;
            }
            return Color3f(lookup<Float>(c.x(), active),
                           lookup<Float>(c.y(), active),
                           lookup<Float>(c.z(), active));
        } else {
            Float x = m_input->eval_1(si, active);
            if (!rgb_table())
                return Color3f(lookup<Float>(x, active));
            return lookup<Color3f>(x, active);
        }
    }

    Vector2f eval_1_grad(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (rgb_table())
            Throw("eval_1_grad(): only lut textures with scalar entries "
                  "provide gradients");

        // A nearest-neighbor lookup is piecewise constant
        if (m_nearest)
            return Vector2f(0.f);

        // Slope of the segment enclosing the input
        Float x = m_input->eval_1(si, active);
        UInt32 i = segment(x).first;
        const FloatStorage &values = m_values.array();
        Float slope = dr::gather<Float>(values, i + 1u, active) -
                      dr::gather<Float>(values, i, active);
        slope = dr::select(x >= m_input_min && x <= m_input_max,
                           slope * scale(), 0.f);

        return slope * m_input->eval_1_grad(si, active);
    }

    ScalarVector2i resolution() const override {
        return m_input->resolution();
    }

    bool is_spatially_varying() const override {
        return m_input->is_spatially_varying();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "LUTTexture[" << std::endl
            << "  input = " << string::indent(m_input) << "," << std::endl
            << "  entries = " << m_values.shape(0) << "," << std::endl
            << "  channels = " << m_values.shape(1) << "," << std::endl
            << "  input_range = [" << m_input_min << ", " << m_input_max << "]," << std::endl
            << "  curve = " << m_curve << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(LUTTexture)

protected:
    using FloatStorage = DynamicBuffer<Float>;

    bool rgb_table() const { return m_values.shape(1) == 3; }

    /// Read the table from an image file. The input range defaults to the
    /// file's ``domain_min`` and ``domain_max`` metadata when present.
    TensorXf load_image(std::string_view filename, double &input_min,
                        double &input_max) const {
        fs::path path = file_resolver()->resolve(filename);
        ref<Bitmap> bitmap = new Bitmap(path);

        Bitmap::PixelFormat pf;
        switch (bitmap->pixel_format()) {
            case Bitmap::PixelFormat::Y:
            case Bitmap::PixelFormat::YA:
                pf = Bitmap::PixelFormat::Y;
                break;
            case Bitmap::PixelFormat::RGB:
            case Bitmap::PixelFormat::RGBA:
            case Bitmap::PixelFormat::XYZ:
            case Bitmap::PixelFormat::XYZA:
                pf = Bitmap::PixelFormat::RGB;
                break;
            default:
                Throw("%s: the table needs a known pixel format (Y[A], "
                      "RGB[A], XYZ[A] are supported)", path.string());
        }
        bitmap = bitmap->convert(pf, struct_type_v<ScalarFloat>, false);

        const Properties &metadata = bitmap->metadata();
        input_min = metadata.get<double>("domain_min", input_min);
        input_max = metadata.get<double>("domain_max", input_max);

        size_t shape[2] = { (size_t) bitmap->pixel_count(),
                            bitmap->channel_count() };
        return TensorXf((const ScalarFloat *) bitmap->data(), 2, shape);
    }

    /// Reshape a table of shape (N,) to (N, 1) and validate it
    void prepare_table(TensorXf &table) const {
        if (table.ndim() == 1)
            table = TensorXf(table.array(), { table.shape(0), 1 });
        if (table.ndim() != 2 || (table.shape(1) != 1 && table.shape(1) != 3))
            Throw("The table must have shape (N,), (N, 1) or (N, 3)");
        if (table.shape(0) < 2)
            Throw("The table must have at least two entries");
    }

    /**
     * \brief Derive the spectral upsampling coefficients of an RGB table
     *
     * The sRGB entries are the authoritative representation, and these
     * coefficients are a cached byproduct that spectral queries use instead.
     */
    void update_coefficients() {
        if constexpr (is_spectral_v<Spectrum>) {
            if (!rgb_table() || m_curve) {
                m_coeffs = FloatStorage();
                return;
            }

            uint32_t n = (uint32_t) m_values.shape(0);
            m_coeffs = dr::empty<FloatStorage>(3 * n);
            auto update = [&](const UInt32 &i) {
                dr::scatter(m_coeffs, SRGBModel<Float, Spectrum>::fetch(
                                dr::gather<Color3f>(m_values.array(), i)), i);
            };

            if constexpr (dr::is_jit_v<Float>)
                update(dr::arange<UInt32>(n));
            else
                for (uint32_t i = 0; i < n; ++i)
                    update(i);
        }
    }

    /// Derivative of the fractional table index with respect to the input
    ScalarFloat scale() const {
        return (ScalarFloat) (m_values.shape(0) - 1) / (m_input_max - m_input_min);
    }

    /// Lower index of the two entries enclosing an input value, and the
    /// interpolation weight of the upper one
    std::pair<UInt32, Float> segment(Float x) const {
        Float f = (dr::clip(x, m_input_min, m_input_max) - m_input_min) * scale(),
              i = dr::minimum(dr::floor(f), (ScalarFloat) m_values.shape(0) - 2);
        return { UInt32(i), f - i };
    }

    /**
     * \brief Look up an input value in a table with entries of type ``Value``
     *
     * The function ``fn`` maps entries to the result type. Linear lookups
     * apply it to the two enclosing entries and interpolate the results.
     */
    template <typename Value, typename Fn>
    auto lookup(const FloatStorage &data, Float x, Mask active, Fn fn) const {
        auto [i, w] = segment(x);
        if (m_nearest)
            return fn(dr::gather<Value>(data, dr::select(w < .5f, i, i + 1u), active));
        return dr::lerp(fn(dr::gather<Value>(data, i, active)),
                        fn(dr::gather<Value>(data, i + 1u, active)), w);
    }

    /// Look up an input value in the table entries (``Float`` or ``Color3f``)
    template <typename Value> Value lookup(Float x, Mask active) const {
        return lookup<Value>(m_values.array(), x, active, [](const Value &v) { return v; });
    }

    // Input texture transformed by the LUT
    ref<Texture> m_input;
    // Table entries (scalar or sRGB) as a tensor of shape (N, C)
    TensorXf m_values;
    // Upsampling coefficients of an sRGB color map (spectral variants only)
    FloatStorage m_coeffs;
    // Inputs that map to the first and last entry
    ScalarFloat m_input_min, m_input_max;
    // Look up each input channel separately
    bool m_curve;
    // Nearest-neighbor lookups instead of linear interpolation
    bool m_nearest;

    MI_TRAVERSE_CB(Texture, m_input, m_values, m_coeffs)
};

MI_EXPORT_PLUGIN(LUTTexture)
NAMESPACE_END(mitsuba)
