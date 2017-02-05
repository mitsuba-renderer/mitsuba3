#include <mitsuba/core/random.h>

NAMESPACE_BEGIN(mitsuba)

template struct MTS_EXPORT_CORE TPCG32<uint32_t>;
template struct MTS_EXPORT_CORE TPCG32<UInt32P>;

template MTS_EXPORT_CORE uint32_t                  sample_tea_32(uint32_t, uint32_t, int);
template MTS_EXPORT_CORE UInt32P                   sample_tea_32(UInt32P,  UInt32P,  int);
template MTS_EXPORT_CORE uint64_array_t<uint32_t>  sample_tea_64(uint32_t, uint32_t, int);
template MTS_EXPORT_CORE uint64_array_t<UInt32P>   sample_tea_64(UInt32P,  UInt32P,  int);
template MTS_EXPORT_CORE float32_array_t<uint32_t> sample_tea_float32(uint32_t, uint32_t, int);
template MTS_EXPORT_CORE float32_array_t<UInt32P>  sample_tea_float32(UInt32P,  UInt32P,  int);
template MTS_EXPORT_CORE float64_array_t<uint32_t> sample_tea_float64(uint32_t, uint32_t, int);
template MTS_EXPORT_CORE float64_array_t<UInt32P>  sample_tea_float64(UInt32P,  UInt32P,  int);

NAMESPACE_END(mitsuba)
