#include <mitsuba/core/string.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <enoki/stl.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-twosided:

Two-sided BRDF adapter (:monosp:`twosided`)
--------------------------------------------

.. pluginparameters::

 * - (Nested plugin)
   - |bsdf|
   - A nested BRDF that should be turned into a two-sided scattering model. If two BRDFs are specified, they will be placed on the front and back side, respectively

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_twosided_cbox_onesided.jpg
   :caption: From this angle, the Cornell box scene shows visible back-facing geometry
.. subfigure:: ../../resources/data/docs/images/render/bsdf_twosided_cbox.jpg
   :caption: Applying the `twosided` plugin fixes the rendering
.. subfigend::
    :label: fig-bsdf-twosided

By default, all non-transmissive scattering models in Mitsuba 2
are *one-sided* --- in other words, they absorb all light
that is received on the interior-facing side of any associated
surfaces. Holes and visible back-facing parts are thus exposed
as black regions.

Usually, this is a good idea, since it will reveal modeling
issues early on. But sometimes one is forced to deal with
improperly closed geometry, where the one-sided behavior is
bothersome. In that case, this plugin can be used to turn
one-sided scattering models into proper two-sided versions of
themselves. The plugin has no parameters other than a required
nested BSDF specification. It is also possible to supply two
different BRDFs that should be placed on the front and back
side, respectively.

The following snippet describes a two-sided diffuse material:

.. code-block:: xml
    :name: twosided

    <bsdf type="twosided">
        <bsdf type="diffuse">
             <spectrum name="reflectance" value="0.4"/>
        </bsdf>
    </bsdf>

 */
template <typename Float, typename Spectrum>
class TwoSidedBRDF final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES()

    TwoSidedBRDF(const Properties &props) : Base(props) {
        auto bsdfs = props.objects();
        if (bsdfs.size() > 0)
            m_brdf[0] = dynamic_cast<Base *>(bsdfs[0].second.get());
        if (bsdfs.size() == 2)
            m_brdf[1] = dynamic_cast<Base *>(bsdfs[1].second.get());
        else if (bsdfs.size() > 2)
            Throw("At most two nested BSDFs can be specified!");

        if (!m_brdf[0])
            Throw("A nested one-sided material is required!");
        if (!m_brdf[1])
            m_brdf[1] = m_brdf[0];

        // Add all nested components, overwriting any front / back side flag.
        for (size_t i = 0; i < m_brdf[0]->component_count(); ++i) {
            auto c = (m_brdf[0]->flags(i) & ~BSDFFlags::BackSide);
            m_components.push_back(c | BSDFFlags::FrontSide);
            m_flags = m_flags | m_components.back();
        }

        for (size_t i = 0; i < m_brdf[1]->component_count(); ++i) {
            auto c = (m_brdf[1]->flags(i) & ~BSDFFlags::FrontSide);
            m_components.push_back(c | BSDFFlags::BackSide);
            m_flags = m_flags | m_components.back();
        }

        if (has_flag(m_flags, BSDFFlags::Transmission))
            Throw("Only materials without a transmission component can be nested!");
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx_,
                                             const SurfaceInteraction3f &si_,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        using Result = std::pair<BSDFSample3f, Spectrum>;

        SurfaceInteraction3f si(si_);
        BSDFContext ctx(ctx_);

        Mask front_side = Frame3f::cos_theta(si.wi) > 0.f && active,
             back_side  = Frame3f::cos_theta(si.wi) < 0.f && active;

        Result result = zero<Result>();
        if (any_or<true>(front_side))
            masked(result, front_side) =
                m_brdf[0]->sample(ctx, si, sample1, sample2, front_side);

        if (any_or<true>(back_side)) {
            if (ctx.component != (uint32_t) -1)
                ctx.component -= (uint32_t) m_brdf[0]->component_count();

            si.wi.z() *= -1.f;
            masked(result, back_side) =
                m_brdf[1]->sample(ctx, si, sample1, sample2, back_side);
            masked(result.first.wo.z(), back_side) *= -1.f;
        }

        return result;
    }

    Spectrum eval(const BSDFContext &ctx_, const SurfaceInteraction3f &si_,
                  const Vector3f &wo_, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        SurfaceInteraction3f si(si_);
        BSDFContext ctx(ctx_);
        Vector3f wo(wo_);
        Spectrum result = 0.f;

        Mask front_side = Frame3f::cos_theta(si.wi) > 0.f && active,
             back_side  = Frame3f::cos_theta(si.wi) < 0.f && active;

        if (any_or<true>(front_side))
            result = m_brdf[0]->eval(ctx, si, wo, front_side);

        if (any_or<true>(back_side)) {
            if (ctx.component != (uint32_t) -1)
                ctx.component -= (uint32_t) m_brdf[0]->component_count();

            si.wi.z() *= -1.f;
            wo.z() *= -1.f;

            masked(result, back_side) = m_brdf[1]->eval(ctx, si, wo, back_side);
        }

        return result;
    }

    Float pdf(const BSDFContext &ctx_, const SurfaceInteraction3f &si_,
              const Vector3f &wo_, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        SurfaceInteraction3f si(si_);
        BSDFContext ctx(ctx_);
        Vector3f wo(wo_);
        Float result = 0.f;

        Mask front_side = Frame3f::cos_theta(si.wi) > 0.f && active,
             back_side  = Frame3f::cos_theta(si.wi) < 0.f && active;

        if (any_or<true>(front_side))
            result = m_brdf[0]->pdf(ctx, si, wo, front_side);

        if (any_or<true>(back_side)) {
            if (ctx.component != (uint32_t) -1)
                ctx.component -= (uint32_t) m_brdf[0]->component_count();

            si.wi.z() *= -1.f;
            wo.z() *= -1.f;

            masked(result, back_side) = m_brdf[1]->pdf(ctx, si, wo, back_side);
        }

        return result;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("brdf_0", m_brdf[0].get());
        callback->put_object("brdf_1", m_brdf[1].get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "TwoSided[" << std::endl
            << "  brdf[0] = " << string::indent(m_brdf[0]) << "," << std::endl
            << "  brdf[1] = " << string::indent(m_brdf[1]) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    ref<Base> m_brdf[2];
};

MTS_IMPLEMENT_CLASS_VARIANT(TwoSidedBRDF, BSDF)
MTS_EXPORT_PLUGIN(TwoSidedBRDF, "Two-sided material adapter")
NAMESPACE_END(mitsuba)
