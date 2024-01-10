#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-thindielectric:

Thin dielectric material (:monosp:`thindielectric`)
---------------------------------------------------

.. pluginparameters::
 :extra-rows: 4

 * - int_ior
   - |float| or |string|
   - Interior index of refraction specified numerically or using a known material name. (Default: bk7 / 1.5046)

 * - ext_ior
   - |float| or |string|
   - Exterior index of refraction specified numerically or using a known material name.  (Default: air / 1.000277)

 * - specular_reflectance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular reflection component. Note that for physical realism, this parameter should never be touched. (Default: 1.0)
   - |exposed|, |differentiable|

 * - specular_transmittance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular transmission component. Note that for physical realism, this parameter should never be touched. (Default: 1.0)
   - |exposed|, |differentiable|

 * - eta
   - |float|
   - Relative index of refraction from the exterior to the interior
   - |exposed|, |differentiable|, |discontinuous|

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_dielectric_glass.jpg
   :caption: Dielectric
.. subfigure:: ../../resources/data/docs/images/render/bsdf_thindielectric_glass.jpg
   :caption: Thindielectric
.. subfigend::
    :label: fig-bsdf-comparison-thindielectric

This plugin models a **thin** dielectric material that is embedded inside another
dielectric---for instance, glass surrounded by air. The interior of the material
is assumed to be so thin that its effect on transmitted rays is negligible,
Hence, light exits such a material without any form of angular deflection
(though there is still specular reflection).
This model should be used for things like glass windows that were modeled using only a
single sheet of triangles or quads. On the other hand, when the window consists of
proper closed geometry, :ref:`dielectric <bsdf-dielectric>` is the right choice.
This is illustrated below:

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/bsdf/dielectric_figure.svg
   :caption: The :ref:`dielectric <bsdf-dielectric>` plugin models a single transition from one index of refraction to another
.. subfigure:: ../../resources/data/docs/images/bsdf/thindielectric_figure.svg
   :caption: The :ref:`thindielectric <bsdf-thindielectric>` plugin models a pair of interfaces causing a transient index of refraction change
.. subfigend::
    :label: fig-bsdf-thindielectric

The implementation correctly accounts for multiple internal reflections
inside the thin dielectric at **no significant extra cost**, i.e. paths
of the type :math:`R, TRT, TR^3T, ..` for reflection and :math:`TT, TR^2, TR^4T, ..` for
refraction, where :math:`T` and :math:`R` denote individual reflection and refraction
events, respectively.

Similar to the :ref:`dielectric <bsdf-dielectric>` plugin, IOR values can either
be specified numerically, or based on a list of known materials (see the
corresponding table in the :ref:`dielectric <bsdf-dielectric>` reference).
When no parameters are given, the plugin activates the default settings,
which describe a borosilicate glass (BK7) ↔ air interface.

.. tabs::
    .. code-tab:: xml
        :name: thindielectric

        <bsdf type="thindielectric">
            <string name="int_ior" value="bk7"/>
            <string name="ext_ior" value="air"/>
        </bsdf>

    .. code-tab:: python

        'type': 'thindielectric',
        'int_ior': 'bk7',
        'ext_ior': 'air'
 */

template <typename Float, typename Spectrum>
class ThinDielectric final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    ThinDielectric(const Properties &props) : Base(props) {
        // Specifies the internal index of refraction at the interface
        ScalarFloat int_ior = lookup_ior(props, "int_ior", "bk7");
        // Specifies the external index of refraction at the interface
        ScalarFloat ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.f || ext_ior < 0.f)
            Throw("The interior and exterior indices of "
                  "refraction must be positive!");

        m_eta = int_ior / ext_ior;

        if (props.has_property("specular_reflectance"))
            m_specular_reflectance   = props.texture<Texture>("specular_reflectance", 1.f);
        if (props.has_property("specular_transmittance"))
            m_specular_transmittance = props.texture<Texture>("specular_transmittance", 1.f);

        m_components.push_back(BSDFFlags::DeltaReflection | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide);
        m_components.push_back(BSDFFlags::Null | BSDFFlags::FrontSide | BSDFFlags::BackSide);
        m_flags = m_components[0] | m_components[1];
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("eta", m_eta, ParamFlags::Differentiable | ParamFlags::Discontinuous);
        if (m_specular_transmittance)
            callback->put_object("specular_transmittance", m_specular_transmittance.get(), +ParamFlags::Differentiable);
        if (m_specular_reflectance)
            callback->put_object("specular_reflectance",   m_specular_reflectance.get(),   +ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        dr::make_opaque(m_eta);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f & /* sample2 */,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        bool has_reflection   = ctx.is_enabled(BSDFFlags::DeltaReflection, 0),
             has_transmission = ctx.is_enabled(BSDFFlags::Null, 1);

        Float r = std::get<0>(fresnel(dr::abs(Frame3f::cos_theta(si.wi)), m_eta));

        // Account for internal reflections: r' = r + trt + tr^3t + ..
        r *= 2.f / (1.f + r);

        Float t = 1.f - r;

        // Select the lobe to be sampled
        BSDFSample3f bs = dr::zeros<BSDFSample3f>();
        UnpolarizedSpectrum weight;
        Mask selected_r;
        if (likely(has_reflection && has_transmission)) {
            selected_r = sample1 <= r && active;
            weight = 1.f;
            bs.pdf = dr::select(selected_r, r, t);
        } else {
            if (has_reflection || has_transmission) {
                selected_r = Mask(has_reflection) && active;
                weight = has_reflection ? r : t;
                bs.pdf = 1.f;
            } else {
                return { bs, 0.f };
            }
        }

        bs.wo = dr::select(selected_r, reflect(si.wi), -si.wi);
        bs.eta = 1.f;
        bs.sampled_component = dr::select(selected_r, UInt32(0), UInt32(1));
        bs.sampled_type =
            dr::select(selected_r, UInt32(+BSDFFlags::DeltaReflection), UInt32(+BSDFFlags::Null));

        if (m_specular_reflectance && dr::any_or<true>(selected_r))
            weight[selected_r] *= m_specular_reflectance->eval(si, selected_r);

        Mask selected_t = !selected_r && active;
        if (m_specular_transmittance && dr::any_or<true>(selected_t))
            weight[selected_t] *= m_specular_transmittance->eval(si, selected_t);

        return { bs, depolarizer<Spectrum>(weight) & active };
    }

    Spectrum eval(const BSDFContext & /* ctx */, const SurfaceInteraction3f & /* si */,
                  const Vector3f & /* wo */, Mask /* active */) const override {
        return 0.f;
    }

    Float pdf(const BSDFContext & /* ctx */, const SurfaceInteraction3f & /* si */,
              const Vector3f & /* wo */, Mask /* active */) const override {
        return 0.f;
    }

    Spectrum eval_null_transmission(const SurfaceInteraction3f & si,
                                    Mask active) const override {

        Float r = std::get<0>(fresnel(dr::abs(Frame3f::cos_theta(si.wi)), m_eta));

        // Account for internal reflections: r' = r + trt + tr^3t + ..
        r *= 2.f / (1.f + r);

        UnpolarizedSpectrum value = 1 - r;

        if (m_specular_transmittance)
            value *= m_specular_transmittance->eval(si, active);

        return depolarizer<Spectrum>(value);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "ThinDielectric[" << std::endl;
        if (m_specular_reflectance)
            oss << "  specular_reflectance = "   << string::indent(m_specular_reflectance)   << "," << std::endl;
        if (m_specular_transmittance)
            oss << "  specular_transmittance = " << string::indent(m_specular_transmittance) << "," << std::endl;
        oss << "  eta = "                    << string::indent(m_eta)                    << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    Float m_eta;
    ref<Texture> m_specular_transmittance;
    ref<Texture> m_specular_reflectance;
};

MI_IMPLEMENT_CLASS_VARIANT(ThinDielectric, BSDF)
MI_EXPORT_PLUGIN(ThinDielectric, "Thin dielectric")
NAMESPACE_END(mitsuba)
