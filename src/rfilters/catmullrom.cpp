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
        m_radius = 2.f;
        init_discretization();
    }

    template <typename Value> Value eval_impl(Value x) const {
        x = abs(x);

        Value x2 = sqr(x), x3 = x2*x,
              B = 0.f, C = .5f;

        Value result = (1.f / 6.f) * select(
           x < 1,
           (12.f - 9.f * B - 6.f * C) * x3 +
               (-18.f + 12.f * B + 6.f * C) * x2 + (6.f - 2.f * B),
           (-B - 6.f * C) * x3 + (6.f * B + 30.f * C) * x2 +
               (-12.f * B - 48.f * C) * x + (8.f * B + 24.f * C)
        );

        return select(x < 2.f, result, 0.f);
    }

    std::string to_string() const override {
        return tfm::format("CatmullRomFilter[radius=%f]", m_radius);
    }

    MTS_IMPLEMENT_RFILTER_ALL()
    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS(CatmullRomFilter, ReconstructionFilter);
MTS_EXPORT_PLUGIN(CatmullRomFilter, "Catmull-Rom filter");
NAMESPACE_END(mitsuba)
