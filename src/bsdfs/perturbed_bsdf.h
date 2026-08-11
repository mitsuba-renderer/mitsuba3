#pragma once

#include <mitsuba/render/bsdf.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Computes Microfacet-based shadowing term for bump/normal maps.
 *
 * Implements Estevez et al., "A Microfacet-Based Shadowing Function to
 * Solve the Bump Terminator Problem", Ray Tracing Gems 2019.
 *
 * Args:
 *     perturbed_n: The perturbed normal in a coordinate frame that is
 *         relative to the original shading frame.
 *
 *     wo: Outgoing direction in the coordinate system of the unperturbed
 *         shading frame.
 *
 * Returns:
 *     The shadowing term that is used to attenuate the BSDF response.
 */
template <typename Float>
Float eval_shadow_terminator(const Normal<Float, 3> &perturbed_n,
                             const Vector<Float, 3> &wo) {
    using Frame3f = Frame<Float>;

    Float alpha2 = dr::minimum(0.125f * Frame3f::tan_theta_2(perturbed_n), 1.f);
    return 2.f / (1.f + dr::sqrt(1.f + alpha2 * Frame3f::tan_theta_2(wo)));
}

/**
 * CRTP Base class of  ``bumpmap`` and ``normalmap``, which are largely
 * identical except for a method ``perturbation(si, active)`` which computes
 * the (un-normalized) * perturbed normal in shading coordinates.
 */
template <typename Derived, typename Float, typename Spectrum>
class PerturbedBSDF : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES()

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        auto [perturbed_si, frame_p_local] = perturb(si, active);
        auto [bs, weight] = m_nested_bsdf->sample(ctx, perturbed_si, sample1,
                                                  sample2, active);
        active &= dr::any(unpolarized_spectrum(weight) != 0.f);

        // Transform the sampled direction back and check its orientation
        Vector3f wo = frame_p_local.to_world(bs.wo);
        active &= Frame3f::cos_theta(bs.wo) * Frame3f::cos_theta(wo) > 0.f;

        bs.wo  = wo;
        bs.pdf = dr::select(active, bs.pdf, 0.f);

        if (m_use_shadowing_function)
            weight *= eval_shadow_terminator(frame_p_local.n, bs.wo);

        return { bs, weight & active };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        auto [perturbed_si, perturbed_wo, frame_p_local] = perturb(si, wo, active);
        Spectrum value =
            m_nested_bsdf->eval(ctx, perturbed_si, perturbed_wo, active);

        if (m_use_shadowing_function)
            value *= eval_shadow_terminator(frame_p_local.n, wo);

        return value & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        auto [perturbed_si, perturbed_wo, frame_p_local] = perturb(si, wo, active);
        return dr::select(
            active, m_nested_bsdf->pdf(ctx, perturbed_si, perturbed_wo, active),
            0.f);
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        auto [perturbed_si, perturbed_wo, frame_p_local] = perturb(si, wo, active);
        auto [value, pdf] =
            m_nested_bsdf->eval_pdf(ctx, perturbed_si, perturbed_wo, active);

        if (m_use_shadowing_function)
            value *= eval_shadow_terminator(frame_p_local.n, wo);

        return { value & active, dr::select(active, pdf, 0.f) };
    }

    Frame3f sh_frame(const SurfaceInteraction3f &si, Mask active) const override {
        return frames(si, active).second;
    }

    Spectrum eval_diffuse_reflectance(const SurfaceInteraction3f &si,
                                      Mask active) const override {
        return m_nested_bsdf->eval_diffuse_reflectance(si, active);
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("nested_bsdf", m_nested_bsdf, ParamFlags::Differentiable);
    }

protected:
    PerturbedBSDF(const Properties &props, uint32_t extra_flags = 0)
        : Base(props) {
        for (auto &prop : props.objects()) {
            if (Base *bsdf = prop.try_get<Base>()) {
                if (m_nested_bsdf)
                    Throw("Only a single BSDF child object can be specified.");
                m_nested_bsdf = bsdf;
            }
        }
        if (!m_nested_bsdf)
            Throw("Exactly one BSDF child object must be specified.");

        m_flip_invalid_normals   = props.get<bool>("flip_invalid_normals", true);
        m_use_shadowing_function = props.get<bool>("use_shadowing_function", true);

        m_flags = m_nested_bsdf->flags() | extra_flags;
        m_components.clear();
        for (size_t i = 0; i < m_nested_bsdf->component_count(); ++i)
            m_components.push_back(m_nested_bsdf->flags(i) | extra_flags);
    }

    /**
     * Build the frame that the perturbation induces, both relative to
     * ``si.sh_frame`` and in the world coordinate system.
     */
    std::pair<Frame3f, Frame3f> frames(const SurfaceInteraction3f &si,
                                       const Mask& active) const {
        Normal3f n = ((const Derived *) this)->perturbation(si, active);

        if (m_flip_invalid_normals) {
            // Keep the shading normal on the side of the incident direction
            // :cite:`Schuessler2017Microfacet`
            Mask flip = Frame3f::cos_theta(si.wi) * dr::dot(si.wi, n) <= 0.f;
            n[flip] = Normal3f(-n.x(), -n.y(), n.z());
        }

        // Be robust to under/overflow
        Float inv_len = dr::rsqrt(dr::squared_norm(n));
        n = dr::select(inv_len > 0.f && dr::isfinite(inv_len),
                       Normal3f(n * inv_len), Normal3f(0, 0, 1));

        // Gram-Schmidt renormalization
        Vector3f s   = dr::fnmadd(Vector3f(n), n.x(), Vector3f(1, 0, 0));
        Float inv_st = dr::rsqrt(dr::squared_norm(s));

        Frame3f frame_p_local;
        frame_p_local.n = n;
        frame_p_local.s = dr::select(dr::isfinite(inv_st), s * inv_st,
                                     Vector3f(0, 1, 0));
        frame_p_local.t = dr::cross(frame_p_local.n, frame_p_local.s);

        const Frame3f &base = si.sh_frame;
        Frame3f frame_p_world;
        frame_p_world.n = base.to_world(frame_p_local.n);
        frame_p_world.s = base.to_world(frame_p_local.s);
        frame_p_world.t = base.to_world(frame_p_local.t);

        return { frame_p_local, frame_p_world };
    }

    /**
     * Build the interaction to hand to the nested BSDF. Also returns that
     * frame relative to
     * ``si.sh_frame``.
     */
    std::pair<SurfaceInteraction3f, Frame3f>
    perturb(const SurfaceInteraction3f &si, const Mask &active) const {
        auto [frame_p_local, frame_p_world] = frames(si, active);

        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = frame_p_world;
        perturbed_si.wi       = frame_p_local.to_local(si.wi);

        return { std::move(perturbed_si), std::move(frame_p_local) };
    }

    /**
     * Also express ``wo`` in the perturbed frame, and clear ``active`` if the
     * original/perturbed versions lie on different sides of the shading frame.
     */
    std::tuple<SurfaceInteraction3f, Vector3f, Frame3f>
    perturb(const SurfaceInteraction3f &si, const Vector3f &wo, Mask &active) const {
        auto [perturbed_si, frame_p_local] = perturb(si, active);

        Vector3f perturbed_wo = frame_p_local.to_local(wo);
        active &= Frame3f::cos_theta(wo) * Frame3f::cos_theta(perturbed_wo) > 0.f;

        return { std::move(perturbed_si), perturbed_wo, frame_p_local };
    }

    ref<Base> m_nested_bsdf;
    bool m_flip_invalid_normals;
    bool m_use_shadowing_function;

    MI_TRAVERSE_CB(Base, m_nested_bsdf)
};

NAMESPACE_END(mitsuba)
