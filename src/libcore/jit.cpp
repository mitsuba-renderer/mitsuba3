#include <mitsuba/core/jit.h>

NAMESPACE_BEGIN(mitsuba)

static Jit *jit = nullptr;

Jit::Jit() : runtime() { }
Jit *Jit::getInstance() { return jit; }

void Jit::staticInitialization() { jit = new Jit(); }
void Jit::staticShutdown() { delete jit; jit = nullptr; }

NAMESPACE_END(mitsuba)
