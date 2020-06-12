#include <mitsuba/core/profiler.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _sampler-stratified:

Stratified sampler (:monosp:`stratified`)
-------------------------------------------

.. pluginparameters::

 * - sample_count
   - |int|
   - Number of samples per pixel. This number should be a square number (Default: 4)
 * - seed
   - |int|
   - Seed offset (Default: 0)
 * - jitter
   - |bool|
   - Adds additional random jitter withing the stratum (Default: True)

The stratified sample generator divides the domain into a discrete number of strata and produces
a sample within each one of them. This generally leads to less sample clumping when compared to
the independent sampler, as well as better convergence.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/sampler_independent_16spp.jpg
   :caption: Independent sampler - 16 samples per pixel
.. subfigure:: ../../resources/data/docs/images/render/sampler_stratified_16spp.jpg
   :caption: Stratified sampler - 16 samples per pixel
.. subfigend::
   :label: fig-stratified-renders

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/sampler/stratified_1024_samples.svg
   :caption: 1024 samples projected onto the first two dimensions which are well distributed
             if we compare to the :monosp:`independent` sampler.
.. subfigure:: ../../resources/data/docs/images/sampler/stratified_64_samples_and_proj.svg
   :caption: 64 samples projected in 2D and on both 1D axis (top and right plot). Every strata
             contains a single sample creating a good distribution when projected in 2D. Projections
             on both 1D axis still exhibit sample clumping which will result in higher variance, for
             instance when sampling a thin streched rectangular area light.
.. subfigend::
   :label: fig-stratified-pattern

 */

template <typename Float, typename Spectrum>
class StratifiedSampler final : public PCG32Sampler<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(PCG32Sampler, m_sample_count, m_base_seed, m_rng, seeded,
                    m_samples_per_wavefront, m_dimension_index,
                    current_sample_index, compute_per_sequence_seed)
    MTS_IMPORT_TYPES()

    StratifiedSampler(const Properties &props = Properties()) : Base(props) {
        m_jitter = props.bool_("jitter", true);

        // Make sure sample_count is a square number
        m_resolution = 1;
        while (sqr(m_resolution) < m_sample_count)
            m_resolution++;

        if (m_sample_count != sqr(m_resolution))
            Log(Warn, "Sample count should be square and power of two, rounding to %i", sqr(m_resolution));

        m_sample_count = sqr(m_resolution);
        m_inv_sample_count = rcp(ScalarFloat(m_sample_count));
        m_inv_resolution   = rcp(ScalarFloat(m_resolution));
        m_resolution_div = m_resolution;
    }

    ref<Sampler<Float, Spectrum>> clone() override {
        StratifiedSampler *sampler = new StratifiedSampler();
        sampler->m_jitter                = m_jitter;
        sampler->m_sample_count          = m_sample_count;
        sampler->m_inv_sample_count      = m_inv_sample_count;
        sampler->m_resolution            = m_resolution;
        sampler->m_inv_resolution        = m_inv_resolution;
        sampler->m_resolution_div        = m_resolution_div;
        sampler->m_samples_per_wavefront = m_samples_per_wavefront;
        sampler->m_base_seed             = m_base_seed;
        return sampler;
    }

    void seed(uint64_t seed_offset, size_t wavefront_size) override {
        Base::seed(seed_offset, wavefront_size);
        m_permutation_seed = compute_per_sequence_seed(seed_offset);
    }

    Float next_1d(Mask active = true) override {
        Assert(seeded());

        UInt32 sample_indices = current_sample_index();
        UInt32 perm_seed = m_permutation_seed + m_dimension_index++;

        // Shuffle the samples order
        UInt32 p = permute_kensler(sample_indices, m_sample_count, perm_seed, active);

        // Add a random perturbation
        Float j = m_jitter ? m_rng.template next_float<Float>(active) : 0.5f;

        return (p + j) * m_inv_sample_count;
    }

    Point2f next_2d(Mask active = true) override {
        Assert(seeded());

        UInt32 sample_indices = current_sample_index();
        UInt32 perm_seed = m_permutation_seed + m_dimension_index++;

        // Shuffle the samples order
        UInt32 p = permute_kensler(sample_indices, m_sample_count, perm_seed, active);

        // Map the index to its 2D cell
        UInt32 y = m_resolution_div(p);  // p / m_resolution
        UInt32 x = p - y * m_resolution; // p % m_resolution

        // Add a random perturbation
        Float jx = 0.5f, jy = 0.5f;
        if (m_jitter) {
            jx = m_rng.template next_float<Float>(active);
            jy = m_rng.template next_float<Float>(active);
        }

        // Construct the final 2D point
        return Point2f(x + jx, y + jy) * m_inv_resolution;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "StratifiedSampler[" << std::endl
            << "  sample_count = " << m_sample_count << std::endl
            << "  jitter = " << m_jitter << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    bool m_jitter;

    /// Stratification grid resolution
    ScalarUInt32 m_resolution;
    ScalarFloat m_inv_resolution;
    ScalarFloat m_inv_sample_count;
    enoki::divisor<ScalarUInt32> m_resolution_div;

    /// Per-sequence permutation seed
    UInt32 m_permutation_seed;
};

MTS_IMPLEMENT_CLASS_VARIANT(StratifiedSampler, Sampler)
MTS_EXPORT_PLUGIN(StratifiedSampler, "Stratified Sampler");
NAMESPACE_END(mitsuba)
