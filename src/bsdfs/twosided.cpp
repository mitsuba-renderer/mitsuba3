#include <mitsuba/core/string.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-twosided:

Two-sided BRDF adapter (:monosp:`twosided`)
--------------------------------------------

.. pluginparameters::

 * - (Nested plugin)
   - |bsdf|
   - A nested BRDF that should be turned into a two-sided scattering model. If two BRDFs are specified, they will be placed on the front and back side, respectively
   - |exposed|, |differentiable|

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_twosided_cbox_onesided.jpg
   :caption: From this angle, the Cornell box scene shows visible back-facing geometry
.. subfigure:: ../../resources/data/docs/images/render/bsdf_twosided_cbox.jpg
   :caption: Applying the `twosided` plugin fixes the rendering
.. subfigend::
    :label: fig-bsdf-twosided

By default, all non-transmissive scattering models in Mitsuba 3
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

.. tabs::
    .. code-tab:: xml
        :name: twosided

        <bsdf type="twosided">
            <bsdf type="diffuse">
                 <rgb name="reflectance" value="0.4"/>
            </bsdf>
        </bsdf>

    .. code-tab:: python

        'type': 'twosided',
        'material': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': 0.4
            }
        }
 */
template <typename Float, typename Spectrum>
class TwoSidedBRDF final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES()

    TwoSidedBRDF(const Properties &props) : Base(props) {
        size_t bsdf_count = 0;
        for (auto &prop : props.objects()) {
            if (Base *bsdf = prop.try_get<Base>()) {
                if (bsdf_count >= 2)
                    Throw("At most two nested BSDFs can be specified!");
                m_brdf[bsdf_count++] = bsdf;
            }
        }

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

        if (!props.get<bool>("allow_transmission", false) && this->has_flag(BSDFFlags::Transmission))
            Throw("Only materials without a transmission component can be nested!");
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("brdf_0", m_brdf[0], ParamFlags::Differentiable);
        cb->put("brdf_1", m_brdf[1], ParamFlags::Differentiable);
    }

    /// Geometric normal in the shading frame, flipped to the side of the
    /// shading normal
    static Vector3f local_normal(const SurfaceInteraction3f &si) {
        Vector3f n = si.to_local(si.n);
        return dr::mulsign(n, n.z());
    }

    /// Test via the geometric normal if ``si.wi`` arrives on the back side.
    static Mask on_back_side(const SurfaceInteraction3f &si, const Vector3f &n) {
        return dr::dot(n, si.wi) < 0.f;
    }

    static void to_front(SurfaceInteraction3f &si) {
        si.wi.z() = dr::abs(si.wi.z());
    }

    /// Do ``si.wi`` and ``wo`` lie on the same geometric side? Always true
    /// when the nested model transmits.
    Mask same_side(const Vector3f &n, const Vector3f &wo, Mask back) const {
        if (this->has_flag(BSDFFlags::Transmission))
            return true;
        return (dr::dot(n, wo) < 0.f) == back;
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx_,
                                             const SurfaceInteraction3f &si_,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        using Result = std::pair<BSDFSample3f, Spectrum>;

        SurfaceInteraction3f si(si_);
        BSDFContext ctx(ctx_);
        Result result = dr::zeros<Result>();
        Vector3f n = local_normal(si);
        Mask back = on_back_side(si, n);
        to_front(si);

        if (m_brdf[0] == m_brdf[1]) {
            result = m_brdf[0]->sample(ctx, si, sample1, sample2, active);
        } else {
            Mask front_side = !back && active,
                 back_side  = back && active;

            if (dr::any_or<true>(front_side))
                dr::masked(result, front_side) =
                    m_brdf[0]->sample(ctx, si, sample1, sample2, front_side);

            if (dr::any_or<true>(back_side)) {
                if (ctx.component != (uint32_t) -1)
                    ctx.component -= (uint32_t) m_brdf[0]->component_count();

                dr::masked(result, back_side) =
                    m_brdf[1]->sample(ctx, si, sample1, sample2, back_side);
            }
        }

        // Directions sampled on the back leave through the back
        dr::masked(result.first.wo.z(), back) *= -1.f;

        Mask invalid = active && !same_side(n, result.first.wo, back);
        dr::masked(result.first.pdf, invalid) = 0.f;
        dr::masked(result.second, invalid) = 0.f;

        return result;
    }

    Spectrum eval(const BSDFContext &ctx_, const SurfaceInteraction3f &si_,
                  const Vector3f &wo_, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        SurfaceInteraction3f si(si_);
        BSDFContext ctx(ctx_);
        Vector3f wo(wo_);
        Spectrum result = 0.f;
        Vector3f n = local_normal(si);
        Mask back = on_back_side(si, n),
             valid = same_side(n, wo_, back);
        active &= valid;
        to_front(si);
        dr::masked(wo.z(), back) *= -1.f;

        if (m_brdf[0] == m_brdf[1]) {
            result = m_brdf[0]->eval(ctx, si, wo, active);
        } else {
            Mask front_side = !back && active,
                 back_side  = back && active;

            if (dr::any_or<true>(front_side))
                result = m_brdf[0]->eval(ctx, si, wo, front_side);

            if (dr::any_or<true>(back_side)) {
                if (ctx.component != (uint32_t) -1)
                    ctx.component -= (uint32_t) m_brdf[0]->component_count();

                dr::masked(result, back_side) =
                    m_brdf[1]->eval(ctx, si, wo, back_side);
            }
        }

        return dr::select(valid, result, Spectrum(0.f));
    }

    Float pdf(const BSDFContext &ctx_, const SurfaceInteraction3f &si_,
              const Vector3f &wo_, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        SurfaceInteraction3f si(si_);
        BSDFContext ctx(ctx_);
        Vector3f wo(wo_);
        Float result = 0.f;
        Vector3f n = local_normal(si);
        Mask back = on_back_side(si, n),
             valid = same_side(n, wo_, back);
        active &= valid;
        to_front(si);
        dr::masked(wo.z(), back) *= -1.f;

        if (m_brdf[0] == m_brdf[1]) {
            result = m_brdf[0]->pdf(ctx, si, wo, active);
        } else {
            Mask front_side = !back && active,
                 back_side  = back && active;

            if (dr::any_or<true>(front_side))
                result = m_brdf[0]->pdf(ctx, si, wo, front_side);

            if (dr::any_or<true>(back_side)) {
                if (ctx.component != (uint32_t) -1)
                    ctx.component -= (uint32_t) m_brdf[0]->component_count();

                dr::masked(result, back_side) = m_brdf[1]->pdf(ctx, si, wo, back_side);
            }
        }

        return dr::select(valid, result, Float(0.f));
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx_,
                                        const SurfaceInteraction3f &si_,
                                        const Vector3f &wo_,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        SurfaceInteraction3f si(si_);
        BSDFContext ctx(ctx_);
        Vector3f wo(wo_);

        Spectrum value = 0.f;
        Float pdf = 0.f;
        Vector3f n = local_normal(si);
        Mask back = on_back_side(si, n),
             valid = same_side(n, wo_, back);
        active &= valid;
        to_front(si);
        dr::masked(wo.z(), back) *= -1.f;

        if (m_brdf[0] == m_brdf[1]) {
            std::tie(value, pdf) = m_brdf[0]->eval_pdf(ctx, si, wo, active);
        } else {
            Mask front_side = !back && active,
                 back_side  = back && active;

            if (dr::any_or<true>(front_side))
                std::tie(value, pdf) = m_brdf[0]->eval_pdf(ctx, si, wo, front_side);

            if (dr::any_or<true>(back_side)) {
                if (ctx.component != (uint32_t) -1)
                    ctx.component -= (uint32_t) m_brdf[0]->component_count();

                auto [back_value, back_pdf] = m_brdf[1]->eval_pdf(ctx, si, wo, back_side);

                dr::masked(value, back_side) = back_value;
                dr::masked(pdf, back_side) = back_pdf;
            }
        }

        return { dr::select(valid, value, Spectrum(0.f)),
                 dr::select(valid, pdf, Float(0.f)) };
    }

    /// Evaluate a side-dependent quantity of the nested models
    template <typename Result, typename Func>
    Result per_side(const SurfaceInteraction3f &si_, Mask active,
                    Func &&func) const {
        SurfaceInteraction3f si(si_);
        Mask back = on_back_side(si, local_normal(si));
        to_front(si);

        if (m_brdf[0] == m_brdf[1])
            return func(m_brdf[0], si, active);

        Result result = dr::zeros<Result>();
        Mask front_side = !back && active,
             back_side  = back && active;

        if (dr::any_or<true>(front_side))
            result = func(m_brdf[0], si, front_side);

        if (dr::any_or<true>(back_side))
            dr::masked(result, back_side) = func(m_brdf[1], si, back_side);

        return result;
    }

    Spectrum eval_null(const SurfaceInteraction3f &si, Mask active) const override {
        return per_side<Spectrum>(si, active,
            [](const Base *bsdf, const SurfaceInteraction3f &si, Mask active) {
                return bsdf->eval_null(si, active);
            });
    }

    BSDFFeatures3f eval_features(const SurfaceInteraction3f &si,
                                 Mask active) const override {
        BSDFFeatures3f features = per_side<BSDFFeatures3f>(si, active,
            [](const Base *bsdf, const SurfaceInteraction3f &si, Mask active) {
                return bsdf->eval_features(si, active);
            });
        // The nested BSDF saw the back side mirrored to the front
        Vector3f n = si.sh_frame.n;
        dr::masked(features.wt, on_back_side(si, local_normal(si))) =
            dr::fmadd(n, -2.f * dr::dot(features.wt, n), features.wt);
        return features;
    }

    Mask has_attribute(const std::string &name, Mask active) const override {
        if (m_brdf[0] == m_brdf[1])
            return m_brdf[0]->has_attribute(name, active);
        else
            return m_brdf[0]->has_attribute(name, active) ||
                   m_brdf[1]->has_attribute(name, active);
    }

    UnpolarizedSpectrum eval_attribute(const std::string &name,
                                       const SurfaceInteraction3f &si,
                                       Mask active) const override {
        return per_side<UnpolarizedSpectrum>(si, active,
            [&name](const Base *bsdf, const SurfaceInteraction3f &si, Mask active) {
                return bsdf->eval_attribute(name, si, active);
            });
    }

    Float eval_attribute_1(const std::string &name,
                           const SurfaceInteraction3f &si,
                           Mask active) const override {
        return per_side<Float>(si, active,
            [&name](const Base *bsdf, const SurfaceInteraction3f &si, Mask active) {
                return bsdf->eval_attribute_1(name, si, active);
            });
    }

    Color3f eval_attribute_3(const std::string &name,
                            const SurfaceInteraction3f &si,
                            Mask active) const override {
        return per_side<Color3f>(si, active,
            [&name](const Base *bsdf, const SurfaceInteraction3f &si, Mask active) {
                return bsdf->eval_attribute_3(name, si, active);
            });
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "TwoSided[" << std::endl
            << "  brdf[0] = " << string::indent(m_brdf[0]) << "," << std::endl
            << "  brdf[1] = " << string::indent(m_brdf[1]) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(TwoSidedBRDF)
protected:
    ref<Base> m_brdf[2];

    MI_TRAVERSE_CB(Base, m_brdf[0], m_brdf[1])
};

MI_EXPORT_PLUGIN(TwoSidedBRDF)
NAMESPACE_END(mitsuba)
