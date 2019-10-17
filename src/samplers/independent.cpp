#include <mitsuba/core/properties.h>
#include <mitsuba/core/random.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class IndependentSampler final : public Sampler<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN()
    using PCG32 = mitsuba::PCG32<UInt32>;
    using Base  = Sampler<Float, Spectrum>;
    using Base::m_sample_count;

    IndependentSampler() : Base(Properties()), m_seed_value(0) {}
    IndependentSampler(const Properties &props) : Base(props), m_seed_value(0) {}

    ref<Base> clone() override {
        IndependentSampler *sampler = new IndependentSampler();
        // TODO: needs a scalar size_t, not a Size
        sampler->seed(m_rng->next_uint64());
        sampler->m_sample_count = m_sample_count;
        return sampler;
    }

    /// Seed the RNG, keeping the current state size
    void seed(size_t seed_value) override {
        if (!m_rng)
            m_rng = std::make_unique<PCG32>();

        if constexpr (is_diff_array_v<Float>) {
            seed(seed_value, m_rng->size());
        } else {
            seed(seed_value, 1);
        }
    }

    /// Seeds the RNG with the specified size, if applicable
    void seed(size_t seed_value, size_t size) override {
        m_seed_value = seed_value;
        m_rng = std::make_unique<PCG32>();

        if constexpr (is_diff_array_v<Float>) {
            UInt64 idx = arange<UInt64>(size),
                   seed_value_c = (uint64_t) seed_value;
            m_rng->seed(sample_tea_64(detach(seed_value_c), detach(idx)),
                        sample_tea_64(detach(idx), detach(seed_value_c)));

        } else if constexpr (is_array_v<Float>) {
            m_rng->seed(seed_value, PCG32_DEFAULT_STREAM + arange<UInt64>());
        } else {
            m_rng->seed(seed_value, PCG32_DEFAULT_STREAM);
        }
    }

    Float next_1d(Mask active = true) override {
        using Scalar = scalar_t<Float>;
        if constexpr (is_diff_array_v<Float>) {
            if (m_rng != nullptr && m_rng->state.size() != 1)
                Throw("next_float(): Requested sample array has incorrect shape %d, expected %d.", active.size(), m_rng->size());
            else
                seed(m_seed_value, active.size());
        }

        // TODO: how to pass indices? (GPU RNG)
        if constexpr (std::is_same_v<Scalar, float>) {
            return m_rng->next_float32(active);
        } else {
            return m_rng->next_float64(active);
        }
    }

    Point2f next_2d(Mask active = true) override {
        // TODO: how to pass indices? (GPU RNG)
        Float f1 = next_float(detach(active)),
              f2 = next_float(detach(active));
        return Point2f(f1, f2);
    }

#if defined(MTS_ENABLE_AUTODIFF)
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

protected:

#if defined(MTS_ENABLE_AUTODIFF)
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
    std::unique_ptr<PCG32> m_rng;
    size_t m_seed_value;

};

MTS_IMPLEMENT_PLUGIN(IndependentSampler, Sampler, "Independent Sampler");
NAMESPACE_END(mitsuba)
