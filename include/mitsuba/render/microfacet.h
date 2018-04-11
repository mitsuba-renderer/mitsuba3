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
    EGGX = 1,
    /// Phong distribution (with the anisotropic extension by Ashikhmin and Shirley)
    EPhong = 2
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
 *  The visible normal sampling code was provided by Eric Heitz and Eugene D'Eon.
 */
template <typename Value>
class MicrofacetDistribution {

    using Mask    = mask_t<Value>;
    using Point2  = Point<Value, 2>;
    using Vector2 = Vector<Value, 2>;
    using Vector3 = Vector<Value, 3>;
    using Normal3 = Normal<Value, 3>;
    using Frame   = mitsuba::Frame<Vector3>;

public:
    /**
     * Create an isotropic microfacet distribution of the specified type
     *
     * \param type
     *     The desired type of microfacet distribution
     * \param alpha
     *     The surface roughness
     */
    MicrofacetDistribution(EType type, const Value &alpha, bool sample_visible = true)
        : m_type(type), m_alpha_u(alpha), m_alpha_v(alpha), m_sample_visible(sample_visible),
          m_exponent_u(0.0f), m_exponent_v(0.0f) {
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
    MicrofacetDistribution(EType type, const Value &alpha_u,
                           const Value &alpha_v, bool sample_visible = true)
        : m_type(type), m_alpha_u(alpha_u), m_alpha_v(alpha_v), m_sample_visible(sample_visible),
          m_exponent_u(0.0f), m_exponent_v(0.0f) {
        configure();
    }

    /**
     * \brief Create a microfacet distribution from a Property data
     * structure
     */
    MicrofacetDistribution(const Properties &props, EType type = EBeckmann,
         Value alpha_u = 0.1f, Value alpha_v = 0.1f, bool sample_visible = true)
        : m_type(type), m_alpha_u(alpha_u), m_alpha_v(alpha_v), m_exponent_u(0.0f),
          m_exponent_v(0.0f) {

        if (props.has_property("distribution")) {
            std::string distr = string::to_lower(props.string("distribution"));
            if (distr == "beckmann")
                m_type = EBeckmann;
            else if (distr == "ggx")
                m_type = EGGX;
            else if (distr == "phong" || distr == "as")
                m_type = EPhong;
            else
                Log(EError, "Specified an invalid distribution \"%s\", must be "
                    "\"beckmann\", \"ggx\", or \"phong\"/\"as\"!", distr.c_str());
        }

        if (props.has_property("alpha")) {
            m_alpha_u = m_alpha_v = props.float_("alpha");
            if (props.has_property("alpha_u") || props.has_property("alpha_v"))
                Log(EError, "Microfacet model: please specify"
                    "either 'alpha' or 'alpha_u'/'alpha_v'.");
        }
        else if (props.has_property("alpha_u") || props.has_property("alpha_v")) {
            if (!props.has_property("alpha_u") || !props.has_property("alpha_v"))
                Log(EError, "Microfacet model: both 'alpha_u' and 'alpha_v' must be specified.");
            if (props.has_property("alpha"))
                Log(EError, "Microfacet model: please specify"
                    "either 'alpha' or 'alpha_u'/'alpha_v'.");
            m_alpha_u = props.float_("alpha_u");
            m_alpha_v = props.float_("alpha_v");
        }

        if (any(eq(m_alpha_u, 0)) || any(eq(m_alpha_v, 0))) {
            Log(EWarn, "Cannot create a microfacet distribution with"
                "alpha_u/alpha_v=0 (clamped to 0.0001). Please use the"
                "corresponding smooth reflectance model to get zero roughness.");
        }

        m_sample_visible = props.bool_("sample_visible", sample_visible);

        configure();
    }

private:
    void configure() {
        m_alpha_u = max(m_alpha_u, 1e-4f);
        m_alpha_v = max(m_alpha_v, 1e-4f);

        // Visible normal sampling is not supported for the Phong / Ashikhmin-Shirley distribution
        if (m_type == EPhong) {
            if (m_sample_visible)
                Log(EWarn, "Visible normal sampling is not supported"
                    " for the Phong / Ashikhmin-Shirley distribution");
            m_sample_visible = false;
            compute_phong_exponent();
        }
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

    /// Return the Phong exponent (isotropic case)
    Value exponent() const { return m_exponent_u; }

    /// Return the Phong exponent along the tangent direction
    Value exponent_u() const { return m_exponent_u; }

    /// Return the Phong exponent along the bitangent direction
    Value exponent_v() const { return m_exponent_v; }

    /// Return whether or not only visible normals are sampled?
    bool sample_visible() const { return m_sample_visible; }

    /// Is this an anisotropic microfacet distribution?
    Mask is_anisotropic() const { return !eq(m_alpha_u, m_alpha_v); }

    /// Is this an anisotropic microfacet distribution?
    Mask is_isotropic() const { return eq(m_alpha_u, m_alpha_v); }

    /// Scale the roughness values by some constant
    void scale_alpha(Value value) {
        m_alpha_u *= value;
        m_alpha_v *= value;
        if (m_type == EPhong)
            compute_phong_exponent();
    }

    /**
     * \brief Evaluate the microfacet distribution function
     *
     * \param m
     *     The microfacet normal
     */
    Value eval(const Vector3 &m, Mask active = true) const {
        Value cos_theta = Frame::cos_theta(m);
        active = active && (cos_theta > 0);

        if (none(active))
            return 0.0f;

        Value cos_theta_2 = cos_theta*cos_theta;
        Value beckmann_exponent = ( (m.x()*m.x()) / (m_alpha_u * m_alpha_u)
                                  + (m.y()*m.y()) / (m_alpha_v * m_alpha_v)) / cos_theta_2;

        Value result;
        switch (m_type) {
            case EBeckmann: {
                // Beckmann distribution function for Gaussian
                // random surfaces - [Walter 2005] evaluation
                result = exp(-beckmann_exponent)
                         / (Pi * m_alpha_u * m_alpha_v * cos_theta_2 * cos_theta_2);
            }
            break;

            case EGGX: {
                // GGX / Trowbridge-Reitz distribution function for rough surfaces
                Value root = (1.0f + beckmann_exponent) * cos_theta_2;
                result = 1.0f / (Pi * m_alpha_u * m_alpha_v * root * root);
            }
            break;

            case EPhong: {
                // Isotropic case: Phong distribution.
                // Anisotropic case: Ashikhmin-Shirley distribution
                Value exponent = interpolate_phong_exponent(m, active);
                result = sqrt((m_exponent_u + 2.0f) * (m_exponent_v + 2.0f))
                         * InvTwoPi * pow(Frame::cos_theta(m), exponent);
            }
            break;

            default:
                Log(EError, "Invalid distribution type!");
                return -1;
        }

        // Prevent potential numerical issues in other stages of the model
        masked(result, ((result * cos_theta < 1e-20f) || !active)) = 0.0f;

        return result;
    }

    /**
     * \brief Wrapper function which calls \ref sample_all() or \ref sample_visible_normals()
     * depending on the parameters of this class
     */
    std::pair<Normal3, Value> sample(const Vector3 &wi, const Point2 &sample,
                                     const Mask &active = true) const {
        if (m_sample_visible) {
            Normal3 m = sample_visible_normals(wi, sample, active);
            return {m , pdf_visible_normals(wi, m, active) };
        } else
            return sample_all(sample, active);
    }

    /**
     * \brief Wrapper function which calls \ref pdf_all() or \ref pdf_visible_normals()
     * depending on the parameters of this class
     */
    Value pdf(const Vector3 &wi, const Vector3 &m, const Mask &active = true) const {
        if (m_sample_visible)
            return pdf_visible_normals(wi, m, active);
        else
            return pdf_all(m, active);
    }

    /**
     * \brief Draw a sample from the microfacet normal distribution
     * (including *all* normals) and return the associated
     * probability density
     *
     * \param sample
     *    A uniformly distributed 2D sample
     * \param pdf
     *    The probability density wrt. solid angles
     */
    std::pair<Normal3, Value> sample_all(const Point2 &sample,
                                         const Mask &active = true) const {
        // The azimuthal component is always selected uniformly regardless of
        // the distribution.
        Value cos_theta_m = 0.0f;
        Value sin_phi_m, cos_phi_m;
        Value alpha_sqr;
        Value pdf(0.0f);

        if (any(active && is_anisotropic())) { // anisotropic case
            switch (m_type) {
                case EBeckmann: {
                    // Beckmann distribution function for Gaussian random surfaces
                    // Sample phi component
                    Value phi_m = atan(m_alpha_v / m_alpha_u * tan(Pi + 2.0f * Pi * sample.y()))
                                  + Pi * floor(2.0f * sample.y() + 0.5f);
                    std::tie(sin_phi_m, cos_phi_m) = sincos(phi_m);

                    Value cos_sc = cos_phi_m / m_alpha_u,
                          sin_sc = sin_phi_m / m_alpha_v;
                    alpha_sqr = 1.0f / (cos_sc * cos_sc + sin_sc * sin_sc);

                    // Sample theta component
                    Value tan_theta_m_sqr = alpha_sqr * -log(1.0f - sample.x());
                    cos_theta_m = 1.0f / sqrt(1.0f + tan_theta_m_sqr);

                    // Compute probability density of the sampled position
                    pdf = (1.0f - sample.x())
                          / (Pi * m_alpha_u * m_alpha_v * cos_theta_m * cos_theta_m * cos_theta_m);
                }
                break;

                case EGGX: {
                    // GGX / Trowbridge-Reitz distribution function for rough surfaces
                    // Sample phi component
                    Value phi_m = atan(m_alpha_v / m_alpha_u * tan(Pi + 2.0f * Pi * sample.y()))
                                  + Pi * floor(2.0f * sample.y() + 0.5f);
                    std::tie( sin_phi_m, cos_phi_m ) = sincos(phi_m);

                    Value cos_sc = cos_phi_m / m_alpha_u,
                          sin_sc = sin_phi_m / m_alpha_v;
                    alpha_sqr = rcp((cos_sc * cos_sc + sin_sc * sin_sc));

                    // Sample theta component
                    Value tan_theta_m_sqr = alpha_sqr * sample.x() / (1.0f - sample.x());
                    cos_theta_m = rcp(sqrt(1.0f + tan_theta_m_sqr));

                    // Compute probability density of the sampled position
                    Value temp = 1.0f + tan_theta_m_sqr / alpha_sqr;
                    pdf = InvPi / (m_alpha_u*m_alpha_v*cos_theta_m*cos_theta_m*cos_theta_m*temp*temp);
                }
                break;

                case EPhong: {
                    Value phi_m;
                    Value exponent;

                    // Sampling method based on code from PBRT

                    Mask sample_quad_1 = sample.y() < 0.25f,
                         sample_quad_2 = sample.y() < 0.5f && !sample_quad_1,
                         sample_quad_3 = sample.y() < 0.75f && !(sample_quad_1 || sample_quad_2),
                         sample_quad_4 = !(sample_quad_1 || sample_quad_2 || sample_quad_3);

                    // TODO add comments

                    Value u1;
                    masked(u1, sample_quad_1) = 4.0f * sample.y();
                    masked(u1, sample_quad_2) = 4.0f * (0.5f - sample.y());
                    masked(u1, sample_quad_3) = 4.0f * (sample.y() - 0.5f);
                    masked(u1, sample_quad_4) = 4.0f * (1.0f - sample.y());

                    std::tie(phi_m, exponent) = sample_first_quadrant(u1);

                    masked(phi_m, sample_quad_2) = Pi - phi_m;
                    masked(phi_m, sample_quad_3) += Pi;
                    masked(phi_m, sample_quad_4) = 2.0f * Pi - phi_m;

                    std::tie(sin_phi_m, cos_phi_m) = sincos(phi_m);
                    cos_theta_m = pow(sample.x(), 1.0f / (exponent + 2.0f));
                    pdf = sqrt((m_exponent_u + 2.0f) * (m_exponent_v + 2.0f))
                          * InvTwoPi * pow(cos_theta_m, exponent + 1.0f);
                }
                break;

                default:
                    Log(EError, "Invalid distribution type!");
                    return { Vector3(-1.0f), -1.0f };
            }
        } else { // isotropic
            switch (m_type) {
                case EBeckmann: {
                    // Beckmann distribution function for Gaussian random surfaces
                    // Sample phi component
                    std::tie(sin_phi_m, cos_phi_m) = sincos((2.0f * Pi) * sample.y());

                    alpha_sqr = m_alpha_u * m_alpha_u;

                    // Sample theta component
                    Value tan_theta_m_sqr = alpha_sqr * -log(1.0f - sample.x());
                    cos_theta_m = rcp(sqrt(1.0f + tan_theta_m_sqr));

                    // Compute probability density of the sampled position
                    pdf = (1.0f - sample.x())
                          / (Pi * m_alpha_u * m_alpha_v * cos_theta_m * cos_theta_m * cos_theta_m);
                }
                break;

                case EGGX: {
                    // GGX / Trowbridge-Reitz distribution function for rough surfaces
                    // Sample phi component
                    std::tie(sin_phi_m, cos_phi_m) = sincos((2.0f * Pi) * sample.y());

                    // Sample theta component
                    alpha_sqr = m_alpha_u * m_alpha_u;

                    // Sample theta component
                    Value tan_theta_m_sqr = alpha_sqr * sample.x() / (1.0f - sample.x());
                    cos_theta_m = rcp(sqrt(1.0f + tan_theta_m_sqr));

                    // Compute probability density of the sampled position
                    Value temp = 1.0f + tan_theta_m_sqr / alpha_sqr;
                    pdf = InvPi / (m_alpha_u * m_alpha_v * cos_theta_m
                                   * cos_theta_m * cos_theta_m * temp * temp);
                }
                break;

                case EPhong: {
                    Value phi_m;
                    Value exponent;
                    phi_m = (2.0f * Pi) * sample.y();
                    exponent = m_exponent_u;

                    std::tie(sin_phi_m, cos_phi_m) = sincos(phi_m);
                    cos_theta_m = pow(sample.x(), rcp((exponent + 2.0f)));
                    pdf = sqrt((m_exponent_u + 2.0f) * (m_exponent_v + 2.0f))
                          * InvTwoPi * pow(cos_theta_m, exponent + 1.0f);
                }
                break;

                default:
                    Log(EError, "Invalid distribution type!");
                    return { Vector3(-1.0f), -1.0f };
            }
        }

        // Prevent potential numerical issues in other stages of the model
        masked(pdf, pdf < 1e-20f) = 0.0f;

        Value sin_theta_m = sqrt(max(0.0f, 1.0f - cos_theta_m * cos_theta_m));

        return { Vector3(
            sin_theta_m * cos_phi_m,
            sin_theta_m * sin_phi_m,
            cos_theta_m
        ), pdf };
    }

    /**
     *  \brief Returns the density function associated with
     *  the \ref sample_all() function.
     *
     * \param m
     *     The microfacet normal
     */
    Value pdf_all(const Vector3 &m, const Mask &active = true) const {
        // PDF is just D(m) * cos(theta_M)
        return eval(m, active) * Frame::cos_theta(m);
    }

    /**
     * \brief Draw a sample from the distribution of visible normals
     *
     * \param _wi
     *    A reference direction that defines the set of visible normals
     * \param sample
     *    A uniformly distributed 2D sample
     * \param pdf
     *    The probability density wrt. solid angles
     */
    Normal3 sample_visible_normals(const Vector3 &_wi,
                                   const Point2 &sample,
                                   const Mask &active = true) const {
        // Step 1: stretch wi
        Vector3 wi = normalize(Vector3(
            m_alpha_u * _wi.x(),
            m_alpha_v * _wi.y(),
            _wi.z()
        ));

        // Get polar coordinates
        Value theta = 0.0f, phi = 0.0f;
        masked(theta, wi.z() < 0.99999f) = acos(wi.z());
        masked(phi,   wi.z() < 0.99999f) = enoki::atan2(wi.y(), wi.x());

        Value sin_phi, cos_phi;
        std::tie(sin_phi, cos_phi) = sincos(phi);

        // Step 2: simulate P22_{wi}(slope.x(), slope.y(), 1, 1)
        Vector2 slope = sample_visible_11(theta, sample, active);

        // Step 3: rotate
        slope = Vector2(
            cos_phi * slope.x() - sin_phi * slope.y(),
            sin_phi * slope.x() + cos_phi * slope.y());

        // Step 4: unstretch
        slope.x() *= m_alpha_u;
        slope.y() *= m_alpha_v;

        // Step 5: compute normal
        Value normalization = rcp(sqrt(slope.x() * slope.x()
            + slope.y() * slope.y() + 1.0f));

        return Normal3(
            -slope.x() * normalization,
            -slope.y() * normalization,
            normalization
        );
    }

    /// Implements the probability density of the function \ref sample_visible_normals()
    Value pdf_visible_normals(const Vector3 &wi, const Vector3 &m, Mask active = true) const {
        Value cos_theta = Frame::cos_theta(wi);
        active = active && !eq(cos_theta, 0.0f);

        if (none(active))
            return 0.0f;

        Value result(0.0f);
        masked(result, active) = smith_g1(wi, m, active) * abs_dot(wi, m)
                                 * eval(m, active) / abs(cos_theta);
        return result;
    }

    /**
     * \brief Smith's shadowing-masking function G1 for each
     * of the supported microfacet distributions
     *
     * \param v
     *     An arbitrary direction
     * \param m
     *     The microfacet normal
     */
    Value smith_g1(const Vector3 &v, const Vector3 &m, Mask active = true) const {
        Value alpha = project_roughness(v, active);
        Value tan_theta = abs(Frame::tan_theta(v));
        Value result;

        switch (m_type) {
            case EPhong:
            case EBeckmann: {
                Value a = rcp(alpha * tan_theta);
                // Use a fast and accurate (<0.35% rel. error) rational
                // approximation to the shadowing-masking function
                Value a_sqr = a * a;
                result = select((a >= 1.6f), 1.0f,
                    (3.535f * a + 2.181f * a_sqr) / (1.0f + 2.276f * a + 2.577f * a_sqr));
            }
            break;

            case EGGX: {
                Value root = alpha * tan_theta;
                result = 2.0f / (1.0f + hypot(Value(1.0f), root));
            }
            break;

            default:
                Log(EError, "Invalid distribution type!");
                return -1.0f;
        }

        // Perpendicular incidence -- no shadowing/masking
        masked(result, eq(tan_theta, 0.0f)) = 1.0f;

        // Ensure consistent orientation (can't see the back
        // of the microfacet from the front and vice versa)
        Mask below_horizon = dot(v, m) * Frame::cos_theta(v) <= 0.0f;
        masked(result, below_horizon) = 0.0f;

        return result;
    }

    /**
     * \brief Separable shadow-masking function based on Smith's
     * one-dimensional masking model
     */
    Value G(const Vector3 &wi, const Vector3 &wo, const Vector3 &m,
            const Mask &active = true) const {
        return smith_g1(wi, m, active) * smith_g1(wo, m, active);
    }

    /// Return a string representation of the name of a distribution
    static std::string distribution_name(EType type) {
        switch (type) {
        case EBeckmann: return "beckmann"; break;
        case EGGX:      return "ggx";      break;
        case EPhong:    return "phong";    break;
        default:        return "invalid";  break;
        }
    }

    /// Return a string representation of the contents of this instance
    std::string to_string() const {
        std::ostringstream os;
        os << "MicrofacetDistribution" << type_suffix<Value>() << "[" << std::endl
           << "  type = " << distribution_name(m_type) << "," << std::endl
           << "  alpha_u = " << m_alpha_u << "," << std::endl
           << "  alpha_v = " << m_alpha_v << std::endl
           << "]";
        return os.str();
    }

protected:
    /// Compute the effective roughness projected on direction \c v
    Value project_roughness(const Vector3 &v, Mask active = true) const {
        Value inv_sin_theta_2 = rcp(Frame::sin_theta_2(v));
        active = active && !(is_isotropic() || inv_sin_theta_2 <= 0.0f);

        if (none(active))
            return m_alpha_u;

        Value cos_phi2 = v.x() * v.x() * inv_sin_theta_2;
        Value sin_phi2 = v.y() * v.y() * inv_sin_theta_2;

        Value result(m_alpha_u);
        masked(result, active) = sqrt(cos_phi2 * m_alpha_u * m_alpha_u
                                      + sin_phi2 * m_alpha_v * m_alpha_v);

        return result;
    }

    /// Compute the interpolated roughness for the Phong model
    Value interpolate_phong_exponent(const Vector3 &v, Mask active = true) const {
        const Value sin_theta_2 = Frame::sin_theta_2(v);
        active = active && !(is_isotropic() || (sin_theta_2 <= RecipOverflow));

        if (none(active))
            return m_exponent_u;

        Value inv_sin_theta_2 = rcp(sin_theta_2);
        Value cos_phi2 = v.x() * v.x() * inv_sin_theta_2;
        Value sin_phi2 = v.y() * v.y() * inv_sin_theta_2;

        Value result(m_exponent_u);
        masked(result, active) = m_exponent_u * cos_phi2 + m_exponent_v * sin_phi2;

        return result;
    }

    /**
     * \brief Visible normal sampling code for the alpha=1 case
     *
     * Source: supplemental material of "Importance Sampling
     * Microfacet-Based BSDFs using the Distribution of Visible Normals"
     */
    Vector2 sample_visible_11(const Value &theta_i, Point2 sample,
                              const Mask &active = true) const {
        const Value InvSqrtPi = 1 / sqrt(Pi);
        Vector2 slope;

        switch (m_type) {
            case EBeckmann: {
                // Special case (normal incidence)
                Mask normal_incidence = theta_i < 1e-4f;
                if (any((normal_incidence && active))) {
                    Value r = sqrt(-log(1.0f - sample.x()));

                    Value sin_phi, cos_phi;
                    std::tie(sin_phi, cos_phi) = sincos(2 * Pi * sample.y());

                    masked(slope, normal_incidence) = Vector2(r * cos_phi, r * sin_phi);
                }

                if (all(normal_incidence || !active))
                    return slope;

                /* The original inversion routine from the paper contained
                discontinuities, which causes issues for QMC integration
                and techniques like Kelemen-style MLT. The following code
                performs a numerical inversion with better behavior */
                Value tan_theta_i = tan(theta_i);
                Value cot_theta_i = 1 / tan_theta_i;

                // Search interval -- everything is parameterized
                // in the erf() domain
                Value a = -1.0f, c = enoki::erf(cot_theta_i);
                Value sample_x = max(sample.x(), 1e-6f);

                // Start with a good initial guess
                //Value b = (1-sample_x) * a + sample_x * c;

                // We can do better (inverse of an approximation computed in Mathematica)
                Value fit = 1 + theta_i * (-0.876f + theta_i * (0.4265f - 0.0594f * theta_i));
                Value b = c - (1 + c) * pow(1 - sample_x, fit);

                // Normalization factor for the CDF
                Value normalization = 1 / (1 + c + InvSqrtPi *
                    tan_theta_i * exp(-cot_theta_i * cot_theta_i));

                Mask active_bisection = !normal_incidence && active;
                int it = 0;
                while (++it < 10) {
                    // Bisection criterion -- the oddly-looking
                    // boolean expression are intentional to check
                    // for NaNs at little additional cost
                    masked(b, active_bisection && !(b >= a && b <= c)) = 0.5f * (a + c);

                    // Evaluate the CDF and its derivative
                    // (i.e. the density function)
                    Value inv_erf = enoki::erfinv(b);
                    Value value = normalization * (1 + b + InvSqrtPi
                                  * tan_theta_i * exp(-inv_erf * inv_erf)) - sample_x;
                    Value derivative = normalization * (1 - inv_erf * tan_theta_i);

                    active_bisection &= abs(value) >= 1e-5f;
                    if (none(active_bisection))
                        break;

                    // Update bisection intervals
                    masked(c, active_bisection && (value >  0.0f)) = b;
                    masked(a, active_bisection && (value <= 0.0f)) = b;

                    masked(b, active_bisection) -= value / derivative;
                }

                // Now convert back into a slope value
                masked(slope.x(), !normal_incidence) = enoki::erfinv(b);

                // Simulate Y component
                masked(slope.y(), !normal_incidence)
                    = enoki::erfinv(2.0f * max(sample.y(), 1e-6f) - 1.0f);
            };
            break;

            case EGGX: {
                // Special case (normal incidence)
                Mask normal_incidence = (theta_i < 1e-4f);
                if (any(normal_incidence && active)) {
                    Value r = safe_sqrt(sample.x() / (1 - sample.x()));

                    Value sin_phi, cos_phi;
                    std::tie(sin_phi, cos_phi) = sincos(2 * Pi * sample.y());

                    masked(slope, normal_incidence) = Vector2(r * cos_phi, r * sin_phi);
                }

                if (all(normal_incidence || !active))
                    return slope;

                // Precomputations
                Value tan_theta_i = tan(theta_i);
                Value a = 1.0f / tan_theta_i;
                Value G1 = 2.0f / (1.0f + safe_sqrt(1.0f + 1.0f / (a*a)));

                // Simulate X component
                Value A = 2.0f * sample.x() / G1 - 1.0f;
                masked(A, abs(A) == 1) -= sign(A) * Epsilon;
                Value tmp = 1.0f / (A * A - 1.0f);
                Value B = tan_theta_i;
                Value D = safe_sqrt(B*B*tmp*tmp - (A*A - B*B) * tmp);
                Value slope_x_1 = B * tmp - D;
                Value slope_x_2 = B * tmp + D;
                masked(slope.x(), !normal_incidence)
                    = select((A < 0.0f || slope_x_2 > 1.0f / tan_theta_i) , slope_x_1 , slope_x_2);

                // Simulate Y component
                Value S = sign(sample.y() - 0.5f);
                sample.y() = 2.0f * S * (sample.y() - 0.5f);

                // Improved fit
                Value z =
                    (sample.y() * (sample.y()
                        * (sample.y() * -0.365728915865723f + 0.790235037209296f)
                        - 0.424965825137544f) + 0.000152998850436920f)
                    / (sample.y() * (sample.y()
                        * (sample.y() * (sample.y() * 0.169507819808272f - 0.397203533833404f)
                           - 0.232500544458471f) + 1.0f) - 0.539825872510702f);

                masked(slope.y(), !normal_incidence) = S * z * sqrt(1.0f + slope.x() * slope.x());
            };
            break;

            default:
                Log(EError, "Invalid distribution type!");
                return Vector2(-1);
        };
        return slope;
    }


    /// Helper routine: convert from Beckmann-style roughness values to Phong exponents (Walter et al.)
    void compute_phong_exponent() {
        m_exponent_u = max(2.0f / (m_alpha_u * m_alpha_u) - 2.0f, 0.0f);
        m_exponent_v = max(2.0f / (m_alpha_v * m_alpha_v) - 2.0f, 0.0f);
    }

    /// Helper routine: sample the azimuthal part of the first quadrant of the A&S distribution
    std::pair<Value, Value> sample_first_quadrant(const Value &u1) const {
        Value cos_phi, sin_phi;
        Value phi = atan(
            sqrt((m_exponent_u + 2.0f) / (m_exponent_v + 2.0f)) *
            tan(Pi * u1 * 0.5f));
        std::tie(sin_phi, cos_phi) = sincos(phi);
        // Return the interpolated roughness
        Value exponent = m_exponent_u * cos_phi * cos_phi + m_exponent_v * sin_phi * sin_phi;
        return { phi, exponent };
    }

public:
    ENOKI_ALIGNED_OPERATOR_NEW()

protected:
    EType m_type;
    Value m_alpha_u, m_alpha_v;
    bool  m_sample_visible;
    Value m_exponent_u, m_exponent_v;
};
NAMESPACE_END(mitsuba)
