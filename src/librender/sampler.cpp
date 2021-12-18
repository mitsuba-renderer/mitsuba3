#include <mitsuba/render/sampler.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/profiler.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Sampler implementations
// =======================================================================

MTS_VARIANT Sampler<Float, Spectrum>::Sampler(const Properties &props)
    : Object() {
    m_sample_count = props.get<uint32_t>("sample_count", 4);
    m_base_seed = props.get<uint32_t>("seed", 0);

    m_dimension_index = ek::opaque<UInt32>(0);
    m_sample_index = ek::opaque<UInt32>(0);
    m_samples_per_wavefront = 1;
    m_wavefront_size = 0;
}

MTS_VARIANT Sampler<Float, Spectrum>::Sampler(const Sampler &sampler)
    : Object() {
    m_sample_count          = sampler.m_sample_count;
    m_base_seed             = sampler.m_base_seed;
    m_wavefront_size        = sampler.m_wavefront_size;
    m_samples_per_wavefront = sampler.m_samples_per_wavefront;
    m_dimension_index       = sampler.m_dimension_index;
    m_sample_index          = sampler.m_sample_index;
}

MTS_VARIANT Sampler<Float, Spectrum>::~Sampler() { }

MTS_VARIANT void Sampler<Float, Spectrum>::seed(uint32_t /* seed */,
                                                uint32_t wavefront_size) {
    if constexpr (ek::is_array_v<Float>) {
        // Only overwrite when specified
        if (wavefront_size != (uint32_t) -1) {
            m_wavefront_size = wavefront_size;
        } else {
            if (m_wavefront_size == 0)
                Throw("Sampler::seed(): wavefront_size should be specified!");
        }
    } else {
        ENOKI_MARK_USED(wavefront_size);
        m_wavefront_size = 1;
    }
    m_dimension_index = ek::opaque<UInt32>(0);
    m_sample_index = ek::opaque<UInt32>(0);
}

MTS_VARIANT void Sampler<Float, Spectrum>::advance() {
    m_dimension_index = ek::opaque<UInt32>(0);
    m_sample_index++;
}

MTS_VARIANT Float Sampler<Float, Spectrum>::next_1d(Mask) {
    NotImplementedError("next_1d");
}

MTS_VARIANT typename Sampler<Float, Spectrum>::Point2f
Sampler<Float, Spectrum>::next_2d(Mask) {
    NotImplementedError("next_2d");
}

MTS_VARIANT void Sampler<Float, Spectrum>::schedule_state() {
    ek::schedule(m_sample_index, m_dimension_index);
}

MTS_VARIANT
void Sampler<Float, Spectrum>::loop_put(ek::Loop<Mask> &loop) {
    loop.put(m_sample_index, m_dimension_index);
}

MTS_VARIANT void
Sampler<Float, Spectrum>::set_samples_per_wavefront(uint32_t samples_per_wavefront) {
    if constexpr (!ek::is_array_v<Float>)
        Throw("set_samples_per_wavefront should not be used in scalar variants of the renderer.");

    m_samples_per_wavefront = samples_per_wavefront;
    if (m_sample_count % m_samples_per_wavefront != 0)
        Throw("sample_count should be a multiple of samples_per_wavefront!");
}

MTS_VARIANT typename Sampler<Float, Spectrum>::UInt32
Sampler<Float, Spectrum>::compute_per_sequence_seed(uint32_t seed) const {
    UInt32 indices      = ek::arange<UInt32>(m_wavefront_size),
           sequence_idx = m_samples_per_wavefront * (indices / m_samples_per_wavefront);

    return sample_tea_32(ek::opaque<UInt32>(m_base_seed, 1),
                         sequence_idx + ek::opaque<UInt32>(seed, 1)).first;
}

MTS_VARIANT typename Sampler<Float, Spectrum>::UInt32
Sampler<Float, Spectrum>::current_sample_index() const {
    // Build an array of offsets for the sample indices in the wavefront
    UInt32 wavefront_sample_offsets = 0;
    if (m_samples_per_wavefront > 1)
        wavefront_sample_offsets = ek::arange<UInt32>(m_wavefront_size) % m_samples_per_wavefront;

    return ek::fmadd(m_sample_index, m_samples_per_wavefront,
                     wavefront_sample_offsets);
}

//! @}
// =======================================================================

// =======================================================================
//! @{ \name PCG32Sampler implementations
// =======================================================================

MTS_VARIANT PCG32Sampler<Float, Spectrum>::PCG32Sampler(const Properties &props)
    : Base(props) { }

MTS_VARIANT void PCG32Sampler<Float, Spectrum>::seed(uint32_t seed,
                                                     uint32_t wavefront_size) {
    Base::seed(seed, wavefront_size);

    uint32_t seed_value = m_base_seed + seed;

    if constexpr (ek::is_array_v<Float>) {
        UInt32 idx = ek::arange<UInt32>(m_wavefront_size),
               tmp = ek::opaque<UInt32>(seed_value);

        /* Scramble seed and stream index using the Tiny Encryption Algorithm.
           Just providing a linearly increasing sequence of integers as streams
           does not produce a sufficiently statistically independent set of RNGs */
        auto [v0, v1] = sample_tea_32(tmp, idx);

        m_rng.seed(m_wavefront_size, v0, v1);
    } else {
        m_rng.seed(1, seed_value, PCG32_DEFAULT_STREAM);
    }
}

MTS_VARIANT void PCG32Sampler<Float, Spectrum>::schedule_state() {
    Base::schedule_state();
    ek::schedule(m_rng.inc, m_rng.state);
}

MTS_VARIANT void
PCG32Sampler<Float, Spectrum>::loop_put(ek::Loop<Mask> &loop) {
    Base::loop_put(loop);
    loop.put(m_rng.state);
}

MTS_VARIANT
PCG32Sampler<Float, Spectrum>::PCG32Sampler(const PCG32Sampler &sampler)
    : Base(sampler) {
    m_rng = sampler.m_rng;
}

//! @}
// =======================================================================

MTS_IMPLEMENT_CLASS_VARIANT(Sampler, Object, "sampler")
MTS_IMPLEMENT_CLASS_VARIANT(PCG32Sampler, Sampler, "PCG32 sampler")

MTS_INSTANTIATE_CLASS(Sampler)
MTS_INSTANTIATE_CLASS(PCG32Sampler)
NAMESPACE_END(mitsuba)
