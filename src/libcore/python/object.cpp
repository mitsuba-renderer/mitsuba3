#include <mitsuba/python/python.h>

extern py::object py_cast(Object *o);

MTS_PY_EXPORT(Object) {
    py::class_<Class>(m, "Class", D(Class));
    py::class_<Object, ref<Object>>(m, "Object", D(Object))
        .def(py::init<>(), D(Object, Object))
        .def(py::init<const Object &>(), D(Object, Object, 2))
        .mdef(Object, ref_count)
        .mdef(Object, inc_ref)
        .mdef(Object, dec_ref, "dealloc"_a = true)
        .def("expand", [](const Object &o) -> py::list {
            auto result = o.expand();
            py::list l;
            for (Object *o: result)
                l.append(py_cast(o));
            return l;
        })
        .def("__repr__", &Object::to_string, D(Object, to_string));
}
