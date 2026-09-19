#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/srgb.h>
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

 * - input_min, input_max
   - |float|
   - These values denote the range of the input texture.
     Inputs outside of this range are clamped. (Default: 0 and 1)

 * - values
   - |string|
   - The table entries as whitespace- or comma-separated numbers, listing the
     channels of each entry in turn. When instantiating the plugin from
     Python, a Dr.Jit tensor of shape ``(N,)``, ``(N, 1)`` or ``(N, 3)`` can
     be passed instead.
   - |exposed|, |differentiable|
CLAUDE: I much don't like the interface where this is passed as a string. I'd prefer if the LUT is loaded from a 1-D texture file.
The channel count can then be inferred.

 * - channels
   - |int|
   - Number of channels of each entry (1 or 3) when ``values`` is a string.
     (Default: 1)
CLAUDE: should not be needed.

 * - filter_type
   - |string|
   - Specifies how the table is read between entries: ``linear`` interpolates
     the two enclosing entries, and ``nearest`` returns the closest one.
     (Default: ``linear``)

 * - per_channel
   - |bool|
   - Apply the table to each channel of the input separately (see below).
     (Default: |false|)
CLAUDE: rename this to curve=true/false and consistently refer to as "curve mode". Can we also support RGB curves?

This texture maps the value of another texture through a table of :math:`N`
scalar or RGB entries :math:`t_0, \ldots, t_{N-1}`. Given the input range
:math:`[a, b]` specified by ``input_min`` and ``input_max``, entry :math:`t_i`
is the value of the table at the input

.. math::

    x_i = a + \frac{i}{N - 1} \, (b - a).

By default, the plugin implements a *color ramp*.
CLAUDE: alternative to "color ramp"? That is a very specific kind of mono->RGB map.
It evaluates the input as a
monochromatic quantity and uses it to look up an entry. In spectral variants,
RGB entries are upsampled to smooth spectra (like in the :ref:`bitmap
<texture-bitmap>` plugin) before they are interpolated.

With ``per_channel`` set to |true|, the plugin implements a *curve* and looks
up every channel of the input (or every wavelength sample) separately. This
requires a scalar table.

A table given as a string whose three channels are identical is reduced to a
scalar table, which is cheaper to evaluate and also qualifies for per-channel
lookups.

.. tabs::
    .. code-tab:: xml
        :name: lut-texture

        <texture type="lut">
            <string name="values" value="0 0 0.5,  1 0.5 0,  1 1 1"/>
            <integer name="channels" value="3"/>
            <texture type="bitmap" name="input">
                <string name="filename" value="mask.png"/>
            </texture>
        </texture>

    .. code-tab:: python

        'type': 'lut',
        'values': '0 0 0.5,  1 0.5 0,  1 1 1',
        'channels': 3,
        'input': { 'type': 'bitmap', 'filename': 'mask.png' }

 */

template <typename Float, typename Spectrum>
class LUTTexture final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    LUTTexture(const Properties &props) : Texture(props) {
        m_input = props.get_texture<Texture>("input");
        m_per_channel = props.get<bool>("per_channel", false);
        m_input_min = props.get<ScalarFloat>("input_min", 0.f);
        m_input_max = props.get<ScalarFloat>("input_max", 1.f);
        if (!(m_input_max > m_input_min))
            Throw("\"input_max\" (%f) must exceed \"input_min\" (%f)",
                  m_input_max, m_input_min);

        std::string_view filter_type = props.get<std::string_view>("filter_type", "linear");
        m_nearest = filter_type == "nearest";
        if (!m_nearest && filter_type != "linear")
            Throw("Invalid filter type \"%s\", must be \"linear\" or \"nearest\"",
                  filter_type);

        m_values = load_table(props);
        update_coefficients();
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("input",  m_input,  ParamFlags::Differentiable);
        cb->put("values", m_values, ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (keys.empty() || string::contains(keys, "values")) {
            prepare_table(m_values);
            update_coefficients();
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>) {
            if (m_per_channel) {
                UnpolarizedSpectrum result = m_input->eval(si, active);
                for (size_t i = 0; i < dr::size_v<UnpolarizedSpectrum>; ++i)
                    result[i] = lookup<Float>(result[i], active);
                return result;
            }

            Float x = m_input->eval_1(si, active);
            if (m_values.shape(1) == 1)
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

        Float x = m_input->eval_1(si, active);
        if (m_values.shape(1) == 1)
            return lookup<Float>(x, active);
        return luminance(lookup<Color3f>(x, active));
    }

    Color3f eval_3(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (m_per_channel) {
            Color3f c = m_input->eval_3(si, active);
            return Color3f(lookup<Float>(c.x(), active),
                           lookup<Float>(c.y(), active),
                           lookup<Float>(c.z(), active));
        } else {
            Float x = m_input->eval_1(si, active);
            if (m_values.shape(1) == 1)
                return Color3f(lookup<Float>(x, active));
            return lookup<Color3f>(x, active);
        }
    }

    Vector2f eval_1_grad(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (m_values.shape(1) != 1)
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
            << "  input_range = [" << m_input_min << ", " << m_input_max << "]," << std::endl
            << "  per_channel = " << m_per_channel << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(LUTTexture)

protected:
    using FloatStorage = DynamicBuffer<Float>;

    /// Read the table from the "values" property into a tensor of shape (N, C)
    TensorXf load_table(const Properties &props) const {
        TensorXf table;

        if (props.type("values") == Properties::Type::String) {
            int64_t channels = props.get<int64_t>("channels", 1);
            if (channels != 1 && channels != 3)
                Throw("\"channels\" must be 1 or 3, not %i", channels);

            std::vector<ScalarFloat> values;
            for (const std::string &s :
                 string::tokenize(props.get<std::string_view>("values"), " ,")) {
                try {
                    values.push_back(string::stof<ScalarFloat>(s));
                } catch (...) {
                    Throw("Could not parse the number \"%s\" in \"values\"", s);
                }
            }

            size_t n = values.size() / channels;
            if (values.size() % channels != 0)
                Throw("The %zu numbers in \"values\" do not form entries of "
                      "%i channels", values.size(), channels);

            // A table with identical channels reduces to a scalar table
            // CLAUDE: we should not need this kind of preprocess anymore the input is properly typed.
            // This will need a full pass over the blender plugin. Right now we generate lots of RGB curves that are actually monchromatic curves. The simplification should rather be there.
            if (channels == 3) {
                bool gray = true;
                for (size_t i = 0; i < values.size(); i += 3)
                    gray &= values[i] == values[i + 1] && values[i] == values[i + 2];
                if (gray) {
                    for (size_t i = 0; i < n; ++i)
                        values[i] = values[3 * i];
                    values.resize(n);
                    channels = 1;
                }
            }

            table = TensorXf(values.data(), { n, (size_t) channels });
        } else {
            table = props.get_any<TensorXf>("values");
        }

        prepare_table(table);
        return table;
    }

    /// Reshape a table of shape (N,) to (N, 1) and validate it
    void prepare_table(TensorXf &table) const {
        if (table.ndim() == 1)
            table = TensorXf(table.array(), { table.shape(0), 1 });
        if (table.ndim() != 2 || (table.shape(1) != 1 && table.shape(1) != 3))
            Throw("The table must have shape (N,), (N, 1) or (N, 3)");
        if (table.shape(0) < 2)
            Throw("The table must have at least two entries");
        if (m_per_channel && table.shape(1) != 1)
            Throw("Per-channel lookups require a scalar table");
    }

    /**
     * \brief Derive the spectral upsampling coefficients of an RGB table
     *
     * The sRGB entries are the authoritative representation, and these
     * coefficients are a cached byproduct that spectral queries use instead.
     */
    void update_coefficients() {
        if constexpr (is_spectral_v<Spectrum>) {
            if (m_values.shape(1) != 3) {
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
    // Upsampling coefficients of an sRGB table (spectral variants only)
    FloatStorage m_coeffs;
    // Inputs that map to the first and last entry
    ScalarFloat m_input_min, m_input_max;
    // Look up each input channel separately
    bool m_per_channel;
    // Nearest-neighbor lookups instead of linear interpolation
    bool m_nearest;

    MI_TRAVERSE_CB(Texture, m_input, m_values, m_coeffs)
};

MI_EXPORT_PLUGIN(LUTTexture)
NAMESPACE_END(mitsuba)
