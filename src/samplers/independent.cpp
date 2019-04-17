#include <mitsuba/core/properties.h>
#include <mitsuba/core/random.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

class IndependentSampler final : public Sampler {
public:
    IndependentSampler() : Sampler(Properties()), m_seed_value(0) {}
    IndependentSampler(const Properties &props) : Sampler(props), m_seed_value(0) {}

public:
    ref<Sampler> clone() override {
        IndependentSampler *sampler = new IndependentSampler();
        sampler->seed(m_rng.next_uint64());
        sampler->m_sample_count = m_sample_count;
        return sampler;
    }

    void seed(size_t seed_value) override {
        m_seed_value = seed_value;
        m_rng.seed(seed_value, PCG32_DEFAULT_STREAM);
        m_rng_p.seed(seed_value, PCG32_DEFAULT_STREAM + arange<UInt64P>());

#if defined(MTS_ENABLE_AUTODIFF)
        if (m_rng_c)
            seed_c(seed_value, m_rng_c->state.size());
#endif
    }
#if defined(MTS_ENABLE_AUTODIFF)
    void seed(size_t seed_value, size_t size) override {
        m_seed_value = seed_value;
        m_rng.seed(seed_value, PCG32_DEFAULT_STREAM);
        m_rng_p.seed(seed_value, PCG32_DEFAULT_STREAM + arange<UInt64P>());

        seed_c(seed_value, size);
    }

    // Creates a new FloatC RNG with the specified state size and seed.
    void seed_c(size_t seed_value, size_t size) {
        m_rng_c = std::unique_ptr<PCG32C>(new PCG32C());

        UInt64C idx = arange<UInt64C>(size),
                seed_value_c = (uint64_t) seed_value;

        m_rng_c->seed(sample_tea_64(seed_value_c, idx),
                      sample_tea_64(idx, seed_value_c));
    }
#endif

    Float next_1d() override {
        return next_float();
    }

    FloatP next_1d_p(MaskP active) override {
        return next_float_p(active);
    }

#if defined(MTS_ENABLE_AUTODIFF)
    FloatD next_1d_d(MaskD active) override {
        return next_float_c(detach(active));
    }

    FloatD next_1d_d(UInt32D index, MaskD active) override {
        return next_float_c(detach(index), detach(active));
    }
#endif

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

#if defined(MTS_ENABLE_AUTODIFF)
    Point2fD next_2d_d(MaskD active) override {
        FloatC f1 = next_float_c(detach(active)),
               f2 = next_float_c(detach(active));
        return Point2fD(f1, f2);
    }

    Point2fD next_2d_d(UInt32D index, MaskD active) override {
        FloatC f1 = next_float_c(detach(index), detach(active)),
               f2 = next_float_c(detach(index), detach(active));
        return Point2fD(f1, f2);
    }
#endif

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "IndependentSampler[" << std::endl
            << "  sample_count = " << m_sample_count << std::endl
            << "]";
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

#if defined(MTS_ENABLE_AUTODIFF)
    MTS_INLINE FloatC next_float_c(MaskC active) {
        if (!m_rng_c || (m_rng_c->state.size() != active.size() && active.size() != 1)) {
            if (m_rng_c != nullptr && m_rng_c->state.size() != 1)
                throw std::runtime_error("next_float_c(): Requested sample "
                                         "array has incorrect shape.");

            seed_c(m_seed_value, active.size());
        }

        #if defined(SINGLE_PRECISION)
            return m_rng_c->next_float32(active);
        #else
            return m_rng_c->next_float64(active);
        #endif
    }

    MTS_INLINE FloatC next_float_c(UInt32C index, MaskC active) {
        if (!m_rng_c)
            throw std::runtime_error("next_float_c(): RNG is uninitialized!");

        #if defined(SINGLE_PRECISION)
            return m_rng_c->next_float32(index, active);
        #else
            return m_rng_c->next_float64(index, active);
        #endif
    }
#endif

protected:
    PCG32 m_rng;
    PCG32P m_rng_p;
    size_t m_seed_value;

#if defined(MTS_ENABLE_AUTODIFF)
    std::unique_ptr<PCG32C> m_rng_c;
#endif
};

MTS_IMPLEMENT_CLASS(IndependentSampler, Sampler);
MTS_EXPORT_PLUGIN(IndependentSampler, "Independent Sampler");
NAMESPACE_END(mitsuba)
