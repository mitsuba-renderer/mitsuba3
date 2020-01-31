#include <mitsuba/core/properties.h>
#include <mitsuba/core/random.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class IndependentSampler final : public Sampler<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Sampler, m_sample_count)
    MTS_IMPORT_TYPES()

    using PCG32 = mitsuba::PCG32<UInt32>;

    IndependentSampler(const Properties &props = Properties()) : Base(props) {
        /* Can't seed yet on the GPU because we don't know yet
           how many entries will be needed. */
        if (!is_dynamic_array_v<Float>)
            seed(PCG32_DEFAULT_STATE);
    }

    ref<Base> clone() override {
        IndependentSampler *sampler = new IndependentSampler();
        sampler->m_sample_count = m_sample_count;
        return sampler;
    }

    /// Seeds the RNG with the specified size, if applicable
    void seed(UInt64 seed_value) override {
        if (!m_rng)
            m_rng = std::make_unique<PCG32>();

        if constexpr (is_dynamic_array_v<Float>) {
            UInt64 idx = arange<UInt64>(seed_value.size());
            m_rng->seed(sample_tea_64(seed_value, idx),
                        sample_tea_64(idx, seed_value));
        } else {
            m_rng->seed(seed_value, PCG32_DEFAULT_STREAM + arange<UInt64>());
        }
    }

    Float next_1d(Mask active = true) override {
        if constexpr (is_dynamic_array_v<Float>) {
            if (m_rng == nullptr)
                Throw("Sampler::seed() must be invoked before using this sampler!");
            if (active.size() != 1 && active.size() != m_rng->state.size())
                Throw("Invalid mask size (%d), expected %d", active.size(), m_rng->state.size());
        }

        if constexpr (is_double_v<ScalarFloat>)
            return m_rng->next_float64(active);
        else
            return m_rng->next_float32(active);
    }

    Point2f next_2d(Mask active = true) override {
        Float f1 = next_1d(active),
              f2 = next_1d(active);
        return Point2f(f1, f2);
    }

    bool ready() const override {
        return m_rng != nullptr;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "IndependentSampler[" << std::endl
            << "  sample_count = " << m_sample_count << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    std::unique_ptr<PCG32> m_rng;
};

MTS_IMPLEMENT_CLASS_VARIANT(IndependentSampler, Sampler)
MTS_EXPORT_PLUGIN(IndependentSampler, "Independent Sampler");
NAMESPACE_END(mitsuba)
