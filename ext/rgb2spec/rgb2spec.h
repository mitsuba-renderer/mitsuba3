#pragma once

#include <stdint.h>

#if defined(__SSE4_2__) || defined(__AVX__)
#  include <immintrin.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/// How many polynomial coefficients?
#define RGB2SPEC_N_COEFFS 3

/// Underlying representation
typedef struct {
    uint32_t res;
    float *scale;
    float *data;
} RGB2Spec;

/// Load a RGB2Spec model from disk
RGB2Spec *rgb2spec_load(const char *filename);

/// Release all memory associated with a RGB2Spec model
void rgb2spec_free(RGB2Spec *model);

/// Convert an RGB value into a RGB2Spec coefficient representation
void rgb2spec_fetch(RGB2Spec *model, float rgb[3], float out[RGB2SPEC_N_COEFFS]);

/// Evaluate the model for a given wavelength
float rgb2spec_eval_precise(float coeff[RGB2SPEC_N_COEFFS], float lambda);

/// Evaluate the model for a given wavelength (fast, with recip. square root)
float rgb2spec_eval_fast(float coeff[RGB2SPEC_N_COEFFS], float lambda);

#if defined(__SSE4_2__)
    /// SSE 4.2 version -- evaluates 4 wavelengths at once
    __m128 rgb2spec_eval_sse(float coeff[RGB2SPEC_N_COEFFS], __m128 lambda);
#endif

#if defined(__AVX__)
   /// AVX version -- evaluates 8 wavelengths at once
   __m256 rgb2spec_eval_avx(float coeff[RGB2SPEC_N_COEFFS], __m256 lambda);
#endif

#if defined(__AVX512F__)
   /// AVX512 version -- evaluates 16 wavelengths at once
   __m512 rgb2spec_eval_avx512(float coeff[RGB2SPEC_N_COEFFS], __m512 lambda);
#endif

#ifdef __cplusplus
} // extern "C"
#endif
