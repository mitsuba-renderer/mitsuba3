#include <mitsuba/core/profiler.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _sampler-independent:

Independent sampler (:monosp:`independent`)
-------------------------------------------

.. pluginparameters::

 * - sample_count
   - |int|
   - Number of samples per pixel (Default: 4)
 * - seed
   - |int|
   - Seed offset (Default: 0)

The independent sampler produces a stream of independent and uniformly
distributed pseudorandom numbers. Internally, it relies on the
`PCG32 random number generator <https://www.pcg-random.org/>`_
by Melissa O’Neill.

This is the most basic sample generator; because no precautions are taken to avoid
sample clumping, images produced using this plugin will usually take longer to converge.
Looking at the figures below where samples are projected onto a 2D unit square, we see that there
are both regions that don't receive many samples (i.e. we don't know much about the behavior of
the function there), and regions where many samples are very close together (which likely have very
similar values), which will result in higher variance in the rendered image.

This sampler is initialized using a deterministic procedure, which means that subsequent runs
of Mitsuba should create the same image. In practice, when rendering with multiple threads
and/or machines, this is not true anymore, since the ordering of samples is influenced by the
operating system scheduler. Although these should be absolutely negligible, with relative errors
on the order of the machine epsilon (:math:`6\cdot 10^{-8}`) in single precision.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/sampler/independent_1024_samples.svg
   :caption: 1024 samples projected onto the first two dimensions.
.. subfigure:: ../../resources/data/docs/images/sampler/independent_64_samples_and_proj.svg
   :caption: 64 samples projected onto the first two dimensions and their
             projection on both 1D axis (top and right plot).
.. subfigend::
   :label: fig-independent-pattern

 */

template <typename Float, typename Spectrum>
class IndependentSampler final : public PCG32Sampler<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(PCG32Sampler, m_sample_count, m_base_seed, m_rng, seed, seeded)
    MTS_IMPORT_TYPES()

    IndependentSampler(const Properties &props = Properties()) : Base(props) {
        /* Can't seed yet on the GPU because we don't know yet
           how many entries will be needed. */
        if (!is_dynamic_array_v<Float>)
            seed(PCG32_DEFAULT_STATE);
    }

    ref<Sampler<Float, Spectrum>> clone() override {
        IndependentSampler *sampler = new IndependentSampler();
        sampler->m_sample_count = m_sample_count;
        sampler->m_base_seed = m_base_seed;
        return sampler;
    }

    Float next_1d(Mask active = true) override {
        Assert(seeded());
        return m_rng.template next_float<Float>(active);
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

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_VARIANT(IndependentSampler, Sampler)
MTS_EXPORT_PLUGIN(IndependentSampler, "Independent Sampler");
NAMESPACE_END(mitsuba)
