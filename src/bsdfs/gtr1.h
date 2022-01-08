NAMESPACE_BEGIN(mitsuba)

/**
 * \brief GTR1_isotropic Microfacet Distribution class
 *
 * This class implements GTR1 Microfacet Distribution Methods
 * for sampling routines of clearcoat lobe in the pricipled BSDF.
 *
 * Based on the paper
 *
 *   "Physically Based Shading at Disney"
 *    by Burley Brent
 *
 * Although it is a Microfacet distribution, it is not added in Microfacet
 * Plugin of Mitsuba since only the Principled BSDF uses it. Also,
 * visible normal sampling procedure is not applied as in Microfacet Plugin
 * because the clearcoat lobe of the principled BSDF has low energy compared to the other
 * lobes and visible normal sampling would not increase the sampling performance
 * considerably.
 */
template <typename Float, typename Spectrum>
class GTR1Isotropic {
public:
    MTS_IMPORT_TYPES();
    /**
     * Create an isotropic microfacet distribution for clearcoat lobe
     * based on GTR1.
     * \param m_alpha
     *     The roughness of the surface.
     */
    GTR1Isotropic(Float alpha) : m_alpha(alpha){};

    Float eval(const Vector3f &m) const {
        Float cos_theta  = Frame3f::cos_theta(m),
        cos_theta2 = ek::sqr(cos_theta), alpha2 = ek ::sqr(m_alpha);

        Float result = (alpha2 - 1.f) / (ek::Pi<Float> * ek::log(alpha2) *
                (1.f + (alpha2 - 1.f) * cos_theta2));

        return ek::select(result * cos_theta > 1e-20f, result, 0.f);
    }

    Float pdf(const Vector3f &m) const {
        return ek::select(m.z() < 0.f, 0.f, Frame3f::cos_theta(m) * eval(m));
    }

    Normal3f sample(const Point2f &sample) const {
        auto [sin_phi, cos_phi] = ek::sincos((2.f * ek::Pi<Float>) *sample.x());
        Float alpha2            = ek::sqr(m_alpha);

        Float cos_theta2 =
                (1.f - ek::pow(alpha2, 1.f - sample.y())) / (1.f - alpha2);

        Float sin_theta = ek::sqrt(ek::max(0.f, 1.f - cos_theta2)),
        cos_theta = ek::sqrt(ek::max(0.f, cos_theta2));

        return Normal3f(cos_phi * sin_theta, sin_phi * sin_theta, cos_theta);
    }

private:
    Float m_alpha;
};
NAMESPACE_END(mitsuba)