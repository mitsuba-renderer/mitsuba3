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

    /**
     * \brief Statically initialize the JIT runtime
     *
     * This function also does a runtime-check to ensure that the host
     * processor supports all instruction sets which were selected at compile
     * time. If not, the application is terminated via \c abort().
     */
    static void static_initialization();

    /// Release all memory used by JIT-compiled routines
    static void static_shutdown();

    static Jit *getInstance();

private:
    Jit();
    Jit(const Jit &) = delete;
};

NAMESPACE_END(mitsuba)
