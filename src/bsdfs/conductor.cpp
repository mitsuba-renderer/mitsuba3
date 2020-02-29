#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-conductor:

Smooth conductor (:monosp:`conductor`)
-------------------------------------------

.. pluginparameters::

 * - eta, k
   - |spectrum| or |texture|
   - Real and imaginary components of the material's index of refraction. (Default: 0.0/1.0)
 * - specular_reflectance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular reflection component.
     Note that for physical realism, this parameter should never be touched. (Default: 1.0)

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_conductor_gold.jpg
   :caption: Gold
.. subfigure:: ../../resources/data/docs/images/render/bsdf_conductor_aluminium.jpg
   :caption: Aluminium
.. subfigend::
    :label: fig-bsdf-conductor

This plugin implements a perfectly smooth interface to a conducting material,
such as a metal. For a similar model that instead describes a rough surface
microstructure, take a look at the separately available
:ref:`bsdf-roughconductor` plugin.
In contrast to dielectric materials, conductors do not transmit
any light. Their index of refraction is complex-valued and tends to undergo
considerable changes throughout the visible color spectrum.

When using this plugin, you should ideally one of the :monosp:`spectral` modes
of the renderer to get the most accurate results. While it also works
in RGB mode, the computations will be more approximate in nature.
Also note that this material is one-sided---that is, observed from the
back side, it will be completely black. If this is undesirable,
consider using the :ref:`bsdf-twosided` BRDF adapter plugin.

The following XML snippet describes a material definition for aluminium:

.. code-block:: xml
    :name: lst-conductor-aluminium

    <bsdf type="conductor">
        <spectrum name="eta" value="0.789000"/>
        <spectrum name="k" value="6.150000"/>
    </bsdf>

In *polarized* rendering modes, the material automatically switches to a polarized
implementation of the Fresnel equations.

 */

template <typename Float, typename Spectrum>
class SmoothConductor final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture)

    SmoothConductor(const Properties &props) : Base(props) {
        m_flags = BSDFFlags::DeltaReflection | BSDFFlags::FrontSide;
        m_components.push_back(m_flags);

        m_specular_reflectance = props.texture<Texture>("specular_reflectance", 1.f);

        std::string material = props.string("material", "none");
        if (props.has_property("eta") || material == "none") {
            m_eta = props.texture<Texture>("eta", 0.f);
            m_k   = props.texture<Texture>("k",   1.f);
            if (material != "none")
                Throw("Should specify either (eta, k) or material, not both.");
        } else {
            std::tie(m_eta, m_k) = complex_ior_from_file<Spectrum, Texture>(props.string("material", "Cu"));
        }
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float /* sample1 */,
                                             const Point2f &/* sample2 */,
                                             Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        BSDFSample3f bs = zero<BSDFSample3f>();
        Spectrum value(0.f);
        if (unlikely(none_or<false>(active) || !ctx.is_enabled(BSDFFlags::DeltaReflection)))
            return { bs, value };

        bs.sampled_component = 0;
        bs.sampled_type = +BSDFFlags::DeltaReflection;
        bs.wo  = reflect(si.wi);
        bs.eta = 1.f;
        bs.pdf = 1.f;

        Complex<UnpolarizedSpectrum> eta(m_eta->eval(si, active),
                                         m_k->eval(si, active));
        UnpolarizedSpectrum reflectance = m_specular_reflectance->eval(si, active);

        if constexpr (is_polarized_v<Spectrum>) {
            /* Due to lack of reciprocity in polarization-aware pBRDFs, they are
               always evaluated w.r.t. the actual light propagation direction, no
               matter the transport mode. In the following, 'wi_hat' is toward the
               light source. */
            Vector3f wi_hat = ctx.mode == TransportMode::Radiance ? bs.wo : si.wi,
                     wo_hat = ctx.mode == TransportMode::Radiance ? si.wi : bs.wo;

            // Mueller matrix for specular reflection.
            value = mueller::specular_reflection(UnpolarizedSpectrum(Frame3f::cos_theta(wi_hat)), eta);

            /* Apply frame reflection, according to "Stellar Polarimetry" by
               David Clarke, Appendix A.2 (A26) */
            value = mueller::reverse(value);

            /* The Stokes reference frame vector of this matrix lies in the plane
               of reflection. */
            Vector3f n(0, 0, 1);
            Vector3f s_axis_in = normalize(cross(n, -wi_hat)),
                     p_axis_in = normalize(cross(-wi_hat, s_axis_in)),
                     s_axis_out = normalize(cross(n, wo_hat)),
                     p_axis_out = normalize(cross(wo_hat, s_axis_out));

            /* Rotate in/out reference vector of M s.t. it aligns with the implicit
               Stokes bases of -wi_hat & wo_hat. */
            value = mueller::rotate_mueller_basis(value,
                                                  -wi_hat, p_axis_in, mueller::stokes_basis(-wi_hat),
                                                   wo_hat, p_axis_out, mueller::stokes_basis(wo_hat));
            value *= mueller::absorber(reflectance);
        } else {
            value = reflectance * fresnel_conductor(UnpolarizedSpectrum(cos_theta_i), eta);
        }

        return { bs, value & active };
    }

    Spectrum eval(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
                  const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    Float pdf(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
              const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("specular_reflectance", m_specular_reflectance.get());
        callback->put_object("eta", m_eta.get());
        callback->put_object("k", m_k.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothConductor[" << std::endl
            << "  eta = " << string::indent(m_eta) << "," << std::endl
            << "  k = "   << string::indent(m_k)   << "," << std::endl
            << "  specular_reflectance = " << string::indent(m_specular_reflectance) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Texture> m_specular_reflectance;
    ref<Texture> m_eta, m_k;
};

MTS_IMPLEMENT_CLASS_VARIANT(SmoothConductor, BSDF)
MTS_EXPORT_PLUGIN(SmoothConductor, "Smooth conductor")
NAMESPACE_END(mitsuba)
