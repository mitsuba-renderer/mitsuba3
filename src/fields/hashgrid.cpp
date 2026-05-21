#include <mitsuba/core/properties.h>
#include <mitsuba/render/field.h>

#include <sstream>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _field-hashgridencoding:

Hash grid encoding field (:monosp:`hashgridencoding`)
-----------------------------------------------------

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
        m_input_dim = props.get<uint32_t>("input_dim", 2);
        m_out_dim = props.get<uint32_t>("out_dim", 0);
        props.mark_queried("n_levels");
        props.mark_queried("n_features_per_level");
        props.mark_queried("base_resolution");
        props.mark_queried("per_level_scale");
        props.mark_queried("hashmap_size");
    }

    FieldValueType out_type() const override { return FieldValueType::Features; }
    FieldDomain domain() const override { return FieldDomain::SurfaceAndInteraction; }
    uint32_t out_dim() const override { return m_out_dim; }
    uint32_t args_dim() const override { return 0; }

    bool supports_scalar() const override { return false; }
    bool supports_jit() const override { return true; }
    bool supports_surface_queries() const override { return true; }
    bool supports_interaction_queries() const override { return m_input_dim == 3; }

    FloatStorage eval(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval");
    }

    FloatStorage eval(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval");
    }

    void traverse(TraversalCallback *) override {
        NotImplementedError("traverse");
    }

    void parameters_changed(const std::vector<std::string> &) override {
        NotImplementedError("parameters_changed");
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
    uint32_t m_input_dim = 2;
    uint32_t m_out_dim = 0;
};

MI_EXPORT_PLUGIN(HashGridEncoding)
NAMESPACE_END(mitsuba)
