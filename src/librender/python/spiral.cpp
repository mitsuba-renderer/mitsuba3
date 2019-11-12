#include <mitsuba/python/python.h>
#include <mitsuba/render/spiral.h>

MTS_PY_EXPORT_VARIANTS(Spiral) {
    using Vector2i = typename Spiral::Vector2i;

    MTS_PY_CHECK_ALIAS(Spiral, m) {
        MTS_PY_CLASS(Spiral, Object)
            .def(py::init<Vector2i, Vector2i, size_t, size_t>(),
                "size"_a, "offset"_a, "block_size"_a = MTS_BLOCK_SIZE, "passes"_a = 1,
                D(Spiral, Spiral))
            .def_method(Spiral, max_block_size)
            .def_method(Spiral, block_count)
            .def_method(Spiral, reset)
            .def_method(Spiral, set_passes)
            .def_method(Spiral, next_block);
    }
}
