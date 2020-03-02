#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _rfilter-lanczos:

Lanczos filter (:monosp:`lanczos`)
----------------------------------

This is a windowed version of the theoretically optimal low-pass filter. It
is generally one of the best available filters in terms of producing sharp
high-quality output. Its main disadvantage is that it produces strong ringing
around discontinuities, which can become a serious problem when rendering
bright objects with sharp edges (a directly visible light source will for
instance have black fringing artifacts around it). This is also the
computationally slowest reconstruction filter.

This plugin has an :paramtype:`integer`-valued parameter named
:paramtype:`lobes`, that sets the desired number of filter side-lobes. The
higher, the closer the filter will approximate an optimal low-pass filter, but
this also increases ringing. Values of 2 or 3 are common (3 is the default).

 */
template <typename Float, typename Spectrum>
class LanczosSincFilter final : public ReconstructionFilter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(ReconstructionFilter, init_discretization, m_radius)
    MTS_IMPORT_TYPES()

    LanczosSincFilter(const Properties &props) : Base(props) {
        m_radius = (ScalarFloat) props.int_("lobes", 3);
        init_discretization();
    }

    Float eval(Float x, mask_t<Float> /* active */) const override {
        x = abs(x);

        Float x1     = math::Pi<Float> * x,
              x2     = x1 / m_radius,
              result = (sin(x1) * sin(x2)) / (x1 * x2);

        return select(x < math::Epsilon<Float>,
                      1.f,
                      select(x > m_radius, 0.f, result));
    }

    std::string to_string() const override {
        return tfm::format("LanczosSincFilter[lobes=%f]", m_radius);
    }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_VARIANT(LanczosSincFilter, ReconstructionFilter)
MTS_EXPORT_PLUGIN(LanczosSincFilter, "Lanczos Sinc filter")
NAMESPACE_END(mitsuba)
