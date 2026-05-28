#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/field.h>
#include <mitsuba/render/texture.h>

#include <sstream>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-neuralbsdf:

Neural diffuse material (:monosp:`neuralbsdf`)
----------------------------------------------

.. pluginparameters::

 * - reflectance
   - |field|
   - Argument-free surface field that predicts the diffuse reflectance or albedo.
     Supported field outputs are ``Float``, ``Color3``, ``Array3``, and
     ``Spectrum``. In spectral variants, dynamic ``Color3``/``Array3`` fields are
     rejected; use a spectrum field so Mitsuba's spectral upsampling policy is
     explicit.
   - |exposed|, |differentiable|

 * - mode
   - |string|
   - Scattering model exposed by this plugin. Only ``diffuse`` is currently
     supported. (Default: ``diffuse``)

The neural BSDF owns scattering semantics while fields provide learned or
tabulated reflectance data. The initial model is intentionally a diffuse BSDF:
it uses cosine-hemisphere sampling, the cosine PDF, and the same front-side
diffuse convention as :monosp:`diffuse`.

A field that predicts direction-dependent or arbitrary scattering values is not
enough to implement consistent ``sample()`` and ``pdf()`` methods and must not be
accepted by this initial plugin mode.

*/

template <typename Float, typename Spectrum>
class NeuralBSDF final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Field, SurfaceField)

    NeuralBSDF(const Properties &props) : Base(props) {
        std::string_view mode = props.get<std::string_view>("mode", "diffuse");
        if (mode != "diffuse")
            Throw("NeuralBSDF: only the structured diffuse mode is supported. "
                  "Arbitrary neural scattering values cannot define a "
                  "consistent sample() and pdf() contract.");

        m_reflectance = props.get_surface_field<Field>("reflectance");
        if (m_reflectance->args_dim() != 0)
            Throw("NeuralBSDF: diffuse mode requires an argument-free "
                  "reflectance field, got args_dim=%u.",
                  m_reflectance->args_dim());

        FieldValueType type = m_reflectance->out_type();
        uint32_t dim = m_reflectance->out_dim();
        bool valid = (type == FieldValueType::Float && dim == 1) ||
                     (type == FieldValueType::Spectrum &&
                      dim == dr::size_v<UnpolarizedSpectrum>) ||
                     (!is_spectral_v<Spectrum> &&
                      ((type == FieldValueType::Color3 && dim == 3) ||
                       (type == FieldValueType::Array3 && dim == 3)));
        if (!valid)
            Throw("NeuralBSDF: reflectance field must output Float[1], "
                  "Spectrum[%u]%s. Got %s[%u].",
                  (uint32_t) dr::size_v<UnpolarizedSpectrum>,
                  is_spectral_v<Spectrum>
                      ? ""
                      : ", Color3[3], or Array3[3]",
                  field_value_type_name(type), dim);

        m_reflectance_surface = dynamic_cast<SurfaceField *>(m_reflectance.get());
        switch (type) {
            case FieldValueType::Float:
                m_reflectance_mode = ReflectanceMode::Scalar;
                break;
            case FieldValueType::Spectrum:
                m_reflectance_mode = ReflectanceMode::Spectral;
                break;
            case FieldValueType::Color3:
                m_reflectance_mode = ReflectanceMode::Color3;
                break;
            case FieldValueType::Array3:
                m_reflectance_mode = ReflectanceMode::Array3;
                break;
            default:
                break;
        }

        m_flags = BSDFFlags::DiffuseReflection | BSDFFlags::FrontSide;
        m_components.push_back(m_flags);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float /* sample1 */,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        BSDFSample3f bs = dr::zeros<BSDFSample3f>();

        active &= cos_theta_i > 0.f;
        if (unlikely(dr::none_or<false>(active) ||
                     !ctx.is_enabled(BSDFFlags::DiffuseReflection)))
            return { bs, 0.f };

        bs.wo = warp::square_to_cosine_hemisphere(sample2);
        bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);
        bs.eta = 1.f;
        bs.sampled_type = +BSDFFlags::DiffuseReflection;
        bs.sampled_component = 0;

        UnpolarizedSpectrum value = reflectance(si, active);
        return { bs, depolarizer<Spectrum>(value) & (active && bs.pdf > 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::DiffuseReflection))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);
        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        UnpolarizedSpectrum value =
            reflectance(si, active) * dr::InvPi<Float> * cos_theta_o;
        return depolarizer<Spectrum>(value) & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::DiffuseReflection))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);
        Float pdf = warp::square_to_cosine_hemisphere_pdf(wo);

        return dr::select(active && cos_theta_i > 0.f && cos_theta_o > 0.f,
                          pdf, 0.f);
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::DiffuseReflection))
            return { 0.f, 0.f };

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);
        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        UnpolarizedSpectrum value =
            reflectance(si, active) * dr::InvPi<Float> * cos_theta_o;
        Float pdf = warp::square_to_cosine_hemisphere_pdf(wo);

        return { depolarizer<Spectrum>(value) & active,
                 dr::select(active, pdf, 0.f) };
    }

    Spectrum eval_diffuse_reflectance(const SurfaceInteraction3f &si,
                                      Mask active) const override {
        return depolarizer<Spectrum>(reflectance(si, active)) & active;
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("reflectance", m_reflectance, ParamFlags::Differentiable);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "NeuralBSDF[" << std::endl
            << "  reflectance = " << string::indent(m_reflectance) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(NeuralBSDF)

private:
    enum class ReflectanceMode {
        Scalar,
        Spectral,
        Color3,
        Array3
    };

    UnpolarizedSpectrum reflectance(const SurfaceInteraction3f &si,
                                    Mask active) const {
        UnpolarizedSpectrum value = 0.f;

        switch (m_reflectance_mode) {
            case ReflectanceMode::Scalar: {
                Float v = m_reflectance_surface
                    ? m_reflectance_surface->eval_1(si, active)
                    : m_reflectance->eval_1(si, typename Field::Args{}, active);
                value = v;
                break;
            }

            case ReflectanceMode::Spectral:
                value = m_reflectance_surface
                    ? m_reflectance_surface->eval(si, active)
                    : m_reflectance->eval_spec(si, typename Field::Args{}, active);
                break;

            case ReflectanceMode::Color3: {
                Color3f color = m_reflectance_surface
                    ? m_reflectance_surface->eval_3(si, active)
                    : m_reflectance->eval_color3(si, typename Field::Args{},
                                                 active);
                if constexpr (is_monochromatic_v<Spectrum>)
                    value = luminance(color);
                else if constexpr (is_spectral_v<Spectrum>)
                    Throw("NeuralBSDF: Color3 reflectance is invalid in "
                          "spectral variants.");
                else
                    value = color;
                break;
            }

            case ReflectanceMode::Array3: {
                Color3f color;
                if (m_reflectance_surface) {
                    color = m_reflectance_surface->eval_3(si, active);
                } else {
                    typename Field::Array3f array =
                        m_reflectance->eval_array3(si, typename Field::Args{},
                                                   active);
                    color = Color3f(array.x(), array.y(), array.z());
                }

                if constexpr (is_monochromatic_v<Spectrum>)
                    value = luminance(color);
                else if constexpr (is_spectral_v<Spectrum>)
                    Throw("NeuralBSDF: Array3 reflectance is invalid in "
                          "spectral variants.");
                else
                    value = color;
                break;
            }
        }

        return dr::clip(value, 0.f, 1.f);
    }

    ref<Field> m_reflectance;
    SurfaceField *m_reflectance_surface = nullptr;
    ReflectanceMode m_reflectance_mode = ReflectanceMode::Spectral;

    MI_TRAVERSE_CB(Base, m_reflectance)
};

MI_EXPORT_PLUGIN(NeuralBSDF)
NAMESPACE_END(mitsuba)
