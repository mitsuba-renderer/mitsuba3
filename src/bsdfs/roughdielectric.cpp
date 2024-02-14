#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-roughdielectric:

Rough dielectric material (:monosp:`roughdielectric`)
-----------------------------------------------------

.. pluginparameters::
 :extra-rows: 6

 * - int_ior
   - |float| or |string|
   - Interior index of refraction specified numerically or using a known material name. (Default: bk7 / 1.5046)

 * - ext_ior
   - |float| or |string|
   - Exterior index of refraction specified numerically or using a known material name.  (Default: air / 1.000277)

 * - specular_reflectance, specular_transmittance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular reflection/transmission components.
     Note that for physical realism, these parameters should never be touched. (Default: 1.0)
   - |exposed|, |differentiable|

 * - distribution
   - |string|
   - Specifies the type of microfacet normal distribution used to model the surface roughness.

     - :monosp:`beckmann`: Physically-based distribution derived from Gaussian random surfaces.
       This is the default.
     - :monosp:`ggx`: The GGX :cite:`Walter07Microfacet` distribution (also known as Trowbridge-Reitz
       :cite:`Trowbridge19975Average` distribution) was designed to better approximate the long
       tails observed in measurements of ground surfaces, which are not modeled by the Beckmann
       distribution.

 * - alpha, alpha_u, alpha_v
   - |texture| or |float|
   - Specifies the roughness of the unresolved surface micro-geometry along the tangent and
     bitangent directions. When the Beckmann distribution is used, this parameter is equal to the
     *root mean square* (RMS) slope of the microfacets. :monosp:`alpha` is a convenience
     parameter to initialize both :monosp:`alpha_u` and :monosp:`alpha_v` to the same value. (Default: 0.1)
   - |exposed|, |differentiable|, |discontinuous|

 * - sample_visible
   - |bool|
   - Enables a sampling technique proposed by Heitz and D'Eon :cite:`Heitz1014Importance`, which
     focuses computation on the visible parts of the microfacet normal distribution, considerably
     reducing variance in some cases. (Default: |true|, i.e. use visible normal sampling)

 * - eta
   - |float|
   - Relative index of refraction from the exterior to the interior
   - |exposed|, |differentiable|, |discontinuous|

This plugin implements a realistic microfacet scattering model for rendering
rough interfaces between dielectric materials, such as a transition from air to
ground glass. Microfacet theory describes rough surfaces as an arrangement of
unresolved and ideally specular facets, whose normal directions are given by
a specially chosen *microfacet distribution*. By accounting for shadowing
and masking effects between these facets, it is possible to reproduce the important
off-specular reflections peaks observed in real-world measurements of such
materials.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_roughdielectric_glass.jpg
   :caption: Anti-glare glass (Beckmann, :math:`\alpha=0.02`)
.. subfigure:: ../../resources/data/docs/images/render/bsdf_roughdielectric_rough.jpg
    :caption: Rough glass (Beckmann, :math:`\alpha=0.1`)
.. subfigure:: ../../resources/data/docs/images/render/bsdf_roughdielectric_textured.jpg
    :caption: Rough glass with textured alpha
.. subfigend::
    :label: fig-bsdf-roughdielectric

This plugin is essentially the *roughened* equivalent of the (smooth) plugin
:ref:`dielectric <bsdf-dielectric>`. For very low values of :math:`\alpha`, the two will
be identical, though scenes using this plugin will take longer to render
due to the additional computational burden of tracking surface roughness.

The implementation is based on the paper *Microfacet Models
for Refraction through Rough Surfaces* by Walter et al.
:cite:`Walter07Microfacet` and supports two different types of microfacet
distributions. Exterior and interior IOR values can be specified independently,
where *exterior* refers to the side that contains the surface normal. Similar to the
:ref:`dielectric <bsdf-dielectric>` plugin, IOR values can either be specified
numerically, or based on a list of known materials (see the
corresponding table in the :ref:`dielectric <bsdf-dielectric>` reference).
When no parameters are given, the plugin activates the default settings,
which describe a borosilicate glass (BK7) ↔ air interface with a light amount of
roughness modeled using a Beckmann distribution.

To get an intuition about the effect of the surface roughness parameter
:math:`\alpha`, consider the following approximate classification: a value of
:math:`\alpha=0.001-0.01` corresponds to a material with slight imperfections
on an otherwise smooth surface finish, :math:`\alpha=0.1` is relatively rough,
and :math:`\alpha=0.3-0.7` is **extremely** rough (e.g. an etched or ground
finish). Values significantly above that are probably not too realistic.

Please note that when using this plugin, it is crucial that the scene contains
meaningful and mutually compatible index of refraction changes---see the
section about :ref:`correctness considerations <bsdf-correctness>` for a
description of what this entails.

The following XML snippet describes a material definition for rough glass:

.. tabs::
    .. code-tab:: xml
        :name: roughdielectric-roughglass

        <bsdf type="roughdielectric">
            <string name="distribution" value="beckmann"/>
            <float name="alpha" value="0.1"/>
            <string name="int_ior" value="bk7"/>
            <string name="ext_ior" value="air"/>
        </bsdf>

    .. code-tab:: python

        'type': 'roughdielectric',
        'distribution': 'beckmann',
        'alpha': 0.1,
        'int_ior': 'bk7',
        'ext_ior': 'air'

Technical details
*****************

All microfacet distributions allow the specification of two distinct
roughness values along the tangent and bitangent directions. This can be
used to provide a material with a *brushed* appearance. The alignment
of the anisotropy will follow the UV parameterization of the underlying
mesh. This means that such an anisotropic material cannot be applied to
triangle meshes that are missing texture coordinates.

Since Mitsuba 0.5.1, this plugin uses a new importance sampling technique
contributed by Eric Heitz and Eugene D'Eon, which restricts the sampling
domain to the set of visible (unmasked) microfacet normals. The previous
approach of sampling all normals is still available and can be enabled
by setting :monosp:`sample_visible` to |false|. However this will lead
to significantly slower convergence.

 */

template <typename Float, typename Spectrum>
class RoughDielectric final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture, MicrofacetDistribution)

    RoughDielectric(const Properties &props) : Base(props) {
        if (props.has_property("specular_reflectance"))
            m_specular_reflectance   = props.texture<Texture>("specular_reflectance", 1.f);

        if (props.has_property("specular_transmittance"))
            m_specular_transmittance = props.texture<Texture>("specular_transmittance", 1.f);

        // Specifies the internal index of refraction at the interface
        ScalarFloat int_ior = lookup_ior(props, "int_ior", "bk7");

        // Specifies the external index of refraction at the interface
        ScalarFloat ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.f || ext_ior < 0.f || int_ior == ext_ior)
            Throw("The interior and exterior indices of "
                  "refraction must be positive and differ!");

        m_eta = int_ior / ext_ior;
        m_inv_eta = ext_ior / int_ior;

        if (props.has_property("distribution")) {
            std::string distr = string::to_lower(props.string("distribution"));
            if (distr == "beckmann")
                m_type = MicrofacetType::Beckmann;
            else if (distr == "ggx")
                m_type = MicrofacetType::GGX;
            else
                Throw("Specified an invalid distribution \"%s\", must be "
                      "\"beckmann\" or \"ggx\"!", distr.c_str());
        } else {
            m_type = MicrofacetType::Beckmann;
        }

        m_sample_visible = props.get<bool>("sample_visible", true);

        if (props.has_property("alpha_u") || props.has_property("alpha_v")) {
            if (!props.has_property("alpha_u") || !props.has_property("alpha_v"))
                Throw("Microfacet model: both 'alpha_u' and 'alpha_v' must be specified.");
            if (props.has_property("alpha"))
                Throw("Microfacet model: please specify"
                      "either 'alpha' or 'alpha_u'/'alpha_v'.");
            m_alpha_u = props.texture<Texture>("alpha_u");
            m_alpha_v = props.texture<Texture>("alpha_v");
        } else {
            m_alpha_u = m_alpha_v = props.texture<Texture>("alpha", 0.1f);
        }

        BSDFFlags extra = (m_alpha_u != m_alpha_v) ? BSDFFlags::Anisotropic : BSDFFlags(0);
        m_components.push_back(BSDFFlags::GlossyReflection | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide | extra);
        m_components.push_back(BSDFFlags::GlossyTransmission | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide | BSDFFlags::NonSymmetric | extra);
        m_flags = m_components[0] | m_components[1];

        parameters_changed();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("eta", m_eta, ParamFlags::Differentiable | ParamFlags::Discontinuous);

        if (!has_flag(m_flags, BSDFFlags::Anisotropic))
            callback->put_object("alpha",                  m_alpha_u.get(),                ParamFlags::Differentiable | ParamFlags::Discontinuous);
        else {
            callback->put_object("alpha_u",                m_alpha_u.get(),                ParamFlags::Differentiable | ParamFlags::Discontinuous);
            callback->put_object("alpha_v",                m_alpha_v.get(),                ParamFlags::Differentiable | ParamFlags::Discontinuous);
        }
        if (m_specular_reflectance)
            callback->put_object("specular_reflectance",   m_specular_reflectance.get(),   +ParamFlags::Differentiable);
        if (m_specular_transmittance)
            callback->put_object("specular_transmittance", m_specular_transmittance.get(), +ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        m_inv_eta = dr::rcp(m_eta);
        dr::make_opaque(m_eta, m_inv_eta);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        // Determine the type of interaction
        bool has_reflection    = ctx.is_enabled(BSDFFlags::GlossyReflection, 0),
             has_transmission  = ctx.is_enabled(BSDFFlags::GlossyTransmission, 1);

        BSDFSample3f bs = dr::zeros<BSDFSample3f>();

        Float cos_theta_i = Frame3f::cos_theta(si.wi);

        // Ignore perfectly grazing configurations
        active &= cos_theta_i != 0.f;

        /* Construct the microfacet distribution matching the roughness values at the current surface position. */
        MicrofacetDistribution distr(m_type,
                                     m_alpha_u->eval_1(si, active),
                                     m_alpha_v->eval_1(si, active),
                                     m_sample_visible);

        /* Trick by Walter et al.: slightly scale the roughness values to
           reduce importance sampling weights. Not needed for the
           Heitz and D'Eon sampling technique. */
        MicrofacetDistribution sample_distr(distr);
        if (unlikely(!m_sample_visible))
            sample_distr.scale_alpha(1.2f - .2f * dr::sqrt(dr::abs(cos_theta_i)));

        // Sample the microfacet normal
        Normal3f m;
        std::tie(m, bs.pdf) =
            sample_distr.sample(dr::mulsign(si.wi, cos_theta_i), sample2);
        active &= bs.pdf != 0.f;

        auto [F, cos_theta_t, eta_it, eta_ti] =
            fresnel(dr::dot(si.wi, m), m_eta);

        // Select the lobe to be sampled
        UnpolarizedSpectrum weight;
        Mask selected_r, selected_t;
        if (likely(has_reflection && has_transmission)) {
            selected_r = sample1 <= F && active;
            weight = 1.f;
            /* For differentiable variants, lobe choice has to be detached to avoid bias.
                Sampling weights should be computed accordingly. */
            if constexpr (dr::is_diff_v<Float>) {
                if (dr::grad_enabled(F)) {
                    Float r_diff = dr::replace_grad(Float(1.f), F / dr::detach(F));
                    Float t_diff = dr::replace_grad(Float(1.f), (1.f - F) / (1.f - dr::detach(F)));
                    weight = dr::select(selected_r, r_diff, t_diff);
                }
            }
            bs.pdf *= dr::detach(dr::select(selected_r, F, 1.f - F));
        } else {
            if (has_reflection || has_transmission) {
                selected_r = Mask(has_reflection) && active;
                weight = has_reflection ? F : (1.f - F);
            } else {
                return { bs, 0.f };
            }
        }

        selected_t = !selected_r && active;

        bs.eta               = dr::select(selected_r, Float(1.f), eta_it);
        bs.sampled_component = dr::select(selected_r, UInt32(0), UInt32(1));
        bs.sampled_type      = dr::select(selected_r,
                                      UInt32(+BSDFFlags::GlossyReflection),
                                      UInt32(+BSDFFlags::GlossyTransmission));

        Float dwh_dwo = 0.f;

        // Reflection sampling
        if (dr::any_or<true>(selected_r)) {
            // Perfect specular reflection based on the microfacet normal
            bs.wo[selected_r] = reflect(si.wi, m);

            if (m_specular_reflectance)
                weight[selected_r] *= m_specular_reflectance->eval(si, selected_r);

            // Jacobian of the half-direction mapping
            dwh_dwo = dr::rcp(4.f * dr::dot(bs.wo, m));
        }

        // Transmission sampling
        if (dr::any_or<true>(selected_t)) {
            // Perfect specular transmission based on the microfacet normal
            bs.wo[selected_t]  = refract(si.wi, m, cos_theta_t, eta_ti);

            /* For transmission, radiance must be scaled to account for the solid
               angle compression that occurs when crossing the interface. */
            UnpolarizedSpectrum factor = (ctx.mode == TransportMode::Radiance) ? dr::square(eta_ti) : Float(1.f);

            if (m_specular_transmittance)
                factor *= m_specular_transmittance->eval(si, selected_t);

            weight[selected_t] *= factor;

            // Jacobian of the half-direction mapping
            dr::masked(dwh_dwo, selected_t) =
                (dr::square(bs.eta) * dr::dot(bs.wo, m)) /
                 dr::square(dr::dot(si.wi, m) + bs.eta * dr::dot(bs.wo, m));
        }

        if (likely(m_sample_visible))
            weight *= distr.smith_g1(bs.wo, m);
        else
            weight *= distr.G(si.wi, bs.wo, m) * dr::dot(si.wi, m) /
                      (cos_theta_i * Frame3f::cos_theta(m));

        bs.pdf *= dr::abs(dwh_dwo);

        return { bs, depolarizer<Spectrum>(weight) & active };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        // Ignore perfectly grazing configurations
        active &= cos_theta_i != 0.f;

        // Determine the type of interaction
        bool has_reflection   = ctx.is_enabled(BSDFFlags::GlossyReflection, 0),
             has_transmission = ctx.is_enabled(BSDFFlags::GlossyTransmission, 1);

        Mask reflect = cos_theta_i * cos_theta_o > 0.f;

        // Determine the relative index of refraction
        Float eta     = dr::select(cos_theta_i > 0.f, m_eta, m_inv_eta),
              inv_eta = dr::select(cos_theta_i > 0.f, m_inv_eta, m_eta);

        // Compute the half-vector
        Vector3f m = dr::normalize(si.wi + wo * dr::select(reflect, Float(1.f), eta));

        // Ensure that the half-vector points into the same hemisphere as the macrosurface normal
        m = dr::mulsign(m, Frame3f::cos_theta(m));

        /* Construct the microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution distr(m_type,
                                     m_alpha_u->eval_1(si, active),
                                     m_alpha_v->eval_1(si, active),
                                     m_sample_visible);

        // Evaluate the microfacet normal distribution
        Float D = distr.eval(m);

        // Fresnel factor
        Float F = std::get<0>(fresnel(dr::dot(si.wi, m), m_eta));

        // Smith's shadow-masking function
        Float G = distr.G(si.wi, wo, m);

        UnpolarizedSpectrum result(0.f);

        Mask eval_r = Mask(has_reflection) && reflect && active,
             eval_t = Mask(has_transmission) && !reflect && active;

        if (dr::any_or<true>(eval_r)) {
            UnpolarizedSpectrum value = F * D * G / (4.f * dr::abs(cos_theta_i));

            if (m_specular_reflectance)
                value *= m_specular_reflectance->eval(si, eval_r);

            result[eval_r] = value;
        }

        if (dr::any_or<true>(eval_t)) {
            /* Missing term in the original paper: account for the solid angle
               compression when tracing radiance -- this is necessary for
               bidirectional methods. */
            Float scale = (ctx.mode == TransportMode::Radiance) ? dr::square(inv_eta) : Float(1.f);

            // Compute the total amount of transmission
            UnpolarizedSpectrum value = dr::abs(
                (scale * (1.f - F) * D * G * eta * eta * dr::dot(si.wi, m) * dr::dot(wo, m)) /
                (cos_theta_i * dr::square(dr::dot(si.wi, m) + eta * dr::dot(wo, m))));

            if (m_specular_transmittance)
                value *= m_specular_transmittance->eval(si, eval_t);

            result[eval_t] = value;
        }

        return depolarizer<Spectrum>(result);
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        // Ignore perfectly grazing configurations
        active &= cos_theta_i != 0.f;

        // Determine the type of interaction
        bool has_reflection   = ctx.is_enabled(BSDFFlags::GlossyReflection, 0),
             has_transmission = ctx.is_enabled(BSDFFlags::GlossyTransmission, 1);

        Mask reflect = cos_theta_i * cos_theta_o > 0.f;
        active &= (Mask(has_reflection)   &&  reflect) ||
                  (Mask(has_transmission) && !reflect);

        // Determine the relative index of refraction
        Float eta = dr::select(cos_theta_i > 0.f, m_eta, m_inv_eta);

        // Compute the half-vector
        Vector3f m = dr::normalize(si.wi + wo * dr::select(reflect, Float(1.f), eta));

        // Ensure that the half-vector points into the same hemisphere as the macrosurface normal
        m = dr::mulsign(m, Frame3f::cos_theta(m));

        /* Filter cases where the micro/macro-surface don't agree on the side.
           This logic is evaluated in smith_g1() called as part of the eval()
           and sample() methods and needs to be replicated in the probability
           density computation as well. */
        active &= dr::dot(si.wi, m) * Frame3f::cos_theta(si.wi) > 0.f &&
                  dr::dot(wo,    m) * Frame3f::cos_theta(wo)    > 0.f;

        // Jacobian of the half-direction mapping
        Float dwh_dwo = dr::select(reflect, dr::rcp(4.f * dr::dot(wo, m)),
                               (eta * eta * dr::dot(wo, m)) /
                                   dr::square(dr::dot(si.wi, m) + eta * dr::dot(wo, m)));

        /* Construct the microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution sample_distr(
            m_type,
            m_alpha_u->eval_1(si, active),
            m_alpha_v->eval_1(si, active),
            m_sample_visible
        );

        /* Trick by Walter et al.: slightly scale the roughness values to
           reduce importance sampling weights. Not needed for the
           Heitz and D'Eon sampling technique. */
        if (unlikely(!m_sample_visible))
            sample_distr.scale_alpha(1.2f - .2f * dr::sqrt(dr::abs(Frame3f::cos_theta(si.wi))));

        // Evaluate the microfacet model sampling density function
        Float prob = sample_distr.pdf(dr::mulsign(si.wi, Frame3f::cos_theta(si.wi)), m);

        if (likely(has_transmission && has_reflection)) {
            Float F = std::get<0>(fresnel(dr::dot(si.wi, m), m_eta));
            prob *= dr::select(reflect, F, 1.f - F);
        }

        return dr::select(active, prob * dr::abs(dwh_dwo), 0.f);
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        // Ignore perfectly grazing configurations
        active &= cos_theta_i != 0.f;

        // Determine the type of interaction
        bool has_reflection   = ctx.is_enabled(BSDFFlags::GlossyReflection, 0),
             has_transmission = ctx.is_enabled(BSDFFlags::GlossyTransmission, 1);

        Mask reflect = cos_theta_i * cos_theta_o > 0.f;

        active &= (Mask(has_reflection)   &&  reflect) ||
                  (Mask(has_transmission) && !reflect);

        // Determine the relative index of refraction
        Float eta     = dr::select(cos_theta_i > 0.f, m_eta, m_inv_eta),
              inv_eta = dr::select(cos_theta_i > 0.f, m_inv_eta, m_eta);

        // Compute the half-vector
        Vector3f m = dr::normalize(si.wi + wo * dr::select(reflect, Float(1.f), eta));

        // Ensure that the half-vector points into the same hemisphere as the macrosurface normal
        m = dr::mulsign(m, Frame3f::cos_theta(m));

        Float dot_wi_m = dr::dot(si.wi, m),
              dot_wo_m = dr::dot(wo   , m);

        /* Filter cases where the micro/macro-surface don't agree on the side.
           This logic is evaluated in smith_g1() called as part of the eval()
           and sample() methods and needs to be replicated in the probability
           density computation as well. */
        active &= dot_wi_m * cos_theta_i > 0.f &&
                  dot_wo_m * cos_theta_o > 0.f;

        /* Construct the microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution distr(m_type,
                                     m_alpha_u->eval_1(si, active),
                                     m_alpha_v->eval_1(si, active),
                                     m_sample_visible);

        // Evaluate the microfacet normal distribution
        Float D = distr.eval(m);

        // Fresnel factor
        Float F = std::get<0>(fresnel(dot_wi_m, m_eta));

        // Smith's shadow-masking function
        Float G = distr.G(si.wi, wo, m);

        UnpolarizedSpectrum result(0.f);

        Mask eval_r = Mask(has_reflection) && reflect && active,
             eval_t = Mask(has_transmission) && !reflect && active;

        if (dr::any_or<true>(eval_r)) {
            UnpolarizedSpectrum value = F * D * G / (4.f * dr::abs(cos_theta_i));

            if (m_specular_reflectance)
                value *= m_specular_reflectance->eval(si, eval_r);

            result[eval_r] = value;
        }

        if (dr::any_or<true>(eval_t)) {
            /* Missing term in the original paper: account for the solid angle
               compression when tracing radiance -- this is necessary for
               bidirectional methods. */
            Float scale = (ctx.mode == TransportMode::Radiance) ? dr::square(inv_eta) : Float(1.f);

            // Compute the total amount of transmission
            UnpolarizedSpectrum value = dr::abs(
                (scale * (1.f - F) * D * G * eta * eta * dot_wi_m * dot_wo_m) /
                (cos_theta_i * dr::square(dot_wi_m + eta * dot_wo_m)));

            if (m_specular_transmittance)
                value *= m_specular_transmittance->eval(si, eval_t);

            result[eval_t] = value;
        }

        /* Trick by Walter et al.: slightly scale the roughness values to
           reduce importance sampling weights. Not needed for the
           Heitz and D'Eon sampling technique. */
        if (unlikely(!m_sample_visible))
            distr.scale_alpha(1.2f - .2f * dr::sqrt(dr::abs(cos_theta_i)));

        // Evaluate the microfacet model sampling density function
        Float pdf = distr.pdf(dr::mulsign(si.wi, cos_theta_i), m);

        if (likely(has_transmission && has_reflection))
            pdf *= dr::select(reflect, F, 1.f - F);

        // Jacobian of the half-direction mapping
        Float dwh_dwo = dr::select(reflect, dr::rcp(4.f * dot_wo_m),
                                   (eta * eta * dot_wo_m) /
                                       dr::square(dot_wi_m + eta * dot_wo_m));

        return { depolarizer<Spectrum>(result),
                 dr::select(active, pdf * dr::abs(dwh_dwo), 0.f) };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RoughDielectric[" << std::endl
            << "  distribution = "           << m_type           << "," << std::endl
            << "  sample_visible = "         << (int) m_sample_visible << "," << std::endl;

        if (!has_flag(m_flags, BSDFFlags::Anisotropic)) {
            oss << "  alpha = "                  << string::indent(m_alpha_v) << "," << std::endl;
        } else {
            oss << "  alpha_u = "                << string::indent(m_alpha_u) << "," << std::endl
                << "  alpha_v = "                << string::indent(m_alpha_v) << "," << std::endl;
        }

        if (m_specular_reflectance)
            oss << "  specular_reflectance = "   << string::indent(m_specular_reflectance) << "," << std::endl;

        if (m_specular_transmittance)
            oss << "  specular_transmittance = " << string::indent(m_specular_transmittance) << ", " << std::endl;

        oss << "  eta = "                    << m_eta << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    ref<Texture> m_specular_reflectance;
    ref<Texture> m_specular_transmittance;
    MicrofacetType m_type;
    ref<Texture> m_alpha_u, m_alpha_v;
    Float m_eta, m_inv_eta;
    bool m_sample_visible;
};

MI_IMPLEMENT_CLASS_VARIANT(RoughDielectric, BSDF)
MI_EXPORT_PLUGIN(RoughDielectric, "Rough dielectric")
NAMESPACE_END(mitsuba)
