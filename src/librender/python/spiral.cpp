#include <mitsuba/python/python.h>
#include <mitsuba/render/spiral.h>

MTS_PY_EXPORT(Spiral) {
    MTS_PY_CLASS(Spiral, Object)
        .def(py::init<const Film*>(), D(Spiral, Spiral))
        .mdef(Spiral, reset)
        .mdef(Spiral, next_block)
        .mdef(Spiral, max_block_size)
        ;
}
