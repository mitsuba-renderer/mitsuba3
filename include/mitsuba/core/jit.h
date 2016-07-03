#pragma once

#include <mitsuba/mitsuba.h>
#include <asmjit/asmjit.h>
#include <mutex>

#if !defined(MTS_JIT_LOG_ASSEMBLY)
#  define MTS_JIT_LOG_ASSEMBLY 0
#endif

NAMESPACE_BEGIN(mitsuba)

struct MTS_EXPORT_CORE Jit {
    std::mutex mutex;
    asmjit::JitRuntime runtime;

    static void staticInitialization();
    static void staticShutdown();

    static Jit *getInstance();

private:
    Jit();
    Jit(const Jit &) = delete;
};

NAMESPACE_END(mitsuba)
