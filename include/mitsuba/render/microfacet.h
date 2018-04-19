#pragma once

#include <enoki/special.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/common.h>

NAMESPACE_BEGIN(mitsuba)

using namespace math;

/// Supported normal distribution functions
enum EType {
    /// Beckmann distribution derived from Gaussian random surfaces
    EBeckmann = 0,

    /// GGX: Long-tailed distribution for very rough surfaces (aka. Trowbridge-Reitz distr.)
    EGGX = 1
};

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
template <typename Value> class MicrofacetDistribution {
public:
    using Mask    = mask_t<Value>;
    using Point2  = Point<Value, 2>;
    using Vector2 = Vector<Value, 2>;
    using Vector3 = Vector<Value, 3>;
    using Normal3 = Normal<Value, 3>;
    using Frame   = mitsuba::Frame<Vector3>;

    /**
     * Create an isotropic microfacet distribution of the specified type
     *
     * \param type
     *     The desired type of microfacet distribution
     * \param alpha
     *     The surface roughness
     */
    MicrofacetDistribution(EType type, Value alpha, bool sample_visible = true)
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
    MicrofacetDistribution(EType type, Value alpha_u, Value alpha_v,
                           bool sample_visible = true)
        : m_type(type), m_alpha_u(alpha_u), m_alpha_v(alpha_v),
          m_sample_visible(sample_visible) {
        configure();
    }

    /**
     * \brief Create a microfacet distribution from a Property data
     * structure
     */
    MicrofacetDistribution(const Properties &props, EType type = EBeckmann,
                           Value alpha_u = .1f, Value alpha_v = .1f,
                           bool sample_visible = true)
        : m_type(type), m_alpha_u(alpha_u), m_alpha_v(alpha_v) {

        if (props.has_property("distribution")) {
            std::string distr = string::to_lower(props.string("distribution"));
            if (distr == "beckmann")
                m_type = EBeckmann;
            else if (distr == "ggx")
                m_type = EGGX;
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

        if (alpha_u == 0.f || alpha_v == 0.f) {
            Log(EWarn, "Cannot create a microfacet distribution with"
                "alpha_u/alpha_v=0 (clamped to 10^-4). Please use the"
                "corresponding smooth reflectance model to get zero roughness.");
        }

        m_sample_visible = props.bool_("sample_visible", sample_visible);

        configure();
    }

public:
    /// Return the distribution type
    EType type() const { return m_type; }

    /// Return the roughness (isotropic case)
    Value alpha() const { return m_alpha_u; }

    /// Return the roughness along the tangent direction
    Value alpha_u() const { return m_alpha_u; }

    /// Return the roughness along the bitangent direction
    Value alpha_v() const { return m_alpha_v; }

    /// Return whether or not only visible normals are sampled?
    bool sample_visible() const { return m_sample_visible; }

    /// Is this an isotropic microfacet distribution?
    bool is_isotropic() const { return m_alpha_u == m_alpha_v; }

    /// Is this an anisotropic microfacet distribution?
    bool is_anisotropic() const { return m_alpha_u != m_alpha_v; }

    /// Scale the roughness values by some constant
    void scale_alpha(Value value) {
        m_alpha_u *= value;
        m_alpha_v *= value;
    }

    /**
     * \brief Evaluate the microfacet distribution function
     *
     * \param m
     *     The microfacet normal
     */
    Value eval(const Vector3 &m) const {
        Value cos_theta         = Frame::cos_theta(m),
              cos_theta_2       = sqr(cos_theta),
              alpha_uv          = m_alpha_u * m_alpha_v,
              beckmann_exponent = (sqr(m.x() / m_alpha_u) +
                                   sqr(m.y() / m_alpha_v)) / cos_theta_2;

        Value result;
        if (m_type == EBeckmann) {
            /* Beckmann distribution function for Gaussian random surfaces */
            result = exp(-beckmann_exponent) /
                     (Pi * alpha_uv * sqr(cos_theta_2));
        } else {
            /* GGX / Trowbridge-Reitz distribution function */
            Value root = (1.f + beckmann_exponent) * cos_theta_2;
            result = rcp(Pi * alpha_uv * sqr(root));
        }

        /* Prevent potential numerical issues in other stages of the model */
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
    Value pdf(const Vector3 &wi, const Vector3 &m) const {
        Value result = eval(m);

        if (m_sample_visible)
            result *= smith_g1(wi, m) * abs_dot(wi, m) / Frame::cos_theta(wi);
        else
            result *= Frame::cos_theta(m);

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
    std::pair<Normal3, Value> sample(const Vector3 &wi,
                                     const Point2 &sample) const {
        if (!m_sample_visible) {
            Value sin_phi, cos_phi, cos_theta, cos_theta_2, alpha_2, pdf;

            /* Sample azimuth component (identical for Beckmann & GGX) */
            if (is_isotropic()) {
                std::tie(sin_phi, cos_phi) = sincos((2.f * Pi) * sample.y());

                alpha_2 = m_alpha_u * m_alpha_u;
            } else {
                Value ratio  = m_alpha_v / m_alpha_u,
                      tmp    = ratio * tan((2.f * Pi) * sample.y());

                cos_phi = rsqrt(fmadd(tmp, tmp, 1));
                cos_phi = mulsign(cos_phi, abs(sample.y() - .5f) - .25f);

                sin_phi = cos_phi * tmp;

                alpha_2 = rcp(sqr(cos_phi / m_alpha_u) +
                              sqr(sin_phi / m_alpha_v));
            }

            /* Sample elevation component */
            if (m_type == EBeckmann) {
                /* Beckmann distribution function for Gaussian random surfaces */
                cos_theta = rsqrt(fnmadd(alpha_2, log(1.f - sample.x()), 1.f));
                cos_theta_2 = sqr(cos_theta);

                /* Compute probability density of the sampled position */
                Value cos_theta_3 = max(cos_theta_2 * cos_theta, 1e-20f);
                pdf = (1.f - sample.x()) / (Pi * m_alpha_u * m_alpha_v * cos_theta_3);
            } else {
                /* GGX / Trowbridge-Reitz distribution function */
                Value tan_theta_m_2 = alpha_2 * sample.x() / (1.f - sample.x());
                cos_theta = rsqrt(1.f + tan_theta_m_2);
                cos_theta_2 = sqr(cos_theta);

                /* Compute probability density of the sampled position */
                Value temp = 1.f + tan_theta_m_2 / alpha_2,
                      cos_theta_3 = max(cos_theta_2 * cos_theta, 1e-20f);
                pdf = rcp(Pi * m_alpha_u * m_alpha_v * cos_theta_3 * sqr(temp));
            }

            Value sin_theta = sqrt(1.f - cos_theta_2);

            return {
                Normal3(cos_phi * sin_theta,
                        sin_phi * sin_theta,
                        cos_theta),
                pdf
            };
        } else {
            /* Visible normal sampling. */
            Value sin_phi, cos_phi, cos_theta;

            /* Step 1: stretch wi */
            Vector3 wi_p = normalize(Vector3(
                m_alpha_u * wi.x(),
                m_alpha_v * wi.y(),
                wi.z()
            ));

            std::tie(sin_phi, cos_phi) = Frame::sincos_phi(wi_p);
            cos_theta = Frame::cos_theta(wi_p);

            /* Step 2: simulate P22_{wi}(slope.x, slope.y, 1, 1) */
            Vector2 slope = sample_visible_11(cos_theta, sample);

            /* Step 3: rotate & unstretch */
            slope = Vector2(
                fmsub(cos_phi, slope.x(), sin_phi * slope.y()) * m_alpha_u,
                fmadd(sin_phi, slope.x(), cos_phi * slope.y()) * m_alpha_v);

            /* Step 4: compute normal & PDF */
            Normal3 m = normalize(Vector3(-slope.x(), -slope.y(), 1));

            Value pdf = smith_g1(wi, m) * abs_dot(wi, m) / Frame::cos_theta(wi);

            return { m, pdf };
        }
    }

    /// Smith's separable shadowing-masking approximation
    Value G(const Vector3 &wi, const Vector3 &wo, const Vector3 &m) const {
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
    Value smith_g1(const Vector3 &v, const Vector3 &m) const {
        Value alpha_2 = project_roughness_2(v);
        Value tan_theta_2 = Frame::tan_theta_2(v);
        Value result;

        if (m_type == EBeckmann) {
            Value a = rsqrt(alpha_2 * tan_theta_2);
            /* Use a fast and accurate (<0.35% rel. error) rational
               approximation to the shadowing-masking function */
            Value a_sqr = sqr(a);
            result      = select((a >= 1.6f), 1.f,
                            (3.535f * a + 2.181f * a_sqr) /
                            (1.f + 2.276f * a + 2.577f * a_sqr));
        } else {
            result = 2.f / (1.f + sqrt(1.f + alpha_2 * tan_theta_2));
        }

        /* Perpendicular incidence -- no shadowing/masking */
        masked(result, eq(tan_theta_2, 0.f)) = 1.f;

        /* Ensure consistent orientation (can't see the back
           of the microfacet from the front and vice versa) */
        masked(result, dot(v, m) * Frame::cos_theta(v) <= 0.f) = 0.f;

        return result;
    }

    /// \brief Visible normal sampling code for the alpha=1 case
    Vector2 sample_visible_11(Value cos_theta_i, Point2 sample) const {
        if (m_type == EBeckmann) {
            /* The original inversion routine from the paper contained
               discontinuities, which causes issues for QMC integration
               and techniques like Kelemen-style MLT. The following code
               performs a numerical inversion with better behavior */

            Value tan_theta_i =
                safe_sqrt(fnmadd(cos_theta_i, cos_theta_i, 1.f)) /
                cos_theta_i;
            Value cot_theta_i = rcp(tan_theta_i);

            /* Search interval -- everything is parameterized
               in the erf() domain */
            Value maxval = erf(cot_theta_i);

            /* Start with a good initial guess (analytic solution for
               theta_i = pi/2, which is the most nonlinear case) */
            sample = max(min(sample, 1.f - 1e-6f), 1e-6f);
            Value x = maxval - (maxval + 1.f) * erf(sqrt(-log(sample.x())));

            /* Normalization factor for the CDF */
            sample.x() *= 1.f + maxval + InvSqrtPi *
                          tan_theta_i * exp(-sqr(cot_theta_i));

            /* Two Newton iterations */
            ENOKI_NOUNROLL for (size_t i = 0; i < 3; ++i) {
                Value slope = erfinv(x),
                      value = 1.f + x + InvSqrtPi * tan_theta_i *
                              exp(-sqr(slope)) - sample.x(),
                      derivative = 1.f - slope * tan_theta_i;

                x -= value / derivative;
            }

            /* Now convert back into a slope value */
            return erfinv(Vector2(x, fmsub(2.f, sample.y(), 1.f)));
        } else {
            /* Choose a projection direction and re-scale the sample */
            Value a = rcp(1.f + cos_theta_i);

            Mask side_mask = sample.y() > a;
            masked(sample.y(), side_mask) -= a;
            sample.y() /= select(side_mask, 1.f - a, a);

            /* Uniformly sample a position on a disk */
            Value sin_phi, cos_phi, r = sqrt(sample.x());
            masked(sample.y(), side_mask) += 1.f;
            std::tie(sin_phi, cos_phi) = sincos(sample.y() * Pi);

            Value sin_theta_i =
                safe_sqrt(fnmadd(cos_theta_i, cos_theta_i, 1.f));

            /* Project onto chosen side of the hemisphere */
            Value p1 = r * cos_phi;
            Value p2 = r * sin_phi * select(side_mask, cos_theta_i, 1.f);
            Value p3 = sqrt(1.f - p1 * p1 - p2 * p2);

            /* Convert to slope */
            Value norm = rcp(sin_theta_i * p2 + cos_theta_i * p3);
            return Vector2(cos_theta_i * p2 - sin_theta_i * p3, p1) * norm;
        }
    }

    ENOKI_ALIGNED_OPERATOR_NEW()

protected:
    void configure() {
        m_alpha_u = max(m_alpha_u, 1e-4f);
        m_alpha_v = max(m_alpha_v, 1e-4f);
    }

    /// Compute the squared 1D roughness along direction \c v
    Value project_roughness_2(const Vector3 &v) const {
        if (is_isotropic())
            return sqr(m_alpha_u);

        Value sin_phi_2, cos_phi_2;
        std::tie(sin_phi_2, cos_phi_2) = Frame::sincos_phi_2(v);

        return sin_phi_2 * sqr(m_alpha_v) + cos_phi_2 * sqr(m_alpha_u);
    }

protected:
    EType m_type;
    Value m_alpha_u, m_alpha_v;
    bool  m_sample_visible;
};

template <typename T>
std::ostream &operator<<(std::ostream &os, const MicrofacetDistribution<T> &md) {
    os << "MicrofacetDistribution[" << std::endl
       << "  type = ";
    if (md.type() == EBeckmann)
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


NAMESPACE_END(mitsuba)
