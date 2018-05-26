#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

class Checkerboard final : public ContinuousSpectrum {
public:
    Checkerboard(const Properties &props) {
        m_color0 = props.spectrum("color0", .4f);
        m_color1 = props.spectrum("color1", .2f);
        m_transform = props.transform("to_uv", Transform4f()).extract();
    }

    template <typename SurfaceInteraction, typename Mask,
              typename Value    = typename SurfaceInteraction::Value,
              typename Spectrum = mitsuba::Spectrum<Value>>
    MTS_INLINE Spectrum eval_impl(const SurfaceInteraction &it,
                                  Mask active) const {
        auto uv = m_transform.transform_affine(it.uv);
        auto mask = (uv - floor(uv)) > .5f;
        Spectrum result = zero<Spectrum>();

        Mask m0 = neq(mask.x(), mask.y()),
             m1 = !m0;

        m0 &= active; m1 &= active;

        if (any(m0))
            result[m0] = m_color0->eval(it, m0);

        if (any(m1))
            result[m1] = m_color1->eval(it, m1);

        return result;
    }

    Float mean() const override {
        return .5f * (m_color0->mean() + m_color1->mean());
    }

    MTS_IMPLEMENT_TEXTURE()
    MTS_DECLARE_CLASS()

protected:
    ref<ContinuousSpectrum> m_color0;
    ref<ContinuousSpectrum> m_color1;
    Transform3f m_transform;
};

MTS_IMPLEMENT_CLASS(Checkerboard, ContinuousSpectrum)
MTS_EXPORT_PLUGIN(Checkerboard, "Checkerboard texture")

NAMESPACE_END(mitsuba)
