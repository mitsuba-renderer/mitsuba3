#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Separable cubic spline reconstruction filter by Mitchell and Netravali.
 * This is often a good compromise between sharpness and ringing.
 *
 * D. Mitchell, A. Netravali, Reconstruction filters for computer graphics,
 * Proceedings of SIGGRAPH 88, Computer Graphics 22(4), pp. 221-228, 1988.
 */
class MitchellNetravaliFilter final : public ReconstructionFilter {
public:
    MitchellNetravaliFilter(const Properties &props) : ReconstructionFilter(props) {
        /* Filter radius */
        m_radius = 2.0f;
        /* B parameter from the paper */
        m_b = props.float_("B", 1.f / 3.f);
        /* C parameter from the paper */
        m_c = props.float_("C", 1.f / 3.f);
        init_discretization();
    }

    Float eval(Float x) const override {
        x = std::abs(x);

        Float x2 = x*x, x3 = x2*x;

        if (x < 1) {
            return 1.f / 6.0f *
                   ((12 - 9 * m_b - 6 * m_c) * x3 +
                    (-18 + 12 * m_b + 6 * m_c) * x2 + (6 - 2 * m_b));
        } else if (x < 2) {
            return 1.f / 6.0f *
                   ((-m_b - 6 * m_c) * x3 + (6 * m_b + 30 * m_c) * x2 +
                    (-12 * m_b - 48 * m_c) * x + (8 * m_b + 24 * m_c));
        } else {
            return 0.0f;
        }
    }

    std::string to_string() const override {
        return tfm::format("MitchellNetravaliFilter[radius=%f, B=%f, C=%f]", m_radius, m_b, m_c);
    }

    MTS_DECLARE_CLASS()
protected:
    Float m_b, m_c;
};

MTS_IMPLEMENT_CLASS(MitchellNetravaliFilter, ReconstructionFilter);
MTS_EXPORT_PLUGIN(MitchellNetravaliFilter, "Mitchell-Netravali filter");
NAMESPACE_END(mitsuba)
