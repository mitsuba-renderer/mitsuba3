#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Box filter: this is the fastest, but also about the worst possible
 * reconstruction filter, since it is prone to severe aliasing.
 *
 * It is included mainly for completeness, though some rare situations
 * may warrant its use.
 */
class BoxFilter final : public ReconstructionFilter {
public:
    BoxFilter(const Properties &props) : ReconstructionFilter(props) {
        /* Filter radius in pixels. A tiny epsilon is added, since some
           samplers (Hammersley and Halton in particular) place samples
           at positions like (0, 0). Without such an epsilon and rounding
           errors, samples may end up not contributing to any pixel. */
        m_radius = props.float_("radius", .5f) + math::Epsilon;
        init_discretization();
    }

    template <typename Value>
    Value eval_impl(Value x) const {
        return select(abs(x) <= m_radius, Value(1.f), Value(0.f));
    }

    std::string to_string() const override {
        return tfm::format("BoxFilter[radius=%f]", m_radius);
    }

    MTS_IMPLEMENT_RFILTER_ALL()
    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS(BoxFilter, ReconstructionFilter);
MTS_EXPORT_PLUGIN(BoxFilter, "Box filter");
NAMESPACE_END(mitsuba)
