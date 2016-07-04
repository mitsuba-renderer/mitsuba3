#include <mitsuba/core/jit.h>
#include <simdfloat/static.h>
#include <iostream>

NAMESPACE_BEGIN(mitsuba)

static Jit *jit = nullptr;

Jit::Jit() : runtime() { }
Jit *Jit::getInstance() { return jit; }

void Jit::staticInitialization() {
    jit = new Jit();

    /* Try to detect mismatches in compilation flags and processor
       capabilities early on */

    using namespace asmjit;
    auto cpu = CpuInfo::getHost();

    #define CHECK(simd_name, name) \
        if (simd_name && !cpu.hasFeature(CpuInfo::kX86Feature##name)) { \
            std::cerr << "Mitsuba was compiled with the " #name " instruction set, " \
                         "but the current processor does not support it!" \
                      << std::endl; \
            abort(); \
        }

    CHECK(simd::has_avx512dq, AVX512DQ);
    CHECK(simd::has_avx512vl, AVX512VL);
    CHECK(simd::has_avx512f,  AVX512F);
    CHECK(simd::has_avx2,     AVX2);
    CHECK(simd::has_fma,      FMA3);
    CHECK(simd::has_f16c,     F16C);
    CHECK(simd::has_avx,      AVX);
    CHECK(simd::has_sse4_2,   SSE4_2);

    #undef CHECK
}

void Jit::staticShutdown() { delete jit; jit = nullptr; }

NAMESPACE_END(mitsuba)
