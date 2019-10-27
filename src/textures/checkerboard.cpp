#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class Checkerboard final : public ContinuousSpectrum<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN(Checkerboard, ContinuousSpectrum)
    using ContinuousSpectrum = ContinuousSpectrum<Float, Spectrum>;

    Checkerboard(const Properties &props) {
        m_color0 = props.spectrum<Float, Spectrum>("color0", .4f);
        m_color1 = props.spectrum<Float, Spectrum>("color1", .2f);
        m_transform = props.transform("to_uv", Transform4f()).extract();
    }

    MTS_INLINE Spectrum eval(const SurfaceInteraction3f &it,
                                  Mask active) const override {
        auto uv = m_transform.transform_affine(it.uv);
        auto mask = (uv - floor(uv)) > .5f;
        Spectrum result = zero<Spectrum>();

        Mask m0 = neq(mask.x(), mask.y()),
             m1 = !m0;

        m0 &= active; m1 &= active;

        if (any_or<true>(m0))
            result[m0] = m_color0->eval(it, m0);

        if (any_or<true>(m1))
            result[m1] = m_color1->eval(it, m1);

        return result;
    }

    Float mean() const override {
        return .5f * (m_color0->mean() + m_color1->mean());
    }

protected:
    ref<ContinuousSpectrum> m_color0;
    ref<ContinuousSpectrum> m_color1;
    Transform3f m_transform;
};

MTS_IMPLEMENT_PLUGIN(Checkerboard, ContinuousSpectrum, "Checkerboard texture")
NAMESPACE_END(mitsuba)
