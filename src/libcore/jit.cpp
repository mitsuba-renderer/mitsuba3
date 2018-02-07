#include <mitsuba/core/jit.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

static Jit *jit = nullptr;

Jit::Jit() { }
Jit *Jit::get_instance() { return jit; }

void Jit::static_initialization() {
#if defined(ENOKI_X86_64)
    jit = new Jit();
#endif
}

void Jit::static_shutdown() {
#if defined(ENOKI_X86_64)
    delete jit;
    jit = nullptr;
#endif
}

NAMESPACE_END(mitsuba)
