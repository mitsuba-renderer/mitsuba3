#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/phase.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class HGPhaseFunction final : public PhaseFunction<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(PhaseFunction)
    MTS_IMPORT_TYPES()

    HGPhaseFunction(const Properties &props) {
        m_g = props.float_("g", 0.8f);
        if (m_g >= 1 || m_g <= -1)
            Log(Error, "The asymmetry parameter must lie in the interval (-1, 1)!");
    }

    Vector3f sample(const MediumInteraction3f & /* mi */, const Point2f &sample,
                               Mask /* active */) const override {
        Float cos_theta;
        if (std::abs(m_g) < math::Epsilon<ScalarFloat>) {
            cos_theta = 1 - 2 * sample.x();
        } else {
            Float sqr_term = (1 - m_g * m_g) / (1 - m_g + 2 * m_g * sample.x());
            cos_theta = (1 + m_g * m_g - sqr_term * sqr_term) / (2 * m_g);
        }

        Float sin_theta = enoki::safe_sqrt(1.0f - cos_theta * cos_theta);
        auto [sin_phi, cos_phi] = enoki::sincos(2 * math::Pi<ScalarFloat> * sample.y());
        return Vector3f(sin_theta * cos_phi, sin_theta * sin_phi, cos_theta);
    }

    Float eval(const Vector3f &wo, Mask /* active */) const override {
        Float temp = 1.0f + m_g * m_g - 2.0f * m_g * wo.z();
        return math::InvFourPi<ScalarFloat> * (1 - m_g * m_g) / (temp * enoki::sqrt(temp));
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("g", m_g);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HGPhaseFunction[" << std::endl
            << "  g = " << string::indent(m_g) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ScalarFloat m_g;

};

MTS_IMPLEMENT_CLASS_VARIANT(HGPhaseFunction, PhaseFunction)
MTS_EXPORT_PLUGIN(HGPhaseFunction, "Henyey-Greenstein phase function")
NAMESPACE_END(mitsuba)