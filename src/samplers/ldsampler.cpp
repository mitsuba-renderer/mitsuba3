#include <mitsuba/core/profiler.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/qmc.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _sampler-ldsampler:

Low discrepancy sampler (:monosp:`ldsampler`)
---------------------------------------------

This plugin implements a simple hybrid sampler that combines aspects of a Quasi-Monte Carlo sequence
with a pseudorandom number generator based on a technique proposed by Kollig and Keller
:cite:`Kollig2002Efficient`. It is a good and fast general-purpose sample generator. Other QMC
samplers exist that can generate even better distributed samples, but this comes at a higher
cost in terms of performance. This plugin is based on Mitsuba 1's default sampler (also called
:monosp:`ldsampler`).

Roughly, the idea of this sampler is that all of the individual 2D sample dimensions are first
filled using the same (0, 2)-sequence, which is then randomly scrambled and permuted using a
shuffle network. The name of this plugin stems from the fact that, by construction,
(0, 2)-sequences achieve a low
`star discrepancy <https://en.wikipedia.org/wiki/Low-discrepancy_sequence>`_,
which is a quality criterion on their spatial distribution.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/sampler_independent_16spp.jpg
   :caption: Independent sampler - 16 samples per pixel
.. subfigure:: ../../resources/data/docs/images/render/sampler_ldsampler_16spp.jpg
   :caption: Low-discrepancy sampler - 16 samples per pixel
.. subfigend::
   :label: fig-ldsampler-renders

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/sampler/ldsampler_1024_samples.svg
   :caption: 1024 samples projected onto the first two dimensions.
.. subfigure:: ../../resources/data/docs/images/sampler/ldsampler_64_samples_and_proj.svg
   :caption: A projection of the first 64 samples onto the first two dimensions and their
             projection on both 1D axis (top and right plot). The 1D stratification is perfect as
             this sampler doesn't add additional random perturbation to the sample positions.
.. subfigend::
   :label: fig-ldsampler-pattern

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/sampler/ldsampler_1024_samples_dim_32.svg
   :caption: 1024 samples projected onto the 32th and 33th dimensions, which look almost identical.
             However, note that the points have been scrambled to reduce correlations between
             dimensions.
.. subfigure:: ../../resources/data/docs/images/sampler/ldsampler_64_samples_and_proj_dim_32.svg
   :caption: A projection of the first 64 samples onto the 32th and 33th dimensions.
.. subfigend::
   :label: fig-ldsampler-pattern_dim32


.. tabs::
    .. code-tab:: xml
        :name: ldsampler-sampler

        <sampler type="ldsampler">
            <integer name="sample_count" value="64"/>
        </sampler>

    .. code-tab:: python

        'type': 'ldsampler',
        'sample_count': '64'

 */

template <typename Float, typename Spectrum>
class LowDiscrepancySampler  final : public Sampler<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sampler, m_sample_count, m_base_seed, seeded,
                    m_samples_per_wavefront, m_dimension_index,
                    current_sample_index, compute_per_sequence_seed)
    MI_IMPORT_TYPES()

    LowDiscrepancySampler (const Properties &props) : Base(props) {
        set_sample_count(m_sample_count);
    }

    void set_sample_count(uint32_t spp) override {
        // Make sure sample_count is power of two and square (e.g. 4, 16, 64, 256, 1024, ...)
        ScalarUInt32 res = 2;
        while (dr::square(res) < spp)
            res = math::round_to_power_of_two(++res);

        if (spp != dr::square(res))
            Log(Warn, "Sample count should be square and power of two, rounding to %i", dr::square(res));

        m_sample_count = dr::square(res);
    }

    ref<Sampler<Float, Spectrum>> fork() override {
        LowDiscrepancySampler  *sampler  = new LowDiscrepancySampler(Properties());
        sampler->m_sample_count          = m_sample_count;
        sampler->m_samples_per_wavefront = m_samples_per_wavefront;
        sampler->m_base_seed             = m_base_seed;
        return sampler;
    }

    ref<Sampler<Float, Spectrum>> clone() override {
        return new LowDiscrepancySampler (*this);
    }

    void seed(UInt32 seed, uint32_t wavefront_size) override {
        Base::seed(seed, wavefront_size);
        m_scramble_seed = compute_per_sequence_seed(seed);
    }

    Float next_1d(Mask /*active*/ = true) override {
        Assert(seeded());

        UInt32 sample_indices = current_sample_index();
        UInt32 perm_seed = m_scramble_seed + m_dimension_index++;

        // Shuffle the samples order
        UInt32 i = permute(sample_indices, m_sample_count, perm_seed);

        // Compute scramble value (unique per sequence)
        UInt32 scramble = sample_tea_32(m_scramble_seed, UInt32(0x48bc48eb)).first;

        return radical_inverse_2(i, scramble);
    }

    Point2f next_2d(Mask /*active*/ = true) override {
        Assert(seeded());

        UInt32 sample_indices = current_sample_index();
        UInt32 perm_seed = m_scramble_seed + m_dimension_index++;

        // Shuffle the samples order
        UInt32 i = permute(sample_indices, m_sample_count, perm_seed);

        // Compute scramble values (unique per sequence) for both axis
        auto [scramble_x, scramble_y] =
            sample_tea_32(m_scramble_seed, UInt32(0x98bc51ab));

        Float x = radical_inverse_2(i, scramble_x),
              y = sobol_2(i, scramble_y);

        return Point2f(x, y);
    }

    void schedule_state() override {
        Base::schedule_state();
        dr::schedule(m_scramble_seed);
    }

    void traverse_1_cb_ro(void *payload, void (*fn)(void *, uint64_t)) const override {
        auto fields = dr::make_tuple(m_scramble_seed, m_dimension_index);
        dr::traverse_1_fn_ro(fields, payload, fn);
    }

    void traverse_1_cb_rw(void *payload, uint64_t (*fn)(void *, uint64_t)) override {
        auto fields = dr::tie(m_scramble_seed, m_dimension_index);
        dr::traverse_1_fn_rw(fields, payload, fn);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "LowDiscrepancySampler [" << std::endl
            << "  sample_count = " << m_sample_count << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

private:
    LowDiscrepancySampler(const LowDiscrepancySampler &sampler) : Base(sampler) {
        m_scramble_seed = sampler.m_scramble_seed;
    }

    /// Per-sequence scramble seed
    UInt32 m_scramble_seed;
};

MI_IMPLEMENT_CLASS_VARIANT(LowDiscrepancySampler , Sampler)
MI_EXPORT_PLUGIN(LowDiscrepancySampler , "Low Discrepancy Sampler");
NAMESPACE_END(mitsuba)
