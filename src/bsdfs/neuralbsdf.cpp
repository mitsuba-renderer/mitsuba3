#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/field.h>

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
   - Scattering model exposed by this plugin. The initial public mode is a
     field-modulated diffuse model. More expressive modes must define matching
     value, sampling, and PDF outputs before becoming public.

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
    MI_IMPORT_TYPES(Field)

    NeuralBSDF(const Properties &props) : Base(props) {
        std::string_view mode = props.get<std::string_view>("mode", "diffuse");
        if (mode != "diffuse")
            Throw("neuralbsdf: only the structured diffuse mode is supported. "
                  "Arbitrary neural scattering values cannot define a "
                  "consistent sample() and pdf() contract.");

        m_reflectance = props.get_surface_field<Field>("reflectance");
        if (!m_reflectance)
            Throw("neuralbsdf: missing reflectance field.");

        if (m_reflectance->args_dim() != 0)
            Throw("neuralbsdf diffuse mode requires an argument-free "
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
            Throw("neuralbsdf: reflectance field must output Float[1], "
                  "Spectrum[%u]%s. Got %s[%u].",
                  (uint32_t) dr::size_v<UnpolarizedSpectrum>,
                  is_spectral_v<Spectrum>
                      ? ""
                      : ", Color3[3], or Array3[3]",
                  field_value_type_name(type), dim);

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
            << "  reflectance = " << m_reflectance << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(NeuralBSDF)

private:
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

    UnpolarizedSpectrum reflectance(const SurfaceInteraction3f &si,
                                    Mask active) const {
        return dr::maximum(0.f, m_reflectance->eval(si, active));
    }

    ref<Field> m_reflectance;

    MI_TRAVERSE_CB(Base, m_reflectance)
};

MI_EXPORT_PLUGIN(NeuralBSDF)
NAMESPACE_END(mitsuba)
