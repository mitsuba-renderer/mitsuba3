#include <mitsuba/render/sampler.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT Sampler<Float, Spectrum>::Sampler(const Properties &props) {
    m_sample_count = props.size_("sample_count", 4);
    m_base_seed = props.size_("seed", 0);
}

MTS_VARIANT Sampler<Float, Spectrum>::~Sampler() { }

MTS_VARIANT void Sampler<Float, Spectrum>::seed(UInt64) { NotImplementedError("seed"); }

MTS_VARIANT Float Sampler<Float, Spectrum>::next_1d(Mask) { NotImplementedError("next_1d"); }

MTS_VARIANT typename Sampler<Float, Spectrum>::Point2f Sampler<Float, Spectrum>::next_2d(Mask) {
    NotImplementedError("next_2d");
}

MTS_IMPLEMENT_CLASS_VARIANT(Sampler, Object, "sampler")
MTS_INSTANTIATE_CLASS(Sampler)
NAMESPACE_END(mitsuba)
