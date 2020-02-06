#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!
.. _bsdf-plastic:

Smooth plastic material (:monosp:`plastic`)
-------------------------------------------

.. pluginparameters::

 * - int_ior
   - |float| or |string|
   - Interior index of refraction specified numerically or using a known material name.
     (Default: polypropylene / 1.49)
 * - ext_ior
   - |float| or |string|
   - Exterior index of refraction specified numerically or using a known material name.
     (Default: air / 1.000277)
 * - specular_reflectance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular reflection component. Note that for
     physical realism, this parameter should never be touched. (Default: 1.0)
 * - diffuse_reflectance
   - |spectrum| or |texture|
   - Optional factor used to modulate the diffuse reflection component. (Default: 0.5)
 * - nonlinear
   - |bool|
   - Account for nonlinear color shifts due to internal scattering? See the main text for details..
     (Default: Don't account for them and preserve the texture colors, i.e. |false|)

.. subfigstart::
.. _fig-plastic_default:

.. figure:: ../../resources/data/docs/images/render/bsdf_plastic_default.jpg
    :alt: A rendering with the default parameters
    :width: 95%
    :align: center

    A rendering with the default parameters

.. _fig-plastic_shiny:

.. figure:: ../../resources/data/docs/images/render/bsdf_plastic_shiny.jpg
    :alt: A rendering with custom parameters
    :width: 95%
    :align: center

    A rendering with custom parameters

.. subfigend::
    :width: 0.49
    :alt: Example diffuse appearances
    :label: fig-diffuse-bsdf

This plugin describes a smooth plastic-like material with internal scattering. It uses the Fresnel
reflection and transmission coefficients to provide direction-dependent specular and diffuse
components. Since it is simple, realistic, and fast, this model is often a better choice than the
:ref:`bsdf-roughplastic` plugins when rendering smooth plastic-like materials. For convenience, this
model allows to specify IOR values either numerically, or based on a list of known materials
(see Table :num:`ior-table-list` for an overview). This plugin accounts for multiple
interreflections inside the material (read on for details), which avoids a serious energy loss
problem of the aforementioned plugin combination.

The following XML snippet describes a shiny material whose diffuse reflectance is specified using
sRGB:

.. code-block:: xml
    :name: plastic-shiny

    <bsdf type="plastic">
        <rgb name="diffuse_reflectance" value="0.1, 0.27, 0.36"/>
        <float name="int_ior" value="1.9"/>
    </bsdf>

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_plastic_diffuse.jpg
   :caption: Diffuse textured rendering
.. subfigure:: ../../resources/data/docs/images/render/bsdf_plastic_preserve.jpg
   :caption: Plastic model, :code:`nonlinear=false`
.. subfigure:: ../../resources/data/docs/images/render/bsdf_plastic_nopreserve.jpg
   :caption: Plastic model, :code:`nonlinear=true`
.. subfigend::
    :label: fig-plastic-nonlinear

When asked to do so, this model can account for subtle nonlinear color shifts due to internal
scattering processes. The above images show a textured object first rendered using :ref:`bsdf-diffuse`,
then :ref:`bsdf-plastic` with the default parameters, and finally using :ref:`bsdf-plastic` and
support for nonlinear color shifts.

Internal scattering
-------------------

Internally, this is model simulates the interaction of light with a diffuse
base surface coated by a thin dielectric layer. This is a convenient
abstraction rather than a restriction. In other words, there are many
materials that can be rendered with this model, even if they might not
fit this description perfectly well.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/bsdf/plastic_intscat_1.svg
    :caption: At the boundary, incident illumination is partly reflected and refracted
    :label: fig-plastic-intscat_a
.. subfigure:: ../../resources/data/docs/images/bsdf/plastic_intscat_2.svg
    :caption: The refracted portion scatters diffusely at the base layer
    :label: fig-plastic-intscat_b
.. subfigure:: ../../resources/data/docs/images/bsdf/plastic_intscat_3.svg
    :caption: An illustration of the scattering events that are internally handled by this plugin
    :label: fig-plastic-intscat_c
.. subfigend::
    :label: fig-plastic-intscat

Given illumination that is incident upon such a material, a portion
of the illumination is specularly reflected at the material
boundary, which results in a sharp reflection in the mirror direction
(Figure :num:`fig-plastic-intscat_a`).
The remaining illumination refracts into the material, where it
scatters from the diffuse base layer. (Figure :num:`fig-plastic-intscat_b`).
While some of the diffusely scattered illumination is able to
directly refract outwards again, the remainder is reflected from the
interior side of the dielectric boundary and will in fact remain
trapped inside the material for some number of internal scattering
events until it is finally able to escape (Figure :num:`fig-plastic-intscat_c`).
Due to the mathematical simplicity of this setup, it is possible to work
out the correct form of the model without actually having to simulate
the potentially large number of internal scattering events.
Note that due to the internal scattering, the diffuse color of the
material is in practice slightly different from the color of the
base layer on its own---in particular, the material color will tend to shift towards
darker colors with higher saturation. Since this can be counter-intuitive when
using bitmap textures, these color shifts are disabled by default. Specify
the parameter :code:`nonlinear=true` to enable them. Figure :num:`fig-plastic-nonlinear`
illustrates the resulting change. This effect is also seen in real life,
for instance a piece of wood will look slightly darker after coating it
with a layer of varnish.
*/

template <typename Float, typename Spectrum>
class SmoothPlastic final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture)

    SmoothPlastic(const Properties &props) : Base(props) {
        // Specifies the internal index of refraction at the interface
        ScalarFloat int_ior = lookup_ior(props, "int_ior", "polypropylene");

        // Specifies the external index of refraction at the interface
        ScalarFloat ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.f || ext_ior < 0.f)
            Throw("The interior and exterior indices of "
                  "refraction must be positive!");

        m_eta = int_ior / ext_ior;

        m_specular_reflectance = props.texture<Texture>("specular_reflectance", 1.f);
        m_diffuse_reflectance  = props.texture<Texture>("diffuse_reflectance", .5f);

        m_nonlinear = props.bool_("nonlinear", false);

        m_components.push_back(BSDFFlags::DeltaReflection | BSDFFlags::FrontSide);
        m_components.push_back(BSDFFlags::DiffuseReflection | BSDFFlags::FrontSide);
        m_flags = m_components[0] | m_components[1];

        parameters_changed();
    }

    void parameters_changed() override {
        m_inv_eta_2 = 1.f / (m_eta * m_eta);

        // Numerically approximate the diffuse Fresnel reflectance
        m_fdr_int = fresnel_diffuse_reflectance(1.f / m_eta);
        m_fdr_ext = fresnel_diffuse_reflectance(m_eta);

        // Compute weights that further steer samples towards the specular or diffuse components
        ScalarFloat d_mean = m_diffuse_reflectance->mean(),
                    s_mean = m_specular_reflectance->mean();

        m_specular_sampling_weight = s_mean / (d_mean + s_mean);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        bool has_specular = ctx.is_enabled(BSDFFlags::DeltaReflection, 0),
             has_diffuse  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        BSDFSample3f bs = zero<BSDFSample3f>();
        Spectrum result(0.f);
        if (unlikely((!has_specular && !has_diffuse) || none_or<false>(active)))
            return { bs, result };

        // Determine which component should be sampled
        Float f_i           = std::get<0>(fresnel(cos_theta_i, Float(m_eta))),
              prob_specular = f_i * m_specular_sampling_weight,
              prob_diffuse  = (1.f - f_i) * (1.f - m_specular_sampling_weight);

        if (unlikely(has_specular != has_diffuse))
            prob_specular = has_specular ? 1.f : 0.f;
        else
            prob_specular = prob_specular / (prob_specular + prob_diffuse);

        prob_diffuse = 1.f - prob_specular;

        Mask sample_specular = active && (sample1 < prob_specular),
             sample_diffuse = active && !sample_specular;

        bs.eta = 1.f;
        bs.pdf = 0.f;

        if (any_or<true>(sample_specular)) {
            masked(bs.wo, sample_specular) = reflect(si.wi);
            masked(bs.pdf, sample_specular) = prob_specular;
            masked(bs.sampled_component, sample_specular) = 0;
            masked(bs.sampled_type, sample_specular) = +BSDFFlags::DeltaReflection;

            Spectrum spec = m_specular_reflectance->eval(si, sample_specular) * f_i / bs.pdf;
            masked(result, sample_specular) = spec;
        }

        if (any_or<true>(sample_diffuse)) {
            masked(bs.wo, sample_diffuse) = warp::square_to_cosine_hemisphere(sample2);
            masked(bs.pdf, sample_diffuse) = prob_diffuse * warp::square_to_cosine_hemisphere_pdf(bs.wo);
            masked(bs.sampled_component, sample_diffuse) = 1;
            masked(bs.sampled_type, sample_diffuse) = +BSDFFlags::DiffuseReflection;

            Float f_o = std::get<0>(fresnel(Frame3f::cos_theta(bs.wo), Float(m_eta)));
            // TODO: handle polarization instead of discarding it here
            UnpolarizedSpectrum diff = depolarize(m_diffuse_reflectance->eval(si, sample_diffuse));
            diff /= 1.f - (m_nonlinear ? (diff * m_fdr_int) : m_fdr_int);
            diff *= m_inv_eta_2 * (1.f - f_i) * (1.f - f_o) / prob_diffuse;
            masked(result, sample_diffuse) = diff;
        }

        return { bs, unpolarized<Spectrum>(result) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        bool has_diffuse = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely(!has_diffuse || none_or<false>(active)))
            return 0.f;

        Float f_i = std::get<0>(fresnel(cos_theta_i, Float(m_eta))),
              f_o = std::get<0>(fresnel(cos_theta_o, Float(m_eta)));

        // TODO: handle polarization instead of discarding it here
        UnpolarizedSpectrum diff = depolarize(m_diffuse_reflectance->eval(si, active));
        diff /= 1.f - (m_nonlinear ? (diff * m_fdr_int) : m_fdr_int);

        diff *= warp::square_to_cosine_hemisphere_pdf(wo) *
                m_inv_eta_2 * (1.f - f_i) * (1.f - f_o);

        return select(active, unpolarized<Spectrum>(diff), zero<Spectrum>());
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely(!ctx.is_enabled(BSDFFlags::DiffuseReflection, 1) || none_or<false>(active)))
            return 0.f;

        Float prob_diffuse = 1.f;

        if (ctx.is_enabled(BSDFFlags::DeltaReflection, 0)) {
            Float f_i           = std::get<0>(fresnel(cos_theta_i, Float(m_eta))),
                  prob_specular = f_i * m_specular_sampling_weight;
            prob_diffuse  = (1.f - f_i) * (1.f - m_specular_sampling_weight);
            prob_diffuse = prob_diffuse / (prob_specular + prob_diffuse);
        }

        Float pdf = warp::square_to_cosine_hemisphere_pdf(wo) * prob_diffuse;

        return select(active, pdf, 0.f);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("eta", m_eta);
        callback->put_object("diffuse_reflectance", m_diffuse_reflectance.get());
        callback->put_object("specular_reflectance", m_specular_reflectance.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothPlastic[" << std::endl
            << "  specular_reflectance = "     << m_specular_reflectance              << "," << std::endl
            << "  diffuse_reflectance = "      << m_diffuse_reflectance               << "," << std::endl
            << "  specular_sampling_weight = " << m_specular_sampling_weight          << "," << std::endl
            << "  specular_sampling_weight = " << (1.f - m_specular_sampling_weight)  << "," << std::endl
            << "  nonlinear = "                << (int) m_nonlinear                   << "," << std::endl
            << "  eta = "                      << m_eta                               << "," << std::endl
            << "  fdr_int = "                  << m_fdr_int                           << "," << std::endl
            << "  fdr_ext = "                  << m_fdr_ext                           << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Texture> m_diffuse_reflectance;
    ref<Texture> m_specular_reflectance;
    ScalarFloat m_eta, m_inv_eta_2;
    ScalarFloat m_fdr_int, m_fdr_ext;
    ScalarFloat m_specular_sampling_weight;
    bool m_nonlinear;
};

MTS_IMPLEMENT_CLASS_VARIANT(SmoothPlastic, BSDF)
MTS_EXPORT_PLUGIN(SmoothPlastic, "Smooth plastic")
NAMESPACE_END(mitsuba)
