#include <mitsuba/python/python.h>
#include <mitsuba/render/spiral.h>

MTS_PY_EXPORT(Spiral) {
    using Vector2i = typename Spiral::Vector2i;

    MTS_PY_CLASS(Spiral, Object)
        .def(py::init<Vector2i, Vector2i, size_t, size_t>(),
             "size"_a, "offset"_a, "block_size"_a = MTS_BLOCK_SIZE, "passes"_a = 1,
             D(Spiral, Spiral))
        .mdef(Spiral, max_block_size)
        .mdef(Spiral, block_count)
        .mdef(Spiral, reset)
        .mdef(Spiral, set_passes)
        .mdef(Spiral, next_block)
        ;
}
