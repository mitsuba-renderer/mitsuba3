#pragma once

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Computes Microfacet-based shadowing term for bump/normal maps.
 *
 * Implements Estevez et al., "A Microfacet-Based Shadowing Function to
 * Solve the Bump Terminator Problem", Ray Tracing Gems 2019.
 *
 * \param perturbed_n
 *     The perturbed normal in a coordinate frame that is relative to the
 *     original shading frame.
 * \param wo
 *     Outgoing direction in the coordinate system of the unperturbed shading
 *     frame.
 *
 * \return The shadowing term that is used to attenuate the BSDF response.
 */
template <typename Float, typename Frame3f = Frame<Float>>
Float eval_shadow_terminator(const Normal<Float, 3> &perturbed_n,
                             const Vector<Float, 3> &wo) {
    Float alpha2 = dr::minimum(0.125f * Frame3f::tan_theta_2(perturbed_n), 1.f);
    return 2.f / (1.f + dr::sqrt(1.f + alpha2 * Frame3f::tan_theta_2(wo)));
}

NAMESPACE_END(mitsuba)
