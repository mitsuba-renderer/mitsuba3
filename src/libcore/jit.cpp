#include <mitsuba/core/jit.h>
#include <mitsuba/core/vector.h>
#include <iostream>

NAMESPACE_BEGIN(mitsuba)

static Jit *jit = nullptr;

Jit::Jit() : runtime() { }
Jit *Jit::getInstance() { return jit; }

void Jit::static_initialization() {
#if defined(ENOKI_X86_32) || defined(ENOKI_X86_64)
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

    CHECK(enoki::has_avx512er, AVX512ER);
    CHECK(enoki::has_avx512pf, AVX512PF);
    CHECK(enoki::has_avx512cd, AVX512CD);
    CHECK(enoki::has_avx512dq, AVX512DQ);
    CHECK(enoki::has_avx512vl, AVX512VL);
    CHECK(enoki::has_avx512bw, AVX512BW);
    CHECK(enoki::has_avx512f,  AVX512F);
    CHECK(enoki::has_avx2,     AVX2);
    CHECK(enoki::has_f16c,     F16C);
    CHECK(enoki::has_avx,      AVX);
    CHECK(enoki::has_sse42,    SSE4_2);

    #undef CHECK

    jit = new Jit();
#endif
}

void Jit::static_shutdown() {
#if defined(ENOKI_X86_32) || defined(ENOKI_X86_64)
    delete jit;
    jit = nullptr;
#endif
}

NAMESPACE_END(mitsuba)
