#include <mitsuba/core/properties.h>
#include <mitsuba/render/field.h>

#include <sstream>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _field-sinusoidalfield:

Sinusoidal field (:monosp:`sinusoidalfield`)
--------------------------------------------

.. pluginparameters::

 * - input_dim
   - |integer|
   - Number of coordinates encoded from the interaction record. A value of 2
     conventionally uses surface UV coordinates, while 3 uses position.
     (Default: 2)

 * - out_dim
   - |integer|
   - Number of feature channels produced by the encoding.

 * - n_frequencies
   - |integer|
   - Number of sinusoidal frequency bands.

 * - min_frequency
   - |float|
   - Lowest frequency used by the encoding.

 * - max_frequency
   - |float|
   - Highest frequency used by the encoding.

This field maps each renderer coordinate channel to deterministic sinusoidal
features. It is intended as an analytic encoding for :monosp:`neuralfield`
configurations and experiments that do not require trainable encoding storage.

The encoding accepts no optional argument channels and has no trainable
parameters.

*/

template <typename Float, typename Spectrum>
class SinusoidalEncoding final : public Field<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Field)
    MI_IMPORT_TYPES()

    using FloatStorage = typename Base::FloatStorage;
    using Args         = typename Base::Args;

    SinusoidalEncoding(const Properties &props) : Base(props) {
        m_input_dim = props.get<uint32_t>("input_dim", 2);
        m_n_frequencies = props.get<uint32_t>("n_frequencies", 4);
        m_min_frequency = props.get<ScalarFloat>("min_frequency", 1.f);
        m_max_frequency = props.get<ScalarFloat>("max_frequency", 8.f);
        uint32_t natural_dim = m_input_dim * m_n_frequencies * 2;
        m_out_dim = props.get<uint32_t>("out_dim", natural_dim);

        if (m_input_dim != 2 && m_input_dim != 3)
            Throw("sinusoidalfield: input_dim must be 2 or 3.");
        if (m_n_frequencies == 0)
            Throw("sinusoidalfield: n_frequencies must be positive.");
        if (m_min_frequency <= 0 || m_max_frequency <= 0)
            Throw("sinusoidalfield: min_frequency and max_frequency must "
                  "be positive.");
        if (m_max_frequency < m_min_frequency)
            Throw("sinusoidalfield: max_frequency must be greater than or "
                  "equal to min_frequency.");
        if (m_out_dim == 0 || m_out_dim > natural_dim)
            Throw("sinusoidalfield: out_dim must be in [1, %u] for "
                  "input_dim=%u and n_frequencies=%u.",
                  natural_dim, m_input_dim, m_n_frequencies);
    }

    FieldValueType out_type() const override { return FieldValueType::Features; }
    FieldDomain domain() const override {
        return m_input_dim == 2 ? FieldDomain::Surface
                                : FieldDomain::SurfaceAndInteraction;
    }
    uint32_t out_dim() const override { return m_out_dim; }
    uint32_t args_dim() const override { return 0; }

    bool supports_scalar() const override { return true; }
    bool supports_jit() const override { return true; }
    bool supports_surface_queries() const override { return true; }
    bool supports_interaction_queries() const override { return m_input_dim == 3; }

    FloatStorage eval(const SurfaceInteraction3f &si, Args args,
                      Mask active) const override {
        validate_args(args);
        return eval_impl(si.uv.x(), si.uv.y(), si.p.z(), active);
    }

    FloatStorage eval(const Interaction3f &it, Args args,
                      Mask active) const override {
        validate_args(args);
        if (m_input_dim != 3)
            Throw("sinusoidalfield: Interaction queries require input_dim=3.");
        return eval_impl(it.p.x(), it.p.y(), it.p.z(), active);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SinusoidalEncoding[" << std::endl
            << "  input_dim = " << m_input_dim << "," << std::endl
            << "  out_dim = " << m_out_dim << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(SinusoidalEncoding)

private:
    void validate_args(Args args) const {
        if (args.size != 0)
            Throw("sinusoidalfield: args_dim is 0, got %u argument "
                  "channel(s).", args.size);
    }

    FloatStorage eval_impl(Float x, Float y, Float z, Mask active) const {
        FloatStorage result = dr::empty<FloatStorage>(m_out_dim);
        Float coords[3] = { x, y, z };
        ScalarFloat log_min = dr::log(m_min_frequency),
                    log_max = dr::log(m_max_frequency);

        uint32_t out = 0;
        for (uint32_t c = 0; c < m_input_dim && out < m_out_dim; ++c) {
            Float coord = coords[c];
            for (uint32_t band = 0; band < m_n_frequencies && out < m_out_dim; ++band) {
                ScalarFloat t = m_n_frequencies == 1
                    ? 0.f
                    : (ScalarFloat) band / (ScalarFloat) (m_n_frequencies - 1);
                Float phase = coord * (Float) (dr::TwoPi<ScalarFloat> *
                                               dr::exp(dr::lerp(log_min, log_max, t)));
                auto [s, c_] = dr::sincos(phase);
                result.entry(out++) = dr::select(active, s, 0.f);
                if (out < m_out_dim)
                    result.entry(out++) = dr::select(active, c_, 0.f);
            }
        }
        return result;
    }

    uint32_t m_input_dim = 2;
    uint32_t m_out_dim = 0;
    uint32_t m_n_frequencies = 4;
    ScalarFloat m_min_frequency = 1.f;
    ScalarFloat m_max_frequency = 8.f;
};

MI_EXPORT_PLUGIN(SinusoidalEncoding)
NAMESPACE_END(mitsuba)
