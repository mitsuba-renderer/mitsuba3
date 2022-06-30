#pragma once

#include <mitsuba/core/frame.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/quad.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/// Supported normal distribution functions
enum class MicrofacetType : uint32_t {
    /// Beckmann distribution derived from Gaussian random surfaces
    Beckmann = 0,

    /// GGX: Long-tailed distribution for very rough surfaces (aka. Trowbridge-Reitz distr.)
    GGX = 1
};

MI_INLINE std::ostream &operator<<(std::ostream &os, MicrofacetType tp) {
    switch (tp) {
        case MicrofacetType::Beckmann:
            os << "beckmann";
            break;
        case MicrofacetType::GGX:
            os << "ggx";
            break;
        default:
            Throw("Unknown microfacet distribution: %s", tp);
    }
    return os;
}

/**
 * \brief Implementation of the Beckman and GGX / Trowbridge-Reitz microfacet
 * distributions and various useful sampling routines
 *
 * Based on the papers
 *
 *   "Microfacet Models for Refraction through Rough Surfaces"
 *    by Bruce Walter, Stephen R. Marschner, Hongsong Li, and Kenneth E. Torrance
 *
 * and
 *
 *   "Importance Sampling Microfacet-Based BSDFs using the Distribution of Visible Normals"
 *    by Eric Heitz and Eugene D'Eon
 *
 * The visible normal sampling code was provided by Eric Heitz and Eugene D'Eon.
 * An improvement of the Beckmann model sampling routine is discussed in
 *
 *   "An Improved Visible Normal Sampling Routine for the Beckmann Distribution"
 *    by Wenzel Jakob
 *
 * An improvement of the GGX model sampling routine is discussed in
 *    "A Simpler and Exact Sampling Routine for the GGX Distribution of Visible Normals"
 *     by Eric Heitz
 */
template <typename Float, typename Spectrum>
class MicrofacetDistribution {
public:
    MI_IMPORT_TYPES()

    /**
     * Create an isotropic microfacet distribution of the specified type
     *
     * \param type
     *     The desired type of microfacet distribution
     * \param alpha
     *     The surface roughness
     */
    MicrofacetDistribution(MicrofacetType type, Float alpha, bool sample_visible = true)
        : m_type(type), m_alpha_u(alpha), m_alpha_v(alpha),
          m_sample_visible(sample_visible) {
        configure();
    }

    /**
     * Create an anisotropic microfacet distribution of the specified type
     *
     * \param type
     *     The desired type of microfacet distribution
     * \param alpha_u
     *     The surface roughness in the tangent direction
     * \param alpha_v
     *     The surface roughness in the bitangent direction
     */
    MicrofacetDistribution(MicrofacetType type, Float alpha_u, Float alpha_v,
                           bool sample_visible = true)
        : m_type(type), m_alpha_u(alpha_u), m_alpha_v(alpha_v),
          m_sample_visible(sample_visible) {
        configure();
    }

    /**
     * \brief Create a microfacet distribution from a Property data
     * structure
     */
    MicrofacetDistribution(const Properties &props,
                           MicrofacetType type = MicrofacetType::Beckmann,
                           Float alpha_u       = Float(0.1f),
                           Float alpha_v       = Float(0.1f),
                           bool sample_visible = true)
        : m_type(type), m_alpha_u(alpha_u), m_alpha_v(alpha_v) {

        if (props.has_property("distribution")) {
            std::string distr = string::to_lower(props.string("distribution"));
            if (distr == "beckmann")
                m_type = MicrofacetType::Beckmann;
            else if (distr == "ggx")
                m_type = MicrofacetType::GGX;
            else
                Throw("Specified an invalid distribution \"%s\", must be "
                      "\"beckmann\" or \"ggx\"!", distr.c_str());
        }

        if (props.has_property("alpha")) {
            m_alpha_u = m_alpha_v = props.get<ScalarFloat>("alpha");
            if (props.has_property("alpha_u") || props.has_property("alpha_v"))
                Throw("Microfacet model: please specify"
                      "either 'alpha' or 'alpha_u'/'alpha_v'.");
        } else if (props.has_property("alpha_u") || props.has_property("alpha_v")) {
            if (!props.has_property("alpha_u") || !props.has_property("alpha_v"))
                Throw("Microfacet model: both 'alpha_u' and 'alpha_v' must be specified.");
            if (props.has_property("alpha"))
                Throw("Microfacet model: please specify"
                    "either 'alpha' or 'alpha_u'/'alpha_v'.");
            m_alpha_u = props.get<ScalarFloat>("alpha_u");
            m_alpha_v = props.get<ScalarFloat>("alpha_v");
        }

        if (alpha_u == 0.f || alpha_v == 0.f)
            Log(Warn,
                "Cannot create a microfacet distribution with alpha_u/alpha_v=0 (clamped to 10^-4). "
                "Please use the corresponding smooth reflectance model to get zero roughness.");

        m_sample_visible = props.get<bool>("sample_visible", sample_visible);

        configure();
    }

public:
    /// Return the distribution type
    MicrofacetType type() const { return m_type; }

    /// Return the roughness (isotropic case)
    Float alpha() const { return m_alpha_u; }

    /// Return the roughness along the tangent direction
    Float alpha_u() const { return m_alpha_u; }

    /// Return the roughness along the bitangent direction
    Float alpha_v() const { return m_alpha_v; }

    /// Return whether or not only visible normals are sampled?
    bool sample_visible() const { return m_sample_visible; }

    /// Is this an isotropic microfacet distribution?
    bool is_isotropic() const {
        if constexpr (dr::is_jit_v<Float>)
            return m_alpha_u.index() == m_alpha_v.index();
        else
            return m_alpha_u == m_alpha_v;
    }

    /// Is this an anisotropic microfacet distribution?
    bool is_anisotropic() const { return m_alpha_u != m_alpha_v; }

    /// Scale the roughness values by some constant
    void scale_alpha(Float value) {
        m_alpha_u *= value;
        m_alpha_v *= value;
    }

    /**
     * \brief Evaluate the microfacet distribution function
     *
     * \param m
     *     The microfacet normal
     */
    Float eval(const Vector3f &m) const {
        Float alpha_uv = m_alpha_u * m_alpha_v,
              cos_theta         = Frame3f::cos_theta(m),
              cos_theta_2       = dr::sqr(cos_theta),
              result;

        if (m_type == MicrofacetType::Beckmann) {
            // Beckmann distribution function for Gaussian random surfaces
            result = dr::exp(-(dr::sqr(m.x() / m_alpha_u) +
                               dr::sqr(m.y() / m_alpha_v)) /
                             cos_theta_2) /
                     (dr::Pi<Float> * alpha_uv * dr::sqr(cos_theta_2));
        } else {
            // GGX / Trowbridge-Reitz distribution function
            result =
                dr::rcp(dr::Pi<Float> * alpha_uv *
                        dr::sqr(dr::sqr(m.x() / m_alpha_u) +
                                dr::sqr(m.y() / m_alpha_v) + dr::sqr(m.z())));
        }

        // Prevent potential numerical issues in other stages of the model
        return dr::select(result * cos_theta > 1e-20f, result, 0.f);
    }

    /**
     * \brief Returns the density function associated with
     * the \ref sample() function.
     *
     * \param wi
     *     The incident direction (only relevant if visible normal sampling is used)
     *
     * \param m
     *     The microfacet normal
     */
    Float pdf(const Vector3f &wi, const Vector3f &m) const {
        Float result = eval(m);

        if (m_sample_visible)
            result *= smith_g1(wi, m) * dr::abs_dot(wi, m) / Frame3f::cos_theta(wi);
        else
            result *= Frame3f::cos_theta(m);

        return result;
    }

    /**
     * \brief Draw a sample from the microfacet normal distribution
     *  and return the associated probability density
     *
     * \param wi
     *    The incident direction. Only used if
     *    visible normal sampling is enabled.
     *
     * \param sample
     *    A uniformly distributed 2D sample
     *
     * \return A tuple consisting of the sampled microfacet normal
     *         and the associated solid angle density
     */
    std::pair<Normal3f, Float> sample(const Vector3f &wi,
                                      const Point2f &sample) const {
        if (!m_sample_visible) {
            Float sin_phi, cos_phi, cos_theta, cos_theta_2, alpha_2, pdf;

            // Sample azimuth component (identical for Beckmann & GGX)
            if (is_isotropic()) {
                std::tie(sin_phi, cos_phi) = dr::sincos((2.f * dr::Pi<Float>) * sample.y());

                alpha_2 = m_alpha_u * m_alpha_u;
            } else {
                Float ratio  = m_alpha_v / m_alpha_u,
                      tmp    = ratio * dr::tan((2.f * dr::Pi<Float>) * sample.y());

                cos_phi = dr::rsqrt(dr::fmadd(tmp, tmp, 1.f));
                cos_phi = dr::mulsign(cos_phi, dr::abs(sample.y() - .5f) - .25f);

                sin_phi = cos_phi * tmp;

                alpha_2 = dr::rcp(dr::sqr(cos_phi / m_alpha_u) +
                                  dr::sqr(sin_phi / m_alpha_v));
            }

            // Sample elevation component
            if (m_type == MicrofacetType::Beckmann) {
                // Beckmann distribution function for Gaussian random surfaces
                cos_theta = dr::rsqrt(dr::fnmadd(alpha_2, dr::log(1.f - sample.x()), 1.f));
                cos_theta_2 = dr::sqr(cos_theta);

                // Compute probability density of the sampled position
                Float cos_theta_3 = dr::maximum(cos_theta_2 * cos_theta, 1e-20f);
                pdf = (1.f - sample.x()) / (dr::Pi<Float> * m_alpha_u * m_alpha_v * cos_theta_3);
            } else {
                // GGX / Trowbridge-Reitz distribution function
                Float tan_theta_m_2 = alpha_2 * sample.x() / (1.f - sample.x());
                cos_theta = dr::rsqrt(1.f + tan_theta_m_2);
                cos_theta_2 = dr::sqr(cos_theta);

                // Compute probability density of the sampled position
                Float temp = 1.f + tan_theta_m_2 / alpha_2,
                      cos_theta_3 = dr::maximum(cos_theta_2 * cos_theta, 1e-20f);
                pdf = dr::rcp(dr::Pi<Float> * m_alpha_u * m_alpha_v * cos_theta_3 * dr::sqr(temp));
            }

            Float sin_theta = dr::sqrt(1.f - cos_theta_2);

            return {
                Normal3f(cos_phi * sin_theta,
                         sin_phi * sin_theta,
                         cos_theta),
                pdf
            };
        } else {
            // Visible normal sampling.
            Float sin_phi, cos_phi, cos_theta;

            // Step 1: stretch wi
            Vector3f wi_p = dr::normalize(Vector3f(
                m_alpha_u * wi.x(),
                m_alpha_v * wi.y(),
                wi.z()
            ));

            std::tie(sin_phi, cos_phi) = Frame3f::sincos_phi(wi_p);
            cos_theta = Frame3f::cos_theta(wi_p);

            // Step 2: simulate P22_{wi}(slope.x, slope.y, 1, 1)
            Vector2f slope = sample_visible_11(cos_theta, sample);

            // Step 3: rotate & unstretch
            slope = Vector2f(
                dr::fmsub(cos_phi, slope.x(), sin_phi * slope.y()) * m_alpha_u,
                dr::fmadd(sin_phi, slope.x(), cos_phi * slope.y()) * m_alpha_v);

            // Step 4: compute normal & PDF
            Normal3f m = dr::normalize(Vector3f(-slope.x(), -slope.y(), 1));

            Float pdf = eval(m) * smith_g1(wi, m) * dr::abs_dot(wi, m) /
                        Frame3f::cos_theta(wi);

            return { m, pdf };
        }
    }

    /// Smith's separable shadowing-masking approximation
    Float G(const Vector3f &wi, const Vector3f &wo, const Vector3f &m) const {
        return smith_g1(wi, m) * smith_g1(wo, m);
    }

    /**
     * \brief Smith's shadowing-masking function for a single direction
     *
     * \param v
     *     An arbitrary direction
     * \param m
     *     The microfacet normal
     */
    Float smith_g1(const Vector3f &v, const Vector3f &m) const {
        Float xy_alpha_2 = dr::sqr(m_alpha_u * v.x()) + dr::sqr(m_alpha_v * v.y()),
              tan_theta_alpha_2 = xy_alpha_2 / dr::sqr(v.z()),
              result;

        if (m_type == MicrofacetType::Beckmann) {
            Float a = dr::rsqrt(tan_theta_alpha_2), a_sqr = dr::sqr(a);
            /* Use a fast and accurate (<0.35% rel. error) rational
               approximation to the shadowing-masking function */
            result = dr::select(a >= 1.6f, 1.f,
                                (3.535f * a + 2.181f * a_sqr) /
                                    (1.f + 2.276f * a + 2.577f * a_sqr));
        } else {
            result = 2.f / (1.f + dr::sqrt(1.f + tan_theta_alpha_2));
        }

        // Perpendicular incidence -- no shadowing/masking
        dr::masked(result, dr::eq(xy_alpha_2, 0.f)) = 1.f;

        /* Ensure consistent orientation (can't see the back
           of the microfacet from the front and vice versa) */
        dr::masked(result, dr::dot(v, m) * Frame3f::cos_theta(v) <= 0.f) = 0.f;

        return result;
    }

    /// \brief Visible normal sampling code for the alpha=1 case
    Vector2f sample_visible_11(Float cos_theta_i, Point2f sample) const {
        if (m_type == MicrofacetType::Beckmann) {
            /* The original inversion routine from the paper contained
               discontinuities, which causes issues for QMC integration
               and techniques like Kelemen-style MLT. The following code
               performs a numerical inversion with better behavior */

            Float tan_theta_i =
                dr::safe_sqrt(dr::fnmadd(cos_theta_i, cos_theta_i, 1.f)) /
                cos_theta_i;
            Float cot_theta_i = dr::rcp(tan_theta_i);

            /* Search interval -- everything is parameterized
               in the erf() domain */
            Float maxval = dr::erf(cot_theta_i);

            /* Start with a good initial guess (analytic solution for
               theta_i = pi/2, which is the most nonlinear case) */
            sample = dr::maximum(dr::minimum(sample, 1.f - 1e-6f), 1e-6f);
            Float x = maxval - (maxval + 1.f) * dr::erf(dr::sqrt(-dr::log(sample.x())));

            // Normalization factor for the CDF
            sample.x() *= 1.f + maxval + dr::InvSqrtPi<Float> *
                          tan_theta_i * dr::exp(-dr::sqr(cot_theta_i));

            // Three Newton iterations
            DRJIT_NOUNROLL for (size_t i = 0; i < 3; ++i) {
                Float slope = dr::erfinv(x),
                      value = 1.f + x + dr::InvSqrtPi<Float> * tan_theta_i *
                              dr::exp(-dr::sqr(slope)) - sample.x(),
                      derivative = 1.f - slope * tan_theta_i;

                x -= value / derivative;
            }

            // Now convert back into a slope value
            return dr::erfinv(Vector2f(x, dr::fmsub(2.f, sample.y(), 1.f)));
        } else {
            // Choose a projection direction and re-scale the sample
            Point2f p = warp::square_to_uniform_disk_concentric(sample);

            Float s = 0.5f * (1.f + cos_theta_i);
            p.y() = dr::lerp(dr::safe_sqrt(1.f - dr::sqr(p.x())), p.y(), s);

            // Project onto chosen side of the hemisphere
            Float x = p.x(), y = p.y(),
                  z = dr::safe_sqrt(1.f - dr::squared_norm(p));

            // Convert to slope
            Float sin_theta_i = dr::safe_sqrt(1.f - dr::sqr(cos_theta_i));
            Float norm = dr::rcp(dr::fmadd(sin_theta_i, y, cos_theta_i * z));
            return Vector2f(dr::fmsub(cos_theta_i, y, sin_theta_i * z), x) * norm;
        }
    }


protected:
    void configure() {
        m_alpha_u = dr::maximum(m_alpha_u, 1e-4f);
        m_alpha_v = dr::maximum(m_alpha_v, 1e-4f);
    }

    /// Compute the squared 1D roughness along direction \c v
    Float project_roughness_2(const Vector3f &v) const {
        if (is_isotropic())
            return dr::sqr(m_alpha_u);

        Float sin_phi_2, cos_phi_2;
        std::tie(sin_phi_2, cos_phi_2) = Frame3f::sincos_phi_2(v);

        return sin_phi_2 * dr::sqr(m_alpha_v) + cos_phi_2 * dr::sqr(m_alpha_u);
    }

protected:
    MicrofacetType m_type;
    Float m_alpha_u, m_alpha_v;
    bool  m_sample_visible;
};

template <typename Float, typename Spectrum>
std::ostream &operator<<(std::ostream &os, const MicrofacetDistribution<Float, Spectrum> &md) {
    os << "MicrofacetDistribution[" << std::endl
       << "  type = ";
    if (md.type() == MicrofacetType::Beckmann)
        os << "beckmann";
    else
        os << "ggx";
    os << "," << std::endl
       << "  alpha_u = " << md.alpha_u() << "," << std::endl
       << "  alpha_v = " << md.alpha_v() << "," << std::endl
       << "  sample_visible = " << md.sample_visible() << std::endl
       << "]";
    return os;
}

template <typename Float, typename MicrofaceDistributionP>
Float eval_reflectance(const MicrofaceDistributionP &distr,
                       const Vector<Float, 3> &wi, dr::scalar_t<Float> eta) {
    MI_IMPORT_CORE_TYPES()

    if (!distr.sample_visible())
        Throw("eval_reflectance(): requires visible normal sampling!");

    int res = eta > 1 ? 32 : 128;

    using FloatX = dr::DynamicArray<dr::scalar_t<Float>>;
    auto [nodes, weights] = quad::gauss_legendre<FloatX>(res);
    Float result = dr::zeros<Float>(dr::width(wi));

    auto [nodes_x, nodes_y]     = dr::meshgrid(nodes, nodes);
    auto [weights_x, weights_y] = dr::meshgrid(weights, weights);

    using FloatP = dr::Packet<dr::scalar_t<Float>>;
    using Normal3fP = Normal<FloatP, 3>;
    using Vector3fP = Vector<FloatP, 3>;

    size_t packet_count = dr::width(wi) / FloatP::Size;

    Assert(dr::width(wi) % FloatP::Size == 0);

    for (size_t i = 0; i < packet_count; ++i) {
        Vector3fP wi_p;
        wi_p.x() = dr::load<FloatP>(wi.x().data() + i * FloatP::Size);
        wi_p.y() = dr::load<FloatP>(wi.y().data() + i * FloatP::Size);
        wi_p.z() = dr::load<FloatP>(wi.z().data() + i * FloatP::Size);

        FloatP result_p = 0.f;

        for (size_t j = 0; j < dr::width(nodes_x); ++j) {
            ScalarVector2f node = { nodes_x[j], nodes_y[j] };
            ScalarVector2f weight = { weights_x[j], weights_y[j] };
            node = dr::fmadd(node, 0.5f, 0.5f);

            Normal3fP m = std::get<0>(distr.sample(wi_p, node));
            Vector3fP wo = reflect(wi_p, m);
            FloatP f = std::get<0>(fresnel(dr::dot(wi_p, m), FloatP(eta)));
            FloatP smith = distr.smith_g1(wo, m) * f;
            dr::masked(smith, wo.z() <= 0.f || wi_p.z() <= 0.f) = 0.f;
            result_p += smith * dr::prod(weight) * 0.25f;
        }

        dr::store(result.data() + i * FloatP::Size, result_p);
    }

    return result;
}

template <typename Float, typename MicrofaceDistributionP>
Float eval_transmittance(const MicrofaceDistributionP &distr,
                         Vector<Float, 3> &wi, dr::scalar_t<Float> eta) {
    MI_IMPORT_CORE_TYPES()

    if (!distr.sample_visible())
        Throw("eval_transmittance(): requires visible normal sampling!");

    int res = eta > 1 ? 32 : 128;

    using ScalarFloat = dr::scalar_t<Float>;
    using FloatX = dr::DynamicArray<ScalarFloat>;
    auto [nodes, weights] = quad::gauss_legendre<FloatX>(res);
    Float result = dr::zeros<Float>(dr::width(wi));

    auto [nodes_x, nodes_y]     = dr::meshgrid(nodes, nodes);
    auto [weights_x, weights_y] = dr::meshgrid(weights, weights);

    using FloatP = dr::Packet<dr::scalar_t<Float>>;
    using Normal3fP = Normal<FloatP, 3>;
    using Vector3fP = Vector<FloatP, 3>;

    size_t packet_count = dr::width(wi) / FloatP::Size;

    Assert(dr::width(wi) % FloatP::Size == 0);

    for (size_t i = 0; i < packet_count; ++i) {
        Vector3fP wi_p;
        wi_p.x() = dr::load<FloatP>(wi.x().data() + i * FloatP::Size);
        wi_p.y() = dr::load<FloatP>(wi.y().data() + i * FloatP::Size);
        wi_p.z() = dr::load<FloatP>(wi.z().data() + i * FloatP::Size);

        FloatP result_p = 0.f;

        for (size_t j = 0; j < dr::width(nodes_x); ++j) {
            ScalarVector2f node = { nodes_x[j], nodes_y[j] };
            ScalarVector2f weight = { weights_x[j], weights_y[j] };
            node = dr::fmadd(node, 0.5f, 0.5f);

            Normal3fP m = std::get<0>(distr.sample(wi_p, node));
            auto [f, cos_theta_t, eta_it, eta_ti] = fresnel(dr::dot(wi_p, m), FloatP(eta));

            Vector3fP wo = refract(wi_p, m, cos_theta_t, eta_ti);
            FloatP smith = distr.smith_g1(wo, m) * (1.f - f);
            dr::masked(smith, wo.z() * wi_p.z() >= 0.f) = 0.f;

            result_p += smith * dr::prod(weight) * 0.25f;
        }

        dr::store(result.data() + i * FloatP::Size, result_p);
    }

    return result;
}

NAMESPACE_END(mitsuba)
