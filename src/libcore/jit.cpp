#include <mitsuba/core/jit.h>
#include <simdarray/array.h>
#include <iostream>

NAMESPACE_BEGIN(mitsuba)

static Jit *jit = nullptr;

Jit::Jit() : runtime() { }
Jit *Jit::getInstance() { return jit; }

#if defined(__GNUC__)
    __attribute__((target("sse")))
#endif
void Jit::staticInitialization() {
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

    CHECK(simd::hasAVX512ER, AVX512ER);
    CHECK(simd::hasAVX512PF, AVX512PF);
    CHECK(simd::hasAVX512CD, AVX512CD);
    CHECK(simd::hasAVX512DQ, AVX512DQ);
    CHECK(simd::hasAVX512VL, AVX512VL);
    CHECK(simd::hasAVX512BW, AVX512BW);
    CHECK(simd::hasAVX512F,  AVX512F);
    CHECK(simd::hasAVX2,     AVX2);
    CHECK(simd::hasFMA,      FMA3);
    CHECK(simd::hasF16C,     F16C);
    CHECK(simd::hasAVX,      AVX);
    CHECK(simd::hasSSE42,    SSE4_2);

    #undef CHECK
    jit = new Jit();
}

void Jit::staticShutdown() { delete jit; jit = nullptr; }

NAMESPACE_END(mitsuba)
