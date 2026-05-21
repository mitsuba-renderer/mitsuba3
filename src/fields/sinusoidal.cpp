#include <mitsuba/core/properties.h>
#include <mitsuba/render/field.h>

#include <sstream>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _field-sinusoidalencoding:

Sinusoidal encoding field (:monosp:`sinusoidalencoding`)
--------------------------------------------------------

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

This field maps renderer interaction records to deterministic sinusoidal
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
        m_out_dim = props.get<uint32_t>("out_dim", 0);
        props.mark_queried("n_frequencies");
        props.mark_queried("min_frequency");
        props.mark_queried("max_frequency");
    }

    FieldValueType out_type() const override { return FieldValueType::Features; }
    FieldDomain domain() const override { return FieldDomain::SurfaceAndInteraction; }
    uint32_t out_dim() const override { return m_out_dim; }
    uint32_t args_dim() const override { return 0; }

    bool supports_scalar() const override { return true; }
    bool supports_jit() const override { return true; }
    bool supports_surface_queries() const override { return true; }
    bool supports_interaction_queries() const override { return m_input_dim == 3; }

    FloatStorage eval(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval");
    }

    FloatStorage eval(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval");
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
    uint32_t m_input_dim = 2;
    uint32_t m_out_dim = 0;
};

MI_EXPORT_PLUGIN(SinusoidalEncoding)
NAMESPACE_END(mitsuba)
