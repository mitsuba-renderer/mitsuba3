#include <mitsuba/render/spiral.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/tuple.h>

MI_PY_EXPORT(Spiral) {
    using Vector2u = typename Spiral::Vector2u;
    MI_PY_CLASS(Spiral, Object)
        .def(nb::init<Vector2u, Vector2u, uint32_t, uint32_t>(),
            "size"_a, "offset"_a, "block_size"_a = MI_BLOCK_SIZE, "passes"_a = 1,
            D(Spiral, Spiral))
        .def_method(Spiral, max_block_size)
        .def_method(Spiral, block_count)
        .def_method(Spiral, reset)
        .def_method(Spiral, next_block);
}
