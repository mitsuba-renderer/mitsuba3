#include <mitsuba/python/python.h>

extern py::object py_cast(Object *o);

MTS_PY_EXPORT_VARIANTS(Object) {
    MTS_PY_CHECK_ALIAS(Class, m) {
        py::class_<Class>(m, "Class", D(Class));
    }

    MTS_PY_CHECK_ALIAS(Object, m) {
        py::class_<Object, ref<Object>>(m, "Object", D(Object))
            .def(py::init<>(), D(Object, Object))
            .def(py::init<const Object &>(), D(Object, Object, 2))
            .def_method(Object, id)
            .def_method(Object, ref_count)
            .def_method(Object, inc_ref)
            .def_method(Object, dec_ref, "dealloc"_a = true)
            .def("expand", [](const Object &o) -> py::list {
                auto result = o.expand();
                py::list l;
                for (Object *o2: result)
                    l.append(py_cast(o2));
                return l;
            })
            .def("children", [](Object &o) -> py::list {
                auto result = o.children();
                py::list l;
                for (Object *o2: result)
                    l.append(py_cast(o2));
                return l;
            })
            .def("__repr__", &Object::to_string, D(Object, to_string))
            ;
    }
}
