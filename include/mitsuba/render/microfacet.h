#pragma once

#include <enoki/special.h>
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

MTS_INLINE std::ostream &operator<<(std::ostream &os, MicrofacetType tp) {
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
    MTS_IMPORT_TYPES()

    static constexpr auto Pi        = math::Pi<scalar_t<Float>>;
    static constexpr auto InvSqrtPi = math::InvSqrtPi<scalar_t<Float>>;

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
            m_alpha_u = m_alpha_v = props.float_("alpha");
            if (props.has_property("alpha_u") || props.has_property("alpha_v"))
                Throw("Microfacet model: please specify"
                      "either 'alpha' or 'alpha_u'/'alpha_v'.");
        } else if (props.has_property("alpha_u") || props.has_property("alpha_v")) {
            if (!props.has_property("alpha_u") || !props.has_property("alpha_v"))
                Throw("Microfacet model: both 'alpha_u' and 'alpha_v' must be specified.");
            if (props.has_property("alpha"))
                Throw("Microfacet model: please specify"
                    "either 'alpha' or 'alpha_u'/'alpha_v'.");
            m_alpha_u = props.float_("alpha_u");
            m_alpha_v = props.float_("alpha_v");
        }

        if (alpha_u == 0.f || alpha_v == 0.f)
            Log(Warn,
                "Cannot create a microfacet distribution with alpha_u/alpha_v=0 (clamped to 10^-4). "
                "Please use the corresponding smooth reflectance model to get zero roughness.");

        m_sample_visible = props.bool_("sample_visible", sample_visible);

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
    bool is_isotropic() const { return m_alpha_u == m_alpha_v; }

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
              cos_theta_2       = sqr(cos_theta),
              result;

        if (m_type == MicrofacetType::Beckmann) {
            // Beckmann distribution function for Gaussian random surfaces
            result = exp(-(sqr(m.x() / m_alpha_u) + sqr(m.y() / m_alpha_v)) / cos_theta_2)
                / (Pi * alpha_uv * sqr(cos_theta_2));
        } else {
            // GGX / Trowbridge-Reitz distribution function
            result = rcp(Pi * alpha_uv *
                sqr(sqr(m.x() / m_alpha_u) + sqr(m.y() / m_alpha_v) + sqr(m.z())));
        }

        // Prevent potential numerical issues in other stages of the model
        return select(result * cos_theta > 1e-20f, result, 0.f);
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
            result *= smith_g1(wi, m) * abs_dot(wi, m) / Frame3f::cos_theta(wi);
        else
            result *= Frame3f::cos_theta(m);

        return result;
    }

    /**
     * \brief Draw a sample from the microfacet normal distribution
     *  and return the associated probability density
     *
     * \param sample
     *    A uniformly distributed 2D sample
     * \param pdf
     *    The probability density wrt. solid angles
     */
    std::pair<Normal3f, Float> sample(const Vector3f &wi,
                                      const Point2f &sample) const {
        if (!m_sample_visible) {
            Float sin_phi, cos_phi, cos_theta, cos_theta_2, alpha_2, pdf;

            // Sample azimuth component (identical for Beckmann & GGX)
            if (is_isotropic()) {
                std::tie(sin_phi, cos_phi) = sincos((2.f * Pi) * sample.y());

                alpha_2 = m_alpha_u * m_alpha_u;
            } else {
                Float ratio  = m_alpha_v / m_alpha_u,
                      tmp    = ratio * tan((2.f * Pi) * sample.y());

                cos_phi = rsqrt(fmadd(tmp, tmp, 1));
                cos_phi = mulsign(cos_phi, abs(sample.y() - .5f) - .25f);

                sin_phi = cos_phi * tmp;

                alpha_2 = rcp(sqr(cos_phi / m_alpha_u) +
                              sqr(sin_phi / m_alpha_v));
            }

            // Sample elevation component
            if (m_type == MicrofacetType::Beckmann) {
                // Beckmann distribution function for Gaussian random surfaces
                cos_theta = rsqrt(fnmadd(alpha_2, log(1.f - sample.x()), 1.f));
                cos_theta_2 = sqr(cos_theta);

                // Compute probability density of the sampled position
                Float cos_theta_3 = max(cos_theta_2 * cos_theta, 1e-20f);
                pdf = (1.f - sample.x()) / (Pi * m_alpha_u * m_alpha_v * cos_theta_3);
            } else {
                // GGX / Trowbridge-Reitz distribution function
                Float tan_theta_m_2 = alpha_2 * sample.x() / (1.f - sample.x());
                cos_theta = rsqrt(1.f + tan_theta_m_2);
                cos_theta_2 = sqr(cos_theta);

                // Compute probability density of the sampled position
                Float temp = 1.f + tan_theta_m_2 / alpha_2,
                      cos_theta_3 = max(cos_theta_2 * cos_theta, 1e-20f);
                pdf = rcp(Pi * m_alpha_u * m_alpha_v * cos_theta_3 * sqr(temp));
            }

            Float sin_theta = sqrt(1.f - cos_theta_2);

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
            Vector3f wi_p = normalize(Vector3f(
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
                fmsub(cos_phi, slope.x(), sin_phi * slope.y()) * m_alpha_u,
                fmadd(sin_phi, slope.x(), cos_phi * slope.y()) * m_alpha_v);

            // Step 4: compute normal & PDF
            Normal3f m = normalize(Vector3f(-slope.x(), -slope.y(), 1));

            Float pdf = eval(m) * smith_g1(wi, m) * abs_dot(wi, m) /
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
        Float xy_alpha_2 = sqr(m_alpha_u * v.x()) + sqr(m_alpha_v * v.y()),
              tan_theta_alpha_2 = xy_alpha_2 / sqr(v.z()),
              result;

        if (m_type == MicrofacetType::Beckmann) {
            Float a = rsqrt(tan_theta_alpha_2), a_sqr = sqr(a);
            /* Use a fast and accurate (<0.35% rel. error) rational
               approximation to the shadowing-masking function */
            result = select(a >= 1.6f, 1.f,
                            (3.535f * a + 2.181f * a_sqr) /
                            (1.f + 2.276f * a + 2.577f * a_sqr));
        } else {
            result = 2.f / (1.f + sqrt(1.f + tan_theta_alpha_2));
        }

        // Perpendicular incidence -- no shadowing/masking
        masked(result, eq(xy_alpha_2, 0.f)) = 1.f;

        /* Ensure consistent orientation (can't see the back
           of the microfacet from the front and vice versa) */
        masked(result, dot(v, m) * Frame3f::cos_theta(v) <= 0.f) = 0.f;

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
                safe_sqrt(fnmadd(cos_theta_i, cos_theta_i, 1.f)) /
                cos_theta_i;
            Float cot_theta_i = rcp(tan_theta_i);

            /* Search interval -- everything is parameterized
               in the erf() domain */
            Float maxval = erf(cot_theta_i);

            /* Start with a good initial guess (analytic solution for
               theta_i = pi/2, which is the most nonlinear case) */
            sample = max(min(sample, 1.f - 1e-6f), 1e-6f);
            Float x = maxval - (maxval + 1.f) * erf(sqrt(-log(sample.x())));

            // Normalization factor for the CDF
            sample.x() *= 1.f + maxval + InvSqrtPi *
                          tan_theta_i * exp(-sqr(cot_theta_i));

            // Three Newton iterations
            ENOKI_NOUNROLL for (size_t i = 0; i < 3; ++i) {
                Float slope = erfinv(x),
                      value = 1.f + x + InvSqrtPi * tan_theta_i *
                              exp(-sqr(slope)) - sample.x(),
                      derivative = 1.f - slope * tan_theta_i;

                x -= value / derivative;
            }

            // Now convert back into a slope value
            return erfinv(Vector2f(x, fmsub(2.f, sample.y(), 1.f)));
        } else {
            // Choose a projection direction and re-scale the sample
            Point2f p = warp::square_to_uniform_disk_concentric(sample);

            Float s = .5f * (1.f + cos_theta_i);
            p.y() = lerp(safe_sqrt(1.f - sqr(p.x())), p.y(), s);

            // Project onto chosen side of the hemisphere
            Float x = p.x(), y = p.y(),
                  z = safe_sqrt(1.f - squared_norm(p));

            // Convert to slope
            Float sin_theta_i = safe_sqrt(1.f - sqr(cos_theta_i));
            Float norm = rcp(fmadd(sin_theta_i, y, cos_theta_i * z));
            return Vector2f(fmsub(cos_theta_i, y, sin_theta_i * z), x) * norm;
        }
    }


protected:
    void configure() {
        m_alpha_u = max(m_alpha_u, 1e-4f);
        m_alpha_v = max(m_alpha_v, 1e-4f);
    }

    /// Compute the squared 1D roughness along direction \c v
    Float project_roughness_2(const Vector3f &v) const {
        if (is_isotropic())
            return sqr(m_alpha_u);

        Float sin_phi_2, cos_phi_2;
        std::tie(sin_phi_2, cos_phi_2) = Frame3f::sincos_phi_2(v);

        return sin_phi_2 * sqr(m_alpha_v) + cos_phi_2 * sqr(m_alpha_u);
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

template <typename FloatP, typename Spectrum>
DynamicArray<FloatP> eval_reflectance(const MicrofacetDistribution<FloatP, Spectrum> &distr,
                                      const Vector<DynamicArray<FloatP>, 3> &wi_,
                                      scalar_t<FloatP> eta) {
    using Float     = scalar_t<FloatP>;
    using FloatX    = DynamicArray<FloatP>;
    using Vector2fP = Vector<FloatP, 2>;
    using Vector3fP = Vector<FloatP, 3>;
    using Normal3fP = Normal<FloatP, 3>;
    using Vector2fX = Vector<FloatX, 2>;
    using Vector3fX = Vector<FloatX, 3>;

    if (!distr.sample_visible())
        Throw("eval_reflectance(): requires visible normal sampling!");

    int res = 128;
    if (eta > 1)
        res = 32;
    while (res % FloatP::Size != 0)
        ++res;

    FloatX nodes, weights, result;
    std::tie(nodes, weights) = quad::gauss_legendre<FloatX>(res);
    set_slices(result, slices(wi_));

    Vector2fX nodes_2    = meshgrid(nodes, nodes),
              weights_2  = meshgrid(weights, weights);

    for (size_t i = 0; i < slices(wi_); ++i) {
        auto wi      = slice(wi_, i);
        FloatP accum = zero<FloatP>();

        for (size_t j = 0; j < packets(nodes_2); ++j) {
            Vector2fP node(packet(nodes_2, j)),
                      weight(packet(weights_2, j));
            node = fmadd(node, .5f, .5f);

            Normal3fP m = std::get<0>(distr.sample(wi, node));
            Vector3fP wo = reflect(Vector3fP(wi), m);
            FloatP f = std::get<0>(fresnel(dot(wi, m), FloatP(eta)));
            FloatP smith = distr.smith_g1(wo, m) * f;
            smith[wo.z() <= 0.f || wi.z() <= 0.f] = 0.f;

            accum += smith * hprod(weight);
        }
        slice(result, i) = hsum(accum) * .25f;
    }
    return result;
}

template <typename FloatP, typename Spectrum>
DynamicArray<FloatP> eval_transmittance(const MicrofacetDistribution<FloatP, Spectrum> &distr,
                                        const Vector<DynamicArray<FloatP>, 3> &wi_,
                                        scalar_t<FloatP> eta) {
    using Float     = scalar_t<FloatP>;
    using FloatX    = DynamicArray<FloatP>;
    using Vector2fP = Vector<FloatP, 2>;
    using Vector3fP = Vector<FloatP, 3>;
    using Normal3fP = Normal<FloatP, 3>;
    using Vector2fX = Vector<FloatX, 2>;
    using Vector3fX = Vector<FloatX, 3>;

    if (!distr.sample_visible())
        Throw("eval_transmittance(): requires visible normal sampling!");

    int res = 128;
    if (eta > 1)
        res = 32;
    while (res % FloatP::Size != 0)
        ++res;

    FloatX nodes, weights, result;
    std::tie(nodes, weights) = quad::gauss_legendre<FloatX>(res);
    set_slices(result, slices(wi_));

    Vector2fX nodes_2    = meshgrid(nodes, nodes),
              weights_2  = meshgrid(weights, weights);

    for (size_t i = 0; i < slices(wi_); ++i) {
        auto wi      = slice(wi_, i);
        FloatP accum = zero<FloatP>();

        for (size_t j = 0; j < packets(nodes_2); ++j) {
            Vector2fP node(packet(nodes_2, j)),
                      weight(packet(weights_2, j));
            node = fmadd(node, .5f, .5f);

            Normal3fP m = std::get<0>(distr.sample(wi, node));
            auto [f, cos_theta_t, eta_it, eta_ti] = fresnel(dot(wi, m), FloatP(eta));
            Vector3fP wo = refract(Vector3fP(wi), m, cos_theta_t, eta_ti);
            FloatP smith = distr.smith_g1(wo, m) * (1.f - f);
            smith[wo.z() * wi.z() >= 0.f] = 0.f;

            accum += smith * hprod(weight);
        }
        slice(result, i) = hsum(accum) * .25f;
    }
    return result;
}

NAMESPACE_END(mitsuba)
