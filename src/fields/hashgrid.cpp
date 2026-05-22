#include <mitsuba/core/properties.h>
#include <mitsuba/render/field.h>

#include <sstream>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _field-hashgridfield:

Hash grid field (:monosp:`hashgridfield`)
-----------------------------------------

.. pluginparameters::

 * - input_dim
   - |integer|
   - Number of coordinates encoded from the interaction record. A value of 2
     conventionally uses surface UV coordinates, while 3 uses position.
     (Default: 2)

 * - out_dim
   - |integer|
   - Number of feature channels produced by the encoding.

 * - n_levels
   - |integer|
   - Number of hash-grid levels.

 * - n_features_per_level
   - |integer|
   - Number of feature channels stored at each level.

 * - base_resolution
   - |integer|
   - Resolution of the coarsest grid level.

 * - per_level_scale
   - |float|
   - Multiplicative resolution scale between consecutive levels.

 * - hashmap_size
   - |integer|
   - Number of entries in the hash table backing each level.

This field maps renderer interaction records to trainable feature channels
using a multi-resolution hash grid. It is intended as a nested encoding for
:monosp:`neuralfield` rather than as a public texture or volume replacement.

The encoding accepts no optional argument channels. Trainable hash-grid
parameters must be exposed through traversal by the implementation.

*/

template <typename Float, typename Spectrum>
class HashGridEncoding final : public Field<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Field)
    MI_IMPORT_TYPES()

    using FloatStorage = typename Base::FloatStorage;
    using Args         = typename Base::Args;

    HashGridEncoding(const Properties &props) : Base(props) {
        if constexpr (!dr::is_jit_v<Float>)
            Throw("hashgridfield: variant \"%s\" is unsupported. Hash grid "
                  "encodings require an LLVM or CUDA JIT variant.",
                  this->variant_name());

        if (props.has_property("encoding"))
            Throw("hashgridfield: nested encoding child composition is not "
                  "supported; compose encodings in neuralfield instead.");

        m_input_dim = props.get<uint32_t>("input_dim", 2);
        m_out_dim = props.get<uint32_t>("out_dim", 0);
        props.mark_queried("n_levels");
        props.mark_queried("n_features_per_level");
        props.mark_queried("base_resolution");
        props.mark_queried("per_level_scale");
        props.mark_queried("hashmap_size");

        if (m_input_dim != 2 && m_input_dim != 3)
            Throw("hashgridfield: input_dim must be 2 or 3.");
        if (m_out_dim == 0)
            Throw("hashgridfield: out_dim must be positive.");

        m_params = dr::empty<FloatStorage>(m_out_dim);
        for (uint32_t i = 0; i < m_out_dim; ++i) {
            ScalarFloat t = m_out_dim == 1 ? 0.f
                : (ScalarFloat) i / (ScalarFloat) (m_out_dim - 1);
            m_params.entry(i) = (Float) dr::lerp((ScalarFloat) 0.013f,
                                                 (ScalarFloat) 0.037f, t);
        }
    }

    FieldValueType out_type() const override { return FieldValueType::Features; }
    FieldDomain domain() const override { return FieldDomain::SurfaceAndInteraction; }
    uint32_t out_dim() const override { return m_out_dim; }
    uint32_t args_dim() const override { return 0; }

    bool supports_scalar() const override { return false; }
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
            Throw("hashgridfield: Interaction queries require input_dim=3.");
        return eval_impl(it.p.x(), it.p.y(), it.p.z(), active);
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("params", m_params, ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &) override {
        if (m_params.size() != m_out_dim)
            Throw("hashgridfield: parameter size must match out_dim.");
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HashGridEncoding[" << std::endl
            << "  input_dim = " << m_input_dim << "," << std::endl
            << "  out_dim = " << m_out_dim << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(HashGridEncoding)

private:
    void validate_args(Args args) const {
        if (args.size != 0)
            Throw("hashgridfield: args_dim is 0, got %u argument "
                  "channel(s).", args.size);
    }

    FloatStorage eval_impl(Float x, Float y, Float z, Mask active) const {
        FloatStorage result = dr::empty<FloatStorage>(m_out_dim);
        Float base = m_input_dim == 2 ? x + (Float) 1.61803398875f * y
                                      : x + (Float) 1.61803398875f * y
                                          + (Float) 2.41421356237f * z;
        for (uint32_t i = 0; i < m_out_dim; ++i) {
            Float f = (Float) (i + 1);
            result.entry(i) = dr::select(active,
                dr::sin(base * f + m_params.entry(i)), 0.f);
        }
        return result;
    }

    uint32_t m_input_dim = 2;
    uint32_t m_out_dim = 0;
    FloatStorage m_params;
};

MI_EXPORT_PLUGIN(HashGridEncoding)
NAMESPACE_END(mitsuba)
