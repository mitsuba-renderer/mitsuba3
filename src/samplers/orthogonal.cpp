#include <mitsuba/core/profiler.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _sampler-orthogonal:

Orthogonal Array sampler (:monosp:`orthogonal`)
-----------------------------------------------

.. pluginparameters::

 * - sample_count
   - |int|
   - Number of samples per pixel. This value has to be the square of a prime number. (Default: 4)

 * - strength
   - |int|
   - Orthogonal array's strength (Default: 2)

 * - seed
   - |int|
   - Seed offset (Default: 0)

 * - jitter
   - |bool|
   - Adds additional random jitter withing the substratum (Default: True)

This plugin implements the Orthogonal Array sampler generator introduced by Jarosz et al.
:cite:`jarosz19orthogonal`. It generalizes correlated multi-jittered sampling to higher dimensions
by using *orthogonal arrays (OAs)*. An OA of strength :math:`s` has the property that projecting
the generated samples to any combination of :math:`s` dimensions will always result in a well
stratified pattern. In other words, when :math:`s=2` (default value), the high-dimensional samples
are simultaneously stratified in all 2D projections as if they had been produced by correlated
multi-jittered sampling. By construction, samples produced by this generator are also well
stratified when projected on both 1D axis.

This sampler supports OA of strength other than 2, although this isn't recommended as the
stratification of 2D projections of those samples wouldn't be ensured anymore.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/sampler_independent_25spp.jpg
   :caption: Independent sampler - 25 samples per pixel
.. subfigure:: ../../resources/data/docs/images/render/sampler_orthogonal_25spp.jpg
   :caption: Orthogonal Array sampler - 25 samples per pixel
.. subfigend::
   :label: fig-orthogonal-renders

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/sampler/orthogonal_1369_samples.svg
   :caption: 1369 samples projected onto the first two dimensions.
.. subfigure:: ../../resources/data/docs/images/sampler/orthogonal_49_samples_and_proj.svg
   :caption: 49 samples projected onto the first two dimensions and their
             projection on both 1D axis (top and right plot). The pattern is well stratified
             in both 2D and 1D projections. This is true for every pair of dimensions of the
             high-dimensional samples.
.. subfigend::
   :label: fig-orthogonal-pattern

.. tabs::
    .. code-tab:: xml
        :name: orthogonal-sampler

        <sampler type="orthogonal">
            <integer name="sample_count" value="4"/>
        </sampler>

    .. code-tab:: python

        'type': 'orthogonal',
        'sample_count': '4'

 */

template <typename Float, typename Spectrum>
class OrthogonalSampler final : public PCG32Sampler<Float, Spectrum> {
public:
    MI_IMPORT_BASE(PCG32Sampler, m_sample_count, m_base_seed, m_rng, seeded,
                    m_samples_per_wavefront, m_dimension_index,
                    current_sample_index, compute_per_sequence_seed)
    MI_IMPORT_TYPES()

    OrthogonalSampler(const Properties &props) : Base(props) {
        m_jitter = props.get<bool>("jitter", true);
        m_strength = props.get<int>("strength", 2);
        set_sample_count(m_sample_count);
    }

    void set_sample_count(uint32_t spp) override {
         // Make sure m_resolution is a prime number
        auto is_prime = [](uint32_t x) {
            for (uint32_t i = 2; i <= x / 2; ++i)
                if (x % i == 0)
                    return false;
            return true;
        };

        m_resolution = 2;
        while (dr::square(m_resolution) < spp || !is_prime(m_resolution))
            m_resolution++;

        if (spp != dr::square(m_resolution))
            Log(Warn, "Sample count should be the square of a prime"
                "number, rounding to %i", dr::square(m_resolution));

        m_sample_count = dr::square(m_resolution);
        m_resolution_div = m_resolution;
    }

    ref<Sampler<Float, Spectrum>> fork() override {
        OrthogonalSampler *sampler       = new OrthogonalSampler(Properties());
        sampler->m_jitter                = m_jitter;
        sampler->m_strength              = m_strength;
        sampler->m_sample_count          = m_sample_count;
        sampler->m_resolution            = m_resolution;
        sampler->m_resolution_div        = m_resolution_div;
        sampler->m_samples_per_wavefront = m_samples_per_wavefront;
        sampler->m_base_seed             = m_base_seed;
        return sampler;
    }

    ref<Sampler<Float, Spectrum>> clone() override {
        return new OrthogonalSampler(*this);
    }

    void seed(UInt32 seed, uint32_t wavefront_size) override {
        Base::seed(seed, wavefront_size);
        m_permutation_seed = compute_per_sequence_seed(seed);
    }

    Float next_1d(Mask active = true) override {
        Assert(seeded());

        if (unlikely(m_strength != 2)) {
            return bush(current_sample_index(),
                        m_dimension_index++,
                        m_permutation_seed, active);
        } else {
            return bose(current_sample_index(),
                        m_dimension_index++,
                        m_permutation_seed, active);
        }
    }

    Point2f next_2d(Mask active = true) override {
        Float f1 = next_1d(active),
              f2 = next_1d(active);
        return Point2f(f1, f2);
    }

    void schedule_state() override {
        Base::schedule_state();
        dr::schedule(m_permutation_seed);
    }

    void traverse_1_cb_ro(void *payload, void (*fn)(void *, uint64_t)) const override {
        auto fields = dr::make_tuple(m_rng, m_dimension_index, m_permutation_seed);
        dr::traverse_1_fn_ro(fields, payload, fn);
    }

    void traverse_1_cb_rw(void *payload, uint64_t (*fn)(void *, uint64_t)) override {
        auto fields = dr::tie(m_rng, m_dimension_index, m_permutation_seed);
        dr::traverse_1_fn_rw(fields, payload, fn);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "OrthogonalSampler[" << std::endl
            << "  sample_count = " << m_sample_count << std::endl
            << "  jitter = " << m_jitter << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

private:
    /// Compute the digits of decimal value \ref i expressed in base \ref m_strength
    std::vector<UInt32> to_base_s(UInt32 i) {
        std::vector<UInt32> digits(m_strength);
        UInt32 tmp = i;
        for (size_t ii = 0; ii < m_strength; i = m_resolution_div(i), ++ii) {
            digits[ii] = tmp - i * m_resolution; // i % m_resolution
            tmp = i;
        }
        return digits;
    }

    /// Evaluate polynomial with coefficients \ref x at location arg
    UInt32 eval_poly(const std::vector<UInt32> &coef, UInt32 x) {
        UInt32 res = 0;
        for (size_t l = coef.size(); l--; )
            res = (res * x) + coef[l];
        return res;
    }

    /// Bush construction technique for orthogonal arrays
    Float bush(UInt32 i, // sample index
               UInt32 j, // dimension
               UInt32 p, // pseudo-random permutation seed
               Mask active = true) {
        uint32_t N = dr::pow(m_resolution, m_strength);
        uint32_t stm = m_resolution_div(N);

        // Convert the permuted sample index to base strength
        i = permute_kensler(i, N, p, active);
        auto i_digits = to_base_s(i);

        // Reinterpret those difits as a base-j number (evaluate the polynomial)
        UInt32 phi = eval_poly(i_digits, j);

        // Multi-jitter flavor with random perturbation
        UInt32 stratum = permute_kensler(phi % m_resolution, m_resolution, p * (j + 1) * 0x51633e2d, active);
        UInt32 sub_stratum = permute_kensler((i / m_resolution) % stm, stm, p * (j + 1) * 0x68bc21eb, active);
        Float jitter = m_jitter ? m_rng.template next_float<Float>(active) : 0.5f;
        return (stratum + (sub_stratum + jitter) / stm) / m_resolution;
    }

    /// Bose construction technique for orthogonal arrays. It only support OA of strength == 2
    Float bose(UInt32 i, // sample index
               UInt32 j, // dimension
               UInt32 p, // pseudo-random permutation seed
               Mask active = true) {

        // Permute the sample index so that samples are obtained in random order
        i = permute_kensler(i % m_sample_count, m_sample_count, p, active);

        // Map a linear index into a regular 2D grid
        UInt32 a_i0 = m_resolution_div(i);     // i / m_resolution
        UInt32 a_i1 = i - a_i0 * m_resolution; // i % m_resolution

        // Bose construction scheme
        // Linear combination of the 2D mapping (modulo the grid resolution)
        UInt32 k = dr::select((j % 2) > 0, j - 1, j + 1);
        UInt32 a_ij = (a_i0 + (j - 1) * a_i1) % m_resolution;
        UInt32 a_ik = (a_i0 + (k - 1) * a_i1) % m_resolution;
        Mask j_is_zero = j == 0u;
        dr::masked(a_ij, j_is_zero) = a_i0;
        dr::masked(a_ik, j_is_zero) = a_i1;
        Mask j_is_one  = j == 1u;
        dr::masked(a_ij, j_is_one)  = a_i1;
        dr::masked(a_ik, j_is_one)  = a_i0;

        // Correlated multi-jitter flavor with random perturbation
        UInt32 stratum     = permute_kensler(a_ij, m_resolution, p * (j + 1) * 0x51633e2d, active);
        UInt32 sub_stratum = permute_kensler(a_ik, m_resolution, p * (j + 1) * 0x68bc21eb, active);
        Float jitter = m_jitter ? m_rng.template next_float<Float>(active) : 0.5f;
        return (stratum + (sub_stratum + jitter) / m_resolution) / m_resolution;
    }

    OrthogonalSampler(const OrthogonalSampler &sampler) : Base(sampler) {
        m_jitter           = sampler.m_jitter;
        m_strength         = sampler.m_strength;
        m_resolution       = sampler.m_resolution;
        m_resolution_div   = sampler.m_resolution_div;
        m_permutation_seed = sampler.m_permutation_seed;
    }

private:
    bool m_jitter;
    uint32_t m_strength;

    /// Stratification grid resolution
    uint32_t m_resolution;
    dr::divisor<uint32_t> m_resolution_div;

    /// Per-sequence permutation seed
    UInt32 m_permutation_seed;
};

MI_IMPLEMENT_CLASS_VARIANT(OrthogonalSampler, Sampler)
MI_EXPORT_PLUGIN(OrthogonalSampler, "Orthogonal Array Sampler");
NAMESPACE_END(mitsuba)
