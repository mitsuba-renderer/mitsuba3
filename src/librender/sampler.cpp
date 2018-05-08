#include <mitsuba/render/sampler.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

Sampler::Sampler(const Properties &props) {
    m_sample_count = props.size_("sample_count", 4);
    if (props.has_property("seed"))
        seed(props.size_("seed"));
}

Sampler::~Sampler() { }

void Sampler::seed(size_t) { NotImplementedError("seed"); }
Float Sampler::next_1d() { NotImplementedError("next_1d"); }
FloatP Sampler::next_1d_p(MaskP) { NotImplementedError("next_1d_p"); }
Point2f Sampler::next_2d() { NotImplementedError("next_2d"); }
Point2fP Sampler::next_2d_p(MaskP) { NotImplementedError("next_2d_p"); }

MTS_IMPLEMENT_CLASS(Sampler, Object);
NAMESPACE_END(mitsuba)
