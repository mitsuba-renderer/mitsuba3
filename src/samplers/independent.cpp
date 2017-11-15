#include <mitsuba/core/properties.h>
#include <mitsuba/core/random.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

/*!\plugin{independent}{Independent sampler}
 * \order{1}
 * \parameters{
 *     \parameter{sample_count}{\Integer}{
 *       Number of samples per pixel \default{4}
 *     }
 * }
 *
 * \renderings{
 *     \unframedrendering{A projection of the first 1024 points
 *     onto the first two dimensions. Note the sample clumping.}{sampler_independent}
 * }
 *
 * The independent sampler produces a stream of independent and uniformly
 * distributed pseudorandom numbers. It relies on Enoki's implementation of
 * the PCG32 algorithm.
 *
 * This is the most basic sample generator; because no precautions are taken to
 * avoid sample clumping, images produced using this plugin will usually take
 * longer to converge.
 * In theory, this sampler is initialized using a deterministic procedure, which
 * means that subsequent runs of Mitsuba should create the same image. In
 * practice, when rendering with multiple threads and/or machines, this is not
 * true anymore, since the ordering of samples is influenced by the operating
 * system scheduler.
 */
class IndependentSampler final : public Sampler {
public:
    IndependentSampler()
        : Sampler(Properties()), m_random(), m_random_p() {
        m_sample_count = 4;
        configure();
    }

    IndependentSampler(const Properties &props)
        : Sampler(props), m_random(), m_random_p() {
        m_sample_count = props.long_("sample_count", 4);
        configure();
    }

    void configure() {
        // Fixed seed for the pseudo-random number generators.
        // TODO: allow custom RNG seed?
        m_random.seed(0, 1u);
        m_random_p.seed(enoki::index_sequence<UInt32P>() + 2u,
                        enoki::index_sequence<UInt32P>() + 3u);
    }

    ref<Sampler> clone() override {
        ref<IndependentSampler> sampler(new IndependentSampler());
        sampler->m_sample_count = m_sample_count;
        // The clone's state should be different, so that the new sampler
        // produces distinct random values.
        sampler->m_random.seed(m_random.state + 1, m_random.inc + 2);
        sampler->m_random_p.seed(m_random_p.state + 3, m_random_p.inc + 4);
        for (const auto & size : m_requests_1d)
            sampler->request_1d_array(size);
        for (const auto & size : m_requests_2d)
            sampler->request_2d_array(size);
        return sampler.get();
    }

    void generate(const Point2i &) override {
        for (size_t i = 0; i < m_requests_1d.size(); i++) {
            for (size_t j = 0; j < m_sample_count * m_requests_1d[i]; ++j)
                m_samples_1d[i][j] = next_float();
        }
        for (size_t i = 0; i < m_requests_2d.size(); i++) {
            for (size_t j = 0; j < m_sample_count * m_requests_2d[i]; ++j) {
                auto f1 = next_float();
                auto f2 = next_float();
                m_samples_2d[i][j] = Point2f(f1, f2);
            }
        }
        m_sample_index = 0;
        m_dimension_1d_array = m_dimension_2d_array = 0;
    }

    Float next_1d() override {
        return next_float();
    }
    FloatP next_1d_p(const mask_t<FloatP> &active = true) override {
        return next_float_p(active);
    }

    Point2f next_2d() override {
        // Evaluation order matters for reproducibility.
        auto f1 = next_float();
        auto f2 = next_float();
        return Point2f(f1, f2);
    }
    Point2fP next_2d_p(const mask_t<FloatP> &active = true) override {
        auto p1 = next_float_p(active);
        auto p2 = next_float_p(active);
        return Point2fP(p1, p2);
    }

    void set_film_resolution(const Vector2i &/*res*/, bool /*blocked*/) override {
        // No-op for the independent sampler.
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "IndependentSampler[" << std::endl
            << "  sample_count = " << m_sample_count << "," << std::endl
            << "  sample_index = " << m_sample_index << "," << std::endl
            << "  random = " << string::indent(m_random) << "," << std::endl
            << "  random_p = " << string::indent(m_random_p) << "," << std::endl
            << "]" << std::endl;
        return oss.str();
    }

    MTS_DECLARE_CLASS()

private:
    Float next_float() {
        #if defined(SINGLE_PRECISION)
            return m_random.next_float32();
        #else
            return m_random.next_float64();
        #endif
    }
    FloatP next_float_p(const mask_t<FloatP> &active) {
        #if defined(SINGLE_PRECISION)
            return m_random_p.next_float32(active);
        #else
            return m_random_p.next_float64(active);
        #endif
    }

protected:
    PCG32 m_random;
    PCG32P m_random_p;
};

MTS_IMPLEMENT_CLASS(IndependentSampler, Sampler);
MTS_EXPORT_PLUGIN(IndependentSampler, "Independent Sampler");
NAMESPACE_END(mitsuba)
