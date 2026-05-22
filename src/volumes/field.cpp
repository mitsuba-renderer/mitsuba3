#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/field.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/volume.h>

#include <sstream>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _volume-fieldvolume:

Field volume (:monosp:`fieldvolume`)
------------------------------------

.. pluginparameters::

 * - field
   - |field|
   - Field evaluated at the volume lookup point. The field must support
     interaction queries and require no argument channels.
   - |exposed|, |differentiable|

 * - to_world
   - |transform|
   - Optional world-space transform inherited from the volume interface.
     Evaluation points are transformed into local field coordinates before the
     nested field is queried. (Default: identity)

 * - max_value
   - |float|
   - Optional majorant/max value returned by :monosp:`max()` and
     :monosp:`max_per_channel()`. This should be provided when using the volume
     as a heterogeneous medium extinction coefficient.
   - |exposed|

This plugin adapts an interaction field to Mitsuba's public volume interface.
It keeps volume-owned transforms, bounding boxes, channel-count metadata, and
majorant metadata at the volume layer while using fixed-size field evaluation
methods for scalar, vector, spectrum, and six-channel lookups.

*/

template <typename Float, typename Spectrum>
class FieldVolume final : public Volume<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Volume, m_to_local, m_bbox, m_channel_count)
    MI_IMPORT_TYPES(Field)

    using Args = typename Field::Args;
    using Array6f = dr::Array<Float, 6>;

    FieldVolume(const Properties &props) : Base(props) {
        m_field = props.get<ref<Field>>("field");

        if (props.has_property("max_value")) {
            m_has_max_value = true;
            m_max_value = props.get<ScalarFloat>("max_value");
        }

        validate_field();
        m_channel_count = m_field->out_dim();
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("field", m_field, ParamFlags::Differentiable);
        if (m_has_max_value)
            cb->put("max_value", m_max_value, ParamFlags::NonDifferentiable);
    }

    UnpolarizedSpectrum eval(const Interaction3f &it,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        Interaction3f query = map_interaction(it);
        switch (m_field->out_type()) {
            case FieldValueType::Float:
                return UnpolarizedSpectrum(m_field->eval_1(query, Args{}, active));

            case FieldValueType::Color3:
                return spectrum_from_color3(
                    m_field->eval_color3(query, Args{}, active));

            case FieldValueType::Spectrum:
                return m_field->eval_spec(query, Args{}, active);

            default:
                Throw("fieldvolume::eval(): field output type %s cannot be "
                      "queried as a spectrum.",
                      field_value_type_name(m_field->out_type()));
        }
    }

    Float eval_1(const Interaction3f &it,
                 Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        Interaction3f query = map_interaction(it);
        switch (m_field->out_type()) {
            case FieldValueType::Float:
                return m_field->eval_1(query, Args{}, active);

            case FieldValueType::Color3:
                return luminance(m_field->eval_color3(query, Args{}, active));

            case FieldValueType::Spectrum:
                return luminance(m_field->eval_spec(query, Args{}, active),
                                 query.wavelengths, active);

            default:
                Throw("fieldvolume::eval_1(): field output type %s cannot be "
                      "queried as a scalar.",
                      field_value_type_name(m_field->out_type()));
        }
    }

    Vector3f eval_3(const Interaction3f &it,
                    Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        Interaction3f query = map_interaction(it);
        switch (m_field->out_type()) {
            case FieldValueType::Color3: {
                Color3f value = m_field->eval_color3(query, Args{}, active);
                return Vector3f(value.x(), value.y(), value.z());
            }

            case FieldValueType::Array3:
                return m_field->eval_array3(query, Args{}, active);

            default:
                Throw("fieldvolume::eval_3(): expected Color3 or Array3 field "
                      "output, got %s[%u].",
                      field_value_type_name(m_field->out_type()),
                      m_field->out_dim());
        }
    }

    Array6f eval_6(const Interaction3f &it,
                   Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (m_field->out_type() != FieldValueType::Features ||
            m_field->out_dim() != 6) {
            Throw("fieldvolume::eval_6(): expected Features[6] field output, "
                  "got %s[%u].", field_value_type_name(m_field->out_type()),
                  m_field->out_dim());
        }

        return m_field->eval_array6(map_interaction(it), Args{}, active);
    }

    void eval_n(const Interaction3f &it, Float *out,
                Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        m_field->eval_n(map_interaction(it), out, m_channel_count, Args{}, active);
    }

    std::pair<UnpolarizedSpectrum, Vector3f>
    eval_gradient(const Interaction3f &it,
                  Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        DRJIT_MARK_USED(it);
        DRJIT_MARK_USED(active);
        Throw("fieldvolume::eval_gradient(): field-backed volumes do not "
              "provide grid gradients.");
    }

    ScalarFloat max() const override {
        if (!m_has_max_value)
            Throw("fieldvolume::max(): no max_value was provided.");
        return m_max_value;
    }

    void max_per_channel(ScalarFloat *out) const override {
        if (!m_has_max_value)
            Throw("fieldvolume::max_per_channel(): no max_value was provided.");

        for (uint32_t i = 0; i < m_channel_count; ++i)
            out[i] = m_max_value;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "FieldVolume[" << std::endl
            << "  to_local = " << string::indent(m_to_local, 13) << ","
            << std::endl
            << "  bbox = " << string::indent(m_bbox) << "," << std::endl
            << "  field = " << string::indent(m_field) << "," << std::endl
            << "  channels = " << m_channel_count << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(FieldVolume)

private:
    void validate_field() const {
        if (!m_field)
            Throw("fieldvolume: missing field.");

        if (!m_field->supports_interaction_queries())
            Throw("fieldvolume: field must support Interaction queries "
                  "(domain=%s).", field_domain_name(m_field->domain()));

        if (m_field->args_dim() != 0)
            Throw("fieldvolume: field args_dim must be 0, got %u.",
                  m_field->args_dim());

        if constexpr (dr::is_jit_v<Float>) {
            if (!m_field->supports_jit())
                Throw("fieldvolume: field does not support JIT variants.");
        } else {
            if (!m_field->supports_scalar())
                Throw("fieldvolume: field does not support scalar variants.");
        }

        FieldValueType type = m_field->out_type();
        uint32_t dim = m_field->out_dim();
        bool valid = (type == FieldValueType::Float && dim == 1) ||
                     (type == FieldValueType::Color3 && dim == 3) ||
                     (type == FieldValueType::Spectrum &&
                      dim == (uint32_t) dr::size_v<UnpolarizedSpectrum>) ||
                     (type == FieldValueType::Array3 && dim == 3) ||
                     (type == FieldValueType::Features && dim > 0);
        if (!valid)
            Throw("fieldvolume: field must output Float[1], Color3[3], "
                  "Spectrum[%u], Array3[3], or positive-dimensional "
                  "Features, got %s[%u].",
                  (uint32_t) dr::size_v<UnpolarizedSpectrum>,
                  field_value_type_name(type), dim);
    }

    Interaction3f map_interaction(const Interaction3f &it) const {
        Interaction3f result = it;
        result.p = m_to_local * it.p;
        return result;
    }

    UnpolarizedSpectrum spectrum_from_color3(const Color3f &value) const {
        if constexpr (is_rgb_v<Spectrum>)
            return value;
        else
            return UnpolarizedSpectrum(luminance(value));
    }

    const char *field_value_type_name(FieldValueType type) const {
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

    const char *field_domain_name(FieldDomain domain) const {
        switch (domain) {
            case FieldDomain::Surface: return "Surface";
            case FieldDomain::Interaction: return "Interaction";
            case FieldDomain::SurfaceAndInteraction:
                return "SurfaceAndInteraction";
            default: return "Unknown";
        }
    }

private:
    ref<Field> m_field;
    bool m_has_max_value = false;
    ScalarFloat m_max_value = 0.f;

    MI_TRAVERSE_CB(Base, m_field)
};

MI_EXPORT_PLUGIN(FieldVolume)
NAMESPACE_END(mitsuba)
