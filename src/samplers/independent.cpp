#include <mitsuba/core/properties.h>
#include <mitsuba/core/random.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

class IndependentSampler final : public Sampler {
public:
    IndependentSampler() : Sampler(Properties()) { }
    IndependentSampler(const Properties &props) : Sampler(props) {}

    ref<Sampler> clone() override {
        IndependentSampler *sampler = new IndependentSampler();
        sampler->seed(m_rng.next_uint64());
        sampler->m_sample_count = m_sample_count;
        return sampler;
    }

    void seed(size_t seed_value) override {
        m_rng.seed(seed_value, PCG32_DEFAULT_STREAM);
        m_rng_p.seed(seed_value, PCG32_DEFAULT_STREAM + index_sequence<UInt64P>());
    }

    Float next_1d() override {
        return next_float();
    }

    FloatP next_1d_p(MaskP active) override {
        return next_float_p(active);
    }

    Point2f next_2d() override {
        Float f1 = next_float(),
              f2 = next_float();
        return Point2f(f1, f2);
    }

    Point2fP next_2d_p(MaskP active) override {
        FloatP f1 = next_float_p(active),
               f2 = next_float_p(active);
        return Point2fP(f1, f2);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "IndependentSampler[" << std::endl
            << "  sample_count = " << m_sample_count << "," << std::endl
            << "  rng = " << string::indent(m_rng) << "," << std::endl
            << "  rng_p = " << string::indent(m_rng_p) << std::endl
            << "]" << std::endl;
        return oss.str();
    }

    MTS_DECLARE_CLASS()

protected:
    MTS_INLINE Float next_float() {
        #if defined(SINGLE_PRECISION)
            return m_rng.next_float32();
        #else
            return m_rng.next_float64();
        #endif
    }

    MTS_INLINE FloatP next_float_p(MaskP active) {
        #if defined(SINGLE_PRECISION)
            return m_rng_p.next_float32(active);
        #else
            return m_rng_p.next_float64(active);
        #endif
    }

protected:
    PCG32 m_rng;
    PCG32P m_rng_p;
};

MTS_IMPLEMENT_CLASS(IndependentSampler, Sampler);
MTS_EXPORT_PLUGIN(IndependentSampler, "Independent Sampler");
NAMESPACE_END(mitsuba)
