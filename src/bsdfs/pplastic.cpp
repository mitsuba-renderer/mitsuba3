#include <mitsuba/core/string.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!
.. _bsdf-pplastic:

Polarized plastic material (:monosp:`pplastic`)
--------------------------------------------------------------------------

.. pluginparameters::
 :extra-rows: 7

 * - diffuse_reflectance
   - |spectrum| or |texture|
   - Optional factor used to modulate the diffuse reflection component. (Default: 0.5)
   - |exposed|, |differentiable|

 * - specular_reflectance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular reflection component.
     Note that for physical realism, this parameter should never be touched. (Default: 1.0)
   - |exposed|, |differentiable|

 * - int_ior
   - |float| or |string|
   - Interior index of refraction specified numerically or using a known material name. (Default: polypropylene / 1.49)

 * - ext_ior
   - |float| or |string|
   - Exterior index of refraction specified numerically or using a known material name.  (Default: air / 1.000277)

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

This plugin implements a scattering model that combines diffuse and specular reflection
where both components can interact with polarized light. This is based on the pBRDF proposed in
"Simultaneous Acquisition of Polarimetric SVBRDF and Normals" by Baek et al. 2018 (:cite:`Baek2018Simultaneous`).

.. figure:: ../../resources/data/docs/images/render/bsdf_pplastic.jpg
        :alt: Polarized plastic
        :width: 75%
        :align: center

Apart from the polarization support, this is similar to the :ref:`plastic <bsdf-plastic>` and :ref:`roughplastic <bsdf-roughplastic>`
plugins. There, the interaction of light with a diffuse base surface coated by a (potentially rough) thin
dielectric layer is used as a way of combining the two components, whereas here the two are added in
a more ad-hoc way:

1. The specular component is a standard rough reflection from a microfacet model.

2. The diffuse Lambert component is attenuated by a smooth refraction into and out of the material where
   conceptually some subsurface scattering occurs in between that causes the light to escape in a diffused
   way.

This is illustrated in the following diagram:

.. figure:: ../../resources/data/docs/images/bsdf/pplastic.svg
        :alt: Polarized plastic diagram
        :width: 75%
        :align: center

The intensity of the rough reflection is always less than the light lost by the two refractions which means the
addition of these components does not result in any extra energy. However, it is also not energy conserving.

What makes this plugin particularly interesting is that both components account for the polarization state of
light when it interacts with the material. For applications without the need of polarization support, it is
recommended to stick to the standard :ref:`plastic <bsdf-plastic>` and :ref:`roughplastic <bsdf-roughplastic>`
plugins.

See the following images of the two components rendered individually together with false-color visualizations
of the resulting ":math:`\mathbf{s}_1`" Stokes vector output that encodes horizontal vs. vertical linear polarization.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_pplastic_specular.jpg
   :caption: Specular component
   :label: fig-pplastic_specular
.. subfigure:: ../../resources/data/docs/images/render/bsdf_pplastic_diffuse.jpg
   :caption: Diffuse component
   :label: fig-pplastic_diffuse
.. subfigure:: ../../resources/data/docs/images/render/bsdf_pplastic_specular_stokes_s1.jpg
   :caption: ":math:`\mathbf{s}_1`" for the specular component
   :label: fig-pplastic_specular_stokes
.. subfigure:: ../../resources/data/docs/images/render/bsdf_pplastic_diffuse_stokes_s1.jpg
   :caption: ":math:`\mathbf{s}_1`" for the diffuse component
   :label: fig-pplastic_diffuse_stokes
.. subfigend::
    :label: fig-bsdf-pplastic_components

Note how the diffuse polarization is comparatively weak and has its orientation flipped by 90 degrees.
This is a property that is commonly exploited in *shape from polarization* applications (:cite:`Kadambi2017`).

The following XML snippet describes the purple material from the test scene above:

.. tabs::
    .. code-tab:: xml
        :name: lst-pplastic

        <bsdf type="pplastic">
            <rgb name="diffuse_reflectance" value="0.05, 0.03, 0.1"/>
            <float name="alpha" value="0.06"/>
        </bsdf>

    .. code-tab:: python

        'type': 'pplastic',
        'diffuse_reflectance': {
            'type': 'rgb',
            'value': [0.05, 0.03, 0.1]
        },
        'alpha': 0.06
 */

template <typename Float, typename Spectrum>
class PolarizedPlastic final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture, MicrofacetDistribution)

    PolarizedPlastic(const Properties &props) : Base(props) {
        m_diffuse_reflectance  = props.texture<Texture>("diffuse_reflectance",  .5f);

        if (props.has_property("specular_reflectance"))
            m_specular_reflectance = props.texture<Texture>("specular_reflectance", 1.f);

        /// Specifies the internal index of refraction at the interface
        ScalarFloat int_ior = lookup_ior(props, "int_ior", "polypropylene");

        /// Specifies the external index of refraction at the interface
        ScalarFloat ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.f || ext_ior < 0.f || int_ior == ext_ior)
            Throw("The interior and exterior indices of "
                  "refraction must be positive and differ!");

        m_eta = int_ior / ext_ior;

        mitsuba::MicrofacetDistribution<ScalarFloat, Spectrum> distr(props);
        m_type = distr.type();
        m_sample_visible = distr.sample_visible();

        m_alpha_u = distr.alpha_u();
        m_alpha_v = distr.alpha_v();

        m_flags = BSDFFlags::GlossyReflection | BSDFFlags::DiffuseReflection;
        if (dr::all(m_alpha_u != m_alpha_v))
            m_flags = m_flags | BSDFFlags::Anisotropic;
        m_flags = m_flags | BSDFFlags::FrontSide;

        m_components.clear();
        m_components.push_back(m_flags);

        parameters_changed();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("diffuse_reflectance", m_diffuse_reflectance.get(), +ParamFlags::Differentiable);
        callback->put_parameter("eta",              m_eta,                       ParamFlags::Differentiable | ParamFlags::Discontinuous);

        if (m_specular_reflectance)
            callback->put_object("specular_reflectance", m_specular_reflectance.get(), +ParamFlags::Differentiable);
        if (!has_flag(m_flags, BSDFFlags::Anisotropic))
            callback->put_parameter("alpha",             m_alpha_u,                    ParamFlags::Differentiable | ParamFlags::Discontinuous);
        else {
            callback->put_parameter("alpha_u",           m_alpha_u,                    ParamFlags::Differentiable | ParamFlags::Discontinuous);
            callback->put_parameter("alpha_v",           m_alpha_v,                    ParamFlags::Differentiable | ParamFlags::Discontinuous);
        }
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        /* Compute weights that further steer samples towards
           the specular or diffuse components */
        Float d_mean = m_diffuse_reflectance->mean(),
              s_mean = 1.f;

        if (m_specular_reflectance)
            s_mean = m_specular_reflectance->mean();

        m_specular_sampling_weight = s_mean / (d_mean + s_mean);

        dr::make_opaque(m_eta, m_alpha_u, m_alpha_v, m_specular_sampling_weight);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        bool has_specular = ctx.is_enabled(BSDFFlags::GlossyReflection, 0),
             has_diffuse  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        BSDFSample3f bs = dr::zeros<BSDFSample3f>();
        if (unlikely((!has_specular && !has_diffuse) || dr::none_or<false>(active)))
            return { bs, 0.f };

        Float prob_specular = m_specular_sampling_weight;
        if (unlikely(has_specular != has_diffuse))
            prob_specular = has_specular ? 1.f : 0.f;

        Mask sample_specular = active && (sample1 < prob_specular),
             sample_diffuse  = active && !sample_specular;

        bs.eta = 1.f;

        if (dr::any_or<true>(sample_specular)) {
            MicrofacetDistribution distr(m_type, m_alpha_u, m_alpha_v, m_sample_visible);
            Normal3f m = std::get<0>(distr.sample(si.wi, sample2));

            dr::masked(bs.wo, sample_specular) = reflect(si.wi, m);
            dr::masked(bs.sampled_component, sample_specular) = 0;
            dr::masked(bs.sampled_type, sample_specular) = +BSDFFlags::GlossyReflection;
        }

        if (dr::any_or<true>(sample_diffuse)) {
            dr::masked(bs.wo, sample_diffuse) = warp::square_to_cosine_hemisphere(sample2);
            dr::masked(bs.sampled_component, sample_diffuse) = 1;
            dr::masked(bs.sampled_type, sample_diffuse) = +BSDFFlags::DiffuseReflection;
        }

        bs.pdf = pdf(ctx, si, bs.wo, active);
        active &= bs.pdf > 0.f;
        Spectrum result = eval(ctx, si, bs.wo, active);

        return { bs, result / bs.pdf & active };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        bool has_specular = ctx.is_enabled(BSDFFlags::GlossyReflection, 0),
             has_diffuse  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;
        if (unlikely((!has_specular && !has_diffuse) || dr::none_or<false>(active)))
            return 0.f;

        Spectrum result = 0.f;

        if constexpr (is_polarized_v<Spectrum>) {
            /* Due to the coordinate system rotations for polarization-aware
               pBSDFs below we need to know the propagation direction of light.
               In the following, light arrives along `-wo_hat` and leaves along
               `+wi_hat`. */
            Vector3f wo_hat = ctx.mode == TransportMode::Radiance ? wo : si.wi,
                     wi_hat = ctx.mode == TransportMode::Radiance ? si.wi : wo;

            if (has_specular) {
                MicrofacetDistribution distr(m_type, m_alpha_u, m_alpha_v, m_sample_visible);
                Vector3f H = dr::normalize(wo + si.wi);
                Float D = distr.eval(H);

                // Mueller matrix for specular reflection.
                Spectrum F = mueller::specular_reflection(dot(wo_hat, H), m_eta);

                /* The Stokes reference frame vector of this matrix lies perpendicular
                   to the plane of reflection. */
                Vector3f s_axis_in  = dr::cross(H, -wo_hat);
                Vector3f s_axis_out = dr::cross(H, wi_hat);

                // Singularity when the input & output are collinear with the normal
                Mask collinear = dr::all(s_axis_in == Vector3f(0));
                s_axis_in  = dr::select(collinear, Vector3f(1, 0, 0),
                                                   dr::normalize(s_axis_in));
                s_axis_out = dr::select(collinear, Vector3f(1, 0, 0),
                                                   dr::normalize(s_axis_out));

                /* Rotate in/out reference vector of F s.t. it aligns with the implicit
                   Stokes bases of -wo_hat & wi_hat. */
                F = mueller::rotate_mueller_basis(F,
                                                  -wo_hat, s_axis_in,  mueller::stokes_basis(-wo_hat),
                                                   wi_hat, s_axis_out, mueller::stokes_basis(wi_hat));

                Float G = distr.G(si.wi, wo, H);
                Float value = D * G / (4.f * cos_theta_i);

                UnpolarizedSpectrum spec = m_specular_reflectance ? m_specular_reflectance->eval(si, active)
                                                                  : 1.f;
                result += spec * F * value;
            }

            if (has_diffuse) {
                /* Diffuse scattering is modeled a a sequence of events:
                   1) Specular refraction inside
                   2) Subsurface scattering
                   3) Specular refraction outside again
                   where both refractions reduce the energy of the diffuse component.
                   The refraction to the outside will introduce some polarization. */

                // Refract inside
                Spectrum To = mueller::specular_transmission(dr::abs(Frame3f::cos_theta(wo_hat)), m_eta);

                // Diffuse subsurface scattering that acts as a depolarizer.
                Spectrum diff = depolarizer<Spectrum>(m_diffuse_reflectance->eval(si, active));

                // Refract outside
                Normal3f n(0.f, 0.f, 1.f);
                Float inv_eta = dr::rcp(m_eta);
                Float cos_theta_i_hat = ctx.mode == TransportMode::Radiance ? cos_theta_i : cos_theta_o;
                Float cos_theta_t_i = std::get<1>(fresnel(cos_theta_i_hat, m_eta));
                Vector3f wi_hat_p = -refract(wi_hat, cos_theta_t_i, inv_eta);
                Spectrum Ti = mueller::specular_transmission(dr::abs(Frame3f::cos_theta(wi_hat_p)), inv_eta);

                diff = Ti * diff * To;

                /* The Stokes reference frame vector of `diff` lies perpendicular
                   to the plane of reflection. */
                Vector3f s_axis_in  = dr::cross(n, -wo_hat);
                Vector3f s_axis_out = dr::cross(n, wi_hat);

                auto handle_singularity = [](const Vector3f &basis) {
                    // Arbitrarily pick a perpendicular direction if the
                    // reflection plane is ill-defined
                    Vector3f output_basis(basis);
                    Mask collinear = dr::all(basis == Vector3f(0));
                    dr::masked(output_basis, collinear) = Vector3f(1, 0, 0);
                    return output_basis;
                };
                s_axis_in = dr::normalize(handle_singularity(s_axis_in));
                s_axis_out = dr::normalize(handle_singularity(s_axis_out));

                /* Rotate in/out reference vector of `diff` s.t. it aligns with the
                   implicit Stokes bases of -wo_hat & wi_hat. */
                diff = mueller::rotate_mueller_basis(diff,
                                                      -wo_hat, s_axis_in,  mueller::stokes_basis(-wo_hat),
                                                       wi_hat, s_axis_out, mueller::stokes_basis(wi_hat));

                result += diff * dr::InvPi<Float> * cos_theta_o;
            }
        } else {
            if (has_specular) {
                MicrofacetDistribution distr(m_type, m_alpha_u, m_alpha_v, m_sample_visible);
                Vector3f H = dr::normalize(wo + si.wi);
                Float D = distr.eval(H);

                Spectrum F = std::get<0>(fresnel(dot(si.wi, H), m_eta));
                Float G = distr.G(si.wi, wo, H);
                Float value = D * G / (4.f * cos_theta_i);

                UnpolarizedSpectrum spec = m_specular_reflectance ? m_specular_reflectance->eval(si, active)
                                                                  : 1.f;
                result += spec * F * value;
            }

            if (has_diffuse) {
                UnpolarizedSpectrum diff = m_diffuse_reflectance->eval(si, active);
                /* Diffuse scattering is modeled a a sequence of events:
                   1) Specular refraction inside
                   2) Subsurface scattering
                   3) Specular refraction outside again
                   where both refractions reduce the energy of the diffuse component. */
                UnpolarizedSpectrum r_i = std::get<0>(fresnel(cos_theta_i, m_eta)),
                                    r_o = std::get<0>(fresnel(cos_theta_o, m_eta));
                diff = (1.f - r_o) * diff * (1.f - r_i);
                result += diff * dr::InvPi<Float> * cos_theta_o;
            }
        }

        return result & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        bool has_specular = ctx.is_enabled(BSDFFlags::GlossyReflection, 0),
             has_diffuse  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely((!has_specular && !has_diffuse) || dr::none_or<false>(active)))
            return 0.f;

        Float prob_specular = m_specular_sampling_weight,
              prob_diffuse  = 1.f - prob_specular;
        if (unlikely(has_specular != has_diffuse))
            prob_specular = has_specular ? 1.f : 0.f;

        // Specular component
        Vector3f H = dr::normalize(wo + si.wi);
        MicrofacetDistribution distr(m_type, m_alpha_u, m_alpha_v, m_sample_visible);

        Float p_specular;
        if (m_sample_visible)
            p_specular = distr.eval(H) * distr.smith_g1(si.wi, H) / (4.f * cos_theta_i);
        else
            p_specular = distr.pdf(si.wi, H) / (4.f * dr::dot(wo, H));
        dr::masked(p_specular, dr::dot(si.wi, H) <= 0.f || dr::dot(wo, H) <= 0.f) = 0.f;

        // Diffuse component
        Float p_diffuse = warp::square_to_cosine_hemisphere_pdf(wo);

        return dr::select(active, prob_specular * p_specular + prob_diffuse * p_diffuse, 0.f);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SpecularDiffusePolarized[" << std::endl
            << "  diffuse_reflectance = " << string::indent(m_diffuse_reflectance) << "," << std::endl;
        if (m_specular_reflectance)
           oss << "  specular_reflectance = " << string::indent(m_specular_reflectance) << "," << std::endl;
        oss << "  distribution = " << m_type << "," << std::endl
            << "  sample_visible = " << m_sample_visible << "," << std::endl
            << "  alpha_u = " << m_alpha_u << "," << std::endl
            << "  alpha_v = " << m_alpha_v << "," << std::endl
            << "  eta = " << m_eta << "," << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    /// Diffuse reflectance component
    ref<Texture> m_diffuse_reflectance;
    /// Specular reflectance component
    ref<Texture> m_specular_reflectance;

    /// Specifies the type of microfacet distribution
    MicrofacetType m_type;
    /// Importance sample the distribution of visible normals?
    bool m_sample_visible;
    /// Roughness value
    Float m_alpha_u, m_alpha_v;

    /// Relative refractive index
    Float m_eta;

    /// Sampling weight for specular component
    Float m_specular_sampling_weight;
};

MI_IMPLEMENT_CLASS_VARIANT(PolarizedPlastic, BSDF)
MI_EXPORT_PLUGIN(PolarizedPlastic, "Polarized plastic")
NAMESPACE_END(mitsuba)
