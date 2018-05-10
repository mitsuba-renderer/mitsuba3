#include <mitsuba/python/python.h>
#include <mitsuba/render/spiral.h>

MTS_PY_EXPORT(Spiral) {
    MTS_PY_CLASS(Spiral, Object)
        .def(py::init<const Film *, size_t, size_t>(),
             D(Spiral, Spiral), "film"_a, "block_size"_a = MTS_BLOCK_SIZE,
             "passes"_a = 1)
        .mdef(Spiral, reset)
        .mdef(Spiral, set_passes)
        .mdef(Spiral, next_block)
        .mdef(Spiral, max_block_size)
        .mdef(Spiral, block_count);
}
