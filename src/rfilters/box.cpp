#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _rfilter-box:

Box filter (:monosp:`box`)
--------------------------

This is the fastest, but also about the worst possible reconstruction filter,
since it is prone to severe aliasing. It is included mainly for completeness,
though some rare situations may warrant its use.

 */

template <typename Float, typename Spectrum>
class BoxFilter final : public ReconstructionFilter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(ReconstructionFilter, init_discretization, m_radius)

    BoxFilter(const Properties &props) : Base(props) {
        /* Filter radius in pixels. A tiny epsilon is added, since some
           samplers (Hammersley and Halton in particular) place samples
           at positions like (0, 0). Without such an epsilon and rounding
           errors, samples may end up not contributing to any pixel. */
        m_radius = props.float_("radius", .5f) + math::RayEpsilon<Float>;
        init_discretization();
    }

    Float eval(Float x, mask_t<Float> /* active */) const override {
        return select(abs(x) <= m_radius, Float(1.f), Float(0.f));
    }

    std::string to_string() const override {
        return tfm::format("BoxFilter[radius=%f]", m_radius);
    }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_VARIANT(BoxFilter, ReconstructionFilter)
MTS_EXPORT_PLUGIN(BoxFilter, "Box filter");
NAMESPACE_END(mitsuba)
