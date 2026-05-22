#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/field.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>

#include <sstream>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-fieldtexture:

Field texture (:monosp:`fieldtexture`)
--------------------------------------

.. pluginparameters::

 * - field
   - |field|
   - Field evaluated at the texture lookup point. The field must support
     surface queries, require no argument channels, and output ``Float``,
     ``Color3``, or ``Spectrum`` values.
   - |exposed|, |differentiable|

 * - to_uv
   - |transform|
   - Optional 2D UV transform applied before evaluating the field.
     (Default: identity)
   - |exposed|

 * - mean_value
   - |float|
   - Optional mean value returned by :monosp:`mean()`. When omitted,
     :monosp:`mean()` reports that the aggregate is unavailable.
   - |exposed|

 * - max_value
   - |float|
   - Optional maximum value returned by :monosp:`max()`. When omitted,
     :monosp:`max()` reports that the aggregate is unavailable.
   - |exposed|

This plugin adapts a surface field to Mitsuba's public texture interface. It is
intended for materials and other renderer components that already consume
textures and should not know about field-specific argument channels. Direct
field users should keep using the field API when they need richer query
context.

*/

template <typename Float, typename Spectrum>
class FieldTexture final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Texture)
    MI_IMPORT_TYPES(Field)

    using Args = typename Field::Args;

    FieldTexture(const Properties &props) : Base(props) {
        m_field = props.get<ref<Field>>("field");
        m_transform = props.get<ScalarAffineTransform3f>(
            "to_uv", ScalarAffineTransform3f());

        if (props.has_property("mean_value")) {
            m_has_mean_value = true;
            m_mean_value = props.get<ScalarFloat>("mean_value");
        }

        if (props.has_property("max_value")) {
            m_has_max_value = true;
            m_max_value = props.get<ScalarFloat>("max_value");
        }

        validate_field();
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("field", m_field, ParamFlags::Differentiable);
        cb->put("to_uv", m_transform, ParamFlags::NonDifferentiable);

        if (m_has_mean_value)
            cb->put("mean_value", m_mean_value, ParamFlags::NonDifferentiable);
        if (m_has_max_value)
            cb->put("max_value", m_max_value, ParamFlags::NonDifferentiable);
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        SurfaceInteraction3f query = map_surface_interaction(si);
        switch (m_field->out_type()) {
            case FieldValueType::Float:
                return UnpolarizedSpectrum(m_field->eval_1(query, Args{}, active));

            case FieldValueType::Color3:
                return spectrum_from_color3(
                    m_field->eval_color3(query, Args{}, active));

            case FieldValueType::Spectrum:
                return m_field->eval_spec(query, Args{}, active);

            default:
                Throw("fieldtexture::eval(): invalid field output type %s.",
                      field_value_type_name(m_field->out_type()));
        }
    }

    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        SurfaceInteraction3f query = map_surface_interaction(si);
        switch (m_field->out_type()) {
            case FieldValueType::Float:
                return m_field->eval_1(query, Args{}, active);

            case FieldValueType::Color3:
                return luminance(m_field->eval_color3(query, Args{}, active));

            case FieldValueType::Spectrum:
                return luminance(m_field->eval_spec(query, Args{}, active),
                                 query.wavelengths, active);

            default:
                Throw("fieldtexture::eval_1(): invalid field output type %s.",
                      field_value_type_name(m_field->out_type()));
        }
    }

    Vector2f eval_1_grad(const SurfaceInteraction3f &si,
                         Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        DRJIT_MARK_USED(si);
        DRJIT_MARK_USED(active);
        Throw("fieldtexture::eval_1_grad(): field-backed textures do not "
              "provide texture-space gradients.");
    }

    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        SurfaceInteraction3f query = map_surface_interaction(si);
        switch (m_field->out_type()) {
            case FieldValueType::Float:
                return Color3f(m_field->eval_1(query, Args{}, active));

            case FieldValueType::Color3:
                return m_field->eval_color3(query, Args{}, active);

            case FieldValueType::Spectrum:
                if constexpr (dr::size_v<UnpolarizedSpectrum> == 3) {
                    UnpolarizedSpectrum value =
                        m_field->eval_spec(query, Args{}, active);
                    return Color3f(value.x(), value.y(), value.z());
                } else {
                    Throw("fieldtexture::eval_3(): Spectrum field output "
                          "cannot be queried as RGB in this variant.");
                }

            default:
                Throw("fieldtexture::eval_3(): expected Color3 field output, "
                      "got %s with out_dim=%u.",
                      field_value_type_name(m_field->out_type()),
                      m_field->out_dim());
        }
    }

    Float mean() const override {
        if (!m_has_mean_value)
            Throw("fieldtexture::mean(): no mean_value was provided.");
        return dr::opaque<Float>(m_mean_value);
    }

    ScalarFloat max() const override {
        if (!m_has_max_value)
            Throw("fieldtexture::max(): no max_value was provided.");
        return m_max_value;
    }

    bool is_spatially_varying() const override { return true; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "FieldTexture[" << std::endl
            << "  field = " << string::indent(m_field) << "," << std::endl
            << "  transform = " << string::indent(m_transform) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(FieldTexture)

private:
    void validate_field() const {
        if (!m_field)
            Throw("fieldtexture: missing field.");

        if (!m_field->supports_surface_queries())
            Throw("fieldtexture: field must support Surface queries "
                  "(domain=%s).", field_domain_name(m_field->domain()));

        if (m_field->args_dim() != 0)
            Throw("fieldtexture: field args_dim must be 0, got %u.",
                  m_field->args_dim());

        if constexpr (dr::is_jit_v<Float>) {
            if (!m_field->supports_jit())
                Throw("fieldtexture: field does not support JIT variants.");
        } else {
            if (!m_field->supports_scalar())
                Throw("fieldtexture: field does not support scalar variants.");
        }

        FieldValueType type = m_field->out_type();
        uint32_t dim = m_field->out_dim();
        bool valid = (type == FieldValueType::Float && dim == 1) ||
                     (type == FieldValueType::Color3 && dim == 3) ||
                     (type == FieldValueType::Spectrum &&
                      dim == (uint32_t) dr::size_v<UnpolarizedSpectrum>);
        if (!valid)
            Throw("fieldtexture: field must output Float[1], Color3[3], or "
                  "Spectrum[%u], got %s[%u].",
                  (uint32_t) dr::size_v<UnpolarizedSpectrum>,
                  field_value_type_name(type), dim);
    }

    SurfaceInteraction3f
    map_surface_interaction(const SurfaceInteraction3f &si) const {
        SurfaceInteraction3f result = si;
        result.uv = m_transform * si.uv;
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
    ScalarAffineTransform3f m_transform;
    bool m_has_mean_value = false;
    bool m_has_max_value = false;
    ScalarFloat m_mean_value = 0.f;
    ScalarFloat m_max_value = 0.f;

    MI_TRAVERSE_CB(Base, m_field)
};

MI_EXPORT_PLUGIN(FieldTexture)
NAMESPACE_END(mitsuba)
