#include <mitsuba/core/rfilter.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Special version of the Mitchell-Netravali filter with constants B and C configured
 * to match the Catmull-Rom spline. It usually does a better job at at preserving sharp
 * features at the cost of more ringing.
 */
class CatmullRomFilter final : public ReconstructionFilter {
public:
    CatmullRomFilter(const Properties &props) : ReconstructionFilter(props) {
        m_radius = 2.0f;
        init_discretization();
    }

    Float eval(Float x) const override {
        x = std::abs(x);

        Float x2 = x*x, x3 = x2*x;
        Float B = 0.0f, C = 0.5f;

        if (x < 1) {
            return 1.f / 6.f *
                   ((12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 +
                    (6 - 2 * B));
        } else if (x < 2) {
            return 1.f / 6.f *
                   ((-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 +
                    (-12 * B - 48 * C) * x + (8 * B + 24 * C));
        } else {
            return 0.0f;
        }
    }

    std::string to_string() const override {
        return tfm::format("CatmullRomFilter[radius=%f]", m_radius);
    }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS(CatmullRomFilter, ReconstructionFilter);
MTS_EXPORT_PLUGIN(CatmullRomFilter, "Catmull-Rom filter");
NAMESPACE_END(mitsuba)
