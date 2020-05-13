#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/texture.h>

#define MTS_ROUGH_TRANSMITTANCE_RES 64

NAMESPACE_BEGIN(mitsuba)

/**!
.. _bsdf-roughplastic:

Rough plastic material (:monosp:`roughplastic`)
-----------------------------------------------

.. pluginparameters::

 * - diffuse_reflectance
   - |spectrum| or |texture|
   - Optional factor used to modulate the diffuse reflection component. (Default: 0.5)
 * - nonlinear
   - |bool|
   - Account for nonlinear color shifts due to internal scattering? See the
     :ref:`plastic <bsdf-plastic>` plugin for details.
     \default{Don't account for them and preserve the texture colors. (Default: |false|)

 * - int_ior
   - |float| or |string|
   - Interior index of refraction specified numerically or using a known material name. (Default: polypropylene / 1.49)
 * - ext_ior
   - |float| or |string|
   - Exterior index of refraction specified numerically or using a known material name.  (Default: air / 1.000277)
 * - specular_reflectance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular reflection component.
     Note that for physical realism, this parameter should never be touched. (Default: 1.0)

 * - distribution
   - |string|
   - Specifies the type of microfacet normal distribution used to model the surface roughness.

     - :monosp:`beckmann`: Physically-based distribution derived from Gaussian random surfaces.
       This is the default.
     - :monosp:`ggx`: The GGX :cite:`Walter07Microfacet` distribution (also known as Trowbridge-Reitz
       :cite:`Trowbridge19975Average` distribution) was designed to better approximate the long
       tails observed in measurements of ground surfaces, which are not modeled by the Beckmann
       distribution.
 * - alpha
   - |float|
   - Specifies the roughness of the unresolved surface micro-geometry along the tangent and
     bitangent directions. When the Beckmann distribution is used, this parameter is equal to the
     **root mean square** (RMS) slope of the microfacets. (Default: 0.1)
 * - sample_visible
   - |bool|
   - Enables a sampling technique proposed by Heitz and D'Eon :cite:`Heitz1014Importance`, which
     focuses computation on the visible parts of the microfacet normal distribution, considerably
     reducing variance in some cases. (Default: |true|, i.e. use visible normal sampling)



This plugin implements a realistic microfacet scattering model for rendering
rough dielectric materials with internal scattering, such as plastic.

Microfacet theory describes rough surfaces as an arrangement of unresolved and ideally specular
facets, whose normal directions are given by a specially chosen *microfacet distribution*.
By accounting for shadowing and masking effects between these facets, it is possible to reproduce
the important off-specular reflections peaks observed in real-world measurements of such materials.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_roughplastic_beckmann.jpg
   :caption: Beckmann, :math:`\alpha=0.1`
.. subfigure:: ../../resources/data/docs/images/render/bsdf_roughplastic_ggx.jpg
   :caption: GGX, :math:`\alpha=0.3`
.. subfigend::
    :label: fig-bsdf-roughplastic

This plugin is essentially the *roughened* equivalent of the (smooth) plugin
:ref:`plastic <bsdf-plastic>`. For very low values of :math:`\alpha`, the two will
be identical, though scenes using this plugin will take longer to render
due to the additional computational burden of tracking surface roughness.

For convenience, this model allows to specify IOR values either numerically, or
based on a list of known materials (see the corresponding table in the
:ref:`dielectric <bsdf-dielectric>` reference). When no parameters are given,
the plugin activates the defaults, which describe a white polypropylene plastic
material with a light amount of roughness modeled using the Beckmann distribution.

To get an intuition about the effect of the surface roughness parameter
:math:`\alpha`, consider the following approximate classification: a value of
:math:`\alpha=0.001-0.01` corresponds to a material with slight imperfections
on an otherwise smooth surface finish, :math:`\alpha=0.1` is relatively rough,
and :math:`\alpha=0.3-0.7` is **extremely** rough (e.g. an etched or ground
finish). Values significantly above that are probably not too realistic.

The following XML snippet describes a material definition for black plastic material.

.. code-block:: xml
    :name: lst-roughplastic-black

    <bsdf type="roughplastic">
        <string name="distribution" value="beckmann"/>
        <float name="int_ior" value="1.61"/>
        <spectrum name="diffuse_reflectance" value="0"/>
    </bsdf>


Like the :ref:`plastic <bsdf-plastic>` material, this model internally simulates the
interaction of light with a diffuse base surface coated by a thin dielectric
layer (where the coating layer is now **rough**). This is a convenient
abstraction rather than a restriction. In other words, there are many
materials that can be rendered with this model, even if they might not
fit this description perfectly well.

The simplicity of this setup makes it possible to account for interesting
nonlinear effects due to internal scattering, which is controlled by
the :monosp:`nonlinear` parameter:

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_roughplastic_diffuse.jpg
   :caption: Diffuse textured rendering
   :label: fig-roughplastic-diffuse
.. subfigure:: ../../resources/data/docs/images/render/bsdf_roughplastic_preserve.jpg
   :caption: Rough plastic model with :monosp:`nonlinear=false`
   :label: fig-roughplastic-preserve
.. subfigure:: ../../resources/data/docs/images/render/bsdf_roughplastic_nopreserve.jpg
   :caption: Textured rough plastic model with :monosp:`nonlinear=true`
   :label: fig-roughplastic-nopreserve
.. subfigend::
    :label: fig-bsdf-roughplastic-nonlinear

For more details, please refer to the description
of this parameter given in the :ref:`plastic <bsdf-plastic>` plugin section.

 */

template <typename Float, typename Spectrum>
class RoughPlastic final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture, MicrofacetDistribution)

    RoughPlastic(const Properties &props) : Base(props) {
        /// Specifies the internal index of refraction at the interface
        ScalarFloat int_ior = lookup_ior(props, "int_ior", "polypropylene");

        /// Specifies the external index of refraction at the interface
        ScalarFloat ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.f || ext_ior < 0.f || int_ior == ext_ior)
            Throw("The interior and exterior indices of "
                  "refraction must be positive and differ!");

        m_eta = int_ior / ext_ior;

        m_diffuse_reflectance  = props.texture<Texture>("diffuse_reflectance",  .5f);

        if (props.has_property("specular_reflectance"))
            m_specular_reflectance = props.texture<Texture>("specular_reflectance", 1.f);

        m_nonlinear = props.bool_("nonlinear", false);

        mitsuba::MicrofacetDistribution<ScalarFloat, Spectrum> distr(props);
        m_type = distr.type();
        m_sample_visible = distr.sample_visible();

        if (distr.is_anisotropic())
            Throw("The 'roughplastic' plugin currently does not support "
                  "anisotropic microfacet distributions!");

        m_alpha = distr.alpha();

        m_components.push_back(BSDFFlags::GlossyReflection | BSDFFlags::FrontSide);
        m_components.push_back(BSDFFlags::DiffuseReflection | BSDFFlags::FrontSide);
        m_flags =  m_components[0] | m_components[1];

        parameters_changed();
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        bool has_specular = ctx.is_enabled(BSDFFlags::GlossyReflection, 0),
             has_diffuse  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        BSDFSample3f bs = zero<BSDFSample3f>();
        Spectrum result(0.f);
        if (unlikely((!has_specular && !has_diffuse) || none_or<false>(active)))
            return { bs, result };

        Float t_i = lerp_gather(m_external_transmittance.data(), cos_theta_i,
                                MTS_ROUGH_TRANSMITTANCE_RES, active);

        // Determine which component should be sampled
        Float prob_specular = (1.f - t_i) * m_specular_sampling_weight,
              prob_diffuse  = t_i * (1.f - m_specular_sampling_weight);

        if (unlikely(has_specular != has_diffuse))
            prob_specular = has_specular ? 1.f : 0.f;
        else
            prob_specular = prob_specular / (prob_specular + prob_diffuse);
        prob_diffuse = 1.f - prob_specular;

        Mask sample_specular = active && (sample1 < prob_specular),
             sample_diffuse = active && !sample_specular;

        bs.eta = 1.f;

        if (any_or<true>(sample_specular)) {
            MicrofacetDistribution distr(m_type, m_alpha, m_sample_visible);
            Normal3f m = std::get<0>(distr.sample(si.wi, sample2));

            masked(bs.wo, sample_specular) = reflect(si.wi, m);
            masked(bs.sampled_component, sample_specular) = 0;
            masked(bs.sampled_type, sample_specular) = +BSDFFlags::GlossyReflection;
        }

        if (any_or<true>(sample_diffuse)) {
            masked(bs.wo, sample_diffuse) = warp::square_to_cosine_hemisphere(sample2);
            masked(bs.sampled_component, sample_diffuse) = 1;
            masked(bs.sampled_type, sample_diffuse) = +BSDFFlags::DiffuseReflection;
        }

        bs.pdf = pdf(ctx, si, bs.wo, active);
        active &= bs.pdf > 0.f;
        result = eval(ctx, si, bs.wo, active);

        return { bs, select(active, unpolarized<Spectrum>(result) / bs.pdf, 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        bool has_specular = ctx.is_enabled(BSDFFlags::GlossyReflection, 0),
             has_diffuse  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        MicrofacetDistribution distr(m_type, m_alpha, m_sample_visible);

        UnpolarizedSpectrum result(0.f);
        if (unlikely((!has_specular && !has_diffuse) || none_or<false>(active)))
            return result;

        if (has_specular) {
            // Calculate the reflection half-vector
            Vector3f H = normalize(wo + si.wi);

            // Evaluate the microfacet normal distribution
            Float D = distr.eval(H);

            // Fresnel term
            Float F = std::get<0>(fresnel(dot(si.wi, H), Float(m_eta)));

            // Smith's shadow-masking function
            Float G = distr.G(si.wi, wo, H);

            // Calculate the specular reflection component
            UnpolarizedSpectrum value = F * D * G / (4.f * cos_theta_i);

            if (m_specular_reflectance)
                value *= m_specular_reflectance->eval(si, active);

            result += value;
        }

        if (has_diffuse) {
            Float t_i = lerp_gather(m_external_transmittance.data(), cos_theta_i,
                                    MTS_ROUGH_TRANSMITTANCE_RES, active),
                  t_o = lerp_gather(m_external_transmittance.data(), cos_theta_o,
                                    MTS_ROUGH_TRANSMITTANCE_RES, active);

            UnpolarizedSpectrum diff = m_diffuse_reflectance->eval(si, active);
            diff /= 1.f - (m_nonlinear ? (diff * m_internal_reflectance)
                                       : UnpolarizedSpectrum(m_internal_reflectance));

            result += diff * (math::InvPi<Float> * m_inv_eta_2 * cos_theta_o * t_i * t_o);
        }

        return select(active, unpolarized<Spectrum>(result), 0.f);
    }

    Float lerp_gather(const scalar_t<Float> *data, Float x, size_t size,
                      Mask active = true) const {
        using UInt = uint_array_t<Float>;
        x *= Float(size - 1);

        UInt index = min(UInt(x), scalar_t<UInt>(size - 2));

        Float v0 = gather<Float>(data, index, active),
              v1 = gather<Float>(data + 1, index, active);

        return lerp(v0, v1, x - Float(index));
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        bool has_specular = ctx.is_enabled(BSDFFlags::GlossyReflection, 0),
             has_diffuse = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely((!has_specular && !has_diffuse) || none_or<false>(active)))
            return 0.f;

        Float t_i = lerp_gather(m_external_transmittance.data(), cos_theta_i,
                                MTS_ROUGH_TRANSMITTANCE_RES, active);

        // Determine which component should be sampled
        Float prob_specular = (1.f - t_i) * m_specular_sampling_weight,
              prob_diffuse  = t_i * (1.f - m_specular_sampling_weight);

        if (unlikely(has_specular != has_diffuse))
            prob_specular = has_specular ? 1.f : 0.f;
        else
            prob_specular = prob_specular / (prob_specular + prob_diffuse);
        prob_diffuse = 1.f - prob_specular;

        Vector3f H = normalize(wo + si.wi);

        MicrofacetDistribution distr(m_type, m_alpha, m_sample_visible);
        Float result = 0.f;
        if (m_sample_visible)
            result = distr.eval(H) * distr.smith_g1(si.wi, H) /
                     (4.f * cos_theta_i);
        else
            result = distr.pdf(si.wi, H) / (4.f * dot(wo, H));
        result *= prob_specular;

        result += prob_diffuse * warp::square_to_cosine_hemisphere_pdf(wo);

        return result;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("alpha", m_alpha);
        callback->put_parameter("eta", m_eta);
        callback->put_object("diffuse_reflectance", m_diffuse_reflectance.get());
        if (m_specular_reflectance)
            callback->put_object("specular_reflectance", m_specular_reflectance.get());
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        // Compute inverse of eta squared
        m_inv_eta_2 = 1.f / (m_eta * m_eta);

        /* Compute weights that further steer samples towards
           the specular or diffuse components */
        ScalarFloat d_mean = m_diffuse_reflectance->mean(),
                    s_mean = 1.f;

        if (m_specular_reflectance)
            s_mean = m_specular_reflectance->mean();

        m_specular_sampling_weight = s_mean / (d_mean + s_mean);

        // Precompute rough reflectance (vectorized)
        if (keys.empty() || string::contains(keys, "alpha") || string::contains(keys, "eta")) {
            using FloatP    = Packet<ScalarFloat>;
            using Vector3fX = Vector<DynamicArray<FloatP>, 3>;

            mitsuba::MicrofacetDistribution<FloatP, Spectrum> distr_p(m_type, m_alpha);
            Vector3fX wi = zero<Vector3fX>(MTS_ROUGH_TRANSMITTANCE_RES);
            for (size_t i = 0; i < slices(wi); ++i) {
                ScalarFloat mu    = std::max((ScalarFloat) 1e-6f, ScalarFloat(i) / ScalarFloat(slices(wi) - 1));
                slice(wi, i) = ScalarVector3f(std::sqrt(1 - mu * mu), 0.f, mu);
            }

            auto external_transmittance = eval_transmittance(distr_p, wi, m_eta);
            m_external_transmittance = DynamicBuffer<Float>::copy(external_transmittance.data(),
                                                                  slices(external_transmittance));
            m_internal_reflectance =
                hmean(eval_reflectance(distr_p, wi, 1.f / m_eta) * wi.z()) * 2.f;
        }
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RoughPlastic[" << std::endl
            << "  distribution = " << m_type << "," << std::endl
            << "  sample_visible = "           << m_sample_visible                    << "," << std::endl
            << "  alpha = "                    << m_alpha                             << "," << std::endl
            << "  diffuse_reflectance = "      << m_diffuse_reflectance               << "," << std::endl;

        if (m_specular_reflectance)
            oss << "  specular_reflectance = "     << m_specular_reflectance              << "," << std::endl;

        oss << "  specular_sampling_weight = " << m_specular_sampling_weight          << "," << std::endl
            << "  eta = "                      << m_eta                               << "," << std::endl
            << "  nonlinear = "                << m_nonlinear                         << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Texture> m_diffuse_reflectance;
    ref<Texture> m_specular_reflectance;
    MicrofacetType m_type;
    ScalarFloat m_eta;
    ScalarFloat m_inv_eta_2;
    ScalarFloat m_alpha;
    ScalarFloat m_specular_sampling_weight;
    bool m_nonlinear;
    bool m_sample_visible;
    DynamicBuffer<Float> m_external_transmittance;
    ScalarFloat m_internal_reflectance;
};

MTS_IMPLEMENT_CLASS_VARIANT(RoughPlastic, BSDF);
MTS_EXPORT_PLUGIN(RoughPlastic, "Rough plastic");
NAMESPACE_END(mitsuba)
