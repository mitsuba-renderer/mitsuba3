#include <mitsuba/core/frame.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _plugin-bsdf-rpv:

Rahman Pinty Verstraete reflection model (:monosp:`rpv`)
--------------------------------------------------------

.. pluginparameters::

 * - rho_0
   - |spectrum| or |texture|
   - :math:`\rho_0 \ge 0`. Default: 0.1
   - |exposed| |differentiable|

 * - k
   - |spectrum| or |texture|
   - :math:`k \in \mathbb{R}`. Default: 0.1
   - |exposed| |differentiable|

 * - g
   - |spectrum| or |texture|
   - :math:`-1 \le g \le 1`. Default: 0.0
   - |exposed| |differentiable|

 * - rho_c
   - |spectrum| or |texture|
   - Default: Equal to `rho_0`
   - |exposed| |differentiable|

This plugin implements the reflection model proposed by
:cite:`Rahman1993CoupledSurfaceatmosphereReflectance`.

Apart from floating point values, model parameters can be defined by nested or
referenced textures which are then mapped onto the shape based on its UV
parameterization.

This plugin also supports the most common extension of the RPV model to four
parameters, namely the :math:`\rho_c` extension, as used in
:cite:`Widlowski2006Rayspread`.

For the fundamental formulae defining the RPV model, please refer to the
Eradiate Scientific Handbook.

Note that this material is one-sided---that is, observed from the
back side, it will be completely black. If this is undesirable,
consider using the ``twosided`` BSDF adapter plugin.
The following snippet describes an RPV material with monochromatic parameters:

.. tab-set-code::

    .. code-block:: python

        "type": "rpv",
        "rho_0": 0.02,
        "k": 0.3,
        "g": -0.12

    .. code-block:: xml

        <bsdf type="rpv">
            <float name="rho_0" value="0.02"/>
            <float name="k" value="0.3"/>
            <float name="g" value="-0.12"/>
        </bsdf>
*/

MI_VARIANT
class RPVBSDF final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    RPVBSDF(const Properties &props) : Base(props) {
        m_rho_0 = props.texture<Texture>("rho_0", 0.1f);
        m_g = props.texture<Texture>("g", 0.f);
        m_k = props.texture<Texture>("k", 0.1f);
        m_rho_c = props.texture<Texture>("rho_c", m_rho_0);
        m_flags = BSDFFlags::GlossyReflection | BSDFFlags::FrontSide;
        dr::set_attr(this, "flags", m_flags);
        m_components.push_back(m_flags);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("rho_0", m_rho_0.get(),
                             +ParamFlags::Differentiable);
        callback->put_object("g", m_g.get(), +ParamFlags::Differentiable);
        callback->put_object("k", m_k.get(), +ParamFlags::Differentiable);
        callback->put_object("rho_c", m_rho_c.get(),
                             +ParamFlags::Differentiable);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float /* position_sample */,
                                             const Point2f &direction_sample,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        BSDFSample3f bs = dr::zeros<BSDFSample3f>();

        active &= cos_theta_i > 0.f;
        if (unlikely(dr::none_or<false>(active) ||
                     !ctx.is_enabled(BSDFFlags::GlossyReflection)))
            return { bs, 0.f };

        bs.wo = warp::square_to_cosine_hemisphere(direction_sample);
        bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);
        bs.eta = 1.f;
        bs.sampled_type = +BSDFFlags::GlossyReflection;
        bs.sampled_component = 0;

        UnpolarizedSpectrum value =
            eval_rpv(si, bs.wo, active) * Frame3f::cos_theta(bs.wo) / bs.pdf;

        return { bs, depolarizer<Spectrum>(value) & (active && bs.pdf > 0.f) };
    }

    /* Evaluation of the RPV BRDF (without foreshortening factor) as per the
       Eradiate Scientific Handbook. */
    UnpolarizedSpectrum eval_rpv(const SurfaceInteraction3f &si,
                                 const Vector3f &wo, Mask active) const {
        Spectrum rho_0 = m_rho_0->eval(si, active);
        Spectrum rho_c = m_rho_c->eval(si, active);
        Spectrum g = m_g->eval(si, active);
        Spectrum k = m_k->eval(si, active);

        auto [sin_phi_i, cos_phi_i] = Frame3f::sincos_phi(si.wi);
        auto [sin_phi_o, cos_phi_o] = Frame3f::sincos_phi(wo);
        Float cos_phi_i_minus_phi_o =
            cos_phi_i * cos_phi_o + sin_phi_i * sin_phi_o;
        Float sin_theta_i = Frame3f::sin_theta(si.wi);
        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        Float tan_theta_i = Frame3f::tan_theta(si.wi);
        Float sin_theta_o = Frame3f::sin_theta(wo);
        Float cos_theta_o = Frame3f::cos_theta(wo);
        Float tan_theta_o = Frame3f::tan_theta(wo);

        // Henyey-Greenstein component
        Float cos_Theta = cos_theta_i * cos_theta_o +
                          sin_theta_i * sin_theta_o * cos_phi_i_minus_phi_o;
        // The following uses cos(pi-x) = -cos(x)
        Spectrum F = (1.f - dr::sqr(g)) /
                     dr::pow((1.f + dr::sqr(g) + 2.f * g * cos_Theta), 1.5f);

        // Hot spot component
        Float G = dr::safe_sqrt(dr::sqr(tan_theta_i) + dr::sqr(tan_theta_o) -
                                2.f * tan_theta_i * tan_theta_o *
                                    cos_phi_i_minus_phi_o);
        Spectrum H = 1.f + (1.f - rho_c) / (1.f + G);

        // Minnaert component
        Spectrum M = dr::pow(
            cos_theta_i * cos_theta_o * (cos_theta_i + cos_theta_o), k - 1.f);

        // Total value
        Spectrum value = rho_0 * M * F * H * dr::InvPi<Float>;

        return depolarizer<Spectrum>(value);
    }

    Spectrum eval(const BSDFContext & /*ctx*/, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;
        Spectrum value = eval_rpv(si, wo, active);

        return dr::select(
            active, depolarizer<Spectrum>(value) * dr::abs(cos_theta_o), 0.f);
    }

    Float pdf(const BSDFContext & /* ctx */, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        Float pdf = warp::square_to_cosine_hemisphere_pdf(wo);

        return dr::select(cos_theta_i > 0.f && cos_theta_o > 0.f, pdf, 0.f);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RPVBSDF[" << std::endl
            << "  rho_0 = " << string::indent(m_rho_0) << "," << std::endl
            << "  g = " << string::indent(m_g) << "," << std::endl
            << "  k = " << string::indent(m_k);
        if (m_rho_0 != m_rho_c) {
            oss << "," << std::endl << "  rho_c = " << string::indent(m_rho_c);
        }
        oss << std::endl << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

private:
    ref<Texture> m_rho_0;
    ref<Texture> m_g;
    ref<Texture> m_k;
    ref<Texture> m_rho_c;
};

MI_IMPLEMENT_CLASS_VARIANT(RPVBSDF, BSDF)
MI_EXPORT_PLUGIN(RPVBSDF, "Rahman-Pinty-Verstraete BSDF")
NAMESPACE_END(mitsuba)
