#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-uv_set_selector:

UV Set Selector texture (:monosp:`uv_set_selector`)
-------------------------------------------------

.. pluginparameters::

 * - uv_set
   - |string|
   - Name of the mesh attribute representing the alternative UV coordinates. It
should start with ``"vertex_"`` or ``"face_"``.
 * - nested
   - |texture|
   - The nested texture that should evaluate colors using the selected UV set.

This plugin provides a mechanism to redirect the UV coordinates evaluated by a
nested texture to a custom UV set (loaded as a mesh attribute).

 */

template <typename Float, typename Spectrum>
class UvSetSelector final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    UvSetSelector(const Properties &props) : Texture(props) {
        m_uv_set = props.get<std::string>("uv_set");
        if (m_uv_set.find("vertex_") == std::string::npos &&
            m_uv_set.find("face_") == std::string::npos)
            Throw("Invalid mesh attribute name: must start with either "
                  "\"vertex_\" or \"face_\" but was \"%s\".",
                  m_uv_set.c_str());

        for (auto &prop : props.objects()) {
            if (Texture *tex = prop.try_get<Texture>()) {
                if (m_nested)
                    Throw("Only a single nested Texture can be specified.");
                m_nested = tex;
            }
        }

        if (!m_nested) {
            if (props.has_property("nested")) {
                m_nested = props.get_texture<Texture>("nested");
            } else {
                Throw("Exactly one nested Texture must be specified.");
            }
        }
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("nested", m_nested, ParamFlags::Differentiable);
    }

    SurfaceInteraction3f select_uv(const SurfaceInteraction3f &si,
                                   Mask active) const {
        SurfaceInteraction3f local_si(si);
        Vector2f attr = si.shape->eval_attribute_2(m_uv_set, si, active);
        local_si.uv  = Point2f(attr.x(), attr.y());
        return local_si;
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return m_nested->eval(select_uv(si, active), active);
    }

    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return m_nested->eval_1(select_uv(si, active), active);
    }

    Vector2f eval_1_grad(const SurfaceInteraction3f &si,
                         Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return m_nested->eval_1_grad(select_uv(si, active), active);
    }

    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return m_nested->eval_3(select_uv(si, active), active);
    }

    Float mean() const override { return m_nested->mean(); }

    bool is_spatially_varying() const override {
        return m_nested->is_spatially_varying();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "UvSetSelector[" << std::endl
            << "  uv_set = \"" << m_uv_set << "\"," << std::endl
            << "  nested = " << string::indent(m_nested) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(UvSetSelector)
protected:
    std::string m_uv_set;
    ref<Texture> m_nested;
};

MI_EXPORT_PLUGIN(UvSetSelector)
NAMESPACE_END(mitsuba)
