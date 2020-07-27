#include <mitsuba/render/sampler.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/profiler.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Sampler implementations
// =======================================================================

MTS_VARIANT Sampler<Float, Spectrum>::Sampler(const Properties &props) {
    m_sample_count = props.size_("sample_count", 4);
    m_base_seed = props.size_("seed", 0);

    m_dimension_index = 0u;
    m_sample_index = 0;
    m_samples_per_wavefront = 1;
    m_wavefront_size = 0;
}

MTS_VARIANT Sampler<Float, Spectrum>::~Sampler() { }

MTS_VARIANT void Sampler<Float, Spectrum>::seed(uint64_t /*seed_offset*/,
                                                size_t wavefront_size) {
    m_wavefront_size = wavefront_size;
    m_dimension_index = 0u;
    m_sample_index = 0;
}

MTS_VARIANT void Sampler<Float, Spectrum>::advance() {
    Assert(m_sample_index < (m_sample_count / m_samples_per_wavefront));
    m_dimension_index = 0u;
    m_sample_index++;
}

MTS_VARIANT Float Sampler<Float, Spectrum>::next_1d(Mask) {
    NotImplementedError("next_1d");
}

MTS_VARIANT typename Sampler<Float, Spectrum>::Point2f
Sampler<Float, Spectrum>::next_2d(Mask) {
    NotImplementedError("next_2d");
}

MTS_VARIANT void
Sampler<Float, Spectrum>::set_samples_per_wavefront(uint32_t samples_per_wavefront) {
    if constexpr (is_scalar_v<Float>)
        Throw("set_samples_per_wavefront should not be used in scalar variants of the renderer.");

    m_samples_per_wavefront = samples_per_wavefront;
    if (m_sample_count % m_samples_per_wavefront != 0)
        Throw("sample_count should be a multiple of samples_per_wavefront!");
}

MTS_VARIANT typename Sampler<Float, Spectrum>::UInt32
Sampler<Float, Spectrum>::compute_per_sequence_seed(uint32_t seed_offset) const {
    UInt32 indices = arange<UInt32>(m_wavefront_size);
    UInt32 sequence_idx = m_samples_per_wavefront * (indices / m_samples_per_wavefront);
    return sample_tea_32(UInt32(m_base_seed), sequence_idx + UInt32(seed_offset));
}


MTS_VARIANT typename Sampler<Float, Spectrum>::UInt32
Sampler<Float, Spectrum>::current_sample_index() const {
    // Build an array of offsets for the sample indices in the wavefront
    UInt32 wavefront_sample_offsets = 0;
    if (m_samples_per_wavefront > 1)
        wavefront_sample_offsets = arange<UInt32>(m_wavefront_size) % m_samples_per_wavefront;

    return m_sample_index * m_samples_per_wavefront + wavefront_sample_offsets;
}

//! @}
// =======================================================================

// =======================================================================
//! @{ \name PCG32Sampler implementations
// =======================================================================

MTS_VARIANT PCG32Sampler<Float, Spectrum>::PCG32Sampler(const Properties &props)
    : Base(props) {}

MTS_VARIANT void PCG32Sampler<Float, Spectrum>::seed(uint64_t seed_offset,
                                                     size_t wavefront_size) {
    Base::seed(seed_offset, wavefront_size);

    uint64_t seed_value = m_base_seed + seed_offset;

    if constexpr (is_dynamic_array_v<Float>) {
        UInt64 idx = arange<UInt64>(wavefront_size);
        m_rng.seed(sample_tea_64(UInt64(seed_value), idx),
                   sample_tea_64(idx, UInt64(seed_value)));
    } else {
        m_rng.seed(seed_value, PCG32_DEFAULT_STREAM + arange<UInt64>());
    }
}

//! @}
// =======================================================================

MTS_IMPLEMENT_CLASS_VARIANT(Sampler, Object, "sampler")
MTS_IMPLEMENT_CLASS_VARIANT(PCG32Sampler, Sampler, "PCG32 sampler")

MTS_INSTANTIATE_CLASS(Sampler)
MTS_INSTANTIATE_CLASS(PCG32Sampler)
NAMESPACE_END(mitsuba)
