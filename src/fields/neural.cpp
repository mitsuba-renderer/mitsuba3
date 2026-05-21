#include <mitsuba/core/properties.h>
#include <mitsuba/render/field.h>

#include <sstream>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

namespace {

FieldValueType parse_field_value_type(std::string_view value) {
    if (value == "Float")
        return FieldValueType::Float;
    if (value == "Spectrum")
        return FieldValueType::Spectrum;
    if (value == "Color3")
        return FieldValueType::Color3;
    if (value == "Array2")
        return FieldValueType::Array2;
    if (value == "Array3")
        return FieldValueType::Array3;
    if (value == "Features")
        return FieldValueType::Features;

    Throw("Invalid field output type \"%s\".", value);
}

FieldDomain parse_field_domain(std::string_view value) {
    if (value == "Surface")
        return FieldDomain::Surface;
    if (value == "Interaction")
        return FieldDomain::Interaction;
    if (value == "SurfaceAndInteraction")
        return FieldDomain::SurfaceAndInteraction;

    Throw("Invalid field domain \"%s\".", value);
}

} // namespace

/**!

.. _field-neuralfield:

Neural field (:monosp:`neuralfield`)
------------------------------------

.. pluginparameters::

 * - domain
   - |string|
   - Interaction records accepted by the field: ``Surface``,
     ``Interaction``, or ``SurfaceAndInteraction``. (Default: ``Surface``)

 * - out_type
   - |string|
   - Semantic type of the predicted output: ``Float``, ``Spectrum``,
     ``Color3``, ``Array2``, ``Array3``, or ``Features``. (Default:
     ``Color3``)

 * - out_dim
   - |integer|
   - Number of output channels. This is required for ``Features`` outputs and
     should match the fixed semantic dimension for other output types.

 * - args_dim
   - |integer|
   - Number of optional argument channels accepted by the field. A value of 0
     means that direct callers should not pass extra arguments. (Default: 0)

 * - encoding
   - |field|
   - Nested encoding field that maps interaction records to feature channels.
     Examples include :monosp:`hashgridencoding`,
     :monosp:`permutoencoding`, and :monosp:`sinusoidalencoding`.

 * - decoder
   - |string|
   - Decoder type applied to the encoded features: ``neural`` or ``linear``.
     (Default: ``neural``)

 * - hidden_size
   - |integer|
   - Hidden layer width used by the neural decoder.

 * - num_layers
   - |integer|
   - Number of decoder layers.

This plugin evaluates a learned field by combining an optional nested encoding
with a decoder network. It is intended for direct learned-field users and for
plugins such as :monosp:`neuralbsdf` that pass richer context through optional
argument channels.

Neural and cooperative-vector-backed implementations may be JIT-only. They
must reject unsupported scalar variants during construction with a clear error
message naming the plugin and supported variants.

*/

template <typename Float, typename Spectrum>
class NeuralField final : public Field<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Field)
    MI_IMPORT_TYPES()

    using FloatStorage = typename Base::FloatStorage;
    using Args         = typename Base::Args;
    using Array2f      = typename Base::Array2f;
    using Array3f      = typename Base::Array3f;
    using Array6f      = typename Base::Array6f;

    NeuralField(const Properties &props) : Base(props) {
        m_domain = parse_field_domain(props.get<std::string_view>("domain", "Surface"));
        m_out_type = parse_field_value_type(props.get<std::string_view>("out_type", "Color3"));
        m_out_dim = props.get<uint32_t>("out_dim", 0);
        m_args_dim = props.get<uint32_t>("args_dim", 0);
        m_decoder = std::string(props.get<std::string_view>("decoder", "neural"));
        props.mark_queried("hidden_size");
        props.mark_queried("num_layers");

        if (props.has_property("encoding"))
            m_encoding = props.get<ref<Base>>("encoding");
    }

    FieldValueType out_type() const override { return m_out_type; }
    FieldDomain domain() const override { return m_domain; }
    uint32_t out_dim() const override { return m_out_dim; }
    uint32_t args_dim() const override { return m_args_dim; }

    bool supports_scalar() const override { return false; }
    bool supports_jit() const override { return true; }
    bool supports_surface_queries() const override {
        return m_domain == FieldDomain::Surface ||
               m_domain == FieldDomain::SurfaceAndInteraction;
    }
    bool supports_interaction_queries() const override {
        return m_domain == FieldDomain::Interaction ||
               m_domain == FieldDomain::SurfaceAndInteraction;
    }

    FloatStorage eval(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval");
    }

    FloatStorage eval(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval");
    }

    Float eval_1(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval_1");
    }

    Float eval_1(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval_1");
    }

    Color3f eval_color3(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval_color3");
    }

    Color3f eval_color3(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval_color3");
    }

    Array2f eval_array2(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval_array2");
    }

    Array2f eval_array2(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval_array2");
    }

    Array3f eval_array3(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval_array3");
    }

    Array3f eval_array3(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval_array3");
    }

    UnpolarizedSpectrum eval_spec(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval_spec");
    }

    UnpolarizedSpectrum eval_spec(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval_spec");
    }

    Array6f eval_array6(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval_array6");
    }

    Array6f eval_array6(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval_array6");
    }

    void eval_n(const SurfaceInteraction3f &, Float *, uint32_t, Args, Mask) const override {
        NotImplementedError("eval_n");
    }

    void eval_n(const Interaction3f &, Float *, uint32_t, Args, Mask) const override {
        NotImplementedError("eval_n");
    }

    void traverse(TraversalCallback *cb) override {
        if (m_encoding)
            cb->put("encoding", m_encoding, ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &) override { }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "NeuralField[" << std::endl
            << "  out_dim = " << m_out_dim << "," << std::endl
            << "  args_dim = " << m_args_dim << "," << std::endl
            << "  decoder = \"" << m_decoder << "\"" << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(NeuralField)

private:
    FieldDomain m_domain = FieldDomain::Surface;
    FieldValueType m_out_type = FieldValueType::Color3;
    uint32_t m_out_dim = 0;
    uint32_t m_args_dim = 0;
    std::string m_decoder;
    ref<Base> m_encoding;

    MI_TRAVERSE_CB(Base, m_encoding)
};

MI_EXPORT_PLUGIN(NeuralField)
NAMESPACE_END(mitsuba)
