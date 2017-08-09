#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * This is a windowed version of the theoretically optimal low-pass filter. It
 * is generally one of the best available filters in terms of producing sharp
 * high-quality output. Its main disadvantage is that it produces ringing
 * around discontinuities, which can become a serious problem when rendering
 * bright objects with sharp edges (a directly visible light source will for
 * instance have black fringing artifacts around it).
 */
class LanczosSincFilter final : public ReconstructionFilter {
public:
    LanczosSincFilter(const Properties &props)
        : ReconstructionFilter(props) {
        m_radius = (Float) props.int_("lobes", 3);
        init_discretization();
    }

    Float eval(Float x) const override {
        x = std::abs(x);

        if (x < math::Epsilon)
            return 1.f;
        else if (x > m_radius)
            return 0.f;

        Float x1 = math::Pi * x;
        Float x2 = x1 / m_radius;

        return (std::sin(x1) * std::sin(x2)) / (x1 * x2);
    }

    std::string to_string() const override {
        return tfm::format("LanczosSincFilter[lobes=%f]", m_radius);
    }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS(LanczosSincFilter, ReconstructionFilter);
MTS_EXPORT_PLUGIN(LanczosSincFilter, "Lanczos Sinc filter");
NAMESPACE_END(mitsuba)
