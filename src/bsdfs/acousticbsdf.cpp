// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!
.. _bsdf-acousticbsdf:

Acoustic material (:monosp:`acousticbsdf`)
-------------------------------------------

.. pluginparameters::

 * - absorption
   - |spectrum| or |texture|
   - Absorption coefficient that determines the percentage of energy that is
     absorbed by the surface. Values should be between 0 and 1, where 0 means no
     absorption and 1 means complete absorption. (Default: 0.5)
   - |exposed|, |differentiable|

 * - scattering
   - |spectrum| or |texture|
   - Scattering coefficient that determines the ratio between the specular and
     diffuse components.
     Values should be between 0 and 1, where 0 favors specular reflection and 1
     favors diffuse scattering. (Default: 0.5)
   - |exposed|, |differentiable|

 * - specular_lobe_width
   - |float|
   - Specifies the width of the specular lobe. Small values produce nearly
     ideal specular reflection while remaining differentiable. (Default: 0.001)
   - |exposed|, |differentiable|, |discontinuous|


This plugin implements an acoustic material model that combines a specular reflection component with diffuse scattering.

The material behavior is controlled by two main parameters: the absorption
coefficient determines how much energy is absorbed by the material, while the
scattering coefficient controls the ratio between the specular and diffuse reflection components.

To ensure a continuously differentiable BSDF, the acoustic model does not assume
ideal specular reflections and instead models a lobe around the specular direction.
The lobe width is controlled by the `specular_lobe_width` parameter.
Small `specular_lobe_width` values (default or lower) approximate ideal specular reflections (see left figure) while remaining continuously differentiable.
Larger values increase the specular lobe width (see right figure).
Under the hood, this BSDF uses the microfacet-based model from
`roughconductor <https://mitsuba.readthedocs.io/en/latest/src/plugin_reference/section_bsdfs.html#bsdf-roughconductor>`_
with a Beckmann distribution. The parameter `specular_lobe_width` corresponds to the parameter `alpha` in the
`roughconductor <https://mitsuba.readthedocs.io/en/latest/src/plugin_reference/section_bsdfs.html#bsdf-roughconductor>`_ BSDF.
Note that microfacet models are not validated for the application to sound propagation, although any non-finite surface will exhibit specular lobes with non-zero width.

The diffuse component follows Lambertian scattering (see
`diffuse <https://mitsuba.readthedocs.io/en/latest/src/plugin_reference/section_bsdfs.html#bsdf-diffuse>`_).

.. subfigstart::
.. subfigure:: ../../resources/data_acoustic/docs/images/bsdf/acoustic_bsdf-1.png
   :caption: Acoustic BSDF with default specular lobe width.
.. subfigure:: ../../resources/data_acoustic/docs/images/bsdf/acoustic_bsdf-2.png
   :caption: Acoustic BSDF with larger specular lobe width.
.. subfigend::
     :label: fig-acoustic


The following XML snippet describes an acoustic material with spectrally varying
absorption and scattering values, given in frequency-value pairs (see :ref:`sec-spectra`).

.. tabs::
    .. code-tab:: xml
        :name: acousticbsdf

        <bsdf type="acousticbsdf">
            <spectrum name="absorption" value="20:0.2, 1000.:0.5, 20000:0.8"/>
            <spectrum name="scattering" value="20:0.1, 1000.:0.6, 20000:0.9"/>
            <float name="specular_lobe_width" value="0.001"/>
        </bsdf>

    .. code-tab:: python

        'type': 'acousticbsdf',
        'absorption': {
            'type': 'spectrum',
            'value': [(20, 0.2), (1000, 0.5), (20000, 0.8)],
        },
        'scattering': {
            'type': 'spectrum',
            'value': [(20, 0.1), (1000, 0.6), (20000, 0.9)],
        },
        'specular_lobe_width': 0.001
*/

template <typename Float, typename Spectrum>
class AcousticBSDF final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture, MicrofacetDistribution)

    AcousticBSDF(const Properties &props) : Base(props) {

        m_absorption = props.get_texture<Texture>("absorption", 0.5f);
        m_scattering = props.get_texture<Texture>("scattering", 0.5f);

        // Beckmann distribution
        m_type           = MicrofacetType::Beckmann;
        m_sample_visible = true;
        m_alpha          = props.get<ScalarFloat>("specular_lobe_width", 0.001f);;

        m_components.push_back(BSDFFlags::GlossyReflection |
                               BSDFFlags::FrontSide);
        m_components.push_back(BSDFFlags::DiffuseReflection |
                               BSDFFlags::FrontSide);
        m_flags = m_components[0] | m_components[1];
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("absorption", m_absorption, ParamFlags::Differentiable);
        cb->put("scattering", m_scattering, ParamFlags::Differentiable);
        cb->put("specular_lobe_width", m_alpha,
                ParamFlags::Differentiable | ParamFlags::Discontinuous);
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
        Spectrum result(0.f);

        // early return if both components are deactivated
        if (unlikely((!has_specular && !has_diffuse) || dr::none_or<false>(active)))
            return { bs, result };

        // Determine which component should be sampled.
        Float scattering = eval_scattering(si, active);
        Mask sample_specular, sample_diffuse;

        if (unlikely(has_specular != has_diffuse)) {
            // set Masks directly if one component is disabled:
            Float prob_specular = has_specular ? 1.f : 0.f;
            Float prob_diffuse  = 1.f - prob_specular;
            sample_specular     = (prob_specular) && active;
            sample_diffuse      = (prob_diffuse) && active;
        } else {
            // choose component analogously to BlendBSDF
            sample_specular = (sample1 > scattering) && active;
            sample_diffuse  = (sample1 <= scattering) && active;
        }

        if (dr::any_or<true>(sample_specular)) { // copied from roughplastic:
            // Perfect specular reflection based on the microfacet normal
            MicrofacetDistribution distr(m_type, m_alpha, m_sample_visible);
            Normal3f m = std::get<0>(distr.sample(si.wi, sample2));
            dr::masked(bs.wo, sample_specular) = reflect(si.wi, m);
            dr::masked(bs.sampled_component, sample_specular) = 0;
            dr::masked(bs.sampled_type, sample_specular) = +BSDFFlags::GlossyReflection;
        }

        if (dr::any_or<true>(sample_diffuse)) { // copied from roughplastic:
            // lambertian diffuse reflection
            dr::masked(bs.wo, sample_diffuse) = warp::square_to_cosine_hemisphere(sample2);
            dr::masked(bs.sampled_component, sample_diffuse) = 1;
            dr::masked(bs.sampled_type, sample_diffuse) = +BSDFFlags::DiffuseReflection;
        }

        bs.pdf = pdf(ctx, si, bs.wo, active);
        active &= bs.pdf > 0.f;
        result = eval(ctx, si, bs.wo, active);

        return { bs, (depolarizer<Spectrum>(result) / bs.pdf) & active };
    }

    Float pdf(const BSDFContext &ctx,
              const SurfaceInteraction3f &si,
              const Vector3f &wo,
              Mask active = true) const override {

        bool has_specular = ctx.is_enabled(BSDFFlags::GlossyReflection, 0),
             has_diffuse  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely((!has_specular && !has_diffuse) || dr::none_or<false>(active)))
            return 0.f;

        Float scattering = eval_scattering(si, active);

        Float prob_specular = 0.f, prob_diffuse = 0.f;
        if (unlikely(has_specular != has_diffuse)) {
            // set probabilities directly if one component is disabled
            prob_specular = has_specular ? 1.f : 0.f;
            prob_diffuse  = 1.f - prob_specular;
        } else {
            prob_specular = 1.f - scattering;
            prob_diffuse  = scattering;
        }

        Float result = 0.f;

        // pdf of specular component (copied from RoughPlastic):
        Vector3f H = dr::normalize(wo + si.wi);
        MicrofacetDistribution distr(m_type, m_alpha, m_sample_visible);
        if (m_sample_visible)
            result = distr.eval(H) * distr.smith_g1(si.wi, H) / (4.f * cos_theta_i);
        else
            result = distr.pdf(si.wi, H) / (4.f * dr::dot(wo, H));
        result *= prob_specular;

        // add pdf of diffuse component:
        result += prob_diffuse * warp::square_to_cosine_hemisphere_pdf(wo);

        return result;
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

        UnpolarizedSpectrum value_specular(0.f);
        if (has_specular) { // Taken from RoughPlastic & RoughConductor:
            MicrofacetDistribution distr(m_type, m_alpha, m_sample_visible);

            // Calculate the reflection half-vector
            Vector3f H = dr::normalize(wo + si.wi);

            // Evaluate the microfacet normal distribution
            Float D = distr.eval(H);

            // Smith's shadow-masking function
            Float G = distr.G(si.wi, wo, H);

            // Calculate the specular reflection component
            Float F        = 1.f; // (Fresnel term is 1 for a perfect mirror)
            value_specular = F * D * G / (4.f * cos_theta_i);
        }

        UnpolarizedSpectrum value_diffuse(0.f);
        if (has_diffuse) { // taken from SmoothDiffuse:
            value_diffuse = dr::InvPi<Float> * cos_theta_o;
        }

        // Evaluate total
        UnpolarizedSpectrum absorption = m_absorption->eval(si, active);
        UnpolarizedSpectrum scattering = m_scattering->eval(si, active);
        UnpolarizedSpectrum value =
            (1 - absorption) *
            ((1 - scattering) * value_specular + (scattering) * value_diffuse);

        return value;
    }

    MI_INLINE Float eval_scattering(const SurfaceInteraction3f &si,
                                    const Mask &active) const {
        /* Evaluate scattering at the frequency stored in
         * For now: evaluate scattering at the ray's frequency (stored in
         * si.wavelengths). because the acoustic variants use a spectrum length
         * of 1, this doesn't discard any information.
         * Later -> Use spectral variants (4 frequencies per ray) in combination
         * with Ratio-Control Variates as sampling strategy for
         * frequency-dependent scattering coefficients.*/
        return dr::clip(m_scattering->eval(si, active)[0], 0.f, 1.f);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "AcousticBSDF[" << std::endl
            << "  absorption = " << string::indent(m_absorption) << "," << std::endl
            << "  scattering = " << string::indent(m_scattering) << "," << std::endl
            << "  specular_lobe_width = " << string::indent(m_alpha) << "," << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(AcousticBSDF)
protected:
    ref<Texture> m_absorption;
    ref<Texture> m_scattering;

    /// Roughconductor variables:
    MicrofacetType m_type;
    Float m_alpha;
    bool m_sample_visible;

    MI_TRAVERSE_CB(Base, m_absorption, m_scattering, m_alpha)
};

MI_EXPORT_PLUGIN(AcousticBSDF)
NAMESPACE_END(mitsuba)
