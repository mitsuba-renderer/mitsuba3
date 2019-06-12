#include "rgb2spec.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#define rgb2spec_min(a, b) (((a) < (b)) ? (a) : (b))
#define rgb2spec_max(a, b) (((a) > (b)) ? (a) : (b))

/// Load a RGB2Spec model from disk
RGB2Spec *rgb2spec_load(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f)
        return NULL;

    char header[4];
    if (fread(header, 4, 1, f) != 1 || memcmp(header, "SPEC", 4) != 0) {
        fclose(f);
        return NULL;
    }

    RGB2Spec *m = (RGB2Spec *) malloc(sizeof(RGB2Spec));
    if (!m || fread(&m->res, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        free(m);
        return NULL;
    }

    size_t size_scale = sizeof(float) * m->res,
           size_data  = sizeof(float) * m->res * m->res *
                        m->res * 3 * RGB2SPEC_N_COEFFS;

    m->scale = (float *) malloc(size_scale);
    m->data = (float *) malloc(size_data);

    if (!m->data || !m->scale ||
        fread(m->scale, size_scale, 1, f) != 1 ||
        fread(m->data, size_data, 1, f) != 1) {
        fclose(f);
        rgb2spec_free(m);
        return NULL;
    }

    fclose(f);
    return m;
}

/// Release all memory associated with a RGB2Spec model
void rgb2spec_free(RGB2Spec *model) {
    free(model->scale);
    free(model->data);
    free(model);
}

static int rgb2spec_find_interval(float *values, int size_, float x) {
    int left = 0,
        last_interval = size_ - 2,
        size = last_interval;

    while (size > 0) {
        int half   = size >> 1,
            middle = left + half + 1;

        if (values[middle] <= x) {
            left = middle;
            size -= half + 1;
        } else {
            size = half;
        }
    }

    return rgb2spec_min(left, last_interval);
}

/// Convert an RGB value into a RGB2Spec coefficient representation
void rgb2spec_fetch(RGB2Spec *model, float rgb_[3], float out[RGB2SPEC_N_COEFFS]) {
    /* Determine largest RGB component */
    int i = 0, res = model->res;
    float rgb[3];
    for (int j = 0; j < 3; ++j)
        rgb[j] = rgb2spec_max(rgb2spec_min(rgb_[j], 1.f), 0.f);

    for (int j = 1; j < 3; ++j)
        if (rgb[j] >= rgb[i])
            i = j;

    float z     = rgb[i],
          scale = (res - 1) / z,
          x     = rgb[(i + 1) % 3] * scale,
          y     = rgb[(i + 2) % 3] * scale;

    /* Trilinearly interpolated lookup */
    uint32_t xi = rgb2spec_min((uint32_t) x, (uint32_t) (res - 2)),
             yi = rgb2spec_min((uint32_t) y, (uint32_t) (res - 2)),
             zi = rgb2spec_find_interval(model->scale, model->res, z),
             offset = (((i * res + zi) * res + yi) * res + xi) * RGB2SPEC_N_COEFFS,
             dx = RGB2SPEC_N_COEFFS,
             dy = RGB2SPEC_N_COEFFS * res,
             dz = RGB2SPEC_N_COEFFS * res * res;

    float x1 = x - xi, x0 = 1.f - x1,
          y1 = y - yi, y0 = 1.f - y1,
          z1 = (z - model->scale[zi]) /
               (model->scale[zi + 1] - model->scale[zi]),
          z0 = 1.f - z1;

    for (int j = 0; j < RGB2SPEC_N_COEFFS; ++j) {
        out[j] = ((model->data[offset               ] * x0 +
                   model->data[offset + dx          ] * x1) * y0 +
                  (model->data[offset + dy          ] * x0 +
                   model->data[offset + dy + dx     ] * x1) * y1) * z0 +
                 ((model->data[offset + dz          ] * x0 +
                   model->data[offset + dz + dx     ] * x1) * y0 +
                  (model->data[offset + dz + dy     ] * x0 +
                   model->data[offset + dz + dy + dx] * x1) * y1) * z1;
        offset++;
    }
}

static inline float rgb2spec_fma(float a, float b, float c) {
    #if defined(__FMA__)
        // Only use fmaf() if implemented in hardware
        return fmaf(a, b, c);
    #else
        return a*b + c;
    #endif
}

float rgb2spec_eval_precise(float coeff[RGB2SPEC_N_COEFFS], float lambda) {
    float x = rgb2spec_fma(rgb2spec_fma(coeff[0], lambda, coeff[1]), lambda, coeff[2]),
          y = 1.f / sqrtf(rgb2spec_fma(x, x, 1.f));
    return rgb2spec_fma(.5f * x, y, .5f);
}

float rgb2spec_eval_fast(float coeff[RGB2SPEC_N_COEFFS], float lambda) {
#if defined(__SSE4_2__)
    float x = rgb2spec_fma(rgb2spec_fma(coeff[0], lambda, coeff[1]), lambda, coeff[2]),
          y = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(rgb2spec_fma(x, x, 1.f))));
    return rgb2spec_fma(.5f * x, y, .5f);
#else
    return rgb2spec_eval_precise(coeff, lambda);
#endif
}


#if defined(__SSE4_2__)
static inline __m128 rgb2spec_fma128(__m128 a, __m128 b, __m128 c) {
    #if defined(__FMA__)
        return _mm_fmadd_ps(a, b, c);
    #else
        /// Fallback for pre-Haswell architectures
        return _mm_add_ps(_mm_mul_ps(a, b), c);
    #endif
}

__m128 rgb2spec_eval_sse(float coeff[RGB2SPEC_N_COEFFS], __m128 lambda) {
    __m128 c0 = _mm_set1_ps(coeff[0]), c1 = _mm_set1_ps(coeff[1]),
           c2 = _mm_set1_ps(coeff[2]), h = _mm_set1_ps(.5f),
           o = _mm_set1_ps(1.f);

    __m128 x = rgb2spec_fma128(rgb2spec_fma128(c0, lambda, c1), lambda, c2),
           y = _mm_rsqrt_ps(rgb2spec_fma128(x, x, o));

    return rgb2spec_fma128(_mm_mul_ps(h, x), y, h);
}
#endif

#if defined(__AVX__)
__m256 rgb2spec_fma256(__m256 a, __m256 b, __m256 c) {
    #if defined(__FMA__)
        return _mm256_fmadd_ps(a, b, c);
    #else
        /// Fallback for pre-Haswell architectures
        return _mm256_add_ps(_mm256_mul_ps(a, b), c);
    #endif
}

__m256 rgb2spec_eval_avx(float coeff[RGB2SPEC_N_COEFFS], __m256 lambda) {
    __m256 c0 = _mm256_set1_ps(coeff[0]), c1 = _mm256_set1_ps(coeff[1]),
           c2 = _mm256_set1_ps(coeff[2]), h = _mm256_set1_ps(.5f),
           o = _mm256_set1_ps(1.f);

    __m256 x = rgb2spec_fma256(rgb2spec_fma256(c0, lambda, c1), lambda, c2),
           y = _mm256_rsqrt_ps(rgb2spec_fma256(x, x, o));

    return rgb2spec_fma256(_mm256_mul_ps(h, x), y, h);
}
#endif

#if defined(__AVX512F__)
__m512 rgb2spec_eval_avx512(float coeff[RGB2SPEC_N_COEFFS], __m512 lambda) {
    __m512 c0 = _mm512_set1_ps(coeff[0]), c1 = _mm512_set1_ps(coeff[1]),
           c2 = _mm512_set1_ps(coeff[2]), h = _mm512_set1_ps(.5f),
           o = _mm512_set1_ps(1.f);

    __m512 x = _mm512_fmadd_ps(_mm512_fmadd_ps(c0, lambda, c1), lambda, c2),
           y = _mm512_rsqrt14_ps(_mm512_fmadd_ps(x, x, o));

    return _mm512_fmadd_ps(_mm512_mul_ps(h, x), y, h);
}
#endif

