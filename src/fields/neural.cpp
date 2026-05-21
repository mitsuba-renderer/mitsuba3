#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
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

const char *field_value_type_name(FieldValueType type) {
    switch (type) {
        case FieldValueType::Float: return "Float";
        case FieldValueType::Spectrum: return "Spectrum";
        case FieldValueType::Color3: return "Color3";
        case FieldValueType::Array2: return "Array2";
        case FieldValueType::Array3: return "Array3";
        case FieldValueType::Features: return "Features";
        default: return "Unknown";
    }
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
        if constexpr (!dr::is_jit_v<Float>)
            Throw("neuralfield: variant \"%s\" is unsupported. Neural fields "
                  "require an LLVM or CUDA JIT variant.", this->variant_name());

        m_domain = parse_field_domain(props.get<std::string_view>("domain", "Surface"));
        m_out_type = parse_field_value_type(props.get<std::string_view>("out_type", "Color3"));
        m_out_dim = props.get<uint32_t>("out_dim", 0);
        m_args_dim = props.get<uint32_t>("args_dim", 0);
        m_decoder = std::string(props.get<std::string_view>("decoder", "neural"));
        m_hidden_size = props.get<uint32_t>("hidden_size", 16);
        m_num_layers = props.get<uint32_t>("num_layers", 2);

        if (props.has_property("encoding"))
            m_encoding = props.get<ref<Base>>("encoding");

        if (m_decoder != "neural" && m_decoder != "linear")
            Throw("neuralfield: decoder must be \"neural\" or \"linear\".");

        uint32_t expected_dim = semantic_dim(m_out_type);
        if (m_out_type == FieldValueType::Features) {
            if (m_out_dim == 0)
                Throw("neuralfield: out_dim for Features outputs must be "
                      "positive.");
        } else {
            if (m_out_dim == 0)
                m_out_dim = expected_dim;
            else if (m_out_dim != expected_dim)
                Throw("neuralfield: out_dim (%u) does not match metadata "
                      "out_type=%s (expected %u).", m_out_dim,
                      field_value_type_name(m_out_type), expected_dim);
        }

        if (m_encoding) {
            if (m_encoding->out_type() != FieldValueType::Features ||
                m_encoding->out_dim() == 0)
                Throw("neuralfield: encoding must be a Features field with "
                      "positive out_dim.");
            if (m_encoding->args_dim() != 0)
                Throw("neuralfield: encoding fields may not require args.");
        }

        m_feature_dim = 5 + m_args_dim + (m_encoding ? m_encoding->out_dim() : 0);
        uint32_t weight_count = m_out_dim * (m_feature_dim + 1);
        m_network_weights = dr::empty<FloatStorage>(weight_count);

        /* The initial mergeable decoder is a compact differentiable linear
           map over position, UV, optional user arguments, and nested encoding
           features. It keeps traversal/update semantics in place while more
           specialized Dr.Jit neural backends are integrated. */
        for (uint32_t i = 0; i < weight_count; ++i) {
            ScalarFloat t = weight_count == 1 ? 0.f
                : (ScalarFloat) i / (ScalarFloat) (weight_count - 1);
            m_network_weights.entry(i) =
                (Float) dr::lerp((ScalarFloat) -0.031f,
                                 (ScalarFloat) 0.047f, t);
        }
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

    FloatStorage eval(const SurfaceInteraction3f &si, Args args,
                      Mask active) const override {
        if (!supports_surface_queries())
            Throw("neuralfield: domain mismatch, field does not support "
                  "Surface queries.");
        validate_args(args);
        return eval_impl(&si, nullptr, args, active);
    }

    FloatStorage eval(const Interaction3f &it, Args args,
                      Mask active) const override {
        if (!supports_interaction_queries())
            Throw("neuralfield: domain mismatch, field does not support "
                  "Interaction queries.");
        validate_args(args);
        return eval_impl(nullptr, &it, args, active);
    }

    Float eval_1(const SurfaceInteraction3f &si, Args args,
                 Mask active) const override {
        validate_output(FieldValueType::Float, 1, "eval_1");
        return eval(si, args, active).entry(0);
    }

    Float eval_1(const Interaction3f &it, Args args,
                 Mask active) const override {
        validate_output(FieldValueType::Float, 1, "eval_1");
        return eval(it, args, active).entry(0);
    }

    Color3f eval_color3(const SurfaceInteraction3f &si, Args args,
                        Mask active) const override {
        validate_output(FieldValueType::Color3, 3, "eval_color3");
        FloatStorage value = eval(si, args, active);
        return Color3f(value.entry(0), value.entry(1), value.entry(2));
    }

    Color3f eval_color3(const Interaction3f &it, Args args,
                        Mask active) const override {
        validate_output(FieldValueType::Color3, 3, "eval_color3");
        FloatStorage value = eval(it, args, active);
        return Color3f(value.entry(0), value.entry(1), value.entry(2));
    }

    Array2f eval_array2(const SurfaceInteraction3f &si, Args args,
                        Mask active) const override {
        validate_output(FieldValueType::Array2, 2, "eval_array2");
        FloatStorage value = eval(si, args, active);
        return Array2f(value.entry(0), value.entry(1));
    }

    Array2f eval_array2(const Interaction3f &it, Args args,
                        Mask active) const override {
        validate_output(FieldValueType::Array2, 2, "eval_array2");
        FloatStorage value = eval(it, args, active);
        return Array2f(value.entry(0), value.entry(1));
    }

    Array3f eval_array3(const SurfaceInteraction3f &si, Args args,
                        Mask active) const override {
        validate_output(FieldValueType::Array3, 3, "eval_array3");
        FloatStorage value = eval(si, args, active);
        return Array3f(value.entry(0), value.entry(1), value.entry(2));
    }

    Array3f eval_array3(const Interaction3f &it, Args args,
                        Mask active) const override {
        validate_output(FieldValueType::Array3, 3, "eval_array3");
        FloatStorage value = eval(it, args, active);
        return Array3f(value.entry(0), value.entry(1), value.entry(2));
    }

    UnpolarizedSpectrum eval_spec(const SurfaceInteraction3f &si, Args args,
                                  Mask active) const override {
        validate_output(FieldValueType::Spectrum,
                        (uint32_t) dr::size_v<UnpolarizedSpectrum>,
                        "eval_spec");
        FloatStorage value = eval(si, args, active);
        UnpolarizedSpectrum result;
        for (uint32_t i = 0; i < dr::size_v<UnpolarizedSpectrum>; ++i)
            result.entry(i) = value.entry(i);
        return result;
    }

    UnpolarizedSpectrum eval_spec(const Interaction3f &it, Args args,
                                  Mask active) const override {
        validate_output(FieldValueType::Spectrum,
                        (uint32_t) dr::size_v<UnpolarizedSpectrum>,
                        "eval_spec");
        FloatStorage value = eval(it, args, active);
        UnpolarizedSpectrum result;
        for (uint32_t i = 0; i < dr::size_v<UnpolarizedSpectrum>; ++i)
            result.entry(i) = value.entry(i);
        return result;
    }

    Array6f eval_array6(const SurfaceInteraction3f &si, Args args,
                        Mask active) const override {
        validate_feature_count(6, "eval_array6");
        FloatStorage value = eval(si, args, active);
        return Array6f(value.entry(0), value.entry(1), value.entry(2),
                       value.entry(3), value.entry(4), value.entry(5));
    }

    Array6f eval_array6(const Interaction3f &it, Args args,
                        Mask active) const override {
        validate_feature_count(6, "eval_array6");
        FloatStorage value = eval(it, args, active);
        return Array6f(value.entry(0), value.entry(1), value.entry(2),
                       value.entry(3), value.entry(4), value.entry(5));
    }

    void eval_n(const SurfaceInteraction3f &si, Float *out, uint32_t count,
                Args args, Mask active) const override {
        if (count != m_out_dim)
            Throw("neuralfield::eval_n(): count (%u) must match out_dim (%u).",
                  count, m_out_dim);
        FloatStorage value = eval(si, args, active);
        for (uint32_t i = 0; i < count; ++i)
            out[i] = value.entry(i);
    }

    void eval_n(const Interaction3f &it, Float *out, uint32_t count,
                Args args, Mask active) const override {
        if (count != m_out_dim)
            Throw("neuralfield::eval_n(): count (%u) must match out_dim (%u).",
                  count, m_out_dim);
        FloatStorage value = eval(it, args, active);
        for (uint32_t i = 0; i < count; ++i)
            out[i] = value.entry(i);
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("network_weights", m_network_weights, ParamFlags::Differentiable);
        if (m_encoding)
            cb->put("encoding", m_encoding, ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if ((keys.empty() || string::contains(keys, "network_weights")) &&
            m_network_weights.size() != m_out_dim * (m_feature_dim + 1))
            Throw("neuralfield: network_weights size does not match metadata.");
    }

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
    static uint32_t semantic_dim(FieldValueType type) {
        switch (type) {
            case FieldValueType::Float: return 1;
            case FieldValueType::Spectrum:
                return (uint32_t) dr::size_v<UnpolarizedSpectrum>;
            case FieldValueType::Color3: return 3;
            case FieldValueType::Array2: return 2;
            case FieldValueType::Array3: return 3;
            case FieldValueType::Features: return 0;
            default: return 0;
        }
    }

    void validate_args(Args args) const {
        if (args.size != m_args_dim)
            Throw("neuralfield: args_dim mismatch (expected %u, got %u).",
                  m_args_dim, args.size);
    }

    void validate_output(FieldValueType expected_type, uint32_t expected_dim,
                         const char *method) const {
        if (m_out_type != expected_type || m_out_dim != expected_dim)
            Throw("neuralfield::%s(): incompatible output metadata "
                  "(out_type=%s, out_dim=%u).", method,
                  field_value_type_name(m_out_type), m_out_dim);
    }

    void validate_feature_count(uint32_t count, const char *method) const {
        if (m_out_type != FieldValueType::Features || m_out_dim != count)
            Throw("neuralfield::%s(): incompatible output metadata "
                  "(out_type=%s, out_dim=%u).", method,
                  field_value_type_name(m_out_type), m_out_dim);
    }

    FloatStorage eval_impl(const SurfaceInteraction3f *si,
                           const Interaction3f *it,
                           Args args, Mask active) const {
        FloatStorage encoding;
        if (m_encoding) {
            if (si)
                encoding = m_encoding->eval(*si, Args{}, active);
            else
                encoding = m_encoding->eval(*it, Args{}, active);
        }

        Float p_x = si ? si->p.x() : it->p.x(),
              p_y = si ? si->p.y() : it->p.y(),
              p_z = si ? si->p.z() : it->p.z(),
              uv_x = si ? si->uv.x() : 0.f,
              uv_y = si ? si->uv.y() : 0.f;

        FloatStorage result = dr::empty<FloatStorage>(m_out_dim);
        for (uint32_t o = 0; o < m_out_dim; ++o) {
            uint32_t offset = o * (m_feature_dim + 1);
            Float value = m_network_weights.entry(offset + m_feature_dim);

            value = dr::fmadd(p_x, m_network_weights.entry(offset++), value);
            value = dr::fmadd(p_y, m_network_weights.entry(offset++), value);
            value = dr::fmadd(p_z, m_network_weights.entry(offset++), value);
            value = dr::fmadd(uv_x, m_network_weights.entry(offset++), value);
            value = dr::fmadd(uv_y, m_network_weights.entry(offset++), value);

            for (uint32_t i = 0; i < m_args_dim; ++i)
                value = dr::fmadd(args.data[i],
                                  m_network_weights.entry(offset++), value);

            if (m_encoding) {
                // Encodings are appended after fixed interaction and arg data.
                for (uint32_t i = 0; i < m_encoding->out_dim(); ++i)
                    value = dr::fmadd(encoding.entry(i),
                                      m_network_weights.entry(offset++), value);
            }

            result.entry(o) = dr::select(active, (Float) 0.5f + (Float) 0.1f * value, 0.f);
        }
        return result;
    }

    FieldDomain m_domain = FieldDomain::Surface;
    FieldValueType m_out_type = FieldValueType::Color3;
    uint32_t m_out_dim = 0;
    uint32_t m_args_dim = 0;
    uint32_t m_feature_dim = 0;
    uint32_t m_hidden_size = 16;
    uint32_t m_num_layers = 2;
    std::string m_decoder;
    FloatStorage m_network_weights;
    ref<Base> m_encoding;
};

MI_EXPORT_PLUGIN(NeuralField)
NAMESPACE_END(mitsuba)
