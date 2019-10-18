#include <mitsuba/core/properties.h>
#include <mitsuba/core/random.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class IndependentSampler final : public Sampler<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN()
    MTS_USING_BASE(Sampler, m_sample_count)
    using PCG32 = mitsuba::PCG32<UInt32>;

    IndependentSampler() : Base(Properties()), m_seed_value(0) {
        // Can't seed yet on the GPU because we don't know yet how many
        // entries will be needed.
        if (!is_diff_array_v<Float>)
            seed(m_seed_value);
    }
    IndependentSampler(const Properties &props) : Base(props), m_seed_value(0) {
        if (!is_diff_array_v<Float>)
            seed(m_seed_value);
    }

    ref<Base> clone() override {
        IndependentSampler *sampler = new IndependentSampler();
        // TODO: needs a scalar size_t, not a Size
        sampler->seed(m_rng->next_uint64());
        sampler->m_sample_count = m_sample_count;
        return sampler;
    }

    /// Seeds the RNG with the specified size, if applicable
    void seed(size_t seed_value, size_t size = 1) override {
        m_seed_value = seed_value;
        m_rng        = std::make_unique<PCG32>();

        if constexpr (is_diff_array_v<Float>) {
            UInt64 idx = arange<UInt64>(size),
                   seed_value_c = (uint64_t) seed_value;
            m_rng->seed(sample_tea_64(detach(seed_value_c), detach(idx)),
                        sample_tea_64(detach(idx), detach(seed_value_c)));

        } else if constexpr (is_array_v<Float>) {
            if (size != array_size_v<Float>)
                Throw("Invalid size %d for packet sampler, expected %d", size, array_size_v<Float>);

            m_rng->seed(seed_value, PCG32_DEFAULT_STREAM + arange<UInt64>());
        } else {
            if (size != 1)
                Throw("Invalid size %d for scalar sampler, expected 1", size);
            m_rng->seed(seed_value, PCG32_DEFAULT_STREAM);
        }
    }

    Float next_1d(Mask active = true) override {
        if constexpr (is_diff_array_v<Float>) {
            if (!m_rng) {
                // We couldn't seed the RNG before because we didn't know
                // the number of entries we would need. Do it now.
                seed(m_seed_value, active.size());
            } else if (active.size() != 1 && active.size() != m_rng->size()) {
                Throw("next_float(): Requested sample array has incorrect shape %d, expected %d.", active.size(), m_rng->size());
            }
        }

        if constexpr (is_double_v<scalar_t<Float>>) {
            return m_rng->next_float64(detach(active));
        } else {
            return m_rng->next_float32(detach(active));
        }
    }

    Point2f next_2d(Mask active = true) override {
        Float f1 = next_1d(active),
              f2 = next_1d(active);
        return Point2f(f1, f2);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "IndependentSampler[" << std::endl
            << "  sample_count = " << m_sample_count << std::endl
            << "]";
        return oss.str();
    }

protected:
    std::unique_ptr<PCG32> m_rng;
    size_t m_seed_value;
};

MTS_IMPLEMENT_PLUGIN(IndependentSampler, Sampler, "Independent Sampler");
NAMESPACE_END(mitsuba)
