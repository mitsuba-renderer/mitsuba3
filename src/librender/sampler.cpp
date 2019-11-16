#include <mitsuba/render/sampler.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
Sampler<Float, Spectrum>::Sampler(const Properties &props) {
    m_sample_count = props.size_("sample_count", 4);
    if (props.has_property("seed"))
        seed(props.size_("seed"));
}

template <typename Float, typename Spectrum>
Sampler<Float, Spectrum>::~Sampler() { }

template <typename Float, typename Spectrum>
void Sampler<Float, Spectrum>::seed(UInt64) { NotImplementedError("seed"); }

template <typename Float, typename Spectrum>
Float Sampler<Float, Spectrum>::next_1d(Mask) { NotImplementedError("next_1d"); }

template <typename Float, typename Spectrum>
typename Sampler<Float, Spectrum>::Point2f Sampler<Float, Spectrum>::next_2d(Mask) {
    NotImplementedError("next_2d");
}

MTS_INSTANTIATE_CLASS(Sampler)
NAMESPACE_END(mitsuba)
